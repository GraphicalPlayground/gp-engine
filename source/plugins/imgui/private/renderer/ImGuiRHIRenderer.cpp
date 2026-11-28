// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "renderer/ImGuiRHIRenderer.hpp"
#include "commands/CommandEnums.hpp"
#include "commands/CommandListExecutor.hpp"
#include "commands/CommandListImmediate.hpp"
#include "commands/RenderPassInfo.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "device/DeviceEnums.hpp"
#include "device/DynamicRHI.hpp"
#include "imgui.h"
#include "ImGuiLog.hpp"
#include "ImGuiScopedContext.hpp"
#include "pipeline/BlendState.hpp"   // IWYU pragma: keep
#include "pipeline/BlendStateDesc.hpp"
#include "pipeline/BoundShaderState.hpp"
#include "pipeline/DepthStencilState.hpp"       // IWYU pragma: keep
#include "pipeline/DepthStencilStateDesc.hpp"
#include "pipeline/GraphicsPipelineState.hpp"   // IWYU pragma: keep
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/RasterizerState.hpp"     // IWYU pragma: keep
#include "pipeline/RasterizerStateDesc.hpp"
#include "pipeline/SamplerState.hpp"        // IWYU pragma: keep
#include "pipeline/SamplerStateDesc.hpp"
#include "pipeline/VertexDeclaration.hpp"   // IWYU pragma: keep
#include "pipeline/VertexDeclarationDesc.hpp"
#include "renderer/ImGuiShaderBytecode.inl"
#include "resources/Buffer.hpp"   // IWYU pragma: keep
#include "resources/BufferCreateDesc.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/TextureInitializer.hpp"
#include "RHIPixelFormat.hpp"
#include "shaders/PixelShader.hpp"    // IWYU pragma: keep
#include "shaders/ShaderParameters.hpp"
#include "shaders/VertexShader.hpp"   // IWYU pragma: keep
#include "sync/SyncEnums.hpp"
#include <cstdint>
#include <cstring>

namespace gp::imgui
{

namespace
{

using detail::LogImGui;
using detail::ScopedImGuiContext;

static_assert(sizeof(ImDrawIdx) == 2u, "plugin/imgui expects 16-bit ImDrawIdx (do not define ImDrawIdx in imconfig).");
static_assert(sizeof(ImDrawVert) == 20u, "plugin/imgui vertex declaration expects the default 20-byte ImDrawVert.");
static_assert(sizeof(ImTextureID) == sizeof(gp::UInt64), "plugin/imgui stores RHI texture pointers in ImTextureID.");

/// @brief Backend identifier published in `io.BackendRendererName`.
constexpr const char* kBackendRendererName = "gp-engine DynamicRHI";

/// @brief Pixel shader descriptor binding of `sTexture` (set 1, binding 0).
constexpr gp::UInt32 kTextureBindingIndex = 0u;

/// @brief Pixel shader descriptor binding of `sSampler` (set 1, binding 1).
constexpr gp::UInt32 kSamplerBindingIndex = 1u;

/// @brief Packed vertex shader globals matching the `Globals` uniform block of `ImGui.vert`.
struct ImGuiProjectionGlobals
{
    /// @brief Clip-space scale (x, y).
    gp::Float32 scale[2]{ 1.0f, 1.0f };

    /// @brief Clip-space translation (x, y).
    gp::Float32 translate[2]{ 0.0f, 0.0f };
};

static_assert(sizeof(ImGuiProjectionGlobals) == 16u, "ImGui projection globals must match the 16-byte GLSL block.");

/// @brief Sentinel registered as `ImGuiPlatformIO::DrawCallback_ResetRenderState`.
/// @details Since ImGui 1.92.8 the "reset render state" request is a renderer-provided function pointer rather than a
///          magic constant. The renderer never calls it: `drawInRenderPass()` compares `UserCallback` against its
///          address and rebinds the pipeline, viewport and vertex stream instead.
/// @param[in] drawList Unused.
/// @param[in] drawCmd Unused.
void resetRenderStateCallback([[maybe_unused]] const ImDrawList* drawList, [[maybe_unused]] const ImDrawCmd* drawCmd)
{}

/// @brief Builds the orthographic projection mapping ImGui display space onto clip space.
/// @details ImGui display space is Y-down with origin at `DisplayPos`. The RHI exposes a Y-up clip space on every
///          backend (the Vulkan backend flips its viewport), so the top edge maps to +1 and the bottom edge to -1.
/// @param[in] drawData Draw data providing display position and size.
/// @return Packed scale/translate pair.
[[nodiscard]] ImGuiProjectionGlobals makeProjection(const ImDrawData* const drawData) noexcept
{
    const gp::Float32 left = drawData->DisplayPos.x;
    const gp::Float32 top = drawData->DisplayPos.y;
    const gp::Float32 width = (drawData->DisplaySize.x > 0.0f) ? drawData->DisplaySize.x : 1.0f;
    const gp::Float32 height = (drawData->DisplaySize.y > 0.0f) ? drawData->DisplaySize.y : 1.0f;

    ImGuiProjectionGlobals globals{};
    globals.scale[0] = 2.0f / width;
    globals.scale[1] = -2.0f / height;
    globals.translate[0] = -1.0f - left * globals.scale[0];
    globals.translate[1] = 1.0f - top * globals.scale[1];
    return globals;
}

/// @brief Encodes a texture slot index into `ImTextureData::BackendUserData` (0 is reserved for "no slot").
/// @param[in] slotIndex Zero-based slot index.
/// @return Opaque user data pointer.
[[nodiscard]] void* encodeSlot(const gp::USize slotIndex) noexcept
{
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(slotIndex + 1u));
}

/// @brief Decodes a texture slot index previously stored with `encodeSlot()`.
/// @param[in] userData Opaque user data pointer.
/// @return Zero-based slot index.
[[nodiscard]] gp::USize decodeSlot(void* const userData) noexcept
{
    return static_cast<gp::USize>(reinterpret_cast<std::uintptr_t>(userData)) - 1u;
}

/// @brief Clamps a float pixel coordinate into `[0, limit]` and converts it to an unsigned pixel index.
/// @param[in] value Pixel coordinate.
/// @param[in] limit Upper bound (render target extent).
/// @return Clamped unsigned coordinate.
[[nodiscard]] gp::UInt32 clampToPixels(const gp::Float32 value, const gp::Float32 limit) noexcept
{
    const gp::Float32 clamped = (value < 0.0f) ? 0.0f : ((value > limit) ? limit : value);
    return static_cast<gp::UInt32>(clamped);
}

}   // namespace

ImGuiRHIRenderer::ImGuiRHIRenderer() noexcept = default;

ImGuiRHIRenderer::~ImGuiRHIRenderer() noexcept
{
    shutdown();
}

bool ImGuiRHIRenderer::initialize(const ImGuiRendererDesc& desc) noexcept
{
    ImGuiContext* const context = ImGui::GetCurrentContext();
    if (context == nullptr || m_context != nullptr)
    {
        GP_LOG_ERROR(LogImGui, "ImGuiRHIRenderer::initialize() requires a current ImGui context and no prior init.");
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.BackendRendererUserData != nullptr)
    {
        GP_LOG_ERROR(LogImGui, "An ImGui renderer backend is already bound to the current context.");
        return false;
    }

    gp::rhi::DynamicRHI* const activeRHI = gp::rhi::getDynamicRHI();
    if (activeRHI == nullptr || activeRHI->getInterfaceType() != gp::rhi::InterfaceType::Vulkan)
    {
        GP_LOG_WARNING(
            LogImGui,
            "ImGui renderer unavailable: embedded 'GPVS' shaders require the Vulkan RHI (active='{}').",
            (activeRHI != nullptr) ? activeRHI->getName() : gp::StringView("None")
        );
        return false;
    }

    m_desc = desc;

    // 1. Shaders: compiled offline by tools/compile_imgui_shaders.py and embedded, so no VFS mount is required.
    m_vertexShader = activeRHI->createVertexShader(
        gp::ConstVectorView<gp::UInt8>(
            detail::kImGuiVertexShaderBytecode, static_cast<gp::USize>(sizeof(detail::kImGuiVertexShaderBytecode))
        ),
        gp::rhi::shader::ShaderHash(detail::kImGuiVertexShaderHashLow, detail::kImGuiVertexShaderHashHigh)
    );
    m_pixelShader = activeRHI->createPixelShader(
        gp::ConstVectorView<gp::UInt8>(
            detail::kImGuiPixelShaderBytecode, static_cast<gp::USize>(sizeof(detail::kImGuiPixelShaderBytecode))
        ),
        gp::rhi::shader::ShaderHash(detail::kImGuiPixelShaderHashLow, detail::kImGuiPixelShaderHashHigh)
    );

    // 2. Vertex layout mirrors ImDrawVert: float2 pos @0, float2 uv @8, RGBA8 unorm color @16.
    const gp::UInt16 stride = static_cast<gp::UInt16>(sizeof(ImDrawVert));
    gp::rhi::pipeline::VertexDeclarationElementList vertexElements{};
    vertexElements.pushBack(
        gp::rhi::pipeline::VertexElement(
            0u,
            static_cast<gp::UInt8>(offsetof(ImDrawVert, pos)),
            gp::rhi::pipeline::VertexElementType::Float2,
            0u,
            stride
        )
    );
    vertexElements.pushBack(
        gp::rhi::pipeline::VertexElement(
            0u,
            static_cast<gp::UInt8>(offsetof(ImDrawVert, uv)),
            gp::rhi::pipeline::VertexElementType::Float2,
            1u,
            stride
        )
    );
    vertexElements.pushBack(
        gp::rhi::pipeline::VertexElement(
            0u,
            static_cast<gp::UInt8>(offsetof(ImDrawVert, col)),
            gp::rhi::pipeline::VertexElementType::UByte4N,
            2u,
            stride
        )
    );
    m_vertexDeclaration = activeRHI->createVertexDeclaration(vertexElements);

    // 3. Fixed-function state: premultiplied-style "over" blending on color, accumulate coverage on alpha,
    //    no culling (ImGui emits both windings), no depth.
    m_blendState = activeRHI->createBlendState(
        gp::rhi::pipeline::BlendStateDesc(
            gp::rhi::pipeline::RenderTargetBlendDesc(
                gp::rhi::pipeline::BlendOperation::Add,
                gp::rhi::pipeline::BlendFactor::SourceAlpha,
                gp::rhi::pipeline::BlendFactor::InverseSourceAlpha,
                gp::rhi::pipeline::BlendOperation::Add,
                gp::rhi::pipeline::BlendFactor::One,
                gp::rhi::pipeline::BlendFactor::InverseSourceAlpha
            )
        )
    );
    m_rasterizerState = activeRHI->createRasterizerState(
        gp::rhi::pipeline::RasterizerStateDesc(
            gp::rhi::pipeline::RasterizerFillMode::Solid, gp::rhi::pipeline::RasterizerCullMode::None, false
        )
    );
    m_depthStencilState = activeRHI->createDepthStencilState(
        gp::rhi::pipeline::DepthStencilStateDesc(false, gp::rhi::pipeline::CompareFunction::Always)
    );
    m_samplerState = activeRHI->createSamplerState(
        gp::rhi::pipeline::SamplerStateDesc(
            gp::rhi::pipeline::SamplerFilter::Bilinear,
            gp::rhi::pipeline::SamplerAddressMode::Clamp,
            gp::rhi::pipeline::SamplerAddressMode::Clamp,
            gp::rhi::pipeline::SamplerAddressMode::Clamp
        )
    );

    gp::rhi::command::CommandListImmediate& cmdList = gp::rhi::command::getImmediateCommandList();
    const bool hasGeometry = ensureGeometryCapacity(cmdList, m_desc.initialVertexCapacity, m_desc.initialIndexCapacity);

    if (!hasGeometry || !m_vertexShader.isValid() || !m_pixelShader.isValid() || !m_vertexDeclaration.isValid() ||
        !m_blendState.isValid() || !m_rasterizerState.isValid() || !m_depthStencilState.isValid() ||
        !m_samplerState.isValid())
    {
        GP_LOG_ERROR(LogImGui, "Failed to create ImGui RHI shaders, states or geometry buffers.");
        m_context = context;
        shutdown();
        return false;
    }

    m_context = context;
    io.BackendRendererName = kBackendRendererName;
    io.BackendRendererUserData = this;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    ImGui::GetPlatformIO().DrawCallback_ResetRenderState = &resetRenderStateCallback;

    GP_LOG_INFO(LogImGui, "ImGui RHI renderer initialized on '{}'.", activeRHI->getName());
    return true;
}

void ImGuiRHIRenderer::shutdown() noexcept
{
    if (m_context == nullptr)
    {
        return;
    }

    if (gp::rhi::hasValidDynamicRHI())
    {
        // Textures may still be referenced by frames in flight; wait once so every release below is final.
        gp::rhi::command::getImmediateCommandList().blockUntilGPUIdle(true);
    }

    {
        const ScopedImGuiContext scope(m_context);

        // Destroy textures this context owns alone. Atlases shared with other contexts (RefCount > 1) are left to
        // the last renderer alive, matching the contract of the official ImGui backends.
        for (ImTextureData* const textureData: ImGui::GetPlatformIO().Textures)
        {
            if (textureData->RefCount == 1 && textureData->BackendUserData != nullptr)
            {
                destroyTexture(textureData);
            }
        }

        ImGuiIO& io = ImGui::GetIO();
        if (io.BackendRendererUserData == this)
        {
            io.BackendRendererName = nullptr;
            io.BackendRendererUserData = nullptr;
            io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
            ImGui::GetPlatformIO().DrawCallback_ResetRenderState = nullptr;
        }
    }

    m_textureSlots.clear();
    m_residentTextures = 0u;

    m_pipelineState.reset();
    m_vertexBuffer.reset();
    m_indexBuffer.reset();
    m_samplerState.reset();
    m_depthStencilState.reset();
    m_rasterizerState.reset();
    m_blendState.reset();
    m_vertexDeclaration.reset();
    m_pixelShader.reset();
    m_vertexShader.reset();

    m_vertexCapacity = 0u;
    m_indexCapacity = 0u;
    m_cachedColorFormat = gp::math::PixelFormat::Unknown;
    m_cachedDepthFormat = gp::math::PixelFormat::Unknown;
    m_cachedTargetCount = 0u;
    m_cachedSampleCount = 0u;
    m_preparedDrawData = nullptr;
    m_stats = ImGuiRendererStats{};
    m_context = nullptr;
}

gp::UInt64 ImGuiRHIRenderer::toTextureId(gp::rhi::resource::Texture* const texture) noexcept
{
    return static_cast<gp::UInt64>(reinterpret_cast<std::uintptr_t>(texture));
}

void ImGuiRHIRenderer::prepareDrawData(
    gp::rhi::command::CommandListImmediate& cmdList, ImDrawData* const drawData
) noexcept
{
    m_preparedDrawData = nullptr;
    if (m_context == nullptr || drawData == nullptr)
    {
        return;
    }

    GP_CHECKF(cmdList.isOutsideRenderPass(), "ImGuiRHIRenderer::prepareDrawData() must run outside a render pass.");

    m_stats.textureRequests = 0u;
    m_stats.drawCalls = 0u;

    // Phase 1a: texture requests. ImGui 1.92+ no longer hands the backend a finished font atlas; it streams
    // create / partial-update / destroy requests as glyphs are rasterized on demand at any size.
    if (drawData->Textures != nullptr)
    {
        for (ImTextureData* const textureData: *drawData->Textures)
        {
            if (textureData->Status != ImTextureStatus_OK)
            {
                updateTexture(cmdList, textureData);
            }
        }
    }

    m_stats.residentTextures = m_residentTextures;
    m_stats.vertexCount = static_cast<gp::UInt32>(drawData->TotalVtxCount);
    m_stats.indexCount = static_cast<gp::UInt32>(drawData->TotalIdxCount);

    if (drawData->TotalVtxCount <= 0 || drawData->TotalIdxCount <= 0)
    {
        m_preparedDrawData = drawData;
        return;
    }

    // Phase 1b: geometry. All draw lists are packed back-to-back into one vertex and one index buffer; draw
    // commands later address their slice through baseVertexIndex / startIndex, so binding happens once per frame.
    const gp::UInt32 vertexCount = static_cast<gp::UInt32>(drawData->TotalVtxCount);
    const gp::UInt32 indexCount = static_cast<gp::UInt32>(drawData->TotalIdxCount);
    if (!ensureGeometryCapacity(cmdList, vertexCount, indexCount))
    {
        return;
    }

    const gp::UInt32 vertexBytes = vertexCount * static_cast<gp::UInt32>(sizeof(ImDrawVert));
    const gp::UInt32 indexBytes = indexCount * static_cast<gp::UInt32>(sizeof(ImDrawIdx));
    void* const vertexMemory =
        cmdList.lockBuffer(m_vertexBuffer.get(), 0u, vertexBytes, gp::rhi::resource::ResourceLockMode::WriteOnly);
    void* const indexMemory =
        cmdList.lockBuffer(m_indexBuffer.get(), 0u, indexBytes, gp::rhi::resource::ResourceLockMode::WriteOnly);

    if (vertexMemory != nullptr && indexMemory != nullptr)
    {
        ImDrawVert* vertexCursor = static_cast<ImDrawVert*>(vertexMemory);
        ImDrawIdx* indexCursor = static_cast<ImDrawIdx*>(indexMemory);
        for (const ImDrawList* const drawList: drawData->CmdLists)
        {
            const gp::USize listVertexBytes = static_cast<gp::USize>(drawList->VtxBuffer.Size) * sizeof(ImDrawVert);
            const gp::USize listIndexBytes = static_cast<gp::USize>(drawList->IdxBuffer.Size) * sizeof(ImDrawIdx);
            std::memcpy(vertexCursor, drawList->VtxBuffer.Data, listVertexBytes);
            std::memcpy(indexCursor, drawList->IdxBuffer.Data, listIndexBytes);
            vertexCursor += drawList->VtxBuffer.Size;
            indexCursor += drawList->IdxBuffer.Size;
        }
    }
    else
    {
        GP_LOG_ERROR(
            LogImGui, "Failed to map ImGui geometry buffers ({} vertices, {} indices).", vertexCount, indexCount
        );
    }

    if (vertexMemory != nullptr)
    {
        cmdList.unlockBuffer(m_vertexBuffer.get());
    }
    if (indexMemory != nullptr)
    {
        cmdList.unlockBuffer(m_indexBuffer.get());
    }

    if (vertexMemory != nullptr && indexMemory != nullptr)
    {
        m_preparedDrawData = drawData;
    }
}

void ImGuiRHIRenderer::drawInRenderPass(
    gp::rhi::command::CommandListImmediate& cmdList, ImDrawData* const drawData
) noexcept
{
    if (m_context == nullptr || drawData == nullptr || drawData->CmdLists.Size == 0)
    {
        return;
    }

    if (m_preparedDrawData != drawData)
    {
        GP_LOG_WARNING(LogImGui, "ImGui draw data was not prepared this frame; call prepareDrawData() first.");
        return;
    }

    GP_CHECKF(cmdList.isInsideRenderPass(), "ImGuiRHIRenderer::drawInRenderPass() requires an open render pass.");

    const gp::Float32 framebufferWidth = drawData->DisplaySize.x * drawData->FramebufferScale.x;
    const gp::Float32 framebufferHeight = drawData->DisplaySize.y * drawData->FramebufferScale.y;
    if (framebufferWidth <= 0.0f || framebufferHeight <= 0.0f)
    {
        return;
    }

    const gp::UInt32 targetWidth = static_cast<gp::UInt32>(framebufferWidth);
    const gp::UInt32 targetHeight = static_cast<gp::UInt32>(framebufferHeight);
    setupRenderState(cmdList, drawData, targetWidth, targetHeight);
    if (!m_pipelineState.isValid())
    {
        return;
    }

    const ImGuiProjectionGlobals projection = makeProjection(drawData);
    const ImVec2 clipOffset = drawData->DisplayPos;
    const ImVec2 clipScale = drawData->FramebufferScale;

    gp::UInt32 globalVertexOffset = 0u;
    gp::UInt32 globalIndexOffset = 0u;
    for (const ImDrawList* const drawList: drawData->CmdLists)
    {
        for (const ImDrawCmd& drawCmd: drawList->CmdBuffer)
        {
            if (drawCmd.UserCallback != nullptr)
            {
                if (drawCmd.UserCallback == &resetRenderStateCallback)
                {
                    setupRenderState(cmdList, drawData, targetWidth, targetHeight);
                }
                else
                {
                    drawCmd.UserCallback(drawList, &drawCmd);
                }
                continue;
            }

            // Clip rectangles are in display space; convert to framebuffer pixels and clamp to the target.
            const gp::UInt32 minX = clampToPixels((drawCmd.ClipRect.x - clipOffset.x) * clipScale.x, framebufferWidth);
            const gp::UInt32 minY = clampToPixels((drawCmd.ClipRect.y - clipOffset.y) * clipScale.y, framebufferHeight);
            const gp::UInt32 maxX = clampToPixels((drawCmd.ClipRect.z - clipOffset.x) * clipScale.x, framebufferWidth);
            const gp::UInt32 maxY = clampToPixels((drawCmd.ClipRect.w - clipOffset.y) * clipScale.y, framebufferHeight);
            if (maxX <= minX || maxY <= minY || drawCmd.ElemCount == 0u)
            {
                continue;
            }

            gp::rhi::resource::Texture* const texture =
                reinterpret_cast<gp::rhi::resource::Texture*>(static_cast<std::uintptr_t>(drawCmd.GetTexID()));
            if (texture == nullptr)
            {
                continue;
            }

            cmdList.setScissorRect(true, minX, minY, maxX, maxY);

            gp::rhi::shader::BatchedShaderParameters& vertexParams = cmdList.getScratchShaderParameters();
            vertexParams.setShaderParameter(0u, 0u, static_cast<gp::UInt32>(sizeof(projection)), &projection);
            cmdList.setBatchedShaderParameters(m_vertexShader.get(), vertexParams);

            gp::rhi::shader::BatchedShaderParameters& pixelParams = cmdList.getScratchShaderParameters();
            pixelParams.setShaderTexture(kTextureBindingIndex, texture);
            pixelParams.setShaderSampler(kSamplerBindingIndex, m_samplerState.get());
            cmdList.setBatchedShaderParameters(m_pixelShader.get(), pixelParams);

            const gp::UInt32 listVertexCount = static_cast<gp::UInt32>(drawList->VtxBuffer.Size);
            cmdList.drawIndexedPrimitive(
                m_indexBuffer.get(),
                static_cast<gp::Int32>(globalVertexOffset + drawCmd.VtxOffset),
                0u,
                listVertexCount - drawCmd.VtxOffset,
                globalIndexOffset + drawCmd.IdxOffset,
                drawCmd.ElemCount / 3u,
                1u
            );
            ++m_stats.drawCalls;
        }

        globalVertexOffset += static_cast<gp::UInt32>(drawList->VtxBuffer.Size);
        globalIndexOffset += static_cast<gp::UInt32>(drawList->IdxBuffer.Size);
    }

    cmdList.setScissorRect(false, 0u, 0u, 0u, 0u);
    m_preparedDrawData = nullptr;
}

void ImGuiRHIRenderer::renderDrawData(
    gp::rhi::command::CommandListImmediate& cmdList,
    ImDrawData* const drawData,
    gp::rhi::resource::Texture* const renderTarget
) noexcept
{
    if (m_context == nullptr || drawData == nullptr || renderTarget == nullptr)
    {
        return;
    }

    prepareDrawData(cmdList, drawData);
    if (drawData->CmdLists.Size == 0 || m_preparedDrawData != drawData)
    {
        return;
    }

    const gp::rhi::command::RenderPassInfo passInfo(renderTarget, gp::rhi::command::RenderTargetActions::LoadStore);
    cmdList.beginRenderPass(passInfo, "ImGui");
    drawInRenderPass(cmdList, drawData);
    cmdList.endRenderPass();
}

void ImGuiRHIRenderer::updateTexture(
    gp::rhi::command::CommandListImmediate& cmdList, ImTextureData* const textureData
) noexcept
{
    if (textureData->Status == ImTextureStatus_WantCreate)
    {
        GP_CHECKF(
            textureData->TexID == ImTextureID_Invalid && textureData->BackendUserData == nullptr,
            "ImGui requested creation of a texture that is already resident."
        );
        GP_CHECKF(
            textureData->Format == ImTextureFormat_RGBA32, "plugin/imgui only supports ImTextureFormat_RGBA32 textures."
        );

        const gp::rhi::resource::TextureCreateDesc createDesc =
            gp::rhi::resource::TextureCreateDesc::create2D(
                "ImGuiTexture", textureData->Width, textureData->Height, gp::rhi::PixelFormat::R8G8B8A8_UNorm
            )
                .setFlags(gp::rhi::resource::TextureCreateFlags::ShaderResource)
                .setInitialState(gp::rhi::sync::ResourceAccess::SRVGraphics);

        gp::rhi::resource::TextureRef texture = cmdList.createTexture(createDesc);
        if (!texture.isValid())
        {
            GP_LOG_ERROR(LogImGui, "Failed to create ImGui texture ({}x{}).", textureData->Width, textureData->Height);
            return;
        }

        cmdList.updateTexture2D(
            texture.get(),
            0u,
            gp::rhi::resource::UpdateTextureRegion2D(
                0u, 0u, 0, 0, static_cast<gp::UInt32>(textureData->Width), static_cast<gp::UInt32>(textureData->Height)
            ),
            static_cast<gp::UInt32>(textureData->GetPitch()),
            static_cast<const gp::UInt8*>(textureData->GetPixels())
        );

        // Reuse a free slot before growing the table, so long sessions with atlas rebuilds stay bounded.
        gp::USize slotIndex = m_textureSlots.size();
        for (gp::USize i = 0u; i < m_textureSlots.size(); ++i)
        {
            if (!m_textureSlots[i].texture.isValid())
            {
                slotIndex = i;
                break;
            }
        }
        if (slotIndex == m_textureSlots.size())
        {
            m_textureSlots.pushBack(TextureSlot{});
        }

        textureData->SetTexID(static_cast<ImTextureID>(toTextureId(texture.get())));
        textureData->BackendUserData = encodeSlot(slotIndex);
        m_textureSlots[slotIndex].texture = texture;
        textureData->SetStatus(ImTextureStatus_OK);

        ++m_residentTextures;
        ++m_stats.textureRequests;
        return;
    }

    if (textureData->Status == ImTextureStatus_WantUpdates)
    {
        gp::rhi::resource::Texture* const texture =
            reinterpret_cast<gp::rhi::resource::Texture*>(static_cast<std::uintptr_t>(textureData->TexID));
        if (texture != nullptr)
        {
            // Source offsets address the full CPU-side atlas; the RHI copies the sub-rectangle into staging memory
            // immediately, so ImGui is free to keep writing glyphs into its pixel buffer afterwards.
            const gp::UInt32 pitch = static_cast<gp::UInt32>(textureData->GetPitch());
            const gp::UInt8* const pixels = static_cast<const gp::UInt8*>(textureData->GetPixels());
            for (const ImTextureRect& rect: textureData->Updates)
            {
                cmdList.updateTexture2D(
                    texture,
                    0u,
                    gp::rhi::resource::UpdateTextureRegion2D(
                        rect.x, rect.y, static_cast<gp::Int32>(rect.x), static_cast<gp::Int32>(rect.y), rect.w, rect.h
                    ),
                    pitch,
                    pixels
                );
            }
        }
        textureData->SetStatus(ImTextureStatus_OK);
        ++m_stats.textureRequests;
        return;
    }

    if (textureData->Status == ImTextureStatus_WantDestroy && textureData->UnusedFrames > 0)
    {
        // Releasing the TextureRef hands the resource to the RHI deferred deletion queue, which keeps it alive until
        // every frame that may still sample it has retired on the GPU.
        destroyTexture(textureData);
        ++m_stats.textureRequests;
    }
}

void ImGuiRHIRenderer::destroyTexture(ImTextureData* const textureData) noexcept
{
    if (textureData->BackendUserData != nullptr)
    {
        const gp::USize slotIndex = decodeSlot(textureData->BackendUserData);
        if (slotIndex < m_textureSlots.size() && m_textureSlots[slotIndex].texture.isValid())
        {
            m_textureSlots[slotIndex].texture.reset();
            if (m_residentTextures > 0u)
            {
                --m_residentTextures;
            }
        }
    }

    textureData->SetTexID(ImTextureID_Invalid);
    textureData->BackendUserData = nullptr;
    textureData->SetStatus(ImTextureStatus_Destroyed);
}

bool ImGuiRHIRenderer::ensureGeometryCapacity(
    gp::rhi::command::CommandListImmediate& cmdList, const gp::UInt32 vertexCount, const gp::UInt32 indexCount
) noexcept
{
    if (!m_vertexBuffer.isValid() || vertexCount > m_vertexCapacity)
    {
        const gp::UInt32 capacity = (m_vertexBuffer.isValid()) ? vertexCount + m_desc.growthSlack : vertexCount;
        gp::rhi::resource::BufferCreateDesc vertexDesc =
            gp::rhi::resource::BufferCreateDesc::createVertex<ImDrawVert>("ImGuiVertexBuffer", capacity);
        vertexDesc.addUsage(gp::rhi::resource::BufferUsageFlags::Dynamic).determineInitialState();
        m_vertexBuffer = cmdList.createBuffer(vertexDesc);
        m_vertexCapacity = m_vertexBuffer.isValid() ? capacity : 0u;
    }

    if (!m_indexBuffer.isValid() || indexCount > m_indexCapacity)
    {
        const gp::UInt32 capacity = (m_indexBuffer.isValid()) ? indexCount + m_desc.growthSlack : indexCount;
        gp::rhi::resource::BufferCreateDesc indexDesc =
            gp::rhi::resource::BufferCreateDesc::createIndex<ImDrawIdx>("ImGuiIndexBuffer", capacity);
        indexDesc.addUsage(gp::rhi::resource::BufferUsageFlags::Dynamic).determineInitialState();
        m_indexBuffer = cmdList.createBuffer(indexDesc);
        m_indexCapacity = m_indexBuffer.isValid() ? capacity : 0u;
    }

    m_stats.vertexCapacity = m_vertexCapacity;
    m_stats.indexCapacity = m_indexCapacity;

    if (!m_vertexBuffer.isValid() || !m_indexBuffer.isValid())
    {
        GP_LOG_ERROR(
            LogImGui, "Failed to allocate ImGui geometry buffers ({} vertices, {} indices).", vertexCount, indexCount
        );
        return false;
    }
    return true;
}

void ImGuiRHIRenderer::setupRenderState(
    gp::rhi::command::CommandListImmediate& cmdList,
    [[maybe_unused]] const ImDrawData* const drawData,
    const gp::UInt32 framebufferWidth,
    const gp::UInt32 framebufferHeight
) noexcept
{
    gp::rhi::pipeline::GraphicsPipelineState* const pipelineState = acquirePipelineState(cmdList);
    if (pipelineState == nullptr)
    {
        return;
    }

    cmdList.setGraphicsPipelineState(pipelineState, 0u, true);
    cmdList.setViewport(
        0.0f, 0.0f, 0.0f, static_cast<gp::Float32>(framebufferWidth), static_cast<gp::Float32>(framebufferHeight), 1.0f
    );
    cmdList.setStreamSource(0u, m_vertexBuffer.get(), 0u);
}

gp::rhi::pipeline::GraphicsPipelineState*
    ImGuiRHIRenderer::acquirePipelineState(gp::rhi::command::CommandListImmediate& cmdList) noexcept
{
    gp::rhi::pipeline::GraphicsPipelineStateInitializer psoInit{};
    psoInit.boundShaderState =
        gp::rhi::pipeline::BoundShaderStateInput(m_vertexDeclaration.get(), m_vertexShader.get(), m_pixelShader.get());
    psoInit.blendState = m_blendState.get();
    psoInit.rasterizerState = m_rasterizerState.get();
    psoInit.depthStencilState = m_depthStencilState.get();
    psoInit.primitiveType = gp::rhi::pipeline::PrimitiveType::TriangleList;
    cmdList.applyCachedRenderTargets(psoInit);

    // Deconstructive note: a PSO is baked against render-pass attachment formats. ImGui may be drawn into a pass
    // with or without depth (inside the scene pass, or in its own overlay pass), so the cache key is the attachment
    // signature rather than "created once".
    const gp::math::PixelFormat colorFormat = psoInit.renderTargetFormats[0];
    const bool isCacheHit = m_pipelineState.isValid() && colorFormat == m_cachedColorFormat &&
                            psoInit.depthStencilTargetFormat == m_cachedDepthFormat &&
                            psoInit.renderTargetsEnabled == m_cachedTargetCount &&
                            psoInit.numSamples == m_cachedSampleCount;
    if (isCacheHit)
    {
        return m_pipelineState.get();
    }

    gp::rhi::DynamicRHI* const activeRHI = gp::rhi::getDynamicRHI();
    if (activeRHI == nullptr)
    {
        return nullptr;
    }

    m_pipelineState = activeRHI->createGraphicsPipelineState(psoInit);
    if (!m_pipelineState.isValid())
    {
        GP_LOG_ERROR(LogImGui, "Failed to create the ImGui graphics pipeline state.");
        return nullptr;
    }

    m_cachedColorFormat = colorFormat;
    m_cachedDepthFormat = psoInit.depthStencilTargetFormat;
    m_cachedTargetCount = psoInit.renderTargetsEnabled;
    m_cachedSampleCount = psoInit.numSamples;
    return m_pipelineState.get();
}

}   // namespace gp::imgui
