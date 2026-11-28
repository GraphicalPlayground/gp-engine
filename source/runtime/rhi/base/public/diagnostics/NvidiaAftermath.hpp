// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/Breadcrumbs.hpp"
#include "diagnostics/DiagnosticsForward.hpp"   // IWYU pragma: keep
#include "RHIBase.hpp"
#include "templates/Enums.hpp"

namespace gp::rhi::diagnostics::nvidia
{

/// @brief Feature flags controlling NVIDIA GPU crash dump instrumentation, shader tracking, and markers.
enum class FeatureFlags : gp::UInt32
{
    /// @brief No GPU crash dump features enabled.
    None = 0u,

    /// @brief Enables command-stream breadcrumb marker instrumentation.
    EnableMarkers = 1u << 0,

    /// @brief Captures CPU call stacks for command list submissions.
    CallStackCapturing = 1u << 1,

    /// @brief Tracks live RHI resources and virtual addresses for page-fault correlation.
    ResourceTracking = 1u << 2,

    /// @brief Generates line-table debug info during shader compilation.
    GenerateShaderDebugInfo = 1u << 3,

    /// @brief Enables hardware shader exception and error reporting.
    EnableShaderErrorReporting = 1u << 4,

    /// @brief Standard crash dump feature mask.
    Default = EnableMarkers | CallStackCapturing | ResourceTracking | EnableShaderErrorReporting
};

/// @brief Outcome returned by `onGPUCrash()` after waiting on crash dump generation.
enum class CrashResult : gp::UInt8
{
    /// @brief Crash dump collection was not enabled or not initialized.
    Disabled = 0u,

    /// @brief Crash dump completed and was written before the timeout expired.
    Dumped = 1u,

    /// @brief Waiting for crash dump completion timed out.
    TimedOut = 2u,

    /// @brief Driver reported an error while generating or writing the crash dump.
    Failed = 3u
};

/// @brief Resolves a `BreadcrumbNode*` into a pointer/size marker payload ready for command buffer injection.
class GP_RHI_API Marker final
{
public:
    /// @brief Resolves `node` (`nullptr` or `Sentinel` maps to `"<root>"`).
    /// @param[in] node Breadcrumb node pointer.
    explicit Marker(const BreadcrumbNode* node) noexcept;

    /// @brief Returns the marker payload pointer.
    /// @return Non-null pointer to marker bytes.
    [[nodiscard]] const void* getData() const noexcept
    {
        return m_resolvedText;
    }

    /// @brief Returns the marker text as a null-terminated C string.
    /// @return Non-null C string.
    [[nodiscard]] const char* getText() const noexcept
    {
        return m_resolvedText;
    }

    /// @brief Returns the marker payload size in bytes (including trailing null terminator).
    /// @return Payload byte size.
    [[nodiscard]] gp::UInt32 getSizeInBytes() const noexcept
    {
        return m_sizeInBytes;
    }

    /// @brief Returns the `BreadcrumbNode::id` (`0` for `<root>`).
    /// @return Node identifier.
    [[nodiscard]] gp::UInt32 getNodeId() const noexcept
    {
        return m_nodeId;
    }

private:
    BreadcrumbBuffer m_scratch{};
    const char* m_resolvedText{ "<root>" };
    gp::UInt32 m_sizeInBytes{ 7u };
    gp::UInt32 m_nodeId{ 0u };
};

/// @brief Metadata retained for a registered shader binary in the crash dump lookup table.
struct RegisteredShaderEntry
{
    /// @brief Deterministic 64-bit FNV-1a hash of the shader binary.
    ShaderHash hash{ 0u };

    /// @brief Byte size of the compiled shader binary.
    gp::UInt32 byteSize{ 0u };

    /// @brief Optional shader debug label/path.
    gp::String debugName{};
};

/// @brief Optional callback invoked right before waiting on a GPU crash dump so deferred shader libraries can register
///        binaries lazily.
using LateShaderAssociateFn = void (*)() noexcept;

/// @brief Backend driver hooks registered by graphics APIs (`rhi/vulkan`, `rhi/d3d12`) that link the NVIDIA crash SDK.
struct BackendCallbacks
{
    /// @brief Pre-device-creation SDK initialization hook.
    using InitBeforeDeviceFn = bool (*)(FeatureFlags flags, void* userData) noexcept;

    /// @brief Post-device-creation SDK attachment hook receiving the native API device pointer.
    using InitDeviceFn = bool (*)(void* nativeDevice, FeatureFlags flags, void* userData) noexcept;

    /// @brief Optional driver shader registration hook.
    using RegisterShaderFn = void (*)(ShaderHash hash, const void* data, gp::UInt32 size, void* userData) noexcept;

    /// @brief Optional driver shader deregistration hook.
    using DeregisterShaderFn = void (*)(ShaderHash hash, void* userData) noexcept;

    /// @brief Crash dump wait/write hook invoked on GPU fault.
    using WaitCrashDumpFn =
        CrashResult (*)(gp::Float32 timeoutSeconds, gp::StringView outputDirectory, void* userData) noexcept;

    InitBeforeDeviceFn initBeforeDevice{ nullptr };
    InitDeviceFn initDevice{ nullptr };
    RegisterShaderFn registerShader{ nullptr };
    DeregisterShaderFn deregisterShader{ nullptr };
    WaitCrashDumpFn waitCrashDump{ nullptr };
    void* userData{ nullptr };
};

/// @brief Enables or disables NVIDIA GPU crash dump instrumentation.
/// @param[in] enabled True to enable crash dump support.
GP_RHI_API void setEnabled(bool enabled) noexcept;

/// @brief Reports whether NVIDIA GPU crash dump instrumentation is enabled.
/// @return True when enabled.
[[nodiscard]] GP_RHI_API bool isEnabled() noexcept;

/// @brief Reports whether command-stream crash markers are enabled (`isEnabled()` and `FeatureFlags::EnableMarkers`).
/// @return True when markers should be injected.
[[nodiscard]] GP_RHI_API bool areMarkersEnabled() noexcept;

/// @brief Sets the active `FeatureFlags` mask.
/// @param[in] flags Feature bitmask.
GP_RHI_API void setFeatureFlags(FeatureFlags flags) noexcept;

/// @brief Returns the active `FeatureFlags` mask.
/// @return Active `FeatureFlags`.
[[nodiscard]] GP_RHI_API FeatureFlags getFeatureFlags() noexcept;

/// @brief Registers backend driver callbacks implemented by a concrete graphics backend.
/// @param[in] callbacks Driver callback table.
GP_RHI_API void registerBackendCallbacks(const BackendCallbacks& callbacks) noexcept;

/// @brief Runs pre-device-creation crash dump SDK setup.
/// @return True if enabled and initialized cleanly.
GP_RHI_API bool initializeBeforeDeviceCreation() noexcept;

/// @brief Binds the crash dump tracker to an initialized `nativeDevice`.
/// @param[in] nativeDevice Opaque backend device handle (`VkDevice`, `ID3D12Device*`, etc.).
/// @return True if device initialization succeeded.
GP_RHI_API bool initializeDevice(void* nativeDevice) noexcept;

/// @brief Registers a compiled shader binary in the thread-safe crash dump lookup table.
/// @param[in] shaderData Pointer to compiled shader bytecode.
/// @param[in] byteSize Byte length of `shaderData`.
/// @param[in] debugName Optional debug name or asset path.
/// @return Computed 64-bit `ShaderHash` (`0` if `shaderData == nullptr || byteSize == 0`).
GP_RHI_API ShaderHash
    registerShaderBinary(const void* shaderData, gp::UInt32 byteSize, gp::StringView debugName = {}) noexcept;

/// @brief Removes a previously registered shader binary identified by `hash`.
/// @param[in] hash Shader hash returned by `registerShaderBinary()`.
/// @return True if an entry matching `hash` was found and removed.
GP_RHI_API bool deregisterShaderBinary(ShaderHash hash) noexcept;

/// @brief Returns the number of currently registered shader binaries.
/// @return Registered shader count.
[[nodiscard]] GP_RHI_API gp::USize getRegisteredShaderCount() noexcept;

/// @brief Looks up metadata for a registered shader binary by `hash`.
/// @param[in] hash Shader hash to query.
/// @param[out] outEntry Populated shader entry when found.
/// @return True if `hash` is currently registered.
[[nodiscard]] GP_RHI_API bool findRegisteredShader(ShaderHash hash, RegisteredShaderEntry& outEntry) noexcept;

/// @brief Installs an optional late shader association callback invoked at the start of `onGPUCrash()`.
/// @param[in] callback Callback pointer (`nullptr` to clear).
GP_RHI_API void setLateShaderAssociateCallback(LateShaderAssociateFn callback) noexcept;

/// @brief Orchestrates late shader association and waits up to `timeoutSeconds` for the GPU crash dump to complete.
/// @param[in] timeoutSeconds Maximum wait time in seconds.
/// @param[in] outputDirectory Optional crash dump output directory.
/// @return `CrashResult` indicating whether a dump was written, timed out, failed, or was disabled.
GP_RHI_API CrashResult onGPUCrash(gp::Float32 timeoutSeconds = 5.0f, gp::StringView outputDirectory = {}) noexcept;

/// @brief Resets all crash dump flags, registered shaders, and callbacks to clean defaults.
GP_RHI_API void reset() noexcept;

}   // namespace gp::rhi::diagnostics::nvidia

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::diagnostics::nvidia::FeatureFlags);
