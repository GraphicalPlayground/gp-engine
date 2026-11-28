// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "core/ResourceForward.hpp"
#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "ImGuiForward.hpp"
#include "ImGuiPlugin.hpp"
#include "maths/color/PixelFormatEnums.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "pipeline/PipelineForward.hpp"
#include "renderer/ImGuiRendererDesc.hpp"
#include "shaders/ShaderForward.hpp"

namespace gp::imgui
{

/// @brief Dear ImGui *renderer* backend that records ImGui draw data through the engine `DynamicRHI` command lists.
/// @details The frame is split into two phases, the same way production renderers separate *upload* from *raster*
///          work (render passes on tile-based GPUs and Vulkan forbid transfer commands inside a pass):
///          1. `prepareDrawData()` runs **outside** any render pass. It honors ImGui's texture requests
///             (`ImGuiBackendFlags_RendererHasTextures`: create / incremental update / destroy of font atlas pages)
///             with `createTexture()` + `updateTexture2D()`, then streams every draw list's vertices and indices into
///             two grow-on-demand `Dynamic` buffers with a single `WriteOnly` lock each. On Vulkan a `WriteOnly`
///             lock of a dynamic buffer *renames* the allocation, so the CPU never stalls on frames still in flight.
///          2. `drawInRenderPass()` runs **inside** the caller's render pass (any color target, with or without a
///             depth attachment). It binds one cached PSO, sets the orthographic projection as 16 bytes of packed
///             globals, and issues one `drawIndexedPrimitive()` per `ImDrawCmd`, with per-command scissor and texture.
///          `renderDrawData()` chains both phases around its own `LoadStore` render pass for the common case.
///
///          Textures: `ImTextureID` values are raw `gp::rhi::resource::Texture*` pointers. ImGui-owned textures keep
///          a strong `TextureRef` in an internal slot table; application textures passed to `ImGui::Image()` through
///          `toTextureId()` are borrowed and must stay alive (and in an SRV state) until the frame is submitted.
/// @note Requires the Vulkan RHI: shaders are embedded as pre-compiled `'GPVS'` SPIR-V blobs. `initialize()` fails
///       gracefully on other backends. All methods must run on the thread recording the immediate command list.
class GP_PLUGIN_IMGUI_API ImGuiRHIRenderer final
{
public:
    /// @brief Constructs an uninitialized renderer.
    ImGuiRHIRenderer() noexcept;

    /// @brief Releases all GPU resources if the renderer is still initialized.
    ~ImGuiRHIRenderer() noexcept;

    ImGuiRHIRenderer(const ImGuiRHIRenderer&) = delete;
    ImGuiRHIRenderer& operator=(const ImGuiRHIRenderer&) = delete;
    ImGuiRHIRenderer(ImGuiRHIRenderer&&) = delete;
    ImGuiRHIRenderer& operator=(ImGuiRHIRenderer&&) = delete;

public:
    /// @brief Creates shaders, fixed-function states and initial geometry buffers, and binds to the current ImGui
    ///        context (`io.BackendRendererName`, `RendererHasTextures`, `RendererHasVtxOffset`).
    /// @param[in] desc Renderer configuration.
    /// @return True on success; false if no ImGui context is current, the RHI is not Vulkan, or creation failed.
    [[nodiscard]] bool initialize(const ImGuiRendererDesc& desc = {}) noexcept;

    /// @brief Waits for the GPU, destroys every ImGui-managed texture and releases all RHI objects.
    void shutdown() noexcept;

    /// @brief Upload phase: honors texture requests and streams vertex/index data for `drawData`.
    /// @note Must be recorded outside of any render pass, after `ImGui::Render()`.
    /// @param[in,out] cmdList Immediate command list of the current frame.
    /// @param[in,out] drawData Value of `ImGui::GetDrawData()`; texture statuses are updated in place.
    void prepareDrawData(gp::rhi::command::CommandListImmediate& cmdList, ImDrawData* drawData) noexcept;

    /// @brief Raster phase: records the ImGui draw commands into the currently open render pass.
    /// @note `drawData` must be the one passed to the preceding `prepareDrawData()` call of this frame.
    /// @param[in,out] cmdList Immediate command list with an open render pass.
    /// @param[in] drawData Value of `ImGui::GetDrawData()`.
    void drawInRenderPass(gp::rhi::command::CommandListImmediate& cmdList, ImDrawData* drawData) noexcept;

    /// @brief Convenience: `prepareDrawData()` + own `LoadStore` render pass on `renderTarget` + `drawInRenderPass()`.
    /// @note `renderTarget` must already be in the `RTV` access state; no render pass may be open.
    /// @param[in,out] cmdList Immediate command list of the current frame.
    /// @param[in,out] drawData Value of `ImGui::GetDrawData()`.
    /// @param[in] renderTarget Color target ImGui is composited onto (typically the viewport back buffer).
    void renderDrawData(
        gp::rhi::command::CommandListImmediate& cmdList, ImDrawData* drawData, gp::rhi::resource::Texture* renderTarget
    ) noexcept;

    /// @brief Converts an application RHI texture into an `ImTextureID` for `ImGui::Image()` and friends.
    /// @param[in] texture Shader-readable 2D texture; borrowed, not retained.
    /// @return Opaque identifier to cast to `ImTextureID`.
    [[nodiscard]] static gp::UInt64 toTextureId(gp::rhi::resource::Texture* texture) noexcept;

    /// @brief Returns the counters of the most recent frame.
    /// @return Per-frame renderer statistics.
    [[nodiscard]] const ImGuiRendererStats& getStats() const noexcept
    {
        return m_stats;
    }

    /// @brief Checks whether GPU resources are created and the backend is bound to an ImGui context.
    /// @return True between a successful `initialize()` and `shutdown()`.
    [[nodiscard]] bool isInitialized() const noexcept
    {
        return m_context != nullptr;
    }

private:
    /// @brief Strong ownership record for one ImGui-managed texture.
    struct TextureSlot
    {
        /// @brief GPU texture; null when the slot is free.
        gp::rhi::resource::TextureRef texture{};
    };

    /// @brief Processes one `ImTextureData` request (create, update or destroy).
    /// @param[in,out] cmdList Immediate command list.
    /// @param[in,out] textureData Texture request to honor.
    void updateTexture(gp::rhi::command::CommandListImmediate& cmdList, ImTextureData* textureData) noexcept;

    /// @brief Destroys the GPU texture backing `textureData` and marks it `ImTextureStatus_Destroyed`.
    /// @param[in,out] textureData Texture to release.
    void destroyTexture(ImTextureData* textureData) noexcept;

    /// @brief Ensures the vertex and index buffers can hold the requested element counts.
    /// @param[in,out] cmdList Immediate command list.
    /// @param[in] vertexCount Required vertex capacity.
    /// @param[in] indexCount Required index capacity.
    /// @return True if both buffers are valid and large enough.
    [[nodiscard]] bool ensureGeometryCapacity(
        gp::rhi::command::CommandListImmediate& cmdList, gp::UInt32 vertexCount, gp::UInt32 indexCount
    ) noexcept;

    /// @brief Binds PSO, viewport, vertex stream and projection globals.
    /// @param[in,out] cmdList Immediate command list.
    /// @param[in] drawData Draw data providing display rectangle and framebuffer scale.
    /// @param[in] framebufferWidth Render target width in pixels.
    /// @param[in] framebufferHeight Render target height in pixels.
    void setupRenderState(
        gp::rhi::command::CommandListImmediate& cmdList,
        const ImDrawData* drawData,
        gp::UInt32 framebufferWidth,
        gp::UInt32 framebufferHeight
    ) noexcept;

    /// @brief Returns a PSO compatible with the open render pass, creating it on first use or on a format change.
    /// @param[in,out] cmdList Immediate command list with an open render pass.
    /// @return Pipeline state, or `nullptr` on failure.
    [[nodiscard]] gp::rhi::pipeline::GraphicsPipelineState*
        acquirePipelineState(gp::rhi::command::CommandListImmediate& cmdList) noexcept;

private:
    ImGuiContext* m_context{ nullptr };
    ImGuiRendererDesc m_desc{};
    ImGuiRendererStats m_stats{};
    const ImDrawData* m_preparedDrawData{ nullptr };

    gp::rhi::shader::VertexShaderRef m_vertexShader;
    gp::rhi::shader::PixelShaderRef m_pixelShader;
    gp::rhi::pipeline::VertexDeclarationRef m_vertexDeclaration;
    gp::rhi::pipeline::BlendStateRef m_blendState;
    gp::rhi::pipeline::RasterizerStateRef m_rasterizerState;
    gp::rhi::pipeline::DepthStencilStateRef m_depthStencilState;
    gp::rhi::pipeline::SamplerStateRef m_samplerState;
    gp::rhi::pipeline::GraphicsPipelineStateRef m_pipelineState;

    gp::rhi::resource::BufferRef m_vertexBuffer;
    gp::rhi::resource::BufferRef m_indexBuffer;
    gp::UInt32 m_vertexCapacity{ 0u };
    gp::UInt32 m_indexCapacity{ 0u };

    gp::Vector<TextureSlot> m_textureSlots;
    gp::UInt32 m_residentTextures{ 0u };

    gp::math::PixelFormat m_cachedColorFormat{ gp::math::PixelFormat::Unknown };
    gp::math::PixelFormat m_cachedDepthFormat{ gp::math::PixelFormat::Unknown };
    gp::UInt32 m_cachedTargetCount{ 0u };
    gp::UInt16 m_cachedSampleCount{ 0u };
};

}   // namespace gp::imgui
