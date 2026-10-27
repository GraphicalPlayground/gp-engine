// File: source/runtime/core/private/platforms/apple/system/AppleThread.cpp
// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/apple/system/AppleThread.hpp"
#include <mach/mach.h>
#include <mach/thread_act.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#include <pthread/qos.h>
#include <sys/sysctl.h>

namespace gp::platform::apple
{

namespace detail
{

/// @brief Extracts a POSIX pthread_t from an opaque NativeHandle.
/// @param[in] handle Opaque pointer handle.
/// @return Restored POSIX pthread_t.
[[nodiscard]] pthread_t fromNativeHandle(const Thread::NativeHandle handle) noexcept
{
    pthread_t thread{};
    __builtin_memcpy(&thread, &handle, sizeof(pthread_t));
    return thread;
}

/// @brief Maps an engine ThreadPriority to a Darwin QoS class and relative priority offset [-15..0].
/// @param[in] priority Engine thread scheduling priority.
/// @param[out] outQosClass Receives the Darwin qos_class_t value.
/// @param[out] outRelativePriority Receives the relative offset within the QoS class.
/// @param[out] outMachImportance Receives the Mach THREAD_PRECEDENCE_POLICY importance value.
/// @return True if @p priority is a valid enumerator, false otherwise.
[[nodiscard]] constexpr bool toDarwinPriority(
    const ThreadPriority priority, qos_class_t& outQosClass, int& outRelativePriority, integer_t& outMachImportance
) noexcept
{
    switch (priority)
    {
    case ThreadPriority::Lowest:
        outQosClass = QOS_CLASS_BACKGROUND;
        outRelativePriority = 0;
        outMachImportance = -15;
        return true;
    case ThreadPriority::BelowNormal:
        outQosClass = QOS_CLASS_UTILITY;
        outRelativePriority = 0;
        outMachImportance = -5;
        return true;
    case ThreadPriority::Normal:
        outQosClass = QOS_CLASS_DEFAULT;
        outRelativePriority = 0;
        outMachImportance = 0;
        return true;
    case ThreadPriority::AboveNormal:
        outQosClass = QOS_CLASS_USER_INITIATED;
        outRelativePriority = 0;
        outMachImportance = 5;
        return true;
    case ThreadPriority::Highest:
        outQosClass = QOS_CLASS_USER_INTERACTIVE;
        outRelativePriority = -5;
        outMachImportance = 10;
        return true;
    case ThreadPriority::TimeCritical:
        outQosClass = QOS_CLASS_USER_INTERACTIVE;
        outRelativePriority = 0;
        outMachImportance = 15;
        return true;
    default:
        return false;
    }
}

/// @brief Pre-creation hook that configures the Darwin QoS class on the pthread_attr_t structure.
/// @param[in,out] attrPtr Pointer to the pthread_attr_t being initialized.
/// @param[in] desc Thread creation configuration descriptor.
/// @return ThreadResult::Success if configured, or ThreadResult::PriorityFailed on error.
[[nodiscard]] ThreadResult configureAppleAttributes(void* const attrPtr, const ThreadDesc& desc) noexcept
{
    qos_class_t qosClass = QOS_CLASS_DEFAULT;
    int relativePrio = 0;
    integer_t machImportance = 0;

    if (!toDarwinPriority(desc.priority, qosClass, relativePrio, machImportance))
    {
        return ThreadResult::InvalidArguments;
    }

    pthread_attr_t* const attr = static_cast<pthread_attr_t*>(attrPtr);
    if (::pthread_attr_set_qos_class_np(attr, qosClass, relativePrio) != 0)
    {
        return ThreadResult::PriorityFailed;
    }

    return ThreadResult::Success;
}

/// @brief Child startup hook that assigns the Darwin debugger thread name and returns pthread_threadid_np.
/// @param[in] name Null-terminated UTF-8 thread name.
/// @return Calling thread's 64-bit Darwin ThreadId.
[[nodiscard]] ThreadId initializeAppleChild(const char* const name) noexcept
{
    if (name != nullptr && name[0] != '\0')
    {
        (void)::pthread_setname_np(name);
    }
    return Thread::currentThreadId();
}

/// @brief Parent startup hook forwarding affinity configuration to AppleThread::setAffinity.
/// @param[in,out] baseThread Reference to the unix::Thread base instance.
/// @param[in] affinityMask Bitmask of allowed logical CPU cores.
/// @return Result of AppleThread::setAffinity.
[[nodiscard]] ThreadResult
    applyAppleAffinity(gp::platform::unix::Thread& baseThread, const gp::UInt64 affinityMask) noexcept
{
    return static_cast<Thread&>(baseThread).setAffinity(affinityMask);
}

/// @brief Parent startup hook forwarding priority configuration to AppleThread::setPriority.
/// @param[in,out] baseThread Reference to the unix::Thread base instance.
/// @param[in] priority Target scheduling priority level.
/// @return Result of AppleThread::setPriority.
[[nodiscard]] ThreadResult
    applyApplePriority(gp::platform::unix::Thread& baseThread, const ThreadPriority priority) noexcept
{
    return static_cast<Thread&>(baseThread).setPriority(priority);
}

}   // namespace detail

ThreadResult Thread::start(const ThreadDesc& desc) noexcept
{
    const StartupHooks appleHooks{ .configureAttributes = &detail::configureAppleAttributes,
                                   .initializeChildThread = &detail::initializeAppleChild,
                                   .applyAffinity = &detail::applyAppleAffinity,
                                   .applyPriority = &detail::applyApplePriority };
    return startWithHooks(desc, appleHooks);
}

ThreadResult Thread::setAffinity(const gp::UInt64 affinityMask) noexcept
{
    if (!isJoinable())
    {
        return ThreadResult::NotJoinable;
    }

    if (affinityMask == 0ULL || affinityMask == kNoAffinityMask)
    {
        return ThreadResult::InvalidArguments;
    }

    const gp::UInt32 totalCores = getHardwareConcurrency();
    gp::UInt32 firstValidCore = 64u;

    for (gp::UInt32 bit = 0u; bit < 64u && bit < totalCores; ++bit)
    {
        if ((affinityMask & (1ULL << bit)) != 0ULL)
        {
            firstValidCore = bit;
            break;
        }
    }

    if (firstValidCore >= totalCores)
    {
        return ThreadResult::AffinityFailed;
    }

    const pthread_t nativeThread = detail::fromNativeHandle(m_handle);
    const thread_port_t machThread = ::pthread_mach_thread_np(nativeThread);
    if (machThread == MACH_PORT_NULL)
    {
        return ThreadResult::AffinityFailed;
    }

    thread_affinity_policy_data_t policyData{};
    policyData.affinity_tag = static_cast<integer_t>(firstValidCore + 1u);

    const kern_return_t kr = ::thread_policy_set(
        machThread, THREAD_AFFINITY_POLICY, reinterpret_cast<thread_policy_t>(&policyData), THREAD_AFFINITY_POLICY_COUNT
    );

    if (kr == KERN_SUCCESS)
    {
        return ThreadResult::Success;
    }

    if (kr == KERN_NOT_SUPPORTED)
    {
        return ThreadResult::UnsupportedPlatform;
    }

    return ThreadResult::AffinityFailed;
}

ThreadResult Thread::setPriority(const ThreadPriority priority) noexcept
{
    if (!isJoinable())
    {
        return ThreadResult::NotJoinable;
    }

    qos_class_t qosClass = QOS_CLASS_DEFAULT;
    int relativePrio = 0;
    integer_t machImportance = 0;

    if (!detail::toDarwinPriority(priority, qosClass, relativePrio, machImportance))
    {
        return ThreadResult::InvalidArguments;
    }

    const pthread_t nativeThread = detail::fromNativeHandle(m_handle);

    // If invoked from the thread itself, update the Darwin QoS class directly
    if (::pthread_equal(::pthread_self(), nativeThread) != 0)
    {
        if (::pthread_set_qos_class_self_np(qosClass, relativePrio) == 0)
        {
            return ThreadResult::Success;
        }
    }

    // Note: Darwin prevents changing another thread's QoS class if a QoS class was already bound,
    // so cross-thread runtime adjustments use Mach THREAD_PRECEDENCE_POLICY on the underlying Mach port.
    const thread_port_t machThread = ::pthread_mach_thread_np(nativeThread);
    if (machThread == MACH_PORT_NULL)
    {
        return ThreadResult::PriorityFailed;
    }

    thread_precedence_policy_data_t precedenceData{};
    precedenceData.importance = machImportance;

    const kern_return_t kr = ::thread_policy_set(
        machThread,
        THREAD_PRECEDENCE_POLICY,
        reinterpret_cast<thread_policy_t>(&precedenceData),
        THREAD_PRECEDENCE_POLICY_COUNT
    );

    return (kr == KERN_SUCCESS) ? ThreadResult::Success : ThreadResult::PriorityFailed;
}

ThreadId Thread::currentThreadId() noexcept
{
    thread_local ThreadId s_cachedTid = 0ULL;
    if (s_cachedTid == 0ULL) [[unlikely]]
    {
        gp::UInt64 tid = 0ULL;
        (void)::pthread_threadid_np(nullptr, &tid);
        s_cachedTid = static_cast<ThreadId>(tid);
    }
    return s_cachedTid;
}

gp::UInt32 Thread::getProcessConcurrency() noexcept
{
    int logicalCpu = 0;
    size_t size = sizeof(logicalCpu);
    if (::sysctlbyname("hw.logicalcpu", &logicalCpu, &size, nullptr, 0) == 0 && logicalCpu > 0)
    {
        return static_cast<gp::UInt32>(logicalCpu);
    }
    return getHardwareConcurrency();
}

}   // namespace gp::platform::apple
