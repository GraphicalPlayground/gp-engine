// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "Application.hpp"
#include "backend/SDL3ApplicationBackend.hpp"
#include "commands/CommandEnums.hpp"
#include "commands/CommandListExecutor.hpp"
#include "commands/CommandListImmediate.hpp"
#include "commands/RenderPassInfo.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/ConsoleTypes.hpp"
#include "console/IConsoleManager.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/strings/CharTraits.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ClearValueBinding.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "core/ResourceTypes.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"          // IWYU pragma: keep
#include "device/DeviceEnums.hpp"
#include "device/DynamicRHI.hpp"
#include "device/RHIGlobals.hpp"
#include "device/RHIModuleDiscovery.hpp"
#include "event/data/TextInputEvent.hpp"   // IWYU pragma: keep
#include "event/input/KeyboardEvent.hpp"   // IWYU pragma: keep
#include "event/input/MouseEvent.hpp"      // IWYU pragma: keep
#include "event/system/WindowEvent.hpp"    // IWYU pragma: keep
#include "imgui.h"
#include "ImGuiLayer.hpp"
#include "ImGuiLayerDesc.hpp"
#include "Launch.hpp"
#include "logging/LogMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "modules/ModuleManager.hpp"
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
#include "pipeline/VertexDeclaration.hpp"   // IWYU pragma: keep
#include "pipeline/VertexDeclarationDesc.hpp"
#include "presentation/PresentationForward.hpp"
#include "presentation/Viewport.hpp"   // IWYU pragma: keep
#include "profiling/Profiler.hpp"
#include "profiling/ProfilingControl.hpp"
#include "renderer/ImGuiRendererDesc.hpp"
#include "resources/Buffer.hpp"   // IWYU pragma: keep
#include "resources/BufferCreateDesc.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "RHIPixelFormat.hpp"
#include "shaders/PixelShader.hpp"   // IWYU pragma: keep
#include "shaders/ShaderForward.hpp"
#include "shaders/ShaderParameters.hpp"
#include "shaders/VertexShader.hpp"   // IWYU pragma: keep
#include "sync/SyncEnums.hpp"
#include "sync/TransitionDesc.hpp"
#include "vfs/VirtualFileSystem.hpp"
#include "window/NativeWindow.hpp"
#include <cmath>
#include <cstdio>

namespace gp
{

namespace
{

GP_DEFINE_LOG_CATEGORY_INLINE(LogEditor, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);
GP_DEFINE_LOG_CATEGORY_INLINE(LogDisplay, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);
GP_DEFINE_LOG_CATEGORY_INLINE(LogInput, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);
GP_DEFINE_LOG_CATEGORY_INLINE(LogDialog, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);
GP_DEFINE_LOG_CATEGORY_INLINE(LogClipboard, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);

/// @brief Toggles keyboard, mouse, and gamepad input polling diagnostics in `EditorApplication`.
gp::console::AutoConsoleVariable<bool> g_cvarEditorLogInput(
    "editor.LogInput",
    false,
    "Logs keyboard, mouse, and gamepad input polling diagnostics from EditorApplication.",
    gp::console::ConsoleVarFlags::None
);

/// @brief Frame interval between periodic FPS telemetry logs in `EditorApplication` (`<= 0` disables).
gp::console::AutoConsoleVariable<gp::Int32> g_cvarEditorFpsLogInterval(
    "editor.FpsLogInterval",
    120,
    "Frame interval between periodic EditorApplication FPS logs (<= 0 disables).",
    gp::console::ConsoleVarFlags::None
);

/// @brief Interleaved vertex record consumed by `Cube.vert` (`0`: position, `1`: color, `2`: normal).
struct CubeVertex
{
    gp::Float32 position[3];
    gp::Float32 color[3];
    gp::Float32 normal[3];
};

/// @brief 24-vertex unit cube (4 distinct vertices per face with crisp per-face colors and normals).
constexpr CubeVertex kCubeVertices[24u] = {
    // +Z Front face (coral red)
    {  { -0.5f, -0.5f, 0.5f }, { 0.95f, 0.25f, 0.25f },  { 0.0f, 0.0f, 1.0f } },
    {   { 0.5f, -0.5f, 0.5f }, { 0.95f, 0.35f, 0.25f },  { 0.0f, 0.0f, 1.0f } },
    {    { 0.5f, 0.5f, 0.5f }, { 1.00f, 0.45f, 0.35f },  { 0.0f, 0.0f, 1.0f } },
    {   { -0.5f, 0.5f, 0.5f }, { 0.90f, 0.25f, 0.35f },  { 0.0f, 0.0f, 1.0f } },

    // -Z Back face (teal)
    {  { 0.5f, -0.5f, -0.5f }, { 0.15f, 0.75f, 0.80f }, { 0.0f, 0.0f, -1.0f } },
    { { -0.5f, -0.5f, -0.5f }, { 0.15f, 0.80f, 0.70f }, { 0.0f, 0.0f, -1.0f } },
    {  { -0.5f, 0.5f, -0.5f }, { 0.25f, 0.90f, 0.85f }, { 0.0f, 0.0f, -1.0f } },
    {   { 0.5f, 0.5f, -0.5f }, { 0.20f, 0.70f, 0.85f }, { 0.0f, 0.0f, -1.0f } },

    // +X Right face (emerald green)
    {   { 0.5f, -0.5f, 0.5f }, { 0.20f, 0.85f, 0.35f },  { 1.0f, 0.0f, 0.0f } },
    {  { 0.5f, -0.5f, -0.5f }, { 0.20f, 0.75f, 0.30f },  { 1.0f, 0.0f, 0.0f } },
    {   { 0.5f, 0.5f, -0.5f }, { 0.35f, 0.95f, 0.45f },  { 1.0f, 0.0f, 0.0f } },
    {    { 0.5f, 0.5f, 0.5f }, { 0.30f, 0.90f, 0.40f },  { 1.0f, 0.0f, 0.0f } },

    // -X Left face (amber orange)
    { { -0.5f, -0.5f, -0.5f }, { 0.95f, 0.60f, 0.15f }, { -1.0f, 0.0f, 0.0f } },
    {  { -0.5f, -0.5f, 0.5f }, { 0.95f, 0.55f, 0.15f }, { -1.0f, 0.0f, 0.0f } },
    {   { -0.5f, 0.5f, 0.5f }, { 1.00f, 0.75f, 0.25f }, { -1.0f, 0.0f, 0.0f } },
    {  { -0.5f, 0.5f, -0.5f }, { 0.90f, 0.65f, 0.20f }, { -1.0f, 0.0f, 0.0f } },

    // +Y Top face (royal blue)
    {   { -0.5f, 0.5f, 0.5f }, { 0.25f, 0.45f, 0.95f },  { 0.0f, 1.0f, 0.0f } },
    {    { 0.5f, 0.5f, 0.5f }, { 0.30f, 0.50f, 1.00f },  { 0.0f, 1.0f, 0.0f } },
    {   { 0.5f, 0.5f, -0.5f }, { 0.40f, 0.65f, 1.00f },  { 0.0f, 1.0f, 0.0f } },
    {  { -0.5f, 0.5f, -0.5f }, { 0.25f, 0.55f, 0.90f },  { 0.0f, 1.0f, 0.0f } },

    // -Y Bottom face (violet)
    { { -0.5f, -0.5f, -0.5f }, { 0.65f, 0.25f, 0.85f }, { 0.0f, -1.0f, 0.0f } },
    {  { 0.5f, -0.5f, -0.5f }, { 0.70f, 0.30f, 0.90f }, { 0.0f, -1.0f, 0.0f } },
    {   { 0.5f, -0.5f, 0.5f }, { 0.75f, 0.40f, 0.95f }, { 0.0f, -1.0f, 0.0f } },
    {  { -0.5f, -0.5f, 0.5f }, { 0.60f, 0.25f, 0.80f }, { 0.0f, -1.0f, 0.0f } }
};

/// @brief 36 triangle-list indices (12 triangles across 6 cube faces).
constexpr gp::UInt16 kCubeIndices[36u] = {
    0u,  1u,  2u,  0u,  2u,  3u,    // +Z
    4u,  5u,  6u,  4u,  6u,  7u,    // -Z
    8u,  9u,  10u, 8u,  10u, 11u,   // +X
    12u, 13u, 14u, 12u, 14u, 15u,   // -X
    16u, 17u, 18u, 16u, 18u, 19u,   // +Y
    20u, 21u, 22u, 20u, 22u, 23u    // -Y
};

/// @brief Column-major 4x4 matrix matching GLSL `mat4` memory layout.
struct OrbitMat4
{
    gp::Float32 m[16]{};
};

/// @brief Multiplies two column-major 4x4 matrices (`lhs * rhs`).
/// @param[in] lhs Left-hand matrix.
/// @param[in] rhs Right-hand matrix.
/// @return Product matrix `lhs * rhs`.
[[nodiscard]] OrbitMat4 multiplyMat4(const OrbitMat4& lhs, const OrbitMat4& rhs) noexcept
{
    OrbitMat4 out{};
    for (gp::UInt32 col = 0u; col < 4u; ++col)
    {
        for (gp::UInt32 row = 0u; row < 4u; ++row)
        {
            gp::Float32 sum = 0.0f;
            for (gp::UInt32 k = 0u; k < 4u; ++k)
            {
                sum += lhs.m[k * 4u + row] * rhs.m[col * 4u + k];
            }
            out.m[col * 4u + row] = sum;
        }
    }
    return out;
}

/// @brief Builds a right-handed perspective projection matrix mapping depth to `[0, 1]`.
/// @param[in] fovYRadians Vertical field of view in radians.
/// @param[in] aspectRatio Viewport width divided by height.
/// @param[in] nearPlane Near clip plane distance (`> 0`).
/// @param[in] farPlane Far clip plane distance (`> nearPlane`).
/// @return Column-major perspective projection matrix.
[[nodiscard]] OrbitMat4 makePerspectiveZeroToOne(
    const gp::Float32 fovYRadians,
    const gp::Float32 aspectRatio,
    const gp::Float32 nearPlane,
    const gp::Float32 farPlane
) noexcept
{
    const gp::Float32 f = 1.0f / std::tan(fovYRadians * 0.5f);
    const gp::Float32 invDepthRange = 1.0f / (nearPlane - farPlane);

    OrbitMat4 out{};
    out.m[0] = f / aspectRatio;
    out.m[5] = f;
    out.m[10] = farPlane * invDepthRange;
    out.m[11] = -1.0f;
    out.m[14] = (nearPlane * farPlane) * invDepthRange;
    return out;
}

/// @brief Builds a right-handed look-at view matrix toward `(0, 0, 0)` with `+Y` up.
/// @param[in] eyeX Camera X position.
/// @param[in] eyeY Camera Y position.
/// @param[in] eyeZ Camera Z position.
/// @return Column-major view matrix.
[[nodiscard]] OrbitMat4
    makeLookAtOrigin(const gp::Float32 eyeX, const gp::Float32 eyeY, const gp::Float32 eyeZ) noexcept
{
    const gp::Float32 fLen = gp::math::sqrt(eyeX * eyeX + eyeY * eyeY + eyeZ * eyeZ);
    const gp::Float32 fx = -eyeX / fLen;
    const gp::Float32 fy = -eyeY / fLen;
    const gp::Float32 fz = -eyeZ / fLen;

    const gp::Float32 sxRaw = -fz;
    const gp::Float32 szRaw = fx;
    const gp::Float32 sLen = gp::math::sqrt(sxRaw * sxRaw + szRaw * szRaw);
    const gp::Float32 sx = (sLen > 1e-6f) ? (sxRaw / sLen) : 1.0f;
    const gp::Float32 sy = 0.0f;
    const gp::Float32 sz = (sLen > 1e-6f) ? (szRaw / sLen) : 0.0f;

    const gp::Float32 ux = sy * fz - sz * fy;
    const gp::Float32 uy = sz * fx - sx * fz;
    const gp::Float32 uz = sx * fy - sy * fx;

    OrbitMat4 out{};
    out.m[0] = sx;
    out.m[4] = sy;
    out.m[8] = sz;
    out.m[12] = -(sx * eyeX + sy * eyeY + sz * eyeZ);

    out.m[1] = ux;
    out.m[5] = uy;
    out.m[9] = uz;
    out.m[13] = -(ux * eyeX + uy * eyeY + uz * eyeZ);

    out.m[2] = -fx;
    out.m[6] = -fy;
    out.m[10] = -fz;
    out.m[14] = (fx * eyeX + fy * eyeY + fz * eyeZ);

    out.m[15] = 1.0f;
    return out;
}

/// @brief Packed `std140` uniform buffer matching `Globals` in `Cube.vert`.
struct OrbitGlobals
{
    OrbitMat4 mvp{};
    OrbitMat4 model{};
    gp::Float32 lightPosAndIntensity[4]{};
    gp::Float32 cameraPosAndMode[4]{};
};

/// @brief Builds a Y-axis rotation matrix.
/// @param[in] angleRadians Rotation angle in radians.
/// @return Column-major rotation matrix.
[[nodiscard]] OrbitMat4 makeRotationY(const gp::Float32 angleRadians) noexcept
{
    const gp::Float32 c = std::cos(angleRadians);
    const gp::Float32 s = std::sin(angleRadians);
    OrbitMat4 out{};
    out.m[0] = c;
    out.m[2] = -s;
    out.m[5] = 1.0f;
    out.m[8] = s;
    out.m[10] = c;
    out.m[15] = 1.0f;
    return out;
}

/// @brief Builds a translation + non-uniform scale matrix.
/// @param[in] tx World X translation.
/// @param[in] ty World Y translation.
/// @param[in] tz World Z translation.
/// @param[in] sx Scale along X.
/// @param[in] sy Scale along Y.
/// @param[in] sz Scale along Z.
/// @return Column-major TRS matrix.
[[nodiscard]] OrbitMat4 makeTranslationScale(
    const gp::Float32 tx,
    const gp::Float32 ty,
    const gp::Float32 tz,
    const gp::Float32 sx,
    const gp::Float32 sy,
    const gp::Float32 sz
) noexcept
{
    OrbitMat4 out{};
    out.m[0] = sx;
    out.m[5] = sy;
    out.m[10] = sz;
    out.m[12] = tx;
    out.m[13] = ty;
    out.m[14] = tz;
    out.m[15] = 1.0f;
    return out;
}

/// @brief Computes the combined view-projection matrix and camera eye position for the orbit camera.
/// @param[in] yawRadians Horizontal orbit azimuth in radians.
/// @param[in] pitchRadians Vertical orbit elevation in radians.
/// @param[in] distance Orbit radius from the cube center.
/// @param[in] aspectRatio Viewport width divided by height.
/// @param[out] outEyeX Camera world X coordinate.
/// @param[out] outEyeY Camera world Y coordinate.
/// @param[out] outEyeZ Camera world Z coordinate.
/// @return Column-major `projection * view` matrix.
[[nodiscard]] OrbitMat4 buildViewProjMatrix(
    const gp::Float32 yawRadians,
    const gp::Float32 pitchRadians,
    const gp::Float32 distance,
    const gp::Float32 aspectRatio,
    gp::Float32& outEyeX,
    gp::Float32& outEyeY,
    gp::Float32& outEyeZ
) noexcept
{
    const gp::Float32 cosPitch = std::cos(pitchRadians);
    outEyeX = distance * cosPitch * std::sin(yawRadians);
    outEyeY = distance * std::sin(pitchRadians);
    outEyeZ = distance * cosPitch * std::cos(yawRadians);

    const OrbitMat4 view = makeLookAtOrigin(outEyeX, outEyeY, outEyeZ);
    const OrbitMat4 proj = makePerspectiveZeroToOne(0.78539816f, aspectRatio, 0.1f, 100.0f);
    return multiplyMat4(proj, view);
}

/// @brief Computes a deterministic 128-bit FNV-1a content hash over a `'GPVS'` shader payload.
/// @param[in] bytes Loaded bytecode buffer.
/// @return 128-bit `ShaderHash` key for RHI shader creation.
[[nodiscard]] gp::rhi::shader::ShaderHash computeShaderHash(const gp::ConstVectorView<gp::UInt8> bytes) noexcept
{
    gp::UInt64 low = 14695981039346656037ull;
    gp::UInt64 high = 1099511628211ull;
    for (gp::USize i = 0u; i < bytes.size(); ++i)
    {
        const gp::UInt64 b = bytes[i];
        low ^= b;
        low *= 1099511628211ull;
        high ^= (b + static_cast<gp::UInt64>(i));
        high *= 14695981039346656037ull;
    }
    return gp::rhi::shader::ShaderHash(low, high);
}

/// @brief Loads a pre-compiled `'GPVS'` shader file via VFS and computes its 128-bit hash.
/// @param[in] virtualUri Virtual URI (e.g. `"game://shaders/Cube.vert.gpvs"`).
/// @param[out] outBytes Loaded raw `'GPVS'` bytecode bytes.
/// @param[out] outHash Computed 128-bit shader hash.
/// @return True if the file was read and non-empty.
[[nodiscard]] bool loadShaderBytecode(
    const gp::StringView virtualUri, gp::Vector<gp::UInt8>& outBytes, gp::rhi::shader::ShaderHash& outHash
) noexcept
{
    const gp::Expected<gp::USize, gp::vfs::VfsError> readResult =
        gp::vfs::VirtualFileSystem::readAllBytes(virtualUri, outBytes);
    if (!readResult.hasValue() || outBytes.isEmpty())
    {
        GP_LOG_ERROR(
            LogEditor,
            "Failed to load pre-compiled shader '{}' (VfsError={}).",
            virtualUri,
            static_cast<gp::Int32>(readResult.error())
        );
        return false;
    }

    outHash = computeShaderHash(gp::ConstVectorView<gp::UInt8>(outBytes.data(), outBytes.size()));
    GP_LOG_INFO(LogEditor, "Loaded pre-compiled shader '{}' ({} bytes).", virtualUri, outBytes.size());
    return true;
}

}   // namespace

/// @brief Sandbox editor host application driving HAL subsystems and a temporary Vulkan 3D Cube renderer.
class EditorApplication : public Application
{
public:
    /// @brief Inherit parent constructors.
    using gp::Application::Application;

    /// @copydoc gp::Application::~Application()
    virtual ~EditorApplication() noexcept override = default;

protected:
    /// @copydoc gp::Application::onInitialize()
    /// @note Initializes the temporary Vulkan swapchain viewport, fixed-function states, and pre-compiled shaders.
    [[nodiscard]] virtual bool onInitialize() noexcept override
    {
        GP_PROFILE_SCOPE_NC_ACTIVE(
            "EditorApplication::OnInitialize", GP_U32_COLOR_PURPLE, profiling::ProfilingControl::isZonesEnabled()
        );

        // TODO(vfs): Mount editor-only content schemes here (e.g., `editor://` pointing to the editor resources root,
        //            plus any active project or developer override layers) once the editor asset directory layout is
        //            defined. The six base ADR-0044 schemes (`engine://`, `game://`, `user://`, `appdata://`,
        //            `cache://`, `temp://`) are already mounted by `Application::initializeInternal()`.
        GP_LOG_INFO(
            LogEditor, "VirtualFileSystem Ready ({} active mounts).", gp::vfs::VirtualFileSystem::getMountCount()
        );

        const gp::console::IConsoleManager& consoleMgr = getConsoleManager();
        GP_LOG_INFO(
            LogEditor,
            "Console CVar Registry Ready ({} CVars, {} Commands).",
            consoleMgr.getVariableCount(),
            consoleMgr.getCommandCount()
        );

        gp::modules::ModuleManager& moduleMgr = getModuleManager();
        GP_LOG_INFO(
            LogEditor,
            "ModuleManager Ready ({} loaded, {} registered).",
            moduleMgr.getLoadedModuleCount(),
            moduleMgr.getRegisteredModuleCount()
        );

        if (isRHIInitialized())
        {
            const gp::rhi::DynamicRHI* const activeRHI = getDynamicRHI();
            GP_LOG_INFO(
                LogEditor,
                "RHI Backend Ready: {} (module='{}', nullRHI={}).",
                (activeRHI != nullptr) ? activeRHI->getName() : gp::StringView("None"),
                getLoadedRHIModuleName(),
                gp::rhi::getRHIGlobals().usingNullRHI
            );
        }

        const gp::UInt32 argumentCount = CommandLine::getArgumentCount();
        for (gp::UInt32 i = 0u; i < argumentCount; ++i)
        {
            const gp::StringView arg = CommandLine::getArgument(i);
            GP_LOG_INFO(LogEditor, "Command-line argument [{}]: {}", i, arg);
        }

        GP_LOG_INFO(LogEditor, "========================================================");
        GP_LOG_INFO(LogEditor, "  GP Engine: EditorApplication Initialized Successfully ");
        GP_LOG_INFO(LogEditor, "========================================================");

        if (const gp::hal::IDisplay* const primary = getBackend()->getPrimaryDisplay())
        {
            const gp::hal::DisplayMode& mode = primary->getCurrentMode();
            GP_LOG_INFO(
                LogDisplay,
                "Primary Monitor: {} ({}x{} @ {:.2f} Hz)",
                primary->getName(),
                mode.resolution.width,
                mode.resolution.height,
                mode.refreshRate
            );
        }

        if (getJobSystem().isInitialized())
        {
            GP_LOG_INFO(
                LogEditor,
                "JobSystem Workers: {} (Total Slots: {} | Standard Fibers: {} | Large Fibers: {})",
                getJobSystem().getWorkerThreadCount(),
                getJobSystem().getTotalWorkerSlotCount(),
                getJobSystem().getStandardFiberCapacity(),
                getJobSystem().getLargeFiberCapacity()
            );
        }

        if (getFileSink().isOpen())
        {
            GP_LOG_INFO(LogEditor, "File log sink active: {}", getFileSink().getFilePath());
        }

        if (!initializeCubeRenderer())
        {
            return false;
        }

        initializeEditorUI();

        GP_LOG_INFO(
            LogEditor,
            "Controls: Arrow keys / Mouse left-drag orbit cube | Mouse wheel zoom | 'Space' wireframe | 'ESC'/'Q' exit | 'F1'/'`' console shell."
        );
        return true;
    }

    /// @copydoc gp::Application::onFixedUpdate()
    virtual void onFixedUpdate([[maybe_unused]] gp::time::Duration fixedDelta) noexcept override
    {}

    /// @copydoc gp::Application::onUpdate()
    virtual void onUpdate(const gp::time::FrameTimeStats& timeStats) noexcept override
    {
        GP_PROFILE_SCOPE_NC_ACTIVE(
            "EditorApplication::OnUpdate", GP_U32_COLOR_GREEN, profiling::ProfilingControl::isZonesEnabled()
        );

        // Dear ImGui frame: built first so `wantsMouseCapture()` / `wantsKeyboardCapture()` below reflect this frame's
        // hover and focus state, and so early returns further down never leave an ImGui frame open.
        updateEditorUI(timeStats);
        const bool isUIMouseCaptured = m_imgui.wantsMouseCapture();
        const bool isUIKeyboardCaptured = m_imgui.wantsKeyboardCapture();

        const gp::hal::input::IInputSystem& input = getInput();

        // Check for exit commands (suppressed while an ImGui widget owns the keyboard, e.g. typing 'q' in a text field)
        if (!isUIKeyboardCaptured && (input.isScancodeDown(gp::hal::input::Scancode::Escape) ||
                                      input.isScancodeDown(gp::hal::input::Scancode::Q)))
        {
            GP_LOG_INFO(LogEditor, "Exit key detected. Requesting shutdown...");
            requestQuit();
            return;
        }

        // TEMP: Switch terminal stdin into an interactive console command shell on F1 or Grave (` / ~).
        if (!isUIKeyboardCaptured && (input.wasScancodeJustPressed(gp::hal::input::Scancode::F1) ||
                                      input.wasScancodeJustPressed(gp::hal::input::Scancode::Grave)))
        {
            runTemporaryStdinShell();
            return;
        }

        const bool logInputDiagnostics = g_cvarEditorLogInput.getValueOnGameThread();

        // Query Keyboard State (Scancode-based WASD movement)
        const bool moveForward = input.isScancodeDown(gp::hal::input::Scancode::W);
        const bool moveBack = input.isScancodeDown(gp::hal::input::Scancode::S);
        const bool moveLeft = input.isScancodeDown(gp::hal::input::Scancode::A);
        const bool moveRight = input.isScancodeDown(gp::hal::input::Scancode::D);

        if (logInputDiagnostics && (moveForward || moveBack || moveLeft || moveRight))
        {
            GP_LOG_INFO(
                LogInput,
                "Movement Held -> Forward: {} | Back: {} | Left: {} | Right: {}",
                moveForward,
                moveBack,
                moveLeft,
                moveRight
            );
        }

        // Orbit camera controls (Arrow keys + Mouse left-drag + Mouse wheel zoom) and wireframe toggle
        const gp::Float32 dt = timeStats.unscaledDeltaTime;
        m_elapsedTime += dt * m_animationSpeed;

        if (!isUIKeyboardCaptured && input.wasKeyJustPressed(gp::hal::input::Keycode::Space))
        {
            m_wireframeEnabled = !m_wireframeEnabled;
            GP_LOG_INFO(LogEditor, "[TEMP] Wireframe mode: {}", m_wireframeEnabled ? "ON" : "OFF");
        }
        constexpr gp::Float32 kKeyOrbitSpeed = 1.8f;
        constexpr gp::Float32 kMouseSensitivity = 0.008f;
        constexpr gp::Float32 kWheelZoomSpeed = 0.35f;
        constexpr gp::Float32 kMaxPitchRadians = 1.48f;

        if (!isUIKeyboardCaptured)
        {
            if (input.isScancodeDown(gp::hal::input::Scancode::Left))
            {
                m_cameraYaw -= kKeyOrbitSpeed * dt;
            }
            if (input.isScancodeDown(gp::hal::input::Scancode::Right))
            {
                m_cameraYaw += kKeyOrbitSpeed * dt;
            }
            if (input.isScancodeDown(gp::hal::input::Scancode::Up))
            {
                m_cameraPitch += kKeyOrbitSpeed * dt;
            }
            if (input.isScancodeDown(gp::hal::input::Scancode::Down))
            {
                m_cameraPitch -= kKeyOrbitSpeed * dt;
            }
        }

        const gp::hal::input::IMouse& mouse = input.getMouse();
        const gp::hal::input::Stick2D mousePos = mouse.getPosition();
        const gp::hal::input::Stick2D mouseDelta = mouse.getDelta();
        if (!isUIMouseCaptured && mouse.isButtonDown(gp::hal::input::MouseButton::Left))
        {
            m_cameraYaw -= mouseDelta.x * kMouseSensitivity;
            m_cameraPitch += mouseDelta.y * kMouseSensitivity;
        }

        const gp::hal::input::Stick2D wheelDelta = mouse.getWheelDelta();
        if (!isUIMouseCaptured && wheelDelta.y != 0.0f)
        {
            m_cameraDistance = gp::math::clamp(m_cameraDistance - wheelDelta.y * kWheelZoomSpeed, 1.5f, 12.0f);
        }

        m_cameraPitch = gp::math::clamp(m_cameraPitch, -kMaxPitchRadians, kMaxPitchRadians);

        if (logInputDiagnostics && mouseDelta.lengthSquared() > 0.0f)
        {
            GP_LOG_INFO(
                LogInput,
                "Mouse Position: ({:.1f}, {:.1f}) | Delta: ({:.1f}, {:.1f})",
                mousePos.x,
                mousePos.y,
                mouseDelta.x,
                mouseDelta.y
            );
        }

        // Query Gamepad State (Player 1 left thumbstick with 2D radial deadzone)
        if (const gp::hal::input::IGamepad* const pad = input.getGamepad(gp::hal::input::GamepadSlot::Player1))
        {
            if (pad->isConnected())
            {
                const auto leftStick = pad->getLeftStick();
                if (logInputDiagnostics && leftStick.lengthSquared() > 0.01f)
                {
                    GP_LOG_INFO(LogInput, "Gamepad P1 Left Stick: ({:.2f}, {:.2f})", leftStick.x, leftStick.y);
                }

                // Example: Trigger dual-motor rumble if 'South' (A/Cross) is pressed
                if (pad->wasButtonJustPressed(gp::hal::input::GamepadButton::South))
                {
                    if (logInputDiagnostics)
                    {
                        GP_LOG_INFO(LogInput, "Gamepad P1 'South' button pressed! Triggering haptic rumble...");
                    }
                    gp::hal::input::HapticRumbleDesc rumble{ 0.75f, 0.4f, 250u };
                    const_cast<gp::hal::input::IGamepad*>(pad)->setRumble(rumble);
                }

                // Move the windows around with the gamepad's left stick (for testing purposes)
                if (getMainWindow()->isActive())
                {
                    const gp::hal::Point2D winPos = getMainWindow()->getPosition();
                    const gp::hal::Point2D newPos{ winPos.x + static_cast<gp::Int32>(leftStick.x * 5.0f),
                                                   winPos.y + static_cast<gp::Int32>(leftStick.y * 5.0f) };
                    getMainWindow()->setPosition(newPos);
                }
            }
        }

        // Periodic FPS printout from the master EngineClock telemetry
        static gp::Int32 s_frameThrottle = 0;
        const gp::Int32 fpsInterval = g_cvarEditorFpsLogInterval.getValueOnGameThread();
        if (fpsInterval > 0 && ++s_frameThrottle >= fpsInterval)
        {
            s_frameThrottle = 0;
            GP_LOG_INFO(
                LogEditor,
                "Smoothed FPS: {:.2f} | Frame Index: {} | Active Device: {}",
                timeStats.smoothedFps,
                timeStats.frameIndex,
                static_cast<gp::Int32>(input.getActiveDeviceType())
            );
        }

        // Trigger a themed 3-button custom modal message box when pressing 'M'
        if (!isUIKeyboardCaptured && getInput().wasKeyJustPressed(gp::hal::input::Keycode::M))
        {
            using namespace gp::hal;
            const DialogButtonDesc buttons[] = {
                DialogButtonDesc(100, "Save & Continue", DialogButtonFlags::ReturnKeyDefault),
                DialogButtonDesc(200, "Discard Changes"),
                DialogButtonDesc(300, "Cancel", DialogButtonFlags::EscapeKeyDefault)
            };
            const DialogColorScheme darkTheme{};

            MessageBoxDesc box{};
            box.title = "Unsaved Scene Changes";
            box.message = "Your level has unsaved modifications. How would you like to proceed?";
            box.parentWindow = getMainWindow();
            box.buttons = buttons;
            box.buttonCount = 3u;
            box.colorScheme = &darkTheme;
            box.icon = DialogIcon::Warning;

            const gp::Int32 choice = getBackend()->getDialog().showMessageBox(box);
            GP_LOG_INFO(LogDialog, "User clicked button ID: {}", choice);
        }

        // Copy text to clipboard on Ctrl+C, and paste on Ctrl+V
        const gp::hal::input::IKeyboard& kb = getInput().getKeyboard();
        if (!isUIKeyboardCaptured && kb.hasModifiers(gp::hal::input::KeyModifier::Ctrl))
        {
            if (kb.wasKeyJustPressed(gp::hal::input::Keycode::C))
            {
                (void)getBackend()->getClipboard().setText("Copied from Graphical Playground Engine!");
            }
            else if (kb.wasKeyJustPressed(gp::hal::input::Keycode::V))
            {
                const gp::StringView pasted = getBackend()->getClipboard().getText();
                GP_LOG_INFO(LogClipboard, "Pasted UTF-8 Text: {}", pasted);
            }
        }

        // Open an asynchronous native File Picker when pressing 'O'
        if (!isUIKeyboardCaptured && getInput().wasKeyJustPressed(gp::hal::input::Keycode::O))
        {
            using namespace gp::hal;
            static constexpr DialogFileFilter kFilters[] = { DialogFileFilter("3D Models (glTF/FBX)", "gltf;glb;fbx"),
                                                             DialogFileFilter("All Files", "*") };

            FileDialogDesc fileDesc{};
            fileDesc.title = "Import 3D Asset";
            fileDesc.parentWindow = getMainWindow();
            fileDesc.filters = kFilters;
            fileDesc.filterCount = 2u;
            fileDesc.allowMultiple = true;
            fileDesc.callback = [](void*, const FileDialogResult& res) noexcept
            {
                if (res.status == DialogFileStatus::Accepted)
                {
                    for (gp::UInt32 i = 0u; i < res.pathCount; ++i)
                    {
                        GP_LOG_INFO(LogDialog, "Selected [{}]: {}", i, res.paths[i]);
                    }
                }
            };

            getBackend()->getDialog().showOpenFileDialog(fileDesc);
        }
    }

    /// @copydoc gp::Application::onRender()
    /// @note Records a single-pass Vulkan 3D Cube frame onto `CommandListImmediate` and presents the swapchain.
    virtual void onRender(const gp::time::FrameTimeStats& timeStats) noexcept override
    {
        GP_PROFILE_SCOPE_NC_ACTIVE(
            "EditorApplication::OnRender", GP_U32_COLOR_BLUE, profiling::ProfilingControl::isZonesEnabled()
        );

        [[maybe_unused]] const gp::Float32 alpha = timeStats.fixedStep.interpolationAlpha;
        renderCube();
    }

    /// @copydoc gp::Application::onShutdown()
    /// @note Drains GPU execution and releases all temporary RHI handles before `Application::shutdownInternal()`
    ///       tears down `DynamicRHI`, unloads the backend module, and closes the OS window.
    virtual void onShutdown() noexcept override
    {
        GP_PROFILE_SCOPE_NC_ACTIVE(
            "EditorApplication::OnShutdown", GP_U32_COLOR_RED, profiling::ProfilingControl::isZonesEnabled()
        );

        // ImGui owns RHI textures and buffers: release them before the cube renderer flushes the deletion queues.
        m_imgui.shutdown();
        shutdownCubeRenderer();
        GP_LOG_INFO(LogEditor, "Application shut down cleanly.");
    }

    /// @copydoc gp::Application::onKeyboardEvent()
    /// @note Forwards to Dear ImGui; engine shortcuts are gated on `ImGuiLayer::wantsKeyboardCapture()` in `onUpdate()`.
    virtual void onKeyboardEvent(const hal::event::KeyboardEvent& ev) noexcept override
    {
        (void)m_imgui.processKeyboardEvent(ev);
    }

    /// @copydoc gp::Application::onMouseEvent()
    /// @note Forwards to Dear ImGui; camera orbit/zoom is gated on `ImGuiLayer::wantsMouseCapture()` in `onUpdate()`.
    virtual void onMouseEvent(const hal::event::MouseEvent& ev) noexcept override
    {
        (void)m_imgui.processMouseEvent(ev);
    }

    /// @copydoc gp::Application::onTextInputEvent()
    virtual void onTextInputEvent(const hal::event::TextInputEvent& ev, StringView utf8Text) noexcept override
    {
        (void)m_imgui.processTextInputEvent(ev, utf8Text);
    }

    /// @copydoc gp::Application::onWindowEvent()
    virtual void onWindowEvent(const hal::event::WindowEvent& ev) noexcept override
    {
        m_imgui.processWindowEvent(ev);
    }

private:
    /// @brief Creates the Dear ImGui layer bound to the main window and the active DynamicRHI.
    /// @note Non-fatal: the editor keeps running without UI when the RHI cannot host the ImGui renderer.
    void initializeEditorUI() noexcept
    {
        if (!m_cubeRendererReady)
        {
            GP_LOG_INFO(LogEditor, "Editor UI skipped (no active Vulkan presentation).");
            return;
        }

        gp::imgui::ImGuiLayerDesc uiDesc{};
        uiDesc.theme = gp::imgui::ImGuiTheme::Dark;
        uiDesc.enableDocking = true;
        uiDesc.iniFilename = nullptr;   // Demo layout is rebuilt every launch; nothing is written to the CWD.
        if (!m_imgui.initialize(getMainWindow(), uiDesc))
        {
            GP_LOG_WARNING(LogEditor, "Dear ImGui layer failed to initialize; continuing without editor UI.");
            return;
        }

        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 6.0f;
        style.FrameRounding = 4.0f;
        style.GrabRounding = 4.0f;
        GP_LOG_INFO(LogEditor, "Dear ImGui {} editor UI ready.", gp::StringView(ImGui::GetVersion()));
    }

    /// @brief Builds this frame's editor UI: dockspace, main menu, scene controls, statistics and optional demos.
    /// @param[in] timeStats Frame timing metrics for the statistics panel.
    void updateEditorUI(const gp::time::FrameTimeStats& timeStats) noexcept
    {
        if (!m_imgui.isInitialized())
        {
            return;
        }

        m_frameTimeHistory[m_frameTimeHistoryOffset] = timeStats.unscaledDeltaTime * 1000.0f;
        m_frameTimeHistoryOffset = (m_frameTimeHistoryOffset + 1u) % kFrameTimeHistorySize;

        m_imgui.beginFrame(timeStats.unscaledDeltaTime);

        // Pass-through central node: the 3D viewport stays visible and interactive behind docked panels.
        ImGui::DockSpaceOverViewport(0u, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        drawMainMenuBar();
        if (m_showScenePanel)
        {
            drawScenePanel();
        }
        if (m_showStatsPanel)
        {
            drawStatsPanel(timeStats);
        }
        if (m_showImGuiDemo)
        {
            ImGui::ShowDemoWindow(&m_showImGuiDemo);
        }
        if (m_showImGuiMetrics)
        {
            ImGui::ShowMetricsWindow(&m_showImGuiMetrics);
        }

        m_imgui.endFrame();
    }

    /// @brief Draws the editor main menu bar (file actions and panel visibility toggles).
    void drawMainMenuBar() noexcept
    {
        if (!ImGui::BeginMainMenuBar())
        {
            return;
        }

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Quit", "Esc"))
            {
                requestQuit();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Scene", nullptr, &m_showScenePanel);
            ImGui::MenuItem("Statistics", nullptr, &m_showStatsPanel);
            ImGui::Separator();
            ImGui::MenuItem("Dear ImGui Demo", nullptr, &m_showImGuiDemo);
            ImGui::MenuItem("Dear ImGui Metrics", nullptr, &m_showImGuiMetrics);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    /// @brief Draws interactive controls for the orbit camera, animation, rasterization mode and UI accent.
    void drawScenePanel() noexcept
    {
        ImGui::SetNextWindowPos(ImVec2(16.0f, 40.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Scene", &m_showScenePanel))
        {
            ImGui::SeparatorText("Orbit Camera");
            ImGui::SliderAngle("Yaw", &m_cameraYaw, -360.0f, 360.0f);
            ImGui::SliderAngle("Pitch", &m_cameraPitch, -84.0f, 84.0f);
            ImGui::SliderFloat("Distance", &m_cameraDistance, 1.5f, 12.0f, "%.2f");
            if (ImGui::Button("Reset Camera"))
            {
                m_cameraYaw = 0.65f;
                m_cameraPitch = 0.45f;
                m_cameraDistance = 3.0f;
            }

            ImGui::SeparatorText("Rendering");
            ImGui::Checkbox("Wireframe (Space)", &m_wireframeEnabled);
            ImGui::SliderFloat("Animation Speed", &m_animationSpeed, 0.0f, 4.0f, "%.2fx");

            ImGui::SeparatorText("Theme");
            if (ImGui::ColorEdit3("Accent", m_accentColor))
            {
                ImGuiStyle& style = ImGui::GetStyle();
                const ImVec4 accent(m_accentColor[0], m_accentColor[1], m_accentColor[2], 1.0f);
                style.Colors[ImGuiCol_CheckMark] = accent;
                style.Colors[ImGuiCol_SliderGrab] = accent;
                style.Colors[ImGuiCol_SliderGrabActive] = accent;
            }

            ImGui::SeparatorText("Text Input");
            ImGui::InputTextWithHint(
                "##note", "Type here: Esc/Q/Space stay inside ImGui", m_noteBuffer, sizeof(m_noteBuffer)
            );
        }
        ImGui::End();
    }

    /// @brief Draws frame timing, RHI identity and ImGui renderer statistics.
    /// @param[in] timeStats Frame timing metrics.
    void drawStatsPanel(const gp::time::FrameTimeStats& timeStats) noexcept
    {
        const ImGuiViewport* const viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 16.0f, viewport->WorkPos.y + 16.0f),
            ImGuiCond_FirstUseEver,
            ImVec2(1.0f, 0.0f)
        );
        ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Statistics", &m_showStatsPanel))
        {
            const gp::rhi::DynamicRHI* const activeRHI = getDynamicRHI();
            const gp::StringView rhiName = (activeRHI != nullptr) ? activeRHI->getName() : gp::StringView("None");
            ImGui::Text("RHI: %.*s", static_cast<int>(rhiName.size()), rhiName.data());
            ImGui::Text("Viewport: %u x %u", m_viewportWidth, m_viewportHeight);

            ImGui::SeparatorText("Frame");
            ImGui::Text(
                "%.1f FPS (%.2f ms)",
                static_cast<gp::Float64>(timeStats.smoothedFps),
                static_cast<gp::Float64>(timeStats.unscaledDeltaTime * 1000.0f)
            );
            ImGui::Text("Frame index: %llu", static_cast<unsigned long long>(timeStats.frameIndex));
            ImGui::PlotLines(
                "##frametimes",
                m_frameTimeHistory,
                static_cast<int>(kFrameTimeHistorySize),
                static_cast<int>(m_frameTimeHistoryOffset),
                "frame time (ms)",
                0.0f,
                33.3f,
                ImVec2(-1.0f, 64.0f)
            );

            drawPresentationControls();

            ImGui::SeparatorText("Dear ImGui Renderer");
            const gp::imgui::ImGuiRendererStats& uiStats = m_imgui.getRenderer().getStats();
            if (ImGui::BeginTable("##uistats", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
            {
                const auto addRow = [](const char* const label, const gp::UInt32 value) -> void
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(label);
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", value);
                };
                addRow("Draw calls", uiStats.drawCalls);
                addRow("Vertices", uiStats.vertexCount);
                addRow("Indices", uiStats.indexCount);
                addRow("Vertex capacity", uiStats.vertexCapacity);
                addRow("Index capacity", uiStats.indexCapacity);
                addRow("Resident textures", uiStats.residentTextures);
                addRow("Texture requests", uiStats.textureRequests);
                ImGui::EndTable();
            }

            const ImGuiIO& io = ImGui::GetIO();
            ImGui::Text(
                "Capture: mouse=%s keyboard=%s",
                io.WantCaptureMouse ? "yes" : "no",
                io.WantCaptureKeyboard ? "yes" : "no"
            );
        }
        ImGui::End();
    }

    /// @brief Draws live presentation controls: window VSync and the `app.TargetFps` foreground frame cap.
    /// @details Diagnostic aid for latency and frame-cap investigations. With VSync on, the swapchain uses MAILBOX/FIFO
    ///          and the observed rate is bounded by the display refresh and by DWM composition; with VSync off it uses
    ///          IMMEDIATE and only the CPU pacer (`app.TargetFps`) limits the loop.
    void drawPresentationControls() noexcept
    {
        ImGui::SeparatorText("Presentation");

        gp::hal::IWindow* const mainWindow = getMainWindow();
        if (mainWindow != nullptr)
        {
            bool isVSyncEnabled = mainWindow->isVSyncEnabled();
            if (ImGui::Checkbox("VSync", &isVSyncEnabled))
            {
                mainWindow->setVSyncEnabled(isVSyncEnabled);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", mainWindow->isMaximized() ? "maximized" : "windowed");
        }

        gp::console::IConsoleVariable* const targetFpsVar =
            gp::console::IConsoleManager::get().findConsoleVariable("app.TargetFps");
        if (targetFpsVar != nullptr)
        {
            gp::Float32 targetFps = static_cast<gp::Float32>(targetFpsVar->getFloat64());
            if (ImGui::SliderFloat("Target FPS", &targetFps, 0.0f, 480.0f, targetFps <= 0.0f ? "Unlimited" : "%.0f"))
            {
                // Rejected only when a higher-priority source (command line, console) already owns the value.
                const gp::console::ConsoleResult result = targetFpsVar->set(targetFps);
                if (result != gp::console::ConsoleResult::Success)
                {
                    GP_LOG_WARNING(LogEditor, "app.TargetFps update rejected by priority rules.");
                }
            }
        }
    }

    /// @brief Creates or recreates the depth buffer matching `width x height` and transitions it to `DSVWrite`.
    /// @param[in] width Target width in pixels.
    /// @param[in] height Target height in pixels.
    /// @return True if `m_depthTexture` is valid after the call.
    [[nodiscard]] bool createOrResizeDepthTexture(const gp::UInt32 width, const gp::UInt32 height) noexcept
    {
        const gp::rhi::resource::TextureCreateDesc depthDesc =
            gp::rhi::resource::TextureCreateDesc::create2D(
                "CubeDepthBuffer",
                static_cast<gp::Int32>(width),
                static_cast<gp::Int32>(height),
                gp::rhi::PixelFormat::D32_Float
            )
                .setFlags(gp::rhi::resource::TextureCreateFlags::DepthStencilTargetable)
                .setClearValue(gp::rhi::resource::ClearValueBinding::kDepthOne);

        gp::rhi::command::CommandListImmediate& cmdList = gp::rhi::command::getImmediateCommandList();
        m_depthTexture = cmdList.createTexture(depthDesc);
        if (!m_depthTexture.isValid())
        {
            GP_LOG_ERROR(LogEditor, "Failed to create Cube depth buffer ({}x{}).", width, height);
            return false;
        }

        cmdList.immediateFlush(gp::rhi::command::ImmediateFlushType::FlushRHIThread);
        return true;
    }

    /// @brief Creates the Vulkan swapchain viewport, cube geometry buffers, depth buffer, states, and file-loaded
    /// shaders.
    /// @details Loads `game://shaders/Cube.vert.gpvs` and `game://shaders/Cube.frag.gpvs` via `VirtualFileSystem`.
    /// @note Must be executed on the main OS / render thread during `onInitialize()`.
    /// @return True if initialization succeeded or was intentionally skipped; false if Vulkan resource creation failed.
    [[nodiscard]] bool initializeCubeRenderer() noexcept
    {
        if (!isRHIInitialized() || gp::rhi::getRHIGlobals().usingNullRHI)
        {
            GP_LOG_INFO(LogEditor, "Cube renderer skipped (RHI inactive or running NullRHI).");
            return true;
        }

        gp::rhi::DynamicRHI* const activeRHI = getDynamicRHI();
        if (activeRHI == nullptr || activeRHI->getInterfaceType() != gp::rhi::InterfaceType::Vulkan)
        {
            GP_LOG_WARNING(
                LogEditor,
                "Cube renderer skipped: pre-compiled 'GPVS' shader files require Vulkan RHI (active='{}').",
                (activeRHI != nullptr) ? activeRHI->getName() : gp::StringView("None")
            );
            return true;
        }

        gp::hal::IWindow* const mainWindow = getMainWindow();
        if (mainWindow == nullptr)
        {
            GP_LOG_INFO(LogEditor, "Cube renderer skipped (no main window).");
            return true;
        }

        m_nativeWindowHandle = mainWindow->getNativeHandle();

        gp::hal::Extent2D fbSize = mainWindow->getFramebufferSize();
        if (fbSize.width == 0u || fbSize.height == 0u)
        {
            fbSize = mainWindow->getSize();
        }

        const gp::UInt32 initialWidth = (fbSize.width > 0u) ? fbSize.width : 1u;
        const gp::UInt32 initialHeight = (fbSize.height > 0u) ? fbSize.height : 1u;
        const bool initialFullscreen = mainWindow->isFullscreen();

        m_viewport = activeRHI->createViewport(
            &m_nativeWindowHandle, initialWidth, initialHeight, initialFullscreen, gp::rhi::PixelFormat::Unknown
        );
        if (!m_viewport.isValid())
        {
            GP_LOG_ERROR(LogEditor, "Failed to create Vulkan RHI swapchain viewport.");
            return false;
        }

        gp::rhi::command::getImmediateCommandList().immediateFlush(
            gp::rhi::command::ImmediateFlushType::FlushRHIThread
        );

        gp::Vector<gp::UInt8> vertexShaderBytes{};
        gp::Vector<gp::UInt8> pixelShaderBytes{};
        gp::rhi::shader::ShaderHash vertexShaderHash{};
        gp::rhi::shader::ShaderHash pixelShaderHash{};
        if (!loadShaderBytecode("game://shaders/Cube.vert.gpvs", vertexShaderBytes, vertexShaderHash) ||
            !loadShaderBytecode("game://shaders/Cube.frag.gpvs", pixelShaderBytes, pixelShaderHash))
        {
            shutdownCubeRenderer();
            return false;
        }

        m_vertexShader = activeRHI->createVertexShader(
            gp::ConstVectorView<gp::UInt8>(vertexShaderBytes.data(), vertexShaderBytes.size()), vertexShaderHash
        );
        m_pixelShader = activeRHI->createPixelShader(
            gp::ConstVectorView<gp::UInt8>(pixelShaderBytes.data(), pixelShaderBytes.size()), pixelShaderHash
        );

        gp::rhi::pipeline::VertexDeclarationElementList vertexElements{};
        const gp::UInt16 vertexStride = static_cast<gp::UInt16>(sizeof(CubeVertex));
        vertexElements.pushBack(
            gp::rhi::pipeline::VertexElement(0u, 0u, gp::rhi::pipeline::VertexElementType::Float3, 0u, vertexStride)
        );
        vertexElements.pushBack(
            gp::rhi::pipeline::VertexElement(0u, 12u, gp::rhi::pipeline::VertexElementType::Float3, 1u, vertexStride)
        );
        vertexElements.pushBack(
            gp::rhi::pipeline::VertexElement(0u, 24u, gp::rhi::pipeline::VertexElementType::Float3, 2u, vertexStride)
        );
        m_vertexDeclaration = activeRHI->createVertexDeclaration(vertexElements);

        gp::rhi::resource::ResourceArrayUploadView vertexUpload(
            kCubeVertices, static_cast<gp::UInt32>(sizeof(kCubeVertices))
        );
        gp::rhi::resource::BufferCreateDesc vertexBufferDesc =
            gp::rhi::resource::BufferCreateDesc::createVertex<CubeVertex>("CubeVertexBuffer", 24u);
        vertexBufferDesc.setInitActionResourceArray(&vertexUpload);
        m_vertexBuffer = gp::rhi::command::getImmediateCommandList().createBuffer(vertexBufferDesc);

        gp::rhi::resource::ResourceArrayUploadView indexUpload(
            kCubeIndices, static_cast<gp::UInt32>(sizeof(kCubeIndices))
        );
        gp::rhi::resource::BufferCreateDesc indexBufferDesc =
            gp::rhi::resource::BufferCreateDesc::createIndex<gp::UInt16>("CubeIndexBuffer", 36u);
        indexBufferDesc.setInitActionResourceArray(&indexUpload);
        m_indexBuffer = gp::rhi::command::getImmediateCommandList().createBuffer(indexBufferDesc);

        m_blendState = activeRHI->createBlendState(gp::rhi::pipeline::BlendStateDesc{});
        m_rasterizerStateSolid = activeRHI->createRasterizerState(
            gp::rhi::pipeline::RasterizerStateDesc(
                gp::rhi::pipeline::RasterizerFillMode::Solid, gp::rhi::pipeline::RasterizerCullMode::None, false
            )
        );
        m_rasterizerStateWireframe = activeRHI->createRasterizerState(
            gp::rhi::pipeline::RasterizerStateDesc(
                gp::rhi::pipeline::RasterizerFillMode::Wireframe, gp::rhi::pipeline::RasterizerCullMode::None, false
            )
        );
        m_depthStencilState = activeRHI->createDepthStencilState(
            gp::rhi::pipeline::DepthStencilStateDesc(true, gp::rhi::pipeline::CompareFunction::LessEqual)
        );

        if (!createOrResizeDepthTexture(initialWidth, initialHeight) || !m_vertexDeclaration.isValid() ||
            !m_vertexShader.isValid() || !m_pixelShader.isValid() || !m_vertexBuffer.isValid() ||
            !m_indexBuffer.isValid() || !m_blendState.isValid() || !m_rasterizerStateSolid.isValid() ||
            !m_rasterizerStateWireframe.isValid() || !m_depthStencilState.isValid())
        {
            GP_LOG_ERROR(LogEditor, "Failed to create Vulkan Cube pipeline or geometry resources.");
            shutdownCubeRenderer();
            return false;
        }

        m_viewportWidth = initialWidth;
        m_viewportHeight = initialHeight;
        m_viewportFullscreen = initialFullscreen;
        m_cubeRendererReady = true;

        GP_LOG_INFO(
            LogEditor, "[TEMP] 3D Cube Vulkan renderer initialized ({}x{}).", m_viewportWidth, m_viewportHeight
        );
        return true;
    }

    /// @brief Records and presents one frame of the orbit-camera 3D Cube through `CommandListImmediate`.
    void renderCube() noexcept
    {
        if (!m_cubeRendererReady || !m_viewport.isValid() || !isRHIInitialized())
        {
            return;
        }

        gp::hal::IWindow* const mainWindow = getMainWindow();
        if (mainWindow == nullptr || mainWindow->isMinimized())
        {
            return;
        }

        const gp::hal::Extent2D fbSize = mainWindow->getFramebufferSize();
        if (fbSize.width == 0u || fbSize.height == 0u)
        {
            return;
        }

        gp::rhi::DynamicRHI* const activeRHI = getDynamicRHI();
        if (activeRHI == nullptr)
        {
            return;
        }

        const bool isFullscreen = mainWindow->isFullscreen();
        if (fbSize.width != m_viewportWidth || fbSize.height != m_viewportHeight ||
            isFullscreen != m_viewportFullscreen)
        {
            activeRHI->resizeViewport(
                m_viewport.get(), fbSize.width, fbSize.height, isFullscreen, gp::rhi::PixelFormat::Unknown
            );
            m_viewportWidth = fbSize.width;
            m_viewportHeight = fbSize.height;
            m_viewportFullscreen = isFullscreen;
            (void)createOrResizeDepthTexture(m_viewportWidth, m_viewportHeight);
        }

        if (!m_depthTexture.isValid())
        {
            return;
        }

        gp::rhi::command::CommandListImmediate& cmdList = gp::rhi::command::getImmediateCommandList();
        cmdList.beginFrame();
        cmdList.beginDrawingViewport(m_viewport.get(), nullptr);

        const gp::rhi::resource::TextureRef backBuffer = activeRHI->getViewportBackBuffer(m_viewport.get());
        if (backBuffer.isValid())
        {
            cmdList.transition(
                gp::rhi::sync::TransitionInfo(
                    backBuffer.get(), gp::rhi::sync::ResourceAccess::Present, gp::rhi::sync::ResourceAccess::RTV
                )
            );

            // ImGui upload phase (texture streaming + geometry copy) must be recorded outside the render pass.
            m_imgui.prepareRender(cmdList);

            const gp::rhi::command::RenderPassInfo renderPassInfo(
                backBuffer.get(),
                gp::rhi::command::RenderTargetActions::ClearStore,
                m_depthTexture.get(),
                gp::rhi::command::DepthStencilTargetActions::ClearDepthNotStencilDontStore
            );
            cmdList.beginRenderPass(renderPassInfo, "OrbitCube");

            const gp::rhi::PixelFormat backBufferFormat = backBuffer->getFormat();
            if (!m_pipelineStateSolid.isValid() || !m_pipelineStateWireframe.isValid() ||
                m_cachedBackBufferFormat != backBufferFormat)
            {
                gp::rhi::pipeline::GraphicsPipelineStateInitializer psoInit{};
                psoInit.boundShaderState = gp::rhi::pipeline::BoundShaderStateInput(
                    m_vertexDeclaration.get(), m_vertexShader.get(), m_pixelShader.get()
                );
                psoInit.blendState = m_blendState.get();
                psoInit.depthStencilState = m_depthStencilState.get();
                psoInit.primitiveType = gp::rhi::pipeline::PrimitiveType::TriangleList;
                cmdList.applyCachedRenderTargets(psoInit);

                psoInit.rasterizerState = m_rasterizerStateSolid.get();
                m_pipelineStateSolid = activeRHI->createGraphicsPipelineState(psoInit);

                psoInit.rasterizerState = m_rasterizerStateWireframe.get();
                m_pipelineStateWireframe = activeRHI->createGraphicsPipelineState(psoInit);

                m_cachedBackBufferFormat = backBufferFormat;
            }

            gp::rhi::pipeline::GraphicsPipelineState* const activePso =
                m_wireframeEnabled ? m_pipelineStateWireframe.get() : m_pipelineStateSolid.get();
            if (activePso != nullptr)
            {
                const gp::UInt32 rtWidth = (backBuffer->getSizeX() > 0u) ? backBuffer->getSizeX() : m_viewportWidth;
                const gp::UInt32 rtHeight = (backBuffer->getSizeY() > 0u) ? backBuffer->getSizeY() : m_viewportHeight;
                const gp::Float32 aspectRatio =
                    static_cast<gp::Float32>(rtWidth) / static_cast<gp::Float32>((rtHeight > 0u) ? rtHeight : 1u);

                gp::Float32 eyeX = 0.0f;
                gp::Float32 eyeY = 0.0f;
                gp::Float32 eyeZ = 0.0f;
                const OrbitMat4 viewProj =
                    buildViewProjMatrix(m_cameraYaw, m_cameraPitch, m_cameraDistance, aspectRatio, eyeX, eyeY, eyeZ);

                const gp::Float32 lightAngle = m_elapsedTime * 0.9f;
                const gp::Float32 lightX = 2.1f * std::cos(lightAngle);
                const gp::Float32 lightY = 1.25f + 0.35f * std::sin(m_elapsedTime * 1.4f);
                const gp::Float32 lightZ = 2.1f * std::sin(lightAngle);

                cmdList.setGraphicsPipelineState(activePso, 0u, true);
                cmdList.setViewport(
                    0.0f, 0.0f, 0.0f, static_cast<gp::Float32>(rtWidth), static_cast<gp::Float32>(rtHeight), 1.0f
                );
                cmdList.setStreamSource(0u, m_vertexBuffer.get(), 0u);

                const gp::Float32 modeFlag = m_wireframeEnabled ? 1.0f : 0.0f;

                struct CubeDrawItem
                {
                    OrbitMat4 model;
                    gp::Float32 mode;
                };

                const CubeDrawItem drawItems[3u] = {
                    { makeRotationY(m_elapsedTime * 0.35f), modeFlag },
                    { makeTranslationScale(0.0f, -1.05f, 0.0f, 3.8f, 0.08f, 3.8f), modeFlag },
                    { makeTranslationScale(lightX, lightY, lightZ, 0.14f, 0.14f, 0.14f), 1.0f }
                };

                for (const CubeDrawItem& item: drawItems)
                {
                    OrbitGlobals globals{};
                    globals.mvp = multiplyMat4(viewProj, item.model);
                    globals.model = item.model;
                    globals.lightPosAndIntensity[0] = lightX;
                    globals.lightPosAndIntensity[1] = lightY;
                    globals.lightPosAndIntensity[2] = lightZ;
                    globals.lightPosAndIntensity[3] = 2.4f;
                    globals.cameraPosAndMode[0] = eyeX;
                    globals.cameraPosAndMode[1] = eyeY;
                    globals.cameraPosAndMode[2] = eyeZ;
                    globals.cameraPosAndMode[3] = item.mode;

                    gp::rhi::shader::BatchedShaderParameters& shaderParams = cmdList.getScratchShaderParameters();
                    shaderParams.setShaderParameter(0u, 0u, static_cast<gp::UInt32>(sizeof(OrbitGlobals)), &globals);
                    cmdList.setBatchedShaderParameters(m_vertexShader.get(), shaderParams);
                    cmdList.drawIndexedPrimitive(m_indexBuffer.get(), 0, 0u, 24u, 0u, 12u, 1u);
                }
            }

            // Overlay: ImGui draws last into the scene pass, compositing over the cube without an extra load/store.
            m_imgui.renderInPass(cmdList);

            cmdList.endRenderPass();

            cmdList.transition(
                gp::rhi::sync::TransitionInfo(
                    backBuffer.get(), gp::rhi::sync::ResourceAccess::RTV, gp::rhi::sync::ResourceAccess::Present
                )
            );
        }

        cmdList.endDrawingViewport(m_viewport.get(), true, mainWindow->isVSyncEnabled());
        cmdList.endFrame();
    }

    /// @brief Waits for GPU idle, releases all temporary Cube RHI handles, and flushes deletion queues.
    void shutdownCubeRenderer() noexcept
    {
        if (isRHIInitialized())
        {
            gp::rhi::command::getImmediateCommandList().blockUntilGPUIdle(true);
        }

        m_pipelineStateSolid.reset();
        m_pipelineStateWireframe.reset();
        m_vertexBuffer.reset();
        m_indexBuffer.reset();
        m_depthTexture.reset();
        m_vertexDeclaration.reset();
        m_vertexShader.reset();
        m_pixelShader.reset();
        m_blendState.reset();
        m_rasterizerStateSolid.reset();
        m_rasterizerStateWireframe.reset();
        m_depthStencilState.reset();
        m_viewport.reset();

        m_cachedBackBufferFormat = gp::rhi::PixelFormat::Unknown;
        m_viewportWidth = 0u;
        m_viewportHeight = 0u;
        m_viewportFullscreen = false;
        m_cubeRendererReady = false;

        if (isRHIInitialized())
        {
            gp::rhi::command::getImmediateCommandList().blockUntilGPUIdle(true);
            gp::rhi::resource::ResourceDeletionQueue::flushPendingDeletes(true);
        }
    }

    /// @brief Modal stdin shell invoked on `F1` / `Grave` (`~`) for ad-hoc CVar and command testing in `gp-editor`.
    void runTemporaryStdinShell() noexcept
    {
        getLogger().flush();

        std::fputs(
            "\n========================================================\n"
            "  [TEMP] Console Stdin Shell Active\n"
            "  Examples: help | cvar.list | cvar.dump | prof.Status | log.Stats | app.Stats | jobs.Stats | rhi.Status | vfs.Status | vfs.Mounts | module.Status | module.List\n"
            "  Set CVar: t.TimeScale 0.5  |  app.TargetFps=60  |  log.GlobalLevel Warning\n"
            "  Commands: 'resume' (or Enter) returns to gp-editor | 'quit' / 'app.Quit' exits\n"
            "========================================================\n",
            stdout
        );
        std::fflush(stdout);

        char lineBuffer[512]{};
        while (true)
        {
            std::fputs("gp-console> ", stdout);
            std::fflush(stdout);

            if (std::fgets(lineBuffer, static_cast<int>(sizeof(lineBuffer)), stdin) == nullptr)
            {
                std::fputs("\n[TEMP] stdin EOF reached; resuming gp-editor.\n", stdout);
                std::fflush(stdout);
                break;
            }

            const gp::USize rawLen = gp::container::CharTraits<char>::length(lineBuffer);
            const gp::StringView trimmed = gp::console::trimConsoleWhitespace(gp::StringView(lineBuffer, rawLen));
            if (trimmed.isEmpty() || trimmed.equalsIgnoreCase("resume") || trimmed.equalsIgnoreCase("exit"))
            {
                std::fputs("[TEMP] Resuming gp-editor window loop...\n", stdout);
                std::fflush(stdout);
                break;
            }

            gp::USize emittedLines = 0u;
            gp::console::ConsoleOutputDelegate shellOut = gp::console::ConsoleOutputDelegate::createLambda(
                [&emittedLines](const gp::StringView text) noexcept -> void
            {
                if (!text.isEmpty())
                {
                    (void)std::fwrite(text.data(), 1u, text.size(), stdout);
                }
                std::fputc('\n', stdout);
                ++emittedLines;
            }
            );

            if (trimmed.equalsIgnoreCase("quit"))
            {
                (void)executeConsoleCommand("app.Quit", &shellOut);
                std::fputs("[TEMP] Shutdown requested.\n", stdout);
                std::fflush(stdout);
                break;
            }

            const gp::console::ConsoleResult res = executeConsoleCommand(trimmed, &shellOut);
            if (emittedLines == 0u && res == gp::console::ConsoleResult::Success)
            {
                std::fputs("[ok]\n", stdout);
            }
            std::fflush(stdout);

            if (!isRunning())
            {
                break;
            }
        }

        // Flush any log messages triggered synchronously during shell execution before returning to frames.
        getLogger().flush();
    }

private:
    /// @brief Cached native OS window handle passed to `DynamicRHI::createViewport()`.
    gp::hal::NativeWindowHandle m_nativeWindowHandle{};

    /// @brief Swapchain presentation viewport bound to the primary editor window.
    gp::rhi::presentation::ViewportRef m_viewport{};

    /// @brief Interleaved vertex input declaration for `CubeVertex` (`position` + `color`).
    gp::rhi::pipeline::VertexDeclarationRef m_vertexDeclaration{};

    /// @brief Vertex buffer holding the 24 cube vertices.
    gp::rhi::resource::BufferRef m_vertexBuffer{};

    /// @brief Index buffer holding the 36 cube triangle-list indices.
    gp::rhi::resource::BufferRef m_indexBuffer{};

    /// @brief Depth buffer texture (`D32_Float`) sized to the swapchain.
    gp::rhi::resource::TextureRef m_depthTexture{};

    /// @brief Compiled Vulkan vertex shader stage loaded from `game://shaders/Cube.vert.gpvs`.
    gp::rhi::shader::VertexShaderRef m_vertexShader{};

    /// @brief Compiled Vulkan pixel shader stage loaded from `game://shaders/Cube.frag.gpvs`.
    gp::rhi::shader::PixelShaderRef m_pixelShader{};

    /// @brief Immutable opaque color blend state.
    gp::rhi::pipeline::BlendStateRef m_blendState{};

    /// @brief Immutable solid, back-face culled rasterizer state.
    gp::rhi::pipeline::RasterizerStateRef m_rasterizerStateSolid{};

    /// @brief Immutable wireframe, unculled rasterizer state.
    gp::rhi::pipeline::RasterizerStateRef m_rasterizerStateWireframe{};

    /// @brief Immutable depth-testing (`LessEqual`) depth-stencil state.
    gp::rhi::pipeline::DepthStencilStateRef m_depthStencilState{};

    /// @brief Cached solid graphics pipeline state matching `m_cachedBackBufferFormat`.
    gp::rhi::pipeline::GraphicsPipelineStateRef m_pipelineStateSolid{};

    /// @brief Cached wireframe graphics pipeline state matching `m_cachedBackBufferFormat`.
    gp::rhi::pipeline::GraphicsPipelineStateRef m_pipelineStateWireframe{};

    /// @brief Backbuffer pixel format compiled into the cached pipeline states.
    gp::rhi::PixelFormat m_cachedBackBufferFormat{ gp::rhi::PixelFormat::Unknown };

    /// @brief Last applied swapchain width in pixels.
    gp::UInt32 m_viewportWidth{ 0u };

    /// @brief Last applied swapchain height in pixels.
    gp::UInt32 m_viewportHeight{ 0u };

    /// @brief Last applied swapchain fullscreen state.
    bool m_viewportFullscreen{ false };

    /// @brief True once all Cube RHI resources have been created and validated.
    bool m_cubeRendererReady{ false };

    /// @brief True when wireframe rasterization is active (`Space` toggles).
    bool m_wireframeEnabled{ false };

    /// @brief Elapsed unscaled time in seconds driving light orbit and cube rotation.
    gp::Float32 m_elapsedTime{ 0.0f };

    /// @brief Orbit camera horizontal azimuth in radians.
    gp::Float32 m_cameraYaw{ 0.65f };

    /// @brief Orbit camera vertical elevation in radians.
    gp::Float32 m_cameraPitch{ 0.45f };

    /// @brief Orbit camera distance from the cube origin.
    gp::Float32 m_cameraDistance{ 3.0f };

    /// @brief Multiplier applied to `m_elapsedTime` progression (cube spin and light orbit).
    gp::Float32 m_animationSpeed{ 1.0f };

    /// @brief Number of frame-time samples kept for the statistics plot.
    static constexpr gp::USize kFrameTimeHistorySize = 120u;

    /// @brief Ring buffer of recent frame times in milliseconds.
    gp::Float32 m_frameTimeHistory[kFrameTimeHistorySize]{};

    /// @brief Next write index into `m_frameTimeHistory`.
    gp::USize m_frameTimeHistoryOffset{ 0u };

    /// @brief Accent color edited in the Scene panel and applied to ImGui check marks and sliders.
    gp::Float32 m_accentColor[3]{ 0.26f, 0.59f, 0.98f };

    /// @brief Scratch UTF-8 buffer for the Scene panel text field.
    char m_noteBuffer[128]{};

    /// @brief Scene panel visibility.
    bool m_showScenePanel{ true };

    /// @brief Statistics panel visibility.
    bool m_showStatsPanel{ true };

    /// @brief Dear ImGui demo window visibility.
    bool m_showImGuiDemo{ false };

    /// @brief Dear ImGui metrics/debugger window visibility.
    bool m_showImGuiMetrics{ false };

    /// @brief Dear ImGui integration (context + HAL platform + DynamicRHI renderer).
    gp::imgui::ImGuiLayer m_imgui{};
};

/// @brief Engine launch entry point invoked by `GP_IMPLEMENT_APPLICATION_ENTRY`.
/// @param[in] argc Command-line argument count.
/// @param[in] argv Command-line argument vector.
/// @return Process exit code (`0` on clean exit).
int launch([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    gp::ApplicationDesc desc{};
    desc.backend.appName = "GP Engine Sandbox";
    desc.backend.appVersion = "1.0.0";
    desc.mainWindow.title = "Graphical Playground - Vulkan Orbit Cube";
    desc.mainWindow.size = gp::hal::Extent2D(1280u, 720u);
    desc.targetForegroundFps = 120.0;
    desc.targetBackgroundFps = 30.0;
    desc.enableFileLogSink = true;
    desc.rhi.hasEditorToken = true;
    desc.rhi.preferredModuleName = gp::rhi::kVulkanRHIModuleName;

    EditorApplication app(desc, gp::makeUnique<gp::hal::sdl3::ApplicationBackend>());
    return app.run();
}

}   // namespace gp
