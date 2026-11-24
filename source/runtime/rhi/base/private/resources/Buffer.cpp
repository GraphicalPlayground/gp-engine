// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/Buffer.hpp"
#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/BufferDesc.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/StagingBuffer.hpp"
#include "resources/StreamSourceSlot.hpp"
#include "sync/SyncEnums.hpp"

namespace gp::rhi::resource
{

sync::ResourceAccess getDefaultResourceState(const BufferUsageFlags usage, const bool hasInitialData) noexcept
{
    sync::ResourceAccess defaultReadingState = sync::ResourceAccess::Unknown;

    if ((usage & BufferUsageFlags::IndexBuffer) != BufferUsageFlags::None)
    {
        defaultReadingState = sync::ResourceAccess::VertexOrIndexBuffer;
    }

    if ((usage & BufferUsageFlags::VertexBuffer) != BufferUsageFlags::None)
    {
        defaultReadingState |= sync::ResourceAccess::VertexOrIndexBuffer | sync::ResourceAccess::SRVMask;
    }

    if ((usage & BufferUsageFlags::StructuredBuffer) != BufferUsageFlags::None)
    {
        defaultReadingState |= sync::ResourceAccess::SRVMask;
    }

    if ((usage & BufferUsageFlags::AccelerationStructure) != BufferUsageFlags::None)
    {
        defaultReadingState |= sync::ResourceAccess::BVHRead;
    }

    sync::ResourceAccess resourceState =
        ((defaultReadingState & sync::ResourceAccess::VertexOrIndexBuffer) == sync::ResourceAccess::None)
            ? sync::ResourceAccess::Unknown
            : defaultReadingState;

    if (hasInitialData)
    {
        resourceState = defaultReadingState;
    }
    else
    {
        if ((usage & BufferUsageFlags::UnorderedAccess) != BufferUsageFlags::None)
        {
            resourceState = sync::ResourceAccess::UAVMask;
        }
        else if ((usage & BufferUsageFlags::AccelerationStructure) != BufferUsageFlags::None)
        {
            resourceState = sync::ResourceAccess::BVHWrite;
        }
        else if ((usage & BufferUsageFlags::ShaderResource) != BufferUsageFlags::None)
        {
            resourceState = defaultReadingState | sync::ResourceAccess::SRVMask;
        }
    }

    GP_CHECKF(
        resourceState != sync::ResourceAccess::Unknown,
        "Unable to derive a valid default ResourceAccess state for the given BufferUsageFlags."
    );
    return resourceState;
}

Buffer::Buffer(const BufferCreateDesc& createDesc) noexcept
    : ViewableResource(ResourceType::Buffer, createDesc.initialState, createDesc.debugName, createDesc.ownerName)
    , m_desc(createDesc)
{}

Buffer::~Buffer() noexcept = default;

void Buffer::takeOwnership(Buffer& other) noexcept
{
    ViewableResource::takeOwnership(other);
    m_desc = other.m_desc;
}

void Buffer::releaseOwnership() noexcept
{
    ViewableResource::releaseOwnership();
    m_desc = BufferDesc::null();
}

StagingBuffer::StagingBuffer() noexcept
    : Resource(ResourceType::StagingBuffer)
    , m_isLocked(false)
{}

StagingBuffer::~StagingBuffer() noexcept = default;

gp::UInt64 StagingBuffer::getGPUSizeBytes() const noexcept
{
    return 0u;
}

StreamSourceSlotRef StreamSourceSlot::create(Buffer* const buffer) noexcept
{
    return StreamSourceSlotRef(new StreamSourceSlot(buffer));
}

StreamSourceSlot::StreamSourceSlot(Buffer* const buffer) noexcept
    : Resource(ResourceType::StreamSourceSlot)
    , m_buffer(buffer)
    , m_offset(0u)
{}

StreamSourceSlot::~StreamSourceSlot() noexcept = default;

void StreamSourceSlot::update(Buffer* const buffer, const gp::UInt32 offset) noexcept
{
    m_buffer = BufferRef(buffer);
    m_offset = offset;
}

}   // namespace gp::rhi::resource
