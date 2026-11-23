// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/Name.hpp"
#include "core/ResourceEnums.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"

namespace gp::rhi::resource
{

/// @brief Pairs a buffer or allocation byte size with its element stride.
struct SizeAndStride
{
    /// @brief Total size in bytes.
    gp::UInt64 size{ 0u };

    /// @brief Size of a single element in bytes, or zero for untyped byte buffers.
    gp::UInt32 stride{ 0u };

    /// @brief Compares two `SizeAndStride` values for equality.
    /// @param[in] other Value to compare against.
    /// @return True when both size and stride match.
    [[nodiscard]] constexpr bool operator==(const SizeAndStride& other) const noexcept = default;
};

/// @brief Byte offset and span of a GPU video memory allocation.
struct VRamAllocation
{
    /// @brief Start address or heap offset of the allocation in bytes.
    gp::UInt64 allocationStart{ 0u };

    /// @brief Total length of the allocation in bytes.
    gp::UInt64 allocationSize{ 0u };

    /// @brief Constructs an empty, invalid VRAM allocation.
    constexpr VRamAllocation() noexcept = default;

    /// @brief Constructs a VRAM allocation with an explicit offset and byte size.
    /// @param[in] inAllocationStart Start address or heap offset in bytes.
    /// @param[in] inAllocationSize Allocation length in bytes.
    constexpr VRamAllocation(const gp::UInt64 inAllocationStart, const gp::UInt64 inAllocationSize) noexcept
        : allocationStart(inAllocationStart)
        , allocationSize(inAllocationSize)
    {}

    /// @brief Reports whether this allocation covers a non-zero byte span.
    /// @return True when `allocationSize > 0`.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return allocationSize > 0u;
    }

    /// @brief Compares two VRAM allocations for equality.
    /// @param[in] other Allocation to compare against.
    /// @return True when both start offset and size match.
    [[nodiscard]] constexpr bool operator==(const VRamAllocation& other) const noexcept = default;
};

/// @brief Diagnostic snapshot describing an RHI resource's identity, type, and VRAM footprint.
struct ResourceInfo
{
    /// @brief Debug name assigned to the resource.
    gp::Name name{};

    /// @brief Concrete RHI resource type tag.
    ResourceType type{ ResourceType::None };

    /// @brief Backing GPU memory range reported by the active backend.
    VRamAllocation vramAllocation{};

    /// @brief True when the resource comes from a transient frame allocator.
    bool isTransient{ false };

    /// @brief True when the populated diagnostic data is valid.
    bool isValid{ true };

    /// @brief True when the resource's GPU backing memory is currently resident.
    bool isResident{ true };

    /// @brief Compares two `ResourceInfo` records for equality.
    /// @param[in] other Record to compare against.
    /// @return True when all fields match.
    [[nodiscard]] constexpr bool operator==(const ResourceInfo& other) const noexcept = default;
};

}   // namespace gp::rhi::resource
