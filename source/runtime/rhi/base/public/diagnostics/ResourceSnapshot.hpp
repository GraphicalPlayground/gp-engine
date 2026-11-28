// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"         // IWYU pragma: keep
#include "containers/strings/String.hpp"        // IWYU pragma: keep
#include "containers/views/StringView.hpp"      // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/DiagnosticsForward.hpp"   // IWYU pragma: keep
#include "resources/TextureMemoryStats.hpp"
#include "RHIBase.hpp"

namespace gp::rhi::diagnostics
{

/// @brief Per-resource memory footprint and usage flag record captured during resource tracking dumps.
struct ResourceStats
{
    /// @brief Resource debug label (`ViewableResource::getName()` or `<unnamed>`).
    gp::String name{};

    /// @brief Owning subsystem/asset label (`Resource::getOwnerName()`).
    gp::String ownerName{};

    /// @brief Concrete RHI resource type string (`"Texture2D"`, `"Buffer"`, etc.).
    gp::String type{};

    /// @brief Storage pixel format string (`"Unknown"` for non-formatted resources).
    gp::String pixelFormat{};

    /// @brief GPU allocation size in bytes.
    gp::UInt64 sizeInBytes{ 0u };

    /// @brief True when created with `TextureCreateFlags::RenderTargetable`.
    bool hasRenderTargetFlag{ false };

    /// @brief True when created with `TextureCreateFlags::DepthStencilTargetable`.
    bool hasDepthStencilFlag{ false };

    /// @brief True when created with `TextureCreateFlags::UAV` or `BufferUsageFlags::UnorderedAccess`.
    bool hasUnorderedAccessFlag{ false };

    /// @brief True when created with `BufferUsageFlags::AccelerationStructure` or ray tracing geometry/scene type.
    bool hasRayTracingAccelerationStructure{ false };

    /// @brief True when backing GPU memory is resident.
    bool isResident{ true };

    /// @brief True when queued for deferred deletion (`Resource::isMarkedForDelete()`).
    bool isMarkedForDelete{ false };

    /// @brief True when backed by a transient heap allocator.
    bool isTransient{ false };

    /// @brief True when marked `TextureCreateFlags::Streamable` or `BufferUsageFlags::Streamable`.
    bool isStreaming{ false };

    /// @brief True when physical memory is currently committed (`Resource::isCommitted()`).
    bool isCommitted{ true };

    /// @brief Default equality comparison across all fields.
    /// @param[in] rhs Record to compare against.
    /// @return True when all fields match.
    [[nodiscard]] bool operator==(const ResourceStats& rhs) const noexcept = default;
};

/// @brief Device, driver, and baseline VRAM telemetry header stored inside a `ResourceSnapshot`.
struct ResourceSnapshotMetadata
{
    /// @brief Active RHI backend name (e.g., `"Vulkan"`, `"D3D12"`, `"Null"`).
    gp::String rhiName{};

    /// @brief GPU adapter name from `getRHIGlobals().gpuInfo.adapterName`.
    gp::String adapterName{};

    /// @brief User-facing GPU driver version string.
    gp::String driverVersion{};

    /// @brief Active shader feature level string (`"SM6"`, `"SM5"`, etc.).
    gp::String featureLevel{};

    /// @brief Host physical RAM capacity in bytes (optional telemetry).
    gp::UInt64 totalPhysicalMemory{ 0u };

    /// @brief Dedicated GPU VRAM in bytes.
    gp::UInt64 dedicatedVideoMemory{ 0u };

    /// @brief Shared host system memory available to the GPU in bytes.
    gp::UInt64 sharedSystemMemory{ 0u };

    /// @brief Sum of `sizeInBytes` across all `ResourceSnapshot::resources` entries.
    gp::UInt64 totalResourceSize{ 0u };

    /// @brief UTC capture timestamp in microseconds since the Unix epoch.
    gp::UInt64 timestampUtcMicroseconds{ 0u };

    /// @brief Hardware/baseline texture memory counters captured alongside the snapshot.
    resource::TextureMemoryStats textureMemoryStats{};

    /// @brief Default equality comparison.
    /// @param[in] rhs Metadata to compare against.
    /// @return True when all fields match.
    [[nodiscard]] bool operator==(const ResourceSnapshotMetadata& rhs) const noexcept = default;
};

/// @brief Binary-serializable snapshot of all live tracked RHI resources and adapter memory telemetry.
struct GP_RHI_API ResourceSnapshot
{
    /// @brief 32-bit file magic `'RHIS'` (`0x52484953`) identifying serialized snapshot files.
    static constexpr gp::UInt32 kMagic = 0x52484953u;

    /// @brief Binary format schema version.
    static constexpr gp::UInt32 kCurrentVersion = 1u;

    /// @brief Snapshot adapter and memory header.
    ResourceSnapshotMetadata metadata{};

    /// @brief Per-resource statistics captured at snapshot time.
    gp::Vector<ResourceStats> resources{};

    /// @brief Serializes this snapshot atomically to `filePath` (computing `metadata.totalResourceSize` if zero).
    /// @param[in] filePath Destination UTF-8 file path.
    /// @return True if written and flushed to disk; false on I/O failure.
    [[nodiscard]] bool saveToFile(gp::StringView filePath) noexcept;

    /// @brief Deserializes a `ResourceSnapshot` from `filePath`, validating magic, version, and payload bounds.
    /// @param[in] filePath Source UTF-8 file path.
    /// @param[out] outSnapshot Populated snapshot on success.
    /// @return True on valid deserialization; false if missing, truncated, or corrupt.
    [[nodiscard]] static bool loadFromFile(gp::StringView filePath, ResourceSnapshot& outSnapshot) noexcept;

    /// @brief Captures all currently tracked RHI resources and writes the resulting snapshot directly to `filePath`.
    /// @param[in] filePath Destination UTF-8 file path.
    /// @return True if captured and written; false on I/O error.
    [[nodiscard]] static bool writeTrackedResourcesToFile(gp::StringView filePath) noexcept;
};

/// @brief Gathers `ResourceStats` records for all RHI resources currently registered in the global tracking list.
/// @param[out] outStats Destination vector cleared and populated with live resource records.
GP_RHI_API void getTrackedResourceStats(gp::Vector<ResourceStats>& outStats) noexcept;

/// @brief Captures a complete in-memory `ResourceSnapshot` from the active `DynamicRHI` and tracked resource list.
/// @return Populated `ResourceSnapshot`.
[[nodiscard]] GP_RHI_API ResourceSnapshot captureResourceSnapshot() noexcept;

/// @brief Formats a human-readable summary of tracked RHI resource memory sorted by descending byte size.
/// @param[in] maxResourcesToList Maximum number of top resources to list individually (`<= 0` lists summary only).
/// @return Formatted multi-line memory report.
[[nodiscard]] GP_RHI_API gp::String dumpResourceMemorySummary(gp::Int32 maxResourcesToList = 32) noexcept;

}   // namespace gp::rhi::diagnostics
