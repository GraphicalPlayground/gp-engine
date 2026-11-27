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
#include "containers/strings/CharTraits.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceDeletionQueue.hpp"
#include "core/ResourceTypes.hpp"            // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "device/DeviceEnums.hpp"
#include "device/DynamicRHI.hpp"
#include "device/RHIGlobals.hpp"
#include "device/RHIModuleDiscovery.hpp"
#include "Launch.hpp"
#include "logging/LogMacros.hpp"
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
#include "resources/Texture.hpp"
#include "RHIPixelFormat.hpp"
#include "shaders/PixelShader.hpp"    // IWYU pragma: keep
#include "shaders/ShaderForward.hpp"
#include "shaders/VertexShader.hpp"   // IWYU pragma: keep
#include "sync/SyncEnums.hpp"
#include "sync/TransitionDesc.hpp"
#include "vfs/VirtualFileSystem.hpp"
#include "window/NativeWindow.hpp"
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
    true,
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

// TEMP: Pre-compiled Vulkan RHI ('GPVS' v1) shader blobs for Hello Triangle verification.
//
// Vertex Shader GLSL 450 (entry point 'main_00000464_b487c9c6', inOutMask = 0x0):
//   out gl_PerVertex { vec4 gl_Position; };
//   layout(location = 0) out vec3 outColor;
//   void main() {
//       const vec2 kPos[3] = vec2[3](vec2(0.0, 0.5), vec2(0.5, -0.5), vec2(-0.5, -0.5));
//       const vec3 kCol[3] = vec3[3](vec3(1.0, 0.2, 0.2), vec3(0.2, 1.0, 0.3), vec3(0.2, 0.4, 1.0));
//       gl_Position = vec4(kPos[gl_VertexIndex], 0.0, 1.0);
//       outColor = kCol[gl_VertexIndex];
//   }
//
// Pixel Shader GLSL 450 (entry point 'main_000001c0_ffca31e2', inOutMask = 0x1):
//   layout(location = 0) in vec3 inColor;
//   layout(location = 0) out vec4 outFragColor;
//   void main() { outFragColor = vec4(inColor, 1.0); }

/// @brief 128-bit content hash for `kHelloTriangleVertexShaderBlob`.
constexpr gp::rhi::shader::ShaderHash kHelloTriangleVertexShaderHash(0xD81AC0B022F6E85Cull, 0x60D6FBCD4166CA0Bull);

/// @brief Pre-compiled serialized `'GPVS'` v1 Vulkan vertex shader blob generating a procedural RGB triangle.
constexpr gp::UInt8 kHelloTriangleVertexShaderBlob[1263u] = {
    0x47u, 0x50u, 0x56u, 0x53u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x5Cu, 0xE8u, 0xF6u, 0x22u, 0xB0u, 0xC0u, 0x1Au, 0xD8u,
    0x0Bu, 0xCAu, 0x66u, 0x41u, 0xCDu, 0xFBu, 0xD6u, 0x60u, 0xC6u, 0xC9u, 0x87u, 0xB4u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x0Fu, 0x00u, 0x00u, 0x00u, 0x48u, 0x65u, 0x6Cu, 0x6Cu, 0x6Fu, 0x54u, 0x72u, 0x69u, 0x61u, 0x6Eu, 0x67u, 0x6Cu,
    0x65u, 0x56u, 0x53u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x64u, 0x04u, 0x00u, 0x00u, 0x03u, 0x02u, 0x23u, 0x07u, 0x00u,
    0x03u, 0x01u, 0x00u, 0x0Bu, 0x00u, 0x0Du, 0x00u, 0x37u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x11u,
    0x00u, 0x02u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x0Bu, 0x00u, 0x06u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x47u,
    0x4Cu, 0x53u, 0x4Cu, 0x2Eu, 0x73u, 0x74u, 0x64u, 0x2Eu, 0x34u, 0x35u, 0x30u, 0x00u, 0x00u, 0x00u, 0x00u, 0x0Eu,
    0x00u, 0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x0Fu, 0x00u, 0x0Cu, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 0x6Du, 0x61u, 0x69u, 0x6Eu, 0x5Fu, 0x30u, 0x30u, 0x30u, 0x30u,
    0x30u, 0x34u, 0x36u, 0x34u, 0x5Fu, 0x62u, 0x34u, 0x38u, 0x37u, 0x63u, 0x39u, 0x63u, 0x36u, 0x00u, 0x00u, 0x0Au,
    0x00u, 0x00u, 0x00u, 0x19u, 0x00u, 0x00u, 0x00u, 0x28u, 0x00u, 0x00u, 0x00u, 0x47u, 0x00u, 0x03u, 0x00u, 0x08u,
    0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x48u, 0x00u, 0x05u, 0x00u, 0x08u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x0Bu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x47u, 0x00u, 0x04u, 0x00u, 0x19u,
    0x00u, 0x00u, 0x00u, 0x0Bu, 0x00u, 0x00u, 0x00u, 0x2Au, 0x00u, 0x00u, 0x00u, 0x47u, 0x00u, 0x04u, 0x00u, 0x28u,
    0x00u, 0x00u, 0x00u, 0x1Eu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x13u, 0x00u, 0x02u, 0x00u, 0x02u,
    0x00u, 0x00u, 0x00u, 0x21u, 0x00u, 0x03u, 0x00u, 0x03u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x16u,
    0x00u, 0x03u, 0x00u, 0x06u, 0x00u, 0x00u, 0x00u, 0x20u, 0x00u, 0x00u, 0x00u, 0x17u, 0x00u, 0x04u, 0x00u, 0x07u,
    0x00u, 0x00u, 0x00u, 0x06u, 0x00u, 0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 0x1Eu, 0x00u, 0x03u, 0x00u, 0x08u,
    0x00u, 0x00u, 0x00u, 0x07u, 0x00u, 0x00u, 0x00u, 0x20u, 0x00u, 0x04u, 0x00u, 0x09u, 0x00u, 0x00u, 0x00u, 0x03u,
    0x00u, 0x00u, 0x00u, 0x08u, 0x00u, 0x00u, 0x00u, 0x3Bu, 0x00u, 0x04u, 0x00u, 0x09u, 0x00u, 0x00u, 0x00u, 0x0Au,
    0x00u, 0x00u, 0x00u, 0x03u, 0x00u, 0x00u, 0x00u, 0x15u, 0x00u, 0x04u, 0x00u, 0x0Bu, 0x00u, 0x00u, 0x00u, 0x20u,
    0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x2Bu, 0x00u, 0x04u, 0x00u, 0x0Bu, 0x00u, 0x00u, 0x00u, 0x0Cu,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x17u, 0x00u, 0x04u, 0x00u, 0x0Du, 0x00u, 0x00u, 0x00u, 0x06u,
    0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x15u, 0x00u, 0x04u, 0x00u, 0x0Eu, 0x00u, 0x00u, 0x00u, 0x20u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x2Bu, 0x00u, 0x04u, 0x00u, 0x0Eu, 0x00u, 0x00u, 0x00u, 0x0Fu,
    0x00u, 0x00u, 0x00u, 0x03u, 0x00u, 0x00u, 0x00u, 0x1Cu, 0x00u, 0x04u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u, 0x0Du,
    0x00u, 0x00u, 0x00u, 0x0Fu, 0x00u, 0x00u, 0x00u, 0x2Bu, 0x00u, 0x04u, 0x00u, 0x06u, 0x00u, 0x00u, 0x00u, 0x11u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x2Bu, 0x00u, 0x04u, 0x00u, 0x06u, 0x00u, 0x00u, 0x00u, 0x12u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x3Fu, 0x2Cu, 0x00u, 0x05u, 0x00u, 0x0Du, 0x00u, 0x00u, 0x00u, 0x13u,
    0x00u, 0x00u, 0x00u, 0x11u, 0x00u, 0x00u, 0x00u, 0x12u, 0x00u, 0x00u, 0x00u, 0x2Bu, 0x00u, 0x04u, 0x00u, 0x06u,
    0x00u, 0x00u, 0x00u, 0x14u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xBFu, 0x2Cu, 0x00u, 0x05u, 0x00u, 0x0Du,
    0x00u, 0x00u, 0x00u, 0x15u, 0x00u, 0x00u, 0x00u, 0x12u, 0x00u, 0x00u, 0x00u, 0x14u, 0x00u, 0x00u, 0x00u, 0x2Cu,
    0x00u, 0x05u, 0x00u, 0x0Du, 0x00u, 0x00u, 0x00u, 0x16u, 0x00u, 0x00u, 0x00u, 0x14u, 0x00u, 0x00u, 0x00u, 0x14u,
    0x00u, 0x00u, 0x00u, 0x2Cu, 0x00u, 0x06u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u, 0x17u, 0x00u, 0x00u, 0x00u, 0x13u,
    0x00u, 0x00u, 0x00u, 0x15u, 0x00u, 0x00u, 0x00u, 0x16u, 0x00u, 0x00u, 0x00u, 0x20u, 0x00u, 0x04u, 0x00u, 0x18u,
    0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x0Bu, 0x00u, 0x00u, 0x00u, 0x3Bu, 0x00u, 0x04u, 0x00u, 0x18u,
    0x00u, 0x00u, 0x00u, 0x19u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x20u, 0x00u, 0x04u, 0x00u, 0x1Bu,
    0x00u, 0x00u, 0x00u, 0x07u, 0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u, 0x20u, 0x00u, 0x04u, 0x00u, 0x1Du,
    0x00u, 0x00u, 0x00u, 0x07u, 0x00u, 0x00u, 0x00u, 0x0Du, 0x00u, 0x00u, 0x00u, 0x2Bu, 0x00u, 0x04u, 0x00u, 0x06u,
    0x00u, 0x00u, 0x00u, 0x20u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x80u, 0x3Fu, 0x20u, 0x00u, 0x04u, 0x00u, 0x24u,
    0x00u, 0x00u, 0x00u, 0x03u, 0x00u, 0x00u, 0x00u, 0x07u, 0x00u, 0x00u, 0x00u, 0x17u, 0x00u, 0x04u, 0x00u, 0x26u,
    0x00u, 0x00u, 0x00u, 0x06u, 0x00u, 0x00u, 0x00u, 0x03u, 0x00u, 0x00u, 0x00u, 0x20u, 0x00u, 0x04u, 0x00u, 0x27u,
    0x00u, 0x00u, 0x00u, 0x03u, 0x00u, 0x00u, 0x00u, 0x26u, 0x00u, 0x00u, 0x00u, 0x3Bu, 0x00u, 0x04u, 0x00u, 0x27u,
    0x00u, 0x00u, 0x00u, 0x28u, 0x00u, 0x00u, 0x00u, 0x03u, 0x00u, 0x00u, 0x00u, 0x1Cu, 0x00u, 0x04u, 0x00u, 0x29u,
    0x00u, 0x00u, 0x00u, 0x26u, 0x00u, 0x00u, 0x00u, 0x0Fu, 0x00u, 0x00u, 0x00u, 0x2Bu, 0x00u, 0x04u, 0x00u, 0x06u,
    0x00u, 0x00u, 0x00u, 0x2Au, 0x00u, 0x00u, 0x00u, 0xCDu, 0xCCu, 0x4Cu, 0x3Eu, 0x2Cu, 0x00u, 0x06u, 0x00u, 0x26u,
    0x00u, 0x00u, 0x00u, 0x2Bu, 0x00u, 0x00u, 0x00u, 0x20u, 0x00u, 0x00u, 0x00u, 0x2Au, 0x00u, 0x00u, 0x00u, 0x2Au,
    0x00u, 0x00u, 0x00u, 0x2Bu, 0x00u, 0x04u, 0x00u, 0x06u, 0x00u, 0x00u, 0x00u, 0x2Cu, 0x00u, 0x00u, 0x00u, 0x9Au,
    0x99u, 0x99u, 0x3Eu, 0x2Cu, 0x00u, 0x06u, 0x00u, 0x26u, 0x00u, 0x00u, 0x00u, 0x2Du, 0x00u, 0x00u, 0x00u, 0x2Au,
    0x00u, 0x00u, 0x00u, 0x20u, 0x00u, 0x00u, 0x00u, 0x2Cu, 0x00u, 0x00u, 0x00u, 0x2Bu, 0x00u, 0x04u, 0x00u, 0x06u,
    0x00u, 0x00u, 0x00u, 0x2Eu, 0x00u, 0x00u, 0x00u, 0xCDu, 0xCCu, 0xCCu, 0x3Eu, 0x2Cu, 0x00u, 0x06u, 0x00u, 0x26u,
    0x00u, 0x00u, 0x00u, 0x2Fu, 0x00u, 0x00u, 0x00u, 0x2Au, 0x00u, 0x00u, 0x00u, 0x2Eu, 0x00u, 0x00u, 0x00u, 0x20u,
    0x00u, 0x00u, 0x00u, 0x2Cu, 0x00u, 0x06u, 0x00u, 0x29u, 0x00u, 0x00u, 0x00u, 0x30u, 0x00u, 0x00u, 0x00u, 0x2Bu,
    0x00u, 0x00u, 0x00u, 0x2Du, 0x00u, 0x00u, 0x00u, 0x2Fu, 0x00u, 0x00u, 0x00u, 0x20u, 0x00u, 0x04u, 0x00u, 0x32u,
    0x00u, 0x00u, 0x00u, 0x07u, 0x00u, 0x00u, 0x00u, 0x29u, 0x00u, 0x00u, 0x00u, 0x20u, 0x00u, 0x04u, 0x00u, 0x34u,
    0x00u, 0x00u, 0x00u, 0x07u, 0x00u, 0x00u, 0x00u, 0x26u, 0x00u, 0x00u, 0x00u, 0x36u, 0x00u, 0x05u, 0x00u, 0x02u,
    0x00u, 0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x03u, 0x00u, 0x00u, 0x00u, 0xF8u,
    0x00u, 0x02u, 0x00u, 0x05u, 0x00u, 0x00u, 0x00u, 0x3Bu, 0x00u, 0x04u, 0x00u, 0x1Bu, 0x00u, 0x00u, 0x00u, 0x1Cu,
    0x00u, 0x00u, 0x00u, 0x07u, 0x00u, 0x00u, 0x00u, 0x3Bu, 0x00u, 0x04u, 0x00u, 0x32u, 0x00u, 0x00u, 0x00u, 0x33u,
    0x00u, 0x00u, 0x00u, 0x07u, 0x00u, 0x00u, 0x00u, 0x3Du, 0x00u, 0x04u, 0x00u, 0x0Bu, 0x00u, 0x00u, 0x00u, 0x1Au,
    0x00u, 0x00u, 0x00u, 0x19u, 0x00u, 0x00u, 0x00u, 0x3Eu, 0x00u, 0x03u, 0x00u, 0x1Cu, 0x00u, 0x00u, 0x00u, 0x17u,
    0x00u, 0x00u, 0x00u, 0x41u, 0x00u, 0x05u, 0x00u, 0x1Du, 0x00u, 0x00u, 0x00u, 0x1Eu, 0x00u, 0x00u, 0x00u, 0x1Cu,
    0x00u, 0x00u, 0x00u, 0x1Au, 0x00u, 0x00u, 0x00u, 0x3Du, 0x00u, 0x04u, 0x00u, 0x0Du, 0x00u, 0x00u, 0x00u, 0x1Fu,
    0x00u, 0x00u, 0x00u, 0x1Eu, 0x00u, 0x00u, 0x00u, 0x51u, 0x00u, 0x05u, 0x00u, 0x06u, 0x00u, 0x00u, 0x00u, 0x21u,
    0x00u, 0x00u, 0x00u, 0x1Fu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x51u, 0x00u, 0x05u, 0x00u, 0x06u,
    0x00u, 0x00u, 0x00u, 0x22u, 0x00u, 0x00u, 0x00u, 0x1Fu, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x50u,
    0x00u, 0x07u, 0x00u, 0x07u, 0x00u, 0x00u, 0x00u, 0x23u, 0x00u, 0x00u, 0x00u, 0x21u, 0x00u, 0x00u, 0x00u, 0x22u,
    0x00u, 0x00u, 0x00u, 0x11u, 0x00u, 0x00u, 0x00u, 0x20u, 0x00u, 0x00u, 0x00u, 0x41u, 0x00u, 0x05u, 0x00u, 0x24u,
    0x00u, 0x00u, 0x00u, 0x25u, 0x00u, 0x00u, 0x00u, 0x0Au, 0x00u, 0x00u, 0x00u, 0x0Cu, 0x00u, 0x00u, 0x00u, 0x3Eu,
    0x00u, 0x03u, 0x00u, 0x25u, 0x00u, 0x00u, 0x00u, 0x23u, 0x00u, 0x00u, 0x00u, 0x3Eu, 0x00u, 0x03u, 0x00u, 0x33u,
    0x00u, 0x00u, 0x00u, 0x30u, 0x00u, 0x00u, 0x00u, 0x41u, 0x00u, 0x05u, 0x00u, 0x34u, 0x00u, 0x00u, 0x00u, 0x35u,
    0x00u, 0x00u, 0x00u, 0x33u, 0x00u, 0x00u, 0x00u, 0x1Au, 0x00u, 0x00u, 0x00u, 0x3Du, 0x00u, 0x04u, 0x00u, 0x26u,
    0x00u, 0x00u, 0x00u, 0x36u, 0x00u, 0x00u, 0x00u, 0x35u, 0x00u, 0x00u, 0x00u, 0x3Eu, 0x00u, 0x03u, 0x00u, 0x28u,
    0x00u, 0x00u, 0x00u, 0x36u, 0x00u, 0x00u, 0x00u, 0xFDu, 0x00u, 0x01u, 0x00u, 0x38u, 0x00u, 0x01u, 0x00u
};

/// @brief 128-bit content hash for `kHelloTrianglePixelShaderBlob`.
constexpr gp::rhi::shader::ShaderHash kHelloTrianglePixelShaderHash(0xAFFBA83EA3812F9Dull, 0x0D10BC44A9E43B0Full);

/// @brief Pre-compiled serialized `'GPVS'` v1 Vulkan pixel shader blob outputting interpolated vertex RGB color.
constexpr gp::UInt8 kHelloTrianglePixelShaderBlob[587u] = {
    0x47u, 0x50u, 0x56u, 0x53u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x9Du, 0x2Fu, 0x81u, 0xA3u, 0x3Eu, 0xA8u, 0xFBu, 0xAFu,
    0x0Fu, 0x3Bu, 0xE4u, 0xA9u, 0x44u, 0xBCu, 0x10u, 0x0Du, 0xE2u, 0x31u, 0xCAu, 0xFFu, 0x00u, 0x00u, 0x00u, 0x00u,
    0x0Fu, 0x00u, 0x00u, 0x00u, 0x48u, 0x65u, 0x6Cu, 0x6Cu, 0x6Fu, 0x54u, 0x72u, 0x69u, 0x61u, 0x6Eu, 0x67u, 0x6Cu,
    0x65u, 0x50u, 0x53u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xC0u, 0x01u, 0x00u, 0x00u, 0x03u, 0x02u, 0x23u, 0x07u, 0x00u,
    0x03u, 0x01u, 0x00u, 0x0Bu, 0x00u, 0x0Du, 0x00u, 0x13u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x11u,
    0x00u, 0x02u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x0Bu, 0x00u, 0x06u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x47u,
    0x4Cu, 0x53u, 0x4Cu, 0x2Eu, 0x73u, 0x74u, 0x64u, 0x2Eu, 0x34u, 0x35u, 0x30u, 0x00u, 0x00u, 0x00u, 0x00u, 0x0Eu,
    0x00u, 0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x0Fu, 0x00u, 0x0Bu, 0x00u, 0x04u,
    0x00u, 0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 0x6Du, 0x61u, 0x69u, 0x6Eu, 0x5Fu, 0x30u, 0x30u, 0x30u, 0x30u,
    0x30u, 0x31u, 0x63u, 0x30u, 0x5Fu, 0x66u, 0x66u, 0x63u, 0x61u, 0x33u, 0x31u, 0x65u, 0x32u, 0x00u, 0x00u, 0x09u,
    0x00u, 0x00u, 0x00u, 0x0Cu, 0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x03u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 0x07u,
    0x00u, 0x00u, 0x00u, 0x47u, 0x00u, 0x04u, 0x00u, 0x09u, 0x00u, 0x00u, 0x00u, 0x1Eu, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x47u, 0x00u, 0x04u, 0x00u, 0x0Cu, 0x00u, 0x00u, 0x00u, 0x1Eu, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x13u, 0x00u, 0x02u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x21u, 0x00u, 0x03u, 0x00u, 0x03u,
    0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x16u, 0x00u, 0x03u, 0x00u, 0x06u, 0x00u, 0x00u, 0x00u, 0x20u,
    0x00u, 0x00u, 0x00u, 0x17u, 0x00u, 0x04u, 0x00u, 0x07u, 0x00u, 0x00u, 0x00u, 0x06u, 0x00u, 0x00u, 0x00u, 0x04u,
    0x00u, 0x00u, 0x00u, 0x20u, 0x00u, 0x04u, 0x00u, 0x08u, 0x00u, 0x00u, 0x00u, 0x03u, 0x00u, 0x00u, 0x00u, 0x07u,
    0x00u, 0x00u, 0x00u, 0x3Bu, 0x00u, 0x04u, 0x00u, 0x08u, 0x00u, 0x00u, 0x00u, 0x09u, 0x00u, 0x00u, 0x00u, 0x03u,
    0x00u, 0x00u, 0x00u, 0x17u, 0x00u, 0x04u, 0x00u, 0x0Au, 0x00u, 0x00u, 0x00u, 0x06u, 0x00u, 0x00u, 0x00u, 0x03u,
    0x00u, 0x00u, 0x00u, 0x20u, 0x00u, 0x04u, 0x00u, 0x0Bu, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x0Au,
    0x00u, 0x00u, 0x00u, 0x3Bu, 0x00u, 0x04u, 0x00u, 0x0Bu, 0x00u, 0x00u, 0x00u, 0x0Cu, 0x00u, 0x00u, 0x00u, 0x01u,
    0x00u, 0x00u, 0x00u, 0x2Bu, 0x00u, 0x04u, 0x00u, 0x06u, 0x00u, 0x00u, 0x00u, 0x0Eu, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x80u, 0x3Fu, 0x36u, 0x00u, 0x05u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x03u, 0x00u, 0x00u, 0x00u, 0xF8u, 0x00u, 0x02u, 0x00u, 0x05u, 0x00u, 0x00u, 0x00u, 0x3Du,
    0x00u, 0x04u, 0x00u, 0x0Au, 0x00u, 0x00u, 0x00u, 0x0Du, 0x00u, 0x00u, 0x00u, 0x0Cu, 0x00u, 0x00u, 0x00u, 0x51u,
    0x00u, 0x05u, 0x00u, 0x06u, 0x00u, 0x00u, 0x00u, 0x0Fu, 0x00u, 0x00u, 0x00u, 0x0Du, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x51u, 0x00u, 0x05u, 0x00u, 0x06u, 0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u, 0x0Du,
    0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x51u, 0x00u, 0x05u, 0x00u, 0x06u, 0x00u, 0x00u, 0x00u, 0x11u,
    0x00u, 0x00u, 0x00u, 0x0Du, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x50u, 0x00u, 0x07u, 0x00u, 0x07u,
    0x00u, 0x00u, 0x00u, 0x12u, 0x00u, 0x00u, 0x00u, 0x0Fu, 0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u, 0x11u,
    0x00u, 0x00u, 0x00u, 0x0Eu, 0x00u, 0x00u, 0x00u, 0x3Eu, 0x00u, 0x03u, 0x00u, 0x09u, 0x00u, 0x00u, 0x00u, 0x12u,
    0x00u, 0x00u, 0x00u, 0xFDu, 0x00u, 0x01u, 0x00u, 0x38u, 0x00u, 0x01u, 0x00u
};

}   // namespace

/// @brief Sandbox editor host application driving HAL subsystems and a temporary Vulkan Hello Triangle renderer.
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

        if (!initializeHelloTriangleRenderer())
        {
            return false;
        }

        GP_LOG_INFO(
            LogEditor,
            "Controls: 'ESC'/'Q' exit | 'F1'/'`' temporary stdin console shell | WASD, mouse, gamepad test input."
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

        const gp::hal::input::IInputSystem& input = getInput();

        // Check for exit commands using keyboard Scancodes or Keycodes
        if (input.isScancodeDown(gp::hal::input::Scancode::Escape) || input.isScancodeDown(gp::hal::input::Scancode::Q))
        {
            GP_LOG_INFO(LogEditor, "Exit key detected. Requesting shutdown...");
            requestQuit();
            return;
        }

        // TEMP: Switch terminal stdin into an interactive console command shell on F1 or Grave (` / ~).
        if (input.wasScancodeJustPressed(gp::hal::input::Scancode::F1) ||
            input.wasScancodeJustPressed(gp::hal::input::Scancode::Grave))
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

        // Query Mouse State (Position and relative motion deltas)
        const auto mousePos = input.getMouse().getPosition();
        const auto mouseDelta = input.getMouse().getDelta();
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
        if (getInput().wasKeyJustPressed(gp::hal::input::Keycode::M))
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
        if (kb.hasModifiers(gp::hal::input::KeyModifier::Ctrl))
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
        if (getInput().wasKeyJustPressed(gp::hal::input::Keycode::O))
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
    /// @note Records a single-pass Vulkan Hello Triangle frame onto `CommandListImmediate` and presents the swapchain.
    virtual void onRender(const gp::time::FrameTimeStats& timeStats) noexcept override
    {
        GP_PROFILE_SCOPE_NC_ACTIVE(
            "EditorApplication::OnRender", GP_U32_COLOR_BLUE, profiling::ProfilingControl::isZonesEnabled()
        );

        [[maybe_unused]] const gp::Float32 alpha = timeStats.fixedStep.interpolationAlpha;
        renderHelloTriangle();
    }

    /// @copydoc gp::Application::onShutdown()
    /// @note Drains GPU execution and releases all temporary RHI handles before `Application::shutdownInternal()`
    ///       tears down `DynamicRHI`, unloads the backend module, and closes the OS window.
    virtual void onShutdown() noexcept override
    {
        GP_PROFILE_SCOPE_NC_ACTIVE(
            "EditorApplication::OnShutdown", GP_U32_COLOR_RED, profiling::ProfilingControl::isZonesEnabled()
        );

        shutdownHelloTriangleRenderer();
        GP_LOG_INFO(LogEditor, "Application shut down cleanly.");
    }

private:
    /// @brief Creates the temporary Vulkan swapchain viewport, fixed-function states, and pre-compiled shaders.
    /// @details Gracefully skips setup and returns `true` when running headless (`usingNullRHI`) or when a non-Vulkan
    ///          backend was forced via command-line flags, because the embedded `'GPVS'` shader blobs are Vulkan-only.
    /// @note Must be executed on the main OS / render thread during `onInitialize()`.
    /// @return True if initialization succeeded or was intentionally skipped; false if Vulkan resource creation failed.
    [[nodiscard]] bool initializeHelloTriangleRenderer() noexcept
    {
        if (!isRHIInitialized() || gp::rhi::getRHIGlobals().usingNullRHI)
        {
            GP_LOG_INFO(LogEditor, "Hello Triangle renderer skipped (RHI inactive or running NullRHI).");
            return true;
        }

        gp::rhi::DynamicRHI* const activeRHI = getDynamicRHI();
        if (activeRHI == nullptr || activeRHI->getInterfaceType() != gp::rhi::InterfaceType::Vulkan)
        {
            GP_LOG_WARNING(
                LogEditor,
                "Hello Triangle renderer skipped: embedded 'GPVS' shader blobs require Vulkan RHI (active='{}').",
                (activeRHI != nullptr) ? activeRHI->getName() : gp::StringView("None")
            );
            return true;
        }

        gp::hal::IWindow* const mainWindow = getMainWindow();
        if (mainWindow == nullptr)
        {
            GP_LOG_INFO(LogEditor, "Hello Triangle renderer skipped (no main window).");
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

        const gp::rhi::pipeline::VertexDeclarationElementList emptyElements{};
        m_vertexDeclaration = activeRHI->createVertexDeclaration(emptyElements);

        m_vertexShader = activeRHI->createVertexShader(
            gp::ConstVectorView<gp::UInt8>(kHelloTriangleVertexShaderBlob, sizeof(kHelloTriangleVertexShaderBlob)),
            kHelloTriangleVertexShaderHash
        );
        m_pixelShader = activeRHI->createPixelShader(
            gp::ConstVectorView<gp::UInt8>(kHelloTrianglePixelShaderBlob, sizeof(kHelloTrianglePixelShaderBlob)),
            kHelloTrianglePixelShaderHash
        );

        m_blendState = activeRHI->createBlendState(gp::rhi::pipeline::BlendStateDesc{});
        m_rasterizerState = activeRHI->createRasterizerState(
            gp::rhi::pipeline::RasterizerStateDesc(
                gp::rhi::pipeline::RasterizerFillMode::Solid, gp::rhi::pipeline::RasterizerCullMode::None, false
            )
        );
        m_depthStencilState = activeRHI->createDepthStencilState(
            gp::rhi::pipeline::DepthStencilStateDesc(false, gp::rhi::pipeline::CompareFunction::Always)
        );

        if (!m_vertexDeclaration.isValid() || !m_vertexShader.isValid() || !m_pixelShader.isValid() ||
            !m_blendState.isValid() || !m_rasterizerState.isValid() || !m_depthStencilState.isValid())
        {
            GP_LOG_ERROR(LogEditor, "Failed to create Vulkan Hello Triangle pipeline state prerequisites.");
            shutdownHelloTriangleRenderer();
            return false;
        }

        m_viewportWidth = initialWidth;
        m_viewportHeight = initialHeight;
        m_viewportFullscreen = initialFullscreen;
        m_helloTriangleReady = true;

        GP_LOG_INFO(
            LogEditor, "[TEMP] Hello Triangle Vulkan renderer initialized ({}x{}).", m_viewportWidth, m_viewportHeight
        );
        return true;
    }

    /// @brief Records and presents one frame of the procedural RGB Hello Triangle through `CommandListImmediate`.
    /// @details Synchronizes swapchain extent against OS framebuffer resizes, transitions the swapchain backbuffer
    ///          `Present -> RTV -> Present` outside the render pass, lazily compiles/caches the `GraphicsPipelineState`
    ///          against the active backbuffer format, and issues a 3-vertex `TriangleList` draw call.
    void renderHelloTriangle() noexcept
    {
        if (!m_helloTriangleReady || !m_viewport.isValid() || !isRHIInitialized())
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

            const gp::rhi::command::RenderPassInfo renderPassInfo(
                backBuffer.get(), gp::rhi::command::RenderTargetActions::ClearStore
            );
            cmdList.beginRenderPass(renderPassInfo, "HelloTriangle");

            const gp::rhi::PixelFormat backBufferFormat = backBuffer->getFormat();
            if (!m_pipelineState.isValid() || m_cachedBackBufferFormat != backBufferFormat)
            {
                gp::rhi::pipeline::GraphicsPipelineStateInitializer psoInit{};
                psoInit.boundShaderState = gp::rhi::pipeline::BoundShaderStateInput(
                    m_vertexDeclaration.get(), m_vertexShader.get(), m_pixelShader.get()
                );
                psoInit.blendState = m_blendState.get();
                psoInit.rasterizerState = m_rasterizerState.get();
                psoInit.depthStencilState = m_depthStencilState.get();
                psoInit.primitiveType = gp::rhi::pipeline::PrimitiveType::TriangleList;
                cmdList.applyCachedRenderTargets(psoInit);

                m_pipelineState = activeRHI->createGraphicsPipelineState(psoInit);
                m_cachedBackBufferFormat = backBufferFormat;
            }

            if (m_pipelineState.isValid())
            {
                const gp::UInt32 rtWidth = (backBuffer->getSizeX() > 0u) ? backBuffer->getSizeX() : m_viewportWidth;
                const gp::UInt32 rtHeight = (backBuffer->getSizeY() > 0u) ? backBuffer->getSizeY() : m_viewportHeight;

                cmdList.setGraphicsPipelineState(m_pipelineState.get(), 0u, true);
                cmdList.setViewport(
                    0.0f, 0.0f, 0.0f, static_cast<gp::Float32>(rtWidth), static_cast<gp::Float32>(rtHeight), 1.0f
                );
                cmdList.drawPrimitive(0u, 1u, 1u);
            }

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

    /// @brief Waits for GPU idle, releases all temporary Hello Triangle RHI handles, and flushes deletion queues.
    void shutdownHelloTriangleRenderer() noexcept
    {
        if (isRHIInitialized())
        {
            gp::rhi::command::getImmediateCommandList().blockUntilGPUIdle(true);
        }

        m_pipelineState.reset();
        m_vertexDeclaration.reset();
        m_vertexShader.reset();
        m_pixelShader.reset();
        m_blendState.reset();
        m_rasterizerState.reset();
        m_depthStencilState.reset();
        m_viewport.reset();

        m_cachedBackBufferFormat = gp::rhi::PixelFormat::Unknown;
        m_viewportWidth = 0u;
        m_viewportHeight = 0u;
        m_viewportFullscreen = false;
        m_helloTriangleReady = false;

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

    /// @brief Empty vertex input declaration (positions and colors are generated via `gl_VertexIndex`).
    gp::rhi::pipeline::VertexDeclarationRef m_vertexDeclaration{};

    /// @brief Compiled Vulkan vertex shader stage for the Hello Triangle pass.
    gp::rhi::shader::VertexShaderRef m_vertexShader{};

    /// @brief Compiled Vulkan pixel shader stage for the Hello Triangle pass.
    gp::rhi::shader::PixelShaderRef m_pixelShader{};

    /// @brief Immutable opaque color blend state.
    gp::rhi::pipeline::BlendStateRef m_blendState{};

    /// @brief Immutable solid, unculled rasterizer state.
    gp::rhi::pipeline::RasterizerStateRef m_rasterizerState{};

    /// @brief Immutable depth-disabled depth-stencil state.
    gp::rhi::pipeline::DepthStencilStateRef m_depthStencilState{};

    /// @brief Cached monolithic graphics pipeline state matching `m_cachedBackBufferFormat`.
    gp::rhi::pipeline::GraphicsPipelineStateRef m_pipelineState{};

    /// @brief Backbuffer pixel format compiled into `m_pipelineState`.
    gp::rhi::PixelFormat m_cachedBackBufferFormat{ gp::rhi::PixelFormat::Unknown };

    /// @brief Last applied swapchain width in pixels.
    gp::UInt32 m_viewportWidth{ 0u };

    /// @brief Last applied swapchain height in pixels.
    gp::UInt32 m_viewportHeight{ 0u };

    /// @brief Last applied swapchain fullscreen state.
    bool m_viewportFullscreen{ false };

    /// @brief True once all Hello Triangle RHI resources have been created and validated.
    bool m_helloTriangleReady{ false };
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
    desc.mainWindow.title = "Graphical Playground - Vulkan Hello Triangle";
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
