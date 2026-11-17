// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/roles/NamedThreadDispatcher.hpp"
#include "threading/queues/JobQueueEnums.hpp"
#include "threading/roles/ThreadRole.hpp"
#include "threading/sync/JobCounter.hpp"

namespace gp::thread
{

namespace detail
{

/// @brief Translates a queue initialization status into the dispatcher's result vocabulary.
/// @param[in] queueResult Status returned by `MPMCRingQueue::initialize()`.
/// @return The matching `ThreadRoleResult`.
[[nodiscard]] static constexpr ThreadRoleResult toThreadRoleResult(const JobQueueResult queueResult) noexcept
{
    switch (queueResult)
    {
    case JobQueueResult::Success:
        return ThreadRoleResult::Success;
    case JobQueueResult::OutOfMemory:
        return ThreadRoleResult::OutOfMemory;
    default:
        break;
    }
    return ThreadRoleResult::InvalidArguments;
}

}   // namespace detail

NamedThreadDispatcher::~NamedThreadDispatcher() noexcept
{
    shutdown();
}

ThreadRoleResult NamedThreadDispatcher::initialize(const NamedThreadDispatcherDesc& desc) noexcept
{
    if (isInitialized())
    {
        return ThreadRoleResult::AlreadyInitialized;
    }

    for (gp::USize index = 0u; index < kNamedThreadCount; ++index)
    {
        const gp::USize capacity = desc.queueCapacities[index];
        const ThreadRoleResult result =
            (capacity == 0u) ? ThreadRoleResult::InvalidArguments
                             : detail::toThreadRoleResult(m_queues[index].initialize(capacity, desc.allocator));
        if (result != ThreadRoleResult::Success) [[unlikely]]
        {
            for (gp::USize rollback = 0u; rollback < kNamedThreadCount; ++rollback)
            {
                m_queues[rollback].shutdown();
            }
            return result;
        }
    }

    m_isInitialized.store(1u, gp::MemoryOrder::Release);
    return ThreadRoleResult::Success;
}

void NamedThreadDispatcher::shutdown() noexcept
{
    if (!isInitialized())
    {
        return;
    }

    m_isInitialized.store(0u, gp::MemoryOrder::Release);
    for (MPMCRingQueue<NamedThreadTask>& queue: m_queues)
    {
        queue.shutdown();
    }
}

ThreadRoleResult NamedThreadDispatcher::dispatch(const NamedThread target, const NamedThreadTask& task) noexcept
{
    if (!isInitialized()) [[unlikely]]
    {
        return ThreadRoleResult::NotInitialized;
    }

    if (!isValidNamedThread(target) || !task.isValid()) [[unlikely]]
    {
        return ThreadRoleResult::InvalidArguments;
    }

    // Increment before the push. Once the task is visible the owner may run it and decrement right away, and the
    // counter must never dip below the true number of outstanding tasks.
    if (task.counter != nullptr)
    {
        (void)task.counter->increment();
    }

    if (m_queues[static_cast<gp::USize>(target)].tryPush(task) != JobQueueResult::Success) [[unlikely]]
    {
        if (task.counter != nullptr)
        {
            (void)task.counter->decrement();
        }
        return ThreadRoleResult::QueueFull;
    }

    return ThreadRoleResult::Success;
}

ThreadRoleResult NamedThreadDispatcher::pump(
    const NamedThread target, const gp::UInt32 maxTaskCount, gp::UInt32& outExecutedCount
) noexcept
{
    outExecutedCount = 0u;

    if (!isInitialized()) [[unlikely]]
    {
        return ThreadRoleResult::NotInitialized;
    }

    if (!isValidNamedThread(target)) [[unlikely]]
    {
        return ThreadRoleResult::InvalidArguments;
    }

    // Only the role owner drains its queue. This is what keeps per-thread execution order deterministic even though
    // the underlying ring would accept any number of consumers.
    if (!ThreadRoleRegistry::isInRole(toThreadRole(target))) [[unlikely]]
    {
        return ThreadRoleResult::WrongThread;
    }

    MPMCRingQueue<NamedThreadTask>& queue = m_queues[static_cast<gp::USize>(target)];
    NamedThreadTask task{};
    while (outExecutedCount < maxTaskCount)
    {
        if (queue.tryPop(task) != JobQueueResult::Success)
        {
            break;
        }

        task.entry(task.userData);
        if (task.counter != nullptr)
        {
            (void)task.counter->decrement();
        }
        ++outExecutedCount;
    }

    return ThreadRoleResult::Success;
}

gp::USize NamedThreadDispatcher::getPendingTaskCountApprox(const NamedThread target) const noexcept
{
    if (!isInitialized() || !isValidNamedThread(target))
    {
        return 0u;
    }
    return m_queues[static_cast<gp::USize>(target)].sizeApprox();
}

gp::USize NamedThreadDispatcher::getQueueCapacity(const NamedThread target) const noexcept
{
    if (!isInitialized() || !isValidNamedThread(target))
    {
        return 0u;
    }
    return m_queues[static_cast<gp::USize>(target)].capacity();
}

}   // namespace gp::thread
