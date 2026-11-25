// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "barriers/IVulkanBarrierCommandRecorder.hpp"
#include "barriers/IVulkanBarrierResourceResolver.hpp"
#include "barriers/VulkanBarrierEnums.hpp"
#include "barriers/VulkanBarrierTypes.hpp"
#include "commands/IComputeContext.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceDeletionQueue.hpp"
#include "CoreMinimal.hpp"
#include "resources/Buffer.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "sync/SyncEnums.hpp"   // IWYU pragma: keep
#include "sync/TransitionDesc.hpp"
#include "sync/VulkanSemaphore.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan::tests
{

// Minimal IComputeContext stub; all methods are no-ops. Used by VulkanBarrierContext as the tracking context.
class DummyComputeContext final : public command::IComputeContext
{
public:
    virtual void setComputePipelineState(pipeline::ComputePipelineState*) noexcept override
    {}

    virtual void dispatchComputeShader(gp::UInt32, gp::UInt32, gp::UInt32) noexcept override
    {}

    virtual void dispatchIndirectComputeShader(resource::Buffer*, gp::UInt32) noexcept override
    {}

    virtual void beginTransitions(gp::ConstVectorView<const sync::Transition*>) noexcept override
    {}

    virtual void endTransitions(gp::ConstVectorView<const sync::Transition*>) noexcept override
    {}

    virtual void clearUAVFloat(view::UnorderedAccessView*, const gp::math::Vector4<gp::Float32>&) noexcept override
    {}

    virtual void clearUAVUint(view::UnorderedAccessView*, const resource::UIntVector4&) noexcept override
    {}

    virtual void setShaderParameters(
        shader::ComputeShader*,
        gp::ConstVectorView<gp::UInt8>,
        gp::ConstVectorView<shader::ShaderParameter>,
        gp::ConstVectorView<shader::ShaderParameterResource>,
        gp::ConstVectorView<shader::ShaderParameterResource>
    ) noexcept override
    {}

    virtual void
        setShaderUnbinds(shader::ComputeShader*, gp::ConstVectorView<shader::ShaderParameterUnbind>) noexcept override
    {}

    virtual void setStaticUniformBuffers(const shader::UniformBufferStaticBindings&) noexcept override
    {}

    virtual void
        copyToStagingBuffer(resource::Buffer*, resource::StagingBuffer*, gp::UInt32, gp::UInt32) noexcept override
    {}

    virtual void writeGPUFence(sync::GPUFence*) noexcept override
    {}
};

// Minimal Buffer subclass; constructed from a descriptor for use in tracking and resolver mapping.
class DummyBuffer final : public resource::Buffer
{
public:
    explicit DummyBuffer(const resource::BufferCreateDesc& createDesc) noexcept
        : Buffer(createDesc)
    {}
};

// Minimal Texture subclass; constructed from a descriptor so the vtable is valid for getTextureReference().
class DummyTexture final : public resource::Texture
{
public:
    explicit DummyTexture(const resource::TextureCreateDesc& createDesc) noexcept
        : Texture(createDesc)
    {}
};

// RAII guard: bypasses the resource deletion queue check so DummyBuffer/DummyTexture can go out of scope freely.
struct ScopedBypassDeleteCheck final
{
    bool previousBypass{ false };

    ScopedBypassDeleteCheck() noexcept
        : previousBypass(resource::ResourceDeletionQueue::isBypassingDeletionQueueCheckOnExit())
    {
        resource::ResourceDeletionQueue::setBypassDeletionQueueCheckOnExit(true);
    }

    ~ScopedBypassDeleteCheck() noexcept
    {
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
        resource::ResourceDeletionQueue::setBypassDeletionQueueCheckOnExit(previousBypass);
    }
};

// Builds a color-texture VulkanBarrierResource around a fake VkImage handle.
[[nodiscard]] inline VulkanBarrierResource
    makeColorBarrierResource(const VkImage image, const bool supportsSampling = true) noexcept
{
    VulkanBarrierResource resource{};
    resource.type = VulkanBarrierResourceType::Texture;
    resource.image = image;
    resource.fullAspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    resource.isDepthOrStencilAspect = false;
    resource.supportsSampling = supportsSampling;
    resource.numMips = 1u;
    resource.numArrayLayers = 1u;
    return resource;
}

// Builds a depth-stencil-texture VulkanBarrierResource around a fake VkImage handle.
[[nodiscard]] inline VulkanBarrierResource makeDepthStencilBarrierResource(const VkImage image) noexcept
{
    VulkanBarrierResource resource{};
    resource.type = VulkanBarrierResourceType::Texture;
    resource.image = image;
    resource.fullAspectMask = static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    resource.isDepthOrStencilAspect = true;
    resource.supportsSampling = false;
    resource.numMips = 1u;
    resource.numArrayLayers = 1u;
    return resource;
}

// Builds a buffer VulkanBarrierResource around a fake VkBuffer handle.
[[nodiscard]] inline VulkanBarrierResource makeBufferBarrierResource(
    const VkBuffer buffer, const VkDeviceSize size = VK_WHOLE_SIZE, const VkBufferUsageFlags usageFlags = 0u
) noexcept
{
    VulkanBarrierResource resource{};
    resource.type = VulkanBarrierResourceType::Buffer;
    resource.buffer = buffer;
    resource.bufferOffset = 0u;
    resource.bufferSize = size;
    resource.bufferUsageFlags = usageFlags;
    return resource;
}

// IVulkanBarrierResourceResolver implementation that returns pre-registered VulkanBarrierResource values.
class FakeResolver final : public IVulkanBarrierResourceResolver
{
public:
    struct Entry
    {
        const void* resourcePtr{ nullptr };
        VulkanBarrierResource resolved{};
    };

    gp::Vector<Entry> m_entries{};

    void addMapping(const void* const resourcePtr, const VulkanBarrierResource& resource) noexcept
    {
        Entry entry{};
        entry.resourcePtr = resourcePtr;
        entry.resolved = resource;
        m_entries.pushBack(entry);
    }

    [[nodiscard]] virtual VulkanBarrierResource resolve(const sync::TransitionInfo& info) const noexcept override
    {
        for (const Entry& entry: m_entries)
        {
            if (entry.resourcePtr == static_cast<const void*>(info.resource))
            {
                return entry.resolved;
            }
        }
        return VulkanBarrierResource{};
    }
};

// Recorded data from a single pipelineBarrier() call.
struct RecordedPipelineBarrierCall
{
    VkPipelineStageFlags srcStageMask{ 0u };
    VkPipelineStageFlags dstStageMask{ 0u };
    VkDependencyFlags dependencyFlags{ 0u };
    gp::Vector<VkMemoryBarrier> memoryBarriers{};
    gp::Vector<VkBufferMemoryBarrier> bufferBarriers{};
    gp::Vector<VkImageMemoryBarrier> imageBarriers{};
};

// Recorded data from a single pipelineBarrier2() call.
struct RecordedBarrier2Call
{
    gp::Vector<VkMemoryBarrier2> memoryBarriers{};
    gp::Vector<VkBufferMemoryBarrier2> bufferBarriers{};
    gp::Vector<VkImageMemoryBarrier2> imageBarriers{};
};

// Recorded data from a single beginSplitBarrier() or endSplitBarrier() call.
struct RecordedSplitBarrierCall
{
    VkEvent event{ VK_NULL_HANDLE };
    bool isBegin{ false };
    gp::Vector<VkMemoryBarrier2> memoryBarriers{};
    gp::Vector<VkBufferMemoryBarrier2> bufferBarriers{};
    gp::Vector<VkImageMemoryBarrier2> imageBarriers{};
};

// Recorded data from a single addWaitSemaphore() call.
struct RecordedWaitSemaphore
{
    VkPipelineStageFlags waitStageMask{ 0u };
    VulkanSemaphoreRef semaphore{};
};

// IVulkanBarrierCommandRecorder implementation that captures every call for inspection.
class RecordingRecorder final : public IVulkanBarrierCommandRecorder
{
public:
    gp::Vector<RecordedPipelineBarrierCall> pipelineBarrierCalls{};
    gp::Vector<RecordedBarrier2Call> barrier2Calls{};
    gp::Vector<RecordedSplitBarrierCall> splitCalls{};
    gp::Vector<VkEvent> releasedEvents{};
    gp::Vector<VulkanSemaphoreRef> signalSemaphores{};
    gp::Vector<RecordedWaitSemaphore> waitSemaphores{};
    gp::Vector<VulkanCommitReservedResourceDesc> lastCommitArgs{};

    void reset() noexcept
    {
        pipelineBarrierCalls.clear();
        barrier2Calls.clear();
        splitCalls.clear();
        releasedEvents.clear();
        signalSemaphores.clear();
        waitSemaphores.clear();
        lastCommitArgs.clear();
    }

    virtual void pipelineBarrier(
        const VkPipelineStageFlags srcStageMask,
        const VkPipelineStageFlags dstStageMask,
        const VkDependencyFlags dependencyFlags,
        const gp::ConstVectorView<VkMemoryBarrier> memoryBarriers,
        const gp::ConstVectorView<VkBufferMemoryBarrier> bufferBarriers,
        const gp::ConstVectorView<VkImageMemoryBarrier> imageBarriers
    ) noexcept override
    {
        RecordedPipelineBarrierCall call{};
        call.srcStageMask = srcStageMask;
        call.dstStageMask = dstStageMask;
        call.dependencyFlags = dependencyFlags;

        for (gp::USize i = 0u; i < memoryBarriers.size(); ++i)
        {
            call.memoryBarriers.pushBack(memoryBarriers[i]);
        }
        for (gp::USize i = 0u; i < bufferBarriers.size(); ++i)
        {
            call.bufferBarriers.pushBack(bufferBarriers[i]);
        }
        for (gp::USize i = 0u; i < imageBarriers.size(); ++i)
        {
            call.imageBarriers.pushBack(imageBarriers[i]);
        }

        pipelineBarrierCalls.pushBack(call);
    }

    virtual void pipelineBarrier2(const VkDependencyInfo& dependencyInfo) noexcept override
    {
        RecordedBarrier2Call call{};

        for (gp::UInt32 i = 0u; i < dependencyInfo.memoryBarrierCount; ++i)
        {
            call.memoryBarriers.pushBack(dependencyInfo.pMemoryBarriers[i]);
        }
        for (gp::UInt32 i = 0u; i < dependencyInfo.bufferMemoryBarrierCount; ++i)
        {
            call.bufferBarriers.pushBack(dependencyInfo.pBufferMemoryBarriers[i]);
        }
        for (gp::UInt32 i = 0u; i < dependencyInfo.imageMemoryBarrierCount; ++i)
        {
            call.imageBarriers.pushBack(dependencyInfo.pImageMemoryBarriers[i]);
        }

        barrier2Calls.pushBack(call);
    }

    virtual void beginSplitBarrier(const VkEvent event, const VkDependencyInfo& dependencyInfo) noexcept override
    {
        RecordedSplitBarrierCall call{};
        call.event = event;
        call.isBegin = true;

        for (gp::UInt32 i = 0u; i < dependencyInfo.memoryBarrierCount; ++i)
        {
            call.memoryBarriers.pushBack(dependencyInfo.pMemoryBarriers[i]);
        }
        for (gp::UInt32 i = 0u; i < dependencyInfo.bufferMemoryBarrierCount; ++i)
        {
            call.bufferBarriers.pushBack(dependencyInfo.pBufferMemoryBarriers[i]);
        }
        for (gp::UInt32 i = 0u; i < dependencyInfo.imageMemoryBarrierCount; ++i)
        {
            call.imageBarriers.pushBack(dependencyInfo.pImageMemoryBarriers[i]);
        }

        splitCalls.pushBack(call);
    }

    virtual void endSplitBarrier(const VkEvent event, const VkDependencyInfo& dependencyInfo) noexcept override
    {
        RecordedSplitBarrierCall call{};
        call.event = event;
        call.isBegin = false;

        for (gp::UInt32 i = 0u; i < dependencyInfo.memoryBarrierCount; ++i)
        {
            call.memoryBarriers.pushBack(dependencyInfo.pMemoryBarriers[i]);
        }
        for (gp::UInt32 i = 0u; i < dependencyInfo.bufferMemoryBarrierCount; ++i)
        {
            call.bufferBarriers.pushBack(dependencyInfo.pBufferMemoryBarriers[i]);
        }
        for (gp::UInt32 i = 0u; i < dependencyInfo.imageMemoryBarrierCount; ++i)
        {
            call.imageBarriers.pushBack(dependencyInfo.pImageMemoryBarriers[i]);
        }

        splitCalls.pushBack(call);
    }

    virtual void releaseBarrierEvent(const VkEvent event) noexcept override
    {
        releasedEvents.pushBack(event);
    }

    virtual void addSignalSemaphore(const VulkanSemaphoreRef& semaphore) noexcept override
    {
        signalSemaphores.pushBack(semaphore);
    }

    virtual void addWaitSemaphore(
        const VkPipelineStageFlags waitStageMask, const VulkanSemaphoreRef& semaphore
    ) noexcept override
    {
        RecordedWaitSemaphore recorded{};
        recorded.waitStageMask = waitStageMask;
        recorded.semaphore = semaphore;
        waitSemaphores.pushBack(recorded);
    }

    virtual void addReservedResourceCommits(
        const gp::ConstVectorView<VulkanCommitReservedResourceDesc> commits
    ) noexcept override
    {
        for (gp::USize i = 0u; i < commits.size(); ++i)
        {
            lastCommitArgs.pushBack(commits[i]);
        }
    }
};

}   // namespace gp::rhi::vulkan::tests
