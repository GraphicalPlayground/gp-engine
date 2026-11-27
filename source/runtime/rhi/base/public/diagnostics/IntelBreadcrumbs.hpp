// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/Breadcrumbs.hpp"
#include "diagnostics/DiagnosticsForward.hpp"   // IWYU pragma: keep
#include "RHIBase.hpp"
#include "templates/Enums.hpp"

namespace gp::rhi::diagnostics::intel
{

/// @brief Bitflags controlling which vendor crash artifacts are written upon an Intel GPU hang or page fault.
enum class CrashDumpFlags : gp::UInt8
{
    /// @brief Crash dumps disabled.
    None = 0u,

    /// @brief Full GPU state and memory crash dump.
    FullDump = 1u << 0,

    /// @brief Compact GPU execution state mini-dump.
    MiniDump = 1u << 1,

    /// @brief Hardware pipeline register state dump.
    StateDump = 1u << 2,

    /// @brief Instruction-level hardware execution trace dump.
    TraceDump = 1u << 3,

    /// @brief Enables every supported dump artifact.
    All = FullDump | MiniDump | StateDump | TraceDump
};

/// @brief Formats a `BreadcrumbNode*` into a vendor-ready crash marker payload without proprietary SDK headers.
class GP_RHI_API Marker final
{
public:
    /// @brief Resolves `node` into a null-terminated marker payload (`"<root>"` when `node` is null or sentinel).
    /// @param[in] node Target breadcrumb node.
    explicit Marker(const BreadcrumbNode* node) noexcept;

    /// @brief Returns the resolved null-terminated marker string.
    /// @return Non-null C string valid for the lifetime of this `Marker`.
    [[nodiscard]] const char* getText() const noexcept
    {
        return m_resolvedText;
    }

    /// @brief Returns the marker string byte length including the trailing null terminator.
    /// @return Byte size (`>= 1`).
    [[nodiscard]] gp::UInt32 getSizeInBytes() const noexcept
    {
        return m_sizeInBytes;
    }

    /// @brief Returns the underlying `BreadcrumbNode::id` (`0` for `<root>`).
    /// @return Breadcrumb node ID.
    [[nodiscard]] gp::UInt32 getNodeId() const noexcept
    {
        return m_nodeId;
    }

    /// @brief Reports whether this marker resolved from a non-null, non-sentinel breadcrumb node.
    /// @return True when backed by a concrete `BreadcrumbNode`.
    [[nodiscard]] bool isNodeMarker() const noexcept
    {
        return m_isNodeMarker;
    }

private:
    BreadcrumbBuffer m_scratch{};
    const char* m_resolvedText{ "<root>" };
    gp::UInt32 m_sizeInBytes{ 7u };
    gp::UInt32 m_nodeId{ 0u };
    bool m_isNodeMarker{ false };
};

/// @brief Backend driver hooks registered by graphics APIs (`rhi/d3d12`, `rhi/vulkan`) that link the Intel extension
///        SDK.
struct BackendCallbacks
{
    /// @brief Pre-device-creation driver hook enabling GPU crash dump instrumentation.
    using InitDriverFn = bool (*)(CrashDumpFlags flags, void* userData) noexcept;

    /// @brief Post-crash driver hook writing binary vendor crash artifacts into `outputDirectory`.
    using WriteDumpFn = bool (*)(gp::StringView outputDirectory, CrashDumpFlags flags, void* userData) noexcept;

    /// @brief Optional pre-device initialization callback.
    InitDriverFn initDriver{ nullptr };

    /// @brief Optional crash dump writer callback.
    WriteDumpFn writeDump{ nullptr };

    /// @brief Opaque backend context forwarded to callbacks.
    void* userData{ nullptr };
};

/// @brief Enables or disables Intel GPU breadcrumb crash dump collection.
/// @param[in] enabled True to enable crash dump collection.
GP_RHI_API void setEnabled(bool enabled) noexcept;

/// @brief Reports whether Intel GPU breadcrumb crash dump collection is currently active and initialized.
/// @return True when enabled.
[[nodiscard]] GP_RHI_API bool isEnabled() noexcept;

/// @brief Configures which crash dump artifact flags are requested on GPU fault.
/// @param[in] flags Bitmask of `CrashDumpFlags`.
GP_RHI_API void setCrashDumpFlags(CrashDumpFlags flags) noexcept;

/// @brief Returns the currently configured `CrashDumpFlags`.
/// @return Active `CrashDumpFlags`.
[[nodiscard]] GP_RHI_API CrashDumpFlags getCrashDumpFlags() noexcept;

/// @brief Registers backend driver callbacks implemented by a concrete graphics backend.
/// @param[in] callbacks Callback table.
GP_RHI_API void registerBackendCallbacks(const BackendCallbacks& callbacks) noexcept;

/// @brief Runs pre-device-creation initialization for Intel GPU crash dumps when enabled.
/// @return True if initialization succeeded (or succeeded with default no-op backend hooks).
GP_RHI_API bool initializeBeforeDeviceCreation() noexcept;

/// @brief Invokes the registered crash dump writer upon a GPU hang or page fault.
/// @param[in] outputDirectory Destination directory for vendor crash dump files.
/// @return True if crash dump collection succeeded.
GP_RHI_API bool onGPUCrash(gp::StringView outputDirectory = {}) noexcept;

/// @brief Resets Intel crash dump state and registered callbacks back to defaults.
GP_RHI_API void reset() noexcept;

}   // namespace gp::rhi::diagnostics::intel

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::diagnostics::intel::CrashDumpFlags);
