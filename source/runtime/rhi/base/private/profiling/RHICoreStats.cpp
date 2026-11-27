// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "profiling/RHICoreStats.hpp"
#include "CoreMinimal.hpp"
#include "memory/MemoryOrder.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/TextureEnums.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::rhi::profiling
{

namespace
{

struct AtomicResourceCounters
{
    gp::thread::Atomic<gp::Int64> renderTarget2DBytes{ 0 };
    gp::thread::Atomic<gp::Int64> renderTarget3DBytes{ 0 };
    gp::thread::Atomic<gp::Int64> renderTargetCubeBytes{ 0 };
    gp::thread::Atomic<gp::Int64> uavTextureBytes{ 0 };
    gp::thread::Atomic<gp::Int64> texture2DBytes{ 0 };
    gp::thread::Atomic<gp::Int64> texture3DBytes{ 0 };
    gp::thread::Atomic<gp::Int64> textureCubeBytes{ 0 };
    gp::thread::Atomic<gp::Int64> streamingTextureBytes{ 0 };
    gp::thread::Atomic<gp::Int64> nonStreamingTextureBytes{ 0 };

    gp::thread::Atomic<gp::Int64> vertexBufferBytes{ 0 };
    gp::thread::Atomic<gp::Int64> indexBufferBytes{ 0 };
    gp::thread::Atomic<gp::Int64> structuredBufferBytes{ 0 };
    gp::thread::Atomic<gp::Int64> byteAddressBufferBytes{ 0 };
    gp::thread::Atomic<gp::Int64> drawIndirectBufferBytes{ 0 };
    gp::thread::Atomic<gp::Int64> accelerationStructureBufferBytes{ 0 };
    gp::thread::Atomic<gp::Int64> miscBufferBytes{ 0 };
    gp::thread::Atomic<gp::Int64> uniformBufferBytes{ 0 };

    gp::thread::Atomic<gp::Int64> reservedUncommittedTextureBytes{ 0 };
    gp::thread::Atomic<gp::Int64> reservedCommittedTextureBytes{ 0 };
    gp::thread::Atomic<gp::Int64> reservedUncommittedBufferBytes{ 0 };
    gp::thread::Atomic<gp::Int64> reservedCommittedBufferBytes{ 0 };
};

AtomicResourceCounters& getCounters() noexcept
{
    static AtomicResourceCounters s_counters{};
    return s_counters;
}

}   // namespace

void updateGlobalTextureStats(
    const resource::TextureDesc& desc,
    const gp::UInt64 allocatedSizeInBytes,
    const bool isAllocating,
    const bool onlyStreamable
) noexcept
{
    if (allocatedSizeInBytes == 0u)
    {
        return;
    }

    const bool isStreamable =
        (desc.flags & resource::TextureCreateFlags::Streamable) != resource::TextureCreateFlags::None;
    if (onlyStreamable && !isStreamable)
    {
        return;
    }

    const gp::Int64 alignedBytes =
        static_cast<gp::Int64>((allocatedSizeInBytes + 1023u) & ~static_cast<gp::UInt64>(1023u));
    const gp::Int64 signedDelta = isAllocating ? alignedBytes : -alignedBytes;

    AtomicResourceCounters& counters = getCounters();

    const bool isRenderTarget =
        ((desc.flags &
          (resource::TextureCreateFlags::RenderTargetable | resource::TextureCreateFlags::ResolveTargetable |
           resource::TextureCreateFlags::DepthStencilTargetable |
           resource::TextureCreateFlags::DepthStencilResolveTarget)) != resource::TextureCreateFlags::None);
    const bool isUAV = (desc.flags & resource::TextureCreateFlags::UAV) != resource::TextureCreateFlags::None;
    const bool isReserved =
        (desc.flags & resource::TextureCreateFlags::ReservedResource) != resource::TextureCreateFlags::None;

    if (isReserved)
    {
        counters.reservedUncommittedTextureBytes.fetchAdd(signedDelta, gp::memory::MemoryOrder::Relaxed);
        return;
    }

    if (isRenderTarget)
    {
        if (desc.isTexture3D())
        {
            counters.renderTarget3DBytes.fetchAdd(signedDelta, gp::memory::MemoryOrder::Relaxed);
        }
        else if (desc.isTextureCube())
        {
            counters.renderTargetCubeBytes.fetchAdd(signedDelta, gp::memory::MemoryOrder::Relaxed);
        }
        else
        {
            counters.renderTarget2DBytes.fetchAdd(signedDelta, gp::memory::MemoryOrder::Relaxed);
        }
    }
    else if (isUAV)
    {
        counters.uavTextureBytes.fetchAdd(signedDelta, gp::memory::MemoryOrder::Relaxed);
    }
    else
    {
        if (desc.isTexture3D())
        {
            counters.texture3DBytes.fetchAdd(signedDelta, gp::memory::MemoryOrder::Relaxed);
        }
        else if (desc.isTextureCube())
        {
            counters.textureCubeBytes.fetchAdd(signedDelta, gp::memory::MemoryOrder::Relaxed);
        }
        else
        {
            counters.texture2DBytes.fetchAdd(signedDelta, gp::memory::MemoryOrder::Relaxed);
        }
    }

    if (isStreamable)
    {
        counters.streamingTextureBytes.fetchAdd(signedDelta, gp::memory::MemoryOrder::Relaxed);
    }
    else if (!isRenderTarget && !isUAV)
    {
        counters.nonStreamingTextureBytes.fetchAdd(signedDelta, gp::memory::MemoryOrder::Relaxed);
    }
}

void fillBaselineTextureMemoryStats(resource::TextureMemoryStats& outStats) noexcept
{
    const AtomicResourceCounters& counters = getCounters();
    outStats.streamingMemorySize = counters.streamingTextureBytes.load(gp::memory::MemoryOrder::Relaxed);
    outStats.nonStreamingMemorySize = counters.nonStreamingTextureBytes.load(gp::memory::MemoryOrder::Relaxed);
}

void updateGlobalBufferStats(
    const resource::BufferDesc& desc, const gp::Int64 bufferSizeInBytes, const bool isAllocating
) noexcept
{
    if (bufferSizeInBytes <= 0)
    {
        return;
    }

    const gp::Int64 signedDelta = isAllocating ? bufferSizeInBytes : -bufferSizeInBytes;
    AtomicResourceCounters& counters = getCounters();

    if ((desc.usage & resource::BufferUsageFlags::ReservedResource) != resource::BufferUsageFlags::None)
    {
        counters.reservedUncommittedBufferBytes.fetchAdd(signedDelta, gp::memory::MemoryOrder::Relaxed);
        return;
    }

    if ((desc.usage & resource::BufferUsageFlags::AccelerationStructure) != resource::BufferUsageFlags::None)
    {
        counters.accelerationStructureBufferBytes.fetchAdd(signedDelta, gp::memory::MemoryOrder::Relaxed);
    }
    else if ((desc.usage & resource::BufferUsageFlags::VertexBuffer) != resource::BufferUsageFlags::None)
    {
        counters.vertexBufferBytes.fetchAdd(signedDelta, gp::memory::MemoryOrder::Relaxed);
    }
    else if ((desc.usage & resource::BufferUsageFlags::IndexBuffer) != resource::BufferUsageFlags::None)
    {
        counters.indexBufferBytes.fetchAdd(signedDelta, gp::memory::MemoryOrder::Relaxed);
    }
    else if ((desc.usage & resource::BufferUsageFlags::StructuredBuffer) != resource::BufferUsageFlags::None)
    {
        counters.structuredBufferBytes.fetchAdd(signedDelta, gp::memory::MemoryOrder::Relaxed);
    }
    else if ((desc.usage & resource::BufferUsageFlags::ByteAddressBuffer) != resource::BufferUsageFlags::None)
    {
        counters.byteAddressBufferBytes.fetchAdd(signedDelta, gp::memory::MemoryOrder::Relaxed);
    }
    else if ((desc.usage & resource::BufferUsageFlags::DrawIndirect) != resource::BufferUsageFlags::None)
    {
        counters.drawIndirectBufferBytes.fetchAdd(signedDelta, gp::memory::MemoryOrder::Relaxed);
    }
    else
    {
        counters.miscBufferBytes.fetchAdd(signedDelta, gp::memory::MemoryOrder::Relaxed);
    }
}

void updateGlobalUniformBufferStats(const gp::Int64 bufferSizeInBytes, const bool isAllocating) noexcept
{
    if (bufferSizeInBytes <= 0)
    {
        return;
    }
    const gp::Int64 signedDelta = isAllocating ? bufferSizeInBytes : -bufferSizeInBytes;
    getCounters().uniformBufferBytes.fetchAdd(signedDelta, gp::memory::MemoryOrder::Relaxed);
}

void updateReservedResourceStatsOnCommit(
    const gp::Int64 commitDeltaBytes, const bool isBuffer, const bool isCommitting
) noexcept
{
    if (commitDeltaBytes <= 0)
    {
        return;
    }

    const gp::Int64 committedDelta = isCommitting ? commitDeltaBytes : -commitDeltaBytes;
    const gp::Int64 uncommittedDelta = -committedDelta;

    AtomicResourceCounters& counters = getCounters();
    if (isBuffer)
    {
        counters.reservedCommittedBufferBytes.fetchAdd(committedDelta, gp::memory::MemoryOrder::Relaxed);
        counters.reservedUncommittedBufferBytes.fetchAdd(uncommittedDelta, gp::memory::MemoryOrder::Relaxed);
    }
    else
    {
        counters.reservedCommittedTextureBytes.fetchAdd(committedDelta, gp::memory::MemoryOrder::Relaxed);
        counters.reservedUncommittedTextureBytes.fetchAdd(uncommittedDelta, gp::memory::MemoryOrder::Relaxed);
    }
}

RHIResourceMemoryCountersSnapshot getGlobalRHIResourceMemoryCounters() noexcept
{
    const AtomicResourceCounters& c = getCounters();
    RHIResourceMemoryCountersSnapshot snap{};
    snap.renderTarget2DBytes = c.renderTarget2DBytes.load(gp::memory::MemoryOrder::Relaxed);
    snap.renderTarget3DBytes = c.renderTarget3DBytes.load(gp::memory::MemoryOrder::Relaxed);
    snap.renderTargetCubeBytes = c.renderTargetCubeBytes.load(gp::memory::MemoryOrder::Relaxed);
    snap.uavTextureBytes = c.uavTextureBytes.load(gp::memory::MemoryOrder::Relaxed);
    snap.texture2DBytes = c.texture2DBytes.load(gp::memory::MemoryOrder::Relaxed);
    snap.texture3DBytes = c.texture3DBytes.load(gp::memory::MemoryOrder::Relaxed);
    snap.textureCubeBytes = c.textureCubeBytes.load(gp::memory::MemoryOrder::Relaxed);
    snap.streamingTextureBytes = c.streamingTextureBytes.load(gp::memory::MemoryOrder::Relaxed);
    snap.nonStreamingTextureBytes = c.nonStreamingTextureBytes.load(gp::memory::MemoryOrder::Relaxed);

    snap.vertexBufferBytes = c.vertexBufferBytes.load(gp::memory::MemoryOrder::Relaxed);
    snap.indexBufferBytes = c.indexBufferBytes.load(gp::memory::MemoryOrder::Relaxed);
    snap.structuredBufferBytes = c.structuredBufferBytes.load(gp::memory::MemoryOrder::Relaxed);
    snap.byteAddressBufferBytes = c.byteAddressBufferBytes.load(gp::memory::MemoryOrder::Relaxed);
    snap.drawIndirectBufferBytes = c.drawIndirectBufferBytes.load(gp::memory::MemoryOrder::Relaxed);
    snap.accelerationStructureBufferBytes = c.accelerationStructureBufferBytes.load(gp::memory::MemoryOrder::Relaxed);
    snap.miscBufferBytes = c.miscBufferBytes.load(gp::memory::MemoryOrder::Relaxed);
    snap.uniformBufferBytes = c.uniformBufferBytes.load(gp::memory::MemoryOrder::Relaxed);

    snap.reservedUncommittedTextureBytes = c.reservedUncommittedTextureBytes.load(gp::memory::MemoryOrder::Relaxed);
    snap.reservedCommittedTextureBytes = c.reservedCommittedTextureBytes.load(gp::memory::MemoryOrder::Relaxed);
    snap.reservedUncommittedBufferBytes = c.reservedUncommittedBufferBytes.load(gp::memory::MemoryOrder::Relaxed);
    snap.reservedCommittedBufferBytes = c.reservedCommittedBufferBytes.load(gp::memory::MemoryOrder::Relaxed);
    return snap;
}

void resetGlobalRHIResourceMemoryCounters() noexcept
{
    AtomicResourceCounters& c = getCounters();
    c.renderTarget2DBytes.store(0, gp::memory::MemoryOrder::Relaxed);
    c.renderTarget3DBytes.store(0, gp::memory::MemoryOrder::Relaxed);
    c.renderTargetCubeBytes.store(0, gp::memory::MemoryOrder::Relaxed);
    c.uavTextureBytes.store(0, gp::memory::MemoryOrder::Relaxed);
    c.texture2DBytes.store(0, gp::memory::MemoryOrder::Relaxed);
    c.texture3DBytes.store(0, gp::memory::MemoryOrder::Relaxed);
    c.textureCubeBytes.store(0, gp::memory::MemoryOrder::Relaxed);
    c.streamingTextureBytes.store(0, gp::memory::MemoryOrder::Relaxed);
    c.nonStreamingTextureBytes.store(0, gp::memory::MemoryOrder::Relaxed);

    c.vertexBufferBytes.store(0, gp::memory::MemoryOrder::Relaxed);
    c.indexBufferBytes.store(0, gp::memory::MemoryOrder::Relaxed);
    c.structuredBufferBytes.store(0, gp::memory::MemoryOrder::Relaxed);
    c.byteAddressBufferBytes.store(0, gp::memory::MemoryOrder::Relaxed);
    c.drawIndirectBufferBytes.store(0, gp::memory::MemoryOrder::Relaxed);
    c.accelerationStructureBufferBytes.store(0, gp::memory::MemoryOrder::Relaxed);
    c.miscBufferBytes.store(0, gp::memory::MemoryOrder::Relaxed);
    c.uniformBufferBytes.store(0, gp::memory::MemoryOrder::Relaxed);

    c.reservedUncommittedTextureBytes.store(0, gp::memory::MemoryOrder::Relaxed);
    c.reservedCommittedTextureBytes.store(0, gp::memory::MemoryOrder::Relaxed);
    c.reservedUncommittedBufferBytes.store(0, gp::memory::MemoryOrder::Relaxed);
    c.reservedCommittedBufferBytes.store(0, gp::memory::MemoryOrder::Relaxed);
}

}   // namespace gp::rhi::profiling
