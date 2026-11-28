// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/GPUMask.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/HashCombine.hpp"
#include "hashing/HashOf.hpp"
#include "resources/BufferEnums.hpp"
#include "RHIBase.hpp"
#include "sync/SyncEnums.hpp"

namespace gp::rhi::resource
{

/// @brief Immutable layout descriptor for a GPU buffer resource.
/// @details Captures byte size, element stride, usage flags, and multi-GPU node affinity. Buffers without backing GPU
///          memory (placeholders used for streaming) are represented by `BufferDesc::null()`.
struct BufferDesc
{
    /// @brief Total size of the buffer allocation in bytes.
    gp::UInt32 size{ 0u };

    /// @brief Distance in bytes between consecutive elements, or zero for untyped/vertex buffers.
    gp::UInt32 stride{ 0u };

    /// @brief Bitmask describing buffer lifetime, memory placement, and pipeline binding roles.
    BufferUsageFlags usage{ BufferUsageFlags::None };

    /// @brief GPU adapter nodes on which this buffer is allocated.
    GPUMask gpuMask{ GPUMask::all() };

    /// @brief Constructs an empty zero-sized buffer descriptor.
    constexpr BufferDesc() noexcept = default;

    /// @brief Constructs a buffer descriptor with explicit size, stride, and usage flags.
    /// @param[in] inSize Total allocation size in bytes.
    /// @param[in] inStride Element stride in bytes.
    /// @param[in] inUsage Usage and binding flags.
    constexpr BufferDesc(const gp::UInt32 inSize, const gp::UInt32 inStride, const BufferUsageFlags inUsage) noexcept
        : size(inSize)
        , stride(inStride)
        , usage(inUsage)
    {}

    /// @brief Constructs a buffer descriptor with explicit size, stride, usage flags, and GPU mask.
    /// @param[in] inSize Total allocation size in bytes.
    /// @param[in] inStride Element stride in bytes.
    /// @param[in] inUsage Usage and binding flags.
    /// @param[in] inGPUMask Multi-GPU node mask.
    constexpr BufferDesc(
        const gp::UInt32 inSize, const gp::UInt32 inStride, const BufferUsageFlags inUsage, const GPUMask inGPUMask
    ) noexcept
        : size(inSize)
        , stride(inStride)
        , usage(inUsage)
        , gpuMask(inGPUMask)
    {}

    /// @brief Returns a descriptor for a null placeholder buffer without backing GPU memory.
    /// @return `BufferDesc` with `size == 0`, `stride == 0`, and `BufferUsageFlags::NullResource`.
    [[nodiscard]] static constexpr BufferDesc null() noexcept
    {
        return BufferDesc(0u, 0u, BufferUsageFlags::NullResource);
    }

    /// @brief Reports whether this descriptor represents a null placeholder buffer.
    /// @details Asserts that `NullResource` is never combined with non-zero size, non-zero stride, or other usage
    /// flags.
    /// @return True when `BufferUsageFlags::NullResource` is set; false otherwise.
    [[nodiscard]] constexpr bool isNull() const noexcept
    {
        if ((usage & BufferUsageFlags::NullResource) != BufferUsageFlags::None)
        {
            GP_CHECK(size == 0u && stride == 0u && usage == BufferUsageFlags::NullResource);
            return true;
        }
        return false;
    }

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Descriptor to compare against.
    /// @return True when size, stride, usage, and GPU mask all match.
    [[nodiscard]] constexpr bool operator==(const BufferDesc& rhs) const noexcept = default;

    /// @brief Computes a deterministic 64-bit hash over size, stride, usage, and GPU mask.
    /// @return 64-bit hash digest.
    [[nodiscard]] constexpr gp::UInt64 getHash() const noexcept
    {
        gp::UInt64 hash = gp::hashing::hashOf(size);
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(stride));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(usage));
        hash = gp::hashing::hashCombine(hash, gpuMask.getHash());
        return hash;
    }

    /// @brief Streams all descriptor fields into a streaming hasher.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target hasher accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.appendAll(size, stride, usage, gpuMask.getNative());
    }
};

/// @brief Derives the default initial GPU resource access state for a buffer from its usage flags.
/// @param[in] usage Buffer usage flags.
/// @param[in] hasInitialData True if initial bytes will be uploaded at creation time.
/// @return Resolved `sync::ResourceAccess` state suitable for initial barrier tracking.
[[nodiscard]] GP_RHI_API sync::ResourceAccess
    getDefaultResourceState(BufferUsageFlags usage, bool hasInitialData) noexcept;

}   // namespace gp::rhi::resource
