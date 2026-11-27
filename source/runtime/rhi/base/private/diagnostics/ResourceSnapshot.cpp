// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "diagnostics/ResourceSnapshot.hpp"
#include "containers/arrays/Vector.hpp"
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "core/Resource.hpp"
#include "core/ResourceEnums.hpp"
#include "core/ResourceTypes.hpp"
#include "core/RHIStrings.hpp"
#include "CoreMinimal.hpp"
#include "device/DynamicRHI.hpp"
#include "device/RHIGlobals.hpp"
#include "memory/Memory.hpp"
#include "platforms/base/system/FileSystem.hpp"
#include "platforms/base/system/Time.hpp"
#include "profiling/RHICoreStats.hpp"
#include "resources/Buffer.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/TextureUtils.hpp"
#include "resources/UniformBuffer.hpp"
#include <cstdio>

namespace gp::rhi::diagnostics
{

namespace
{

constexpr gp::UInt32 kMaxSnapshotStringBytes = 16u * 1024u;
constexpr gp::UInt32 kMaxSnapshotResourceCount = 4u * 1024u * 1024u;

enum PackedResourceStatFlags : gp::UInt32
{
    kStatFlagRenderTarget = 1u << 0,
    kStatFlagDepthStencil = 1u << 1,
    kStatFlagUnorderedAccess = 1u << 2,
    kStatFlagRayTracingAS = 1u << 3,
    kStatFlagResident = 1u << 4,
    kStatFlagMarkedForDelete = 1u << 5,
    kStatFlagTransient = 1u << 6,
    kStatFlagStreaming = 1u << 7,
    kStatFlagCommitted = 1u << 8
};

void writeUInt32(gp::Vector<gp::Byte>& out, const gp::UInt32 value) noexcept
{
    const gp::USize offset = out.size();
    out.resize(offset + sizeof(gp::UInt32));
    gp::memory::copyMemory(out.data() + offset, &value, sizeof(gp::UInt32));
}

void writeUInt64(gp::Vector<gp::Byte>& out, const gp::UInt64 value) noexcept
{
    const gp::USize offset = out.size();
    out.resize(offset + sizeof(gp::UInt64));
    gp::memory::copyMemory(out.data() + offset, &value, sizeof(gp::UInt64));
}

void writeInt64(gp::Vector<gp::Byte>& out, const gp::Int64 value) noexcept
{
    const gp::USize offset = out.size();
    out.resize(offset + sizeof(gp::Int64));
    gp::memory::copyMemory(out.data() + offset, &value, sizeof(gp::Int64));
}

void writeString(gp::Vector<gp::Byte>& out, const gp::StringView str) noexcept
{
    const gp::UInt32 len =
        (str.size() <= kMaxSnapshotStringBytes) ? static_cast<gp::UInt32>(str.size()) : kMaxSnapshotStringBytes;
    writeUInt32(out, len);
    if (len > 0u)
    {
        const gp::USize offset = out.size();
        out.resize(offset + len);
        gp::memory::copyMemory(out.data() + offset, str.data(), len);
    }
}

[[nodiscard]] bool readUInt32(const gp::Vector<gp::Byte>& in, gp::USize& cursor, gp::UInt32& outValue) noexcept
{
    if (cursor + sizeof(gp::UInt32) > in.size())
    {
        return false;
    }
    gp::memory::copyMemory(&outValue, in.data() + cursor, sizeof(gp::UInt32));
    cursor += sizeof(gp::UInt32);
    return true;
}

[[nodiscard]] bool readUInt64(const gp::Vector<gp::Byte>& in, gp::USize& cursor, gp::UInt64& outValue) noexcept
{
    if (cursor + sizeof(gp::UInt64) > in.size())
    {
        return false;
    }
    gp::memory::copyMemory(&outValue, in.data() + cursor, sizeof(gp::UInt64));
    cursor += sizeof(gp::UInt64);
    return true;
}

[[nodiscard]] bool readInt64(const gp::Vector<gp::Byte>& in, gp::USize& cursor, gp::Int64& outValue) noexcept
{
    if (cursor + sizeof(gp::Int64) > in.size())
    {
        return false;
    }
    gp::memory::copyMemory(&outValue, in.data() + cursor, sizeof(gp::Int64));
    cursor += sizeof(gp::Int64);
    return true;
}

[[nodiscard]] bool readString(const gp::Vector<gp::Byte>& in, gp::USize& cursor, gp::String& outValue) noexcept
{
    gp::UInt32 len = 0u;
    if (!readUInt32(in, cursor, len) || len > kMaxSnapshotStringBytes || (cursor + len) > in.size())
    {
        return false;
    }
    outValue.clear();
    if (len > 0u)
    {
        outValue.append(reinterpret_cast<const char*>(in.data() + cursor), static_cast<gp::USize>(len));
        cursor += static_cast<gp::USize>(len);
    }
    return true;
}

}   // namespace

void getTrackedResourceStats(gp::Vector<ResourceStats>& outStats) noexcept
{
    outStats.clear();
#if GP_RHI_ENABLE_RESOURCE_INFO
    gp::Vector<resource::Resource*> tracked{};
    resource::Resource::gatherTrackedResources(tracked);
    outStats.reserve(tracked.size());

    DynamicRHI* const rhi = getDynamicRHI();

    for (resource::Resource* const res: tracked)
    {
        if (res == nullptr)
        {
            continue;
        }

        ResourceStats stat{};
        const resource::ResourceType resType = res->getType();
        stat.type = gp::String(stringFromRHIResourceType(resType));
        stat.pixelFormat = gp::String("Unknown");
        stat.ownerName = res->getOwnerName().toString();
        stat.isMarkedForDelete = res->isMarkedForDelete();
        stat.isCommitted = res->isCommitted();

        resource::ResourceInfo info{};
        if (res->getResourceInfo(info) && info.isValid)
        {
            stat.isResident = info.isResident;
            stat.isTransient = info.isTransient;
            if (!info.name.isNone())
            {
                stat.name = info.name.toString();
            }
            if (info.vramAllocation.isValid())
            {
                stat.sizeInBytes = info.vramAllocation.allocationSize;
            }
        }

        if (resType == resource::ResourceType::Texture)
        {
            resource::Texture* const tex = static_cast<resource::Texture*>(res);
            const resource::TextureDesc& desc = tex->getDesc();
            if (stat.name.isEmpty())
            {
                stat.name = tex->getName().toString();
            }
            const char* const dimStr = getTextureDimensionString(desc.dimension);
            if (dimStr != nullptr && dimStr[0] != '\0')
            {
                stat.type = gp::String(dimStr);
            }
            stat.pixelFormat = gp::String(getPixelFormatString(desc.format));
            if (stat.sizeInBytes == 0u)
            {
                stat.sizeInBytes =
                    (rhi != nullptr) ? rhi->computeMemorySize(tex) : resource::calculateTextureSize(desc);
            }
            stat.hasRenderTargetFlag =
                (desc.flags & resource::TextureCreateFlags::RenderTargetable) != resource::TextureCreateFlags::None;
            stat.hasDepthStencilFlag = (desc.flags & resource::TextureCreateFlags::DepthStencilTargetable) !=
                                       resource::TextureCreateFlags::None;
            stat.hasUnorderedAccessFlag =
                (desc.flags & resource::TextureCreateFlags::UAV) != resource::TextureCreateFlags::None;
            stat.isStreaming =
                (desc.flags & resource::TextureCreateFlags::Streamable) != resource::TextureCreateFlags::None;
            if ((desc.flags & resource::TextureCreateFlags::Memoryless) != resource::TextureCreateFlags::None)
            {
                stat.isTransient = true;
            }
        }
        else if (resType == resource::ResourceType::Buffer)
        {
            const resource::Buffer* const buf = static_cast<const resource::Buffer*>(res);
            const resource::BufferDesc& desc = buf->getDesc();
            if (stat.name.isEmpty())
            {
                stat.name = buf->getName().toString();
            }
            if (stat.sizeInBytes == 0u)
            {
                stat.sizeInBytes = static_cast<gp::UInt64>(desc.size);
            }
            stat.hasUnorderedAccessFlag =
                (desc.usage & resource::BufferUsageFlags::UnorderedAccess) != resource::BufferUsageFlags::None;
            stat.hasRayTracingAccelerationStructure =
                (desc.usage & resource::BufferUsageFlags::AccelerationStructure) != resource::BufferUsageFlags::None;
            stat.isTransient = (desc.usage & resource::BufferUsageFlags::Transient) != resource::BufferUsageFlags::None;
        }
        else if (resType == resource::ResourceType::UniformBuffer)
        {
            const resource::UniformBuffer* const ub = static_cast<const resource::UniformBuffer*>(res);
            if (stat.name.isEmpty())
            {
                stat.name = gp::String(ub->getLayout().getDebugName());
            }
            if (stat.sizeInBytes == 0u)
            {
                stat.sizeInBytes = static_cast<gp::UInt64>(ub->getSize());
            }
        }
        else if (
            resType == resource::ResourceType::RayTracingGeometry || resType == resource::ResourceType::RayTracingScene
        )
        {
            stat.hasRayTracingAccelerationStructure = true;
        }

        if (stat.name.isEmpty())
        {
            stat.name = gp::String("None");
        }

        outStats.emplaceBack(static_cast<ResourceStats&&>(stat));
    }
#endif
}

ResourceSnapshot captureResourceSnapshot() noexcept
{
    ResourceSnapshot snapshot{};
    const RHIGlobals& globals = getRHIGlobals();
    DynamicRHI* const rhi = getDynamicRHI();

    snapshot.metadata.rhiName = gp::String((rhi != nullptr) ? rhi->getName() : "None");
    snapshot.metadata.adapterName = globals.gpuInfo.adapterName;
    snapshot.metadata.driverVersion = globals.gpuInfo.adapterUserDriverVersion;
    snapshot.metadata.featureLevel = featureLevelToString(globals.maxSupportedFeatureLevel);
    snapshot.metadata.timestampUtcMicroseconds = gp::platform::Time::utcTimestampMicroseconds();

    if (rhi != nullptr)
    {
        rhi->getTextureMemoryStats(snapshot.metadata.textureMemoryStats);
    }
    if (!snapshot.metadata.textureMemoryStats.areHardwareStatsValid())
    {
        profiling::fillBaselineTextureMemoryStats(snapshot.metadata.textureMemoryStats);
    }

    if (snapshot.metadata.textureMemoryStats.dedicatedVideoMemory > 0)
    {
        snapshot.metadata.dedicatedVideoMemory =
            static_cast<gp::UInt64>(snapshot.metadata.textureMemoryStats.dedicatedVideoMemory);
    }
    if (snapshot.metadata.textureMemoryStats.sharedSystemMemory > 0)
    {
        snapshot.metadata.sharedSystemMemory =
            static_cast<gp::UInt64>(snapshot.metadata.textureMemoryStats.sharedSystemMemory);
    }

    getTrackedResourceStats(snapshot.resources);

    gp::UInt64 totalBytes = 0u;
    for (const ResourceStats& entry: snapshot.resources)
    {
        totalBytes += entry.sizeInBytes;
    }
    snapshot.metadata.totalResourceSize = totalBytes;
    return snapshot;
}

bool ResourceSnapshot::saveToFile(const gp::StringView filePath) noexcept
{
    if (filePath.isEmpty())
    {
        return false;
    }

    if (metadata.totalResourceSize == 0u && !resources.isEmpty())
    {
        gp::UInt64 computedTotal = 0u;
        for (const ResourceStats& stat: resources)
        {
            computedTotal += stat.sizeInBytes;
        }
        metadata.totalResourceSize = computedTotal;
    }

    gp::Vector<gp::Byte> payload{};
    payload.reserve(256u + resources.size() * 96u);

    writeUInt32(payload, kMagic);
    writeUInt32(payload, kCurrentVersion);

    writeString(payload, metadata.rhiName.view());
    writeString(payload, metadata.adapterName.view());
    writeString(payload, metadata.driverVersion.view());
    writeString(payload, metadata.featureLevel.view());
    writeUInt64(payload, metadata.totalPhysicalMemory);
    writeUInt64(payload, metadata.dedicatedVideoMemory);
    writeUInt64(payload, metadata.sharedSystemMemory);
    writeUInt64(payload, metadata.totalResourceSize);
    writeUInt64(payload, metadata.timestampUtcMicroseconds);

    writeInt64(payload, metadata.textureMemoryStats.dedicatedVideoMemory);
    writeInt64(payload, metadata.textureMemoryStats.dedicatedSystemMemory);
    writeInt64(payload, metadata.textureMemoryStats.sharedSystemMemory);
    writeInt64(payload, metadata.textureMemoryStats.totalGraphicsMemory);
    writeInt64(payload, metadata.textureMemoryStats.streamingMemorySize);
    writeInt64(payload, metadata.textureMemoryStats.nonStreamingMemorySize);
    writeInt64(payload, metadata.textureMemoryStats.texturePoolSize);

    const gp::UInt32 count = (resources.size() <= kMaxSnapshotResourceCount) ? static_cast<gp::UInt32>(resources.size())
                                                                             : kMaxSnapshotResourceCount;
    writeUInt32(payload, count);

    for (gp::UInt32 idx = 0u; idx < count; ++idx)
    {
        const ResourceStats& stat = resources[idx];
        writeString(payload, stat.name.view());
        writeString(payload, stat.ownerName.view());
        writeString(payload, stat.type.view());
        writeString(payload, stat.pixelFormat.view());
        writeUInt64(payload, stat.sizeInBytes);

        gp::UInt32 packedFlags = 0u;
        if (stat.hasRenderTargetFlag)
        {
            packedFlags |= kStatFlagRenderTarget;
        }
        if (stat.hasDepthStencilFlag)
        {
            packedFlags |= kStatFlagDepthStencil;
        }
        if (stat.hasUnorderedAccessFlag)
        {
            packedFlags |= kStatFlagUnorderedAccess;
        }
        if (stat.hasRayTracingAccelerationStructure)
        {
            packedFlags |= kStatFlagRayTracingAS;
        }
        if (stat.isResident)
        {
            packedFlags |= kStatFlagResident;
        }
        if (stat.isMarkedForDelete)
        {
            packedFlags |= kStatFlagMarkedForDelete;
        }
        if (stat.isTransient)
        {
            packedFlags |= kStatFlagTransient;
        }
        if (stat.isStreaming)
        {
            packedFlags |= kStatFlagStreaming;
        }
        if (stat.isCommitted)
        {
            packedFlags |= kStatFlagCommitted;
        }
        writeUInt32(payload, packedFlags);
    }

    const gp::Expected<bool, gp::platform::FileError> writeRes =
        gp::platform::FileSystem::writeAtomic(filePath, payload.data(), payload.size());
    return writeRes.hasValue() && writeRes.value();
}

bool ResourceSnapshot::loadFromFile(const gp::StringView filePath, ResourceSnapshot& outSnapshot) noexcept
{
    outSnapshot = ResourceSnapshot{};
    if (filePath.isEmpty())
    {
        return false;
    }

    gp::Vector<gp::Byte> bytes{};
    const gp::Expected<gp::USize, gp::platform::FileError> readRes =
        gp::platform::FileSystem::readAllBytes(filePath, bytes);
    if (!readRes.hasValue() || bytes.size() < 8u)
    {
        return false;
    }

    gp::USize cursor = 0u;
    gp::UInt32 magic = 0u;
    gp::UInt32 version = 0u;
    if (!readUInt32(bytes, cursor, magic) || !readUInt32(bytes, cursor, version))
    {
        return false;
    }

    if (magic != kMagic || version != kCurrentVersion)
    {
        return false;
    }

    ResourceSnapshot parsed{};
    if (!readString(bytes, cursor, parsed.metadata.rhiName) ||
        !readString(bytes, cursor, parsed.metadata.adapterName) ||
        !readString(bytes, cursor, parsed.metadata.driverVersion) ||
        !readString(bytes, cursor, parsed.metadata.featureLevel) ||
        !readUInt64(bytes, cursor, parsed.metadata.totalPhysicalMemory) ||
        !readUInt64(bytes, cursor, parsed.metadata.dedicatedVideoMemory) ||
        !readUInt64(bytes, cursor, parsed.metadata.sharedSystemMemory) ||
        !readUInt64(bytes, cursor, parsed.metadata.totalResourceSize) ||
        !readUInt64(bytes, cursor, parsed.metadata.timestampUtcMicroseconds) ||
        !readInt64(bytes, cursor, parsed.metadata.textureMemoryStats.dedicatedVideoMemory) ||
        !readInt64(bytes, cursor, parsed.metadata.textureMemoryStats.dedicatedSystemMemory) ||
        !readInt64(bytes, cursor, parsed.metadata.textureMemoryStats.sharedSystemMemory) ||
        !readInt64(bytes, cursor, parsed.metadata.textureMemoryStats.totalGraphicsMemory) ||
        !readInt64(bytes, cursor, parsed.metadata.textureMemoryStats.streamingMemorySize) ||
        !readInt64(bytes, cursor, parsed.metadata.textureMemoryStats.nonStreamingMemorySize) ||
        !readInt64(bytes, cursor, parsed.metadata.textureMemoryStats.texturePoolSize))
    {
        return false;
    }

    gp::UInt32 count = 0u;
    if (!readUInt32(bytes, cursor, count) || count > kMaxSnapshotResourceCount)
    {
        return false;
    }

    parsed.resources.reserve(count);
    for (gp::UInt32 idx = 0u; idx < count; ++idx)
    {
        ResourceStats stat{};
        gp::UInt32 packedFlags = 0u;
        if (!readString(bytes, cursor, stat.name) || !readString(bytes, cursor, stat.ownerName) ||
            !readString(bytes, cursor, stat.type) || !readString(bytes, cursor, stat.pixelFormat) ||
            !readUInt64(bytes, cursor, stat.sizeInBytes) || !readUInt32(bytes, cursor, packedFlags))
        {
            return false;
        }

        stat.hasRenderTargetFlag = (packedFlags & kStatFlagRenderTarget) != 0u;
        stat.hasDepthStencilFlag = (packedFlags & kStatFlagDepthStencil) != 0u;
        stat.hasUnorderedAccessFlag = (packedFlags & kStatFlagUnorderedAccess) != 0u;
        stat.hasRayTracingAccelerationStructure = (packedFlags & kStatFlagRayTracingAS) != 0u;
        stat.isResident = (packedFlags & kStatFlagResident) != 0u;
        stat.isMarkedForDelete = (packedFlags & kStatFlagMarkedForDelete) != 0u;
        stat.isTransient = (packedFlags & kStatFlagTransient) != 0u;
        stat.isStreaming = (packedFlags & kStatFlagStreaming) != 0u;
        stat.isCommitted = (packedFlags & kStatFlagCommitted) != 0u;

        parsed.resources.emplaceBack(static_cast<ResourceStats&&>(stat));
    }

    if (cursor != bytes.size())
    {
        return false;
    }

    outSnapshot = static_cast<ResourceSnapshot&&>(parsed);
    return true;
}

bool ResourceSnapshot::writeTrackedResourcesToFile(const gp::StringView filePath) noexcept
{
    ResourceSnapshot snapshot = captureResourceSnapshot();
    return snapshot.saveToFile(filePath);
}

gp::String dumpResourceMemorySummary(const gp::Int32 maxResourcesToList) noexcept
{
    gp::Vector<ResourceStats> stats{};
    getTrackedResourceStats(stats);

    // Insertion-sort descending by sizeInBytes (avoiding STL algorithms/allocations).
    for (gp::USize i = 1u; i < stats.size(); ++i)
    {
        for (gp::USize j = i; j > 0u && stats[j].sizeInBytes > stats[j - 1u].sizeInBytes; --j)
        {
            ResourceStats temp = static_cast<ResourceStats&&>(stats[j]);
            stats[j] = static_cast<ResourceStats&&>(stats[j - 1u]);
            stats[j - 1u] = static_cast<ResourceStats&&>(temp);
        }
    }

    gp::UInt64 totalBytes = 0u;
    for (const ResourceStats& stat: stats)
    {
        totalBytes += stat.sizeInBytes;
    }

    char buf[512]{};
    gp::String output{};
    {
        const double totalMiB = static_cast<double>(totalBytes) / (1024.0 * 1024.0);
        const int len = std::snprintf(
            buf,
            sizeof(buf),
            "RHI Tracked Resource Summary: %zu resource(s), %.3f MiB (%llu bytes)\n",
            stats.size(),
            totalMiB,
            static_cast<unsigned long long>(totalBytes)
        );
        if (len > 0)
        {
            output.append(buf, static_cast<gp::USize>(len));
        }
    }

    if (maxResourcesToList > 0)
    {
        const gp::USize limit = (static_cast<gp::USize>(maxResourcesToList) < stats.size())
                                    ? static_cast<gp::USize>(maxResourcesToList)
                                    : stats.size();
        for (gp::USize idx = 0u; idx < limit; ++idx)
        {
            const ResourceStats& stat = stats[idx];
            const int len = std::snprintf(
                buf,
                sizeof(buf),
                "  [%zu] %s (%s | %s) - %llu bytes [Owner=%s%s%s]\n",
                idx,
                stat.name.cString(),
                stat.type.cString(),
                stat.pixelFormat.cString(),
                static_cast<unsigned long long>(stat.sizeInBytes),
                stat.ownerName.isEmpty() ? "None" : stat.ownerName.cString(),
                stat.isTransient ? ", Transient" : "",
                stat.isStreaming ? ", Streaming" : ""
            );
            if (len > 0)
            {
                output.append(buf, static_cast<gp::USize>(len));
            }
        }
    }

    return output;
}

}   // namespace gp::rhi::diagnostics
