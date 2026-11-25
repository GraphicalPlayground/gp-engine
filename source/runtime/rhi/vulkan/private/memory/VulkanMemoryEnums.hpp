// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "RHIVulkan.hpp"
#include "templates/Enums.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Lowest device memory residency priority passed to `VK_EXT_memory_priority`.
inline constexpr gp::Float32 kVulkanMemoryLowPriority = 0.0f;

/// @brief Medium device memory residency priority passed to `VK_EXT_memory_priority`.
inline constexpr gp::Float32 kVulkanMemoryMediumPriority = 0.5f;

/// @brief Elevated device memory residency priority passed to `VK_EXT_memory_priority`.
inline constexpr gp::Float32 kVulkanMemoryHigherPriority = 0.75f;

/// @brief Highest device memory residency priority passed to `VK_EXT_memory_priority`.
inline constexpr gp::Float32 kVulkanMemoryHighestPriority = 1.0f;

/// @brief Broad Vulkan resource kind separating image pools from buffer pools.
enum class VulkanResourceType : gp::UInt8
{
    Image = 0u,
    Buffer,
    Count
};

/// @brief Number of distinct `VulkanResourceType` categories.
inline constexpr gp::USize kVulkanResourceTypeCount = static_cast<gp::USize>(VulkanResourceType::Count);

/// @brief Allocation backing category stored inside `VulkanAllocation`.
enum class VulkanAllocationType : gp::UInt8
{
    Empty = 0u,
    PooledBuffer,
    Buffer,
    Image,
    ImageDedicated,
    Count
};

/// @brief Diagnostic allocation usage category for memory accounting and dumps.
enum class VulkanAllocationMetaType : gp::UInt8
{
    Unknown = 0u,
    UniformBuffer,
    MultiBuffer,
    RingBuffer,
    FrameTempBuffer,
    ImageRenderTarget,
    ImageOther,
    BufferUAV,
    BufferStaging,
    BufferOther,
    Count
};

/// @brief Number of distinct `VulkanAllocationMetaType` categories.
inline constexpr gp::USize kVulkanAllocationMetaTypeCount = static_cast<gp::USize>(VulkanAllocationMetaType::Count);

/// @brief Caller-provided hints controlling heap selection, dedication, and binding of a GPU allocation.
enum class VulkanAllocationFlags : gp::UInt16
{
    None = 0x0000u,
    HostVisible = 0x0001u,
    HostCached = 0x0002u,
    PreferBAR = 0x0004u,
    Dedicated = 0x0008u,
    External = 0x0010u,
    Memoryless = 0x0020u,
    Protected = 0x0040u,
    NoError = 0x0080u,
    AutoBind = 0x0100u
};

/// @brief Controls whether freeing a `VulkanAllocation` defers release until GPU completion.
enum class VulkanFreeFlags : gp::UInt8
{
    None = 0x0u,
    DontDefer = 0x1u
};

/// @brief Selects which categories of pooled device-local allocations may be compacted by defragmentation.
enum class VulkanPoolDefragMode : gp::Int32
{
    Off = 0,
    All = 1,
    ImagesOnly = 2,
    BuffersOnly = 3
};

/// @brief Aggregated local (VRAM) and system (host) memory budget and usage snapshot.
struct VulkanMemoryStats final
{
    /// @brief Total budget across device-local heaps in bytes.
    gp::UInt64 budgetLocal{ 0u };

    /// @brief Current usage across device-local heaps in bytes.
    gp::UInt64 usedLocal{ 0u };

    /// @brief Remaining headroom across device-local heaps in bytes.
    gp::UInt64 availableLocal{ 0u };

    /// @brief Amount by which device-local usage exceeds budget in bytes.
    gp::UInt64 demotedLocal{ 0u };

    /// @brief Total budget across non-device-local (system) heaps in bytes.
    gp::UInt64 budgetSystem{ 0u };

    /// @brief Current usage across non-device-local (system) heaps in bytes.
    gp::UInt64 usedSystem{ 0u };

    /// @brief Remaining headroom across non-device-local (system) heaps in bytes.
    gp::UInt64 availableSystem{ 0u };

    /// @brief Amount by which system usage exceeds budget in bytes.
    gp::UInt64 demotedSystem{ 0u };
};

/// @brief Aggregated allocation and page statistics for a heap, pool allocator, or staging manager.
struct VulkanResourceHeapStats final
{
    /// @brief Number of active buffer allocations.
    gp::UInt64 bufferAllocations{ 0u };

    /// @brief Number of active image allocations.
    gp::UInt64 imageAllocations{ 0u };

    /// @brief Bytes actively consumed by image allocations.
    gp::UInt64 usedImageMemory{ 0u };

    /// @brief Bytes actively consumed by buffer allocations.
    gp::UInt64 usedBufferMemory{ 0u };

    /// @brief Total backing memory owned in bytes.
    gp::UInt64 totalMemory{ 0u };

    /// @brief Total number of backing memory pages.
    gp::UInt64 pages{ 0u };

    /// @brief Number of backing pages serving images.
    gp::UInt64 imagePages{ 0u };

    /// @brief Number of backing pages serving buffers.
    gp::UInt64 bufferPages{ 0u };

    /// @brief Bitwise OR of `VkMemoryPropertyFlags` represented by these stats.
    VkMemoryPropertyFlags memoryFlags{ 0u };

    /// @brief Accumulates another stats snapshot into this instance.
    /// @param[in] other Stats snapshot to add.
    /// @return Reference to `*this`.
    VulkanResourceHeapStats& operator+=(const VulkanResourceHeapStats& other) noexcept
    {
        bufferAllocations += other.bufferAllocations;
        imageAllocations += other.imageAllocations;
        usedImageMemory += other.usedImageMemory;
        usedBufferMemory += other.usedBufferMemory;
        totalMemory += other.totalMemory;
        pages += other.pages;
        imagePages += other.imagePages;
        bufferPages += other.bufferPages;
        memoryFlags |= other.memoryFlags;
        return *this;
    }
};

/// @brief Contiguous `[offset, offset + size)` byte range used by linear free-list helpers.
struct VulkanMemoryRange final
{
    /// @brief Starting byte offset.
    gp::UInt32 offset{ 0u };

    /// @brief Length in bytes.
    gp::UInt32 size{ 0u };

    /// @brief Orders ranges by ascending starting offset.
    /// @param[in] other Range to compare against.
    /// @return True when `offset < other.offset`.
    [[nodiscard]] constexpr bool operator<(const VulkanMemoryRange& other) const noexcept
    {
        return offset < other.offset;
    }

    /// @brief Merges contiguous adjacent ranges in an already offset-sorted array.
    /// @param[in,out] ranges Sorted range list to coalesce in place.
    static void joinConsecutiveRanges(gp::Vector<VulkanMemoryRange>& ranges) noexcept
    {
        if (ranges.size() <= 1u)
        {
            return;
        }

        for (gp::USize index = ranges.size() - 1u; index > 0u; --index)
        {
            const VulkanMemoryRange& current = ranges[index];
            VulkanMemoryRange& prev = ranges[index - 1u];
            if (prev.offset + prev.size == current.offset)
            {
                prev.size += current.size;
                ranges.erase(ranges.begin() + static_cast<gp::IntPtr>(index));
            }
        }
    }

    /// @brief Inserts `item` into offset-sorted `ranges` and coalesces with adjacent neighbors.
    /// @param[in,out] ranges Sorted free-range array.
    /// @param[in] item Range to insert.
    /// @return Index of the inserted or merged range.
    static gp::USize addAndMerge(gp::Vector<VulkanMemoryRange>& ranges, const VulkanMemoryRange item) noexcept
    {
        if (item.size == 0u)
        {
            return ranges.size();
        }

        gp::USize insertIndex = 0u;
        while (insertIndex < ranges.size() && ranges[insertIndex].offset < item.offset)
        {
            ++insertIndex;
        }

        const bool mergePrev =
            (insertIndex > 0u) && (ranges[insertIndex - 1u].offset + ranges[insertIndex - 1u].size == item.offset);
        const bool mergeNext = (insertIndex < ranges.size()) && (item.offset + item.size == ranges[insertIndex].offset);

        if (mergePrev && mergeNext)
        {
            ranges[insertIndex - 1u].size += item.size + ranges[insertIndex].size;
            ranges.erase(ranges.begin() + static_cast<gp::IntPtr>(insertIndex));
            return insertIndex - 1u;
        }
        if (mergePrev)
        {
            ranges[insertIndex - 1u].size += item.size;
            return insertIndex - 1u;
        }
        if (mergeNext)
        {
            ranges[insertIndex].offset = item.offset;
            ranges[insertIndex].size += item.size;
            return insertIndex;
        }

        ranges.insert(ranges.begin() + static_cast<gp::IntPtr>(insertIndex), item);
        return insertIndex;
    }

    /// @brief Consumes `sizeToAllocate` bytes from the front of `ranges[index]`, erasing the entry if exhausted.
    /// @param[in,out] ranges Range array.
    /// @param[in] index Entry index to consume from.
    /// @param[in] sizeToAllocate Byte count to carve out (`<= ranges[index].size`).
    static void allocateFromEntry(
        gp::Vector<VulkanMemoryRange>& ranges, const gp::USize index, const gp::UInt32 sizeToAllocate
    ) noexcept
    {
        GP_CHECK(index < ranges.size());
        GP_CHECK(sizeToAllocate <= ranges[index].size);

        if (sizeToAllocate == ranges[index].size)
        {
            ranges.erase(ranges.begin() + static_cast<gp::IntPtr>(index));
        }
        else
        {
            ranges[index].offset += sizeToAllocate;
            ranges[index].size -= sizeToAllocate;
        }
    }
};

/// @brief Returns a human-readable ASCII name for `type`.
/// @param[in] type Allocation category.
/// @return Static null-terminated label string.
[[nodiscard]] constexpr const char* vulkanAllocationTypeToString(const VulkanAllocationType type) noexcept
{
    switch (type)
    {
    case VulkanAllocationType::Empty:
        return "Empty";
    case VulkanAllocationType::PooledBuffer:
        return "PooledBuffer";
    case VulkanAllocationType::Buffer:
        return "Buffer";
    case VulkanAllocationType::Image:
        return "Image";
    case VulkanAllocationType::ImageDedicated:
        return "ImageDedicated";
    default:
        return "Unknown";
    }
}

/// @brief Returns a human-readable ASCII name for `metaType`.
/// @param[in] metaType Allocation usage category.
/// @return Static null-terminated label string.
[[nodiscard]] constexpr const char* vulkanAllocationMetaTypeToString(const VulkanAllocationMetaType metaType) noexcept
{
    switch (metaType)
    {
    case VulkanAllocationMetaType::Unknown:
        return "Unknown";
    case VulkanAllocationMetaType::UniformBuffer:
        return "UBO";
    case VulkanAllocationMetaType::MultiBuffer:
        return "MultiBuf";
    case VulkanAllocationMetaType::RingBuffer:
        return "RingBuf";
    case VulkanAllocationMetaType::FrameTempBuffer:
        return "FrameTemp";
    case VulkanAllocationMetaType::ImageRenderTarget:
        return "ImageRT";
    case VulkanAllocationMetaType::ImageOther:
        return "Image";
    case VulkanAllocationMetaType::BufferUAV:
        return "BufferUAV";
    case VulkanAllocationMetaType::BufferStaging:
        return "BufferStg";
    case VulkanAllocationMetaType::BufferOther:
        return "BufOther";
    default:
        return "Unknown";
    }
}

/// @brief Tests whether any bits of `mask` are set in `flags`.
/// @param[in] flags Source flag mask.
/// @param[in] mask Bits to test.
/// @return True when `(flags & mask) != 0`.
[[nodiscard]] constexpr bool
    hasAnyAllocationFlags(const VulkanAllocationFlags flags, const VulkanAllocationFlags mask) noexcept
{
    return (static_cast<gp::UInt16>(flags) & static_cast<gp::UInt16>(mask)) != 0u;
}

/// @brief Translates high-level `VulkanAllocationFlags` into native `VkMemoryPropertyFlags`.
/// @param[in] allocFlags Requested allocation flags.
/// @param[in] hasUnifiedMemory True on UMA devices where device-local memory is also host-visible and coherent.
/// @return Resolved `VkMemoryPropertyFlags`.
[[nodiscard]] inline VkMemoryPropertyFlags
    getMemoryPropertyFlags(const VulkanAllocationFlags allocFlags, const bool hasUnifiedMemory) noexcept
{
    GP_CHECKF(
        !(hasAnyAllocationFlags(allocFlags, VulkanAllocationFlags::PreferBAR) &&
          !hasAnyAllocationFlags(allocFlags, VulkanAllocationFlags::HostVisible)),
        "PreferBAR must always be paired with HostVisible."
    );

    if (hasAnyAllocationFlags(allocFlags, VulkanAllocationFlags::Memoryless))
    {
        return VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    }

    VkMemoryPropertyFlags memFlags = 0u;
    if (hasAnyAllocationFlags(allocFlags, VulkanAllocationFlags::HostCached))
    {
        memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    }
    else if (hasAnyAllocationFlags(allocFlags, VulkanAllocationFlags::HostVisible))
    {
        memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        if (hasAnyAllocationFlags(allocFlags, VulkanAllocationFlags::PreferBAR))
        {
            memFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }
    }
    else
    {
        memFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        if (hasAnyAllocationFlags(allocFlags, VulkanAllocationFlags::Protected))
        {
            memFlags |= VK_MEMORY_PROPERTY_PROTECTED_BIT;
        }
        else if (hasUnifiedMemory)
        {
            memFlags |= (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        }
    }

    return memFlags;
}

/// @brief Returns the active runtime pool defragmentation mode configured by `r.Vulkan.PoolAllocator.Defrag`.
/// @return Clamped `VulkanPoolDefragMode`.
[[nodiscard]] GP_RHI_VULKAN_API VulkanPoolDefragMode getVulkanPoolDefragMode() noexcept;

}   // namespace gp::rhi::vulkan

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::vulkan::VulkanAllocationFlags);
GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::vulkan::VulkanFreeFlags);
