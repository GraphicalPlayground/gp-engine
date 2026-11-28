// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "sync/VulkanSyncPoint.hpp"
#include "memory/MemoryOrder.hpp"
#include "memory/pointers/SharedPtr.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::rhi::vulkan
{

namespace
{

gp::thread::Atomic<VulkanSyncPointWaitCallback> g_syncPointWaitCallback{ nullptr };

}   // namespace

VulkanSyncPointRef VulkanSyncPoint::create(const VulkanSyncPointType type, const char* const debugName) noexcept
{
    return gp::makeShared<VulkanSyncPoint>(type, debugName);
}

void VulkanSyncPoint::wait() const noexcept
{
    if (isComplete())
    {
        return;
    }

    const VulkanSyncPointWaitCallback waitCallback = g_syncPointWaitCallback.load(gp::memory::MemoryOrder::Acquire);
    if (waitCallback != nullptr)
    {
        waitCallback(*this);
    }

    if (!isComplete())
    {
        static_cast<void>(m_cpuEvent.wait());
    }
}

void VulkanSyncPoint::setWaitCallback(const VulkanSyncPointWaitCallback callback) noexcept
{
    g_syncPointWaitCallback.store(callback, gp::memory::MemoryOrder::Release);
}

VulkanSyncPointWaitCallback VulkanSyncPoint::getWaitCallback() noexcept
{
    return g_syncPointWaitCallback.load(gp::memory::MemoryOrder::Acquire);
}

}   // namespace gp::rhi::vulkan
