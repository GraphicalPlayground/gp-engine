// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/linux/system/LinuxThread.hpp"
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace gp::platform::linux
{

namespace detail
{

/// @brief Linux kernel TASK_COMM_LEN limit (15 characters + null terminator).
constexpr gp::UInt32 kLinuxTaskCommLimit = 16u;

/// @brief Extracts a POSIX pthread_t from an opaque NativeHandle.
/// @param[in] handle Opaque pointer handle.
/// @return Restored POSIX pthread_t.
[[nodiscard]] pthread_t fromNativeHandle(const Thread::NativeHandle handle) noexcept
{
    pthread_t thread{};
    __builtin_memcpy(&thread, &handle, sizeof(pthread_t));
    return thread;
}

/// @brief Maps an engine ThreadPriority to a Linux kernel nice value [-20..19].
/// @param[in] priority Engine thread priority enumerator.
/// @param[out] outNice Receives the target nice value.
/// @return True if @p priority is a valid enumerator, false otherwise.
[[nodiscard]] constexpr bool toLinuxNiceValue(const ThreadPriority priority, int& outNice) noexcept
{
    switch (priority)
    {
    case ThreadPriority::Lowest:
        outNice = 19;
        return true;
    case ThreadPriority::BelowNormal:
        outNice = 10;
        return true;
    case ThreadPriority::Normal:
        outNice = 0;
        return true;
    case ThreadPriority::AboveNormal:
        outNice = -5;
        return true;
    case ThreadPriority::Highest:
        outNice = -10;
        return true;
    case ThreadPriority::TimeCritical:
        outNice = -20;
        return true;
    default:
        return false;
    }
}

/// @brief Child startup hook that assigns the 15-character Linux prctl/pthread comm name and returns SYS_gettid.
/// @param[in] name Null-terminated UTF-8 thread name.
/// @return Calling thread's Linux kernel TID.
[[nodiscard]] ThreadId initializeLinuxChild(const char* const name) noexcept
{
    if (name != nullptr && name[0] != '\0')
    {
        char commName[kLinuxTaskCommLimit] = {};
        gp::UInt32 i = 0u;
        for (; (i + 1u) < kLinuxTaskCommLimit && name[i] != '\0'; ++i)
        {
            commName[i] = name[i];
        }
        commName[i] = '\0';
        (void)::pthread_setname_np(::pthread_self(), commName);
    }

    return Thread::currentThreadId();
}

/// @brief Parent startup hook forwarding affinity configuration to LinuxThread::setAffinity.
/// @param[in,out] baseThread Reference to the unix::Thread base instance.
/// @param[in] affinityMask Bitmask of allowed logical CPU cores.
/// @return Result of LinuxThread::setAffinity.
[[nodiscard]] ThreadResult
    applyLinuxAffinity(gp::platform::unix::Thread& baseThread, const gp::UInt64 affinityMask) noexcept
{
    return static_cast<Thread&>(baseThread).setAffinity(affinityMask);
}

/// @brief Parent startup hook forwarding priority configuration to LinuxThread::setPriority.
/// @param[in,out] baseThread Reference to the unix::Thread base instance.
/// @param[in] priority Target scheduling priority level.
/// @return Result of LinuxThread::setPriority.
[[nodiscard]] ThreadResult
    applyLinuxPriority(gp::platform::unix::Thread& baseThread, const ThreadPriority priority) noexcept
{
    return static_cast<Thread&>(baseThread).setPriority(priority);
}

}   // namespace detail

ThreadResult Thread::start(const ThreadDesc& desc) noexcept
{
    const StartupHooks linuxHooks{ .configureAttributes = nullptr,
                                   .initializeChildThread = &detail::initializeLinuxChild,
                                   .applyAffinity = &detail::applyLinuxAffinity,
                                   .applyPriority = &detail::applyLinuxPriority };
    return startWithHooks(desc, linuxHooks);
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

    cpu_set_t cpuset{};
    CPU_ZERO(&cpuset);

    for (gp::UInt32 bit = 0u; bit < 64u; ++bit)
    {
        if ((affinityMask & (1ULL << bit)) != 0ULL)
        {
            CPU_SET(static_cast<int>(bit), &cpuset);
        }
    }

    const pthread_t nativeThread = detail::fromNativeHandle(m_handle);
    const int err = ::pthread_setaffinity_np(nativeThread, sizeof(cpu_set_t), &cpuset);
    return (err == 0) ? ThreadResult::Success : ThreadResult::AffinityFailed;
}

ThreadResult Thread::setPriority(const ThreadPriority priority) noexcept
{
    if (!isJoinable())
    {
        return ThreadResult::NotJoinable;
    }

    int targetNice = 0;
    if (!detail::toLinuxNiceValue(priority, targetNice))
    {
        return ThreadResult::InvalidArguments;
    }

    const pthread_t nativeThread = detail::fromNativeHandle(m_handle);

    // For TimeCritical, attempt real-time SCHED_FIFO first if the process holds CAP_SYS_NICE
    if (priority == ThreadPriority::TimeCritical)
    {
        struct sched_param rtParam{};
        const int maxFifo = ::sched_get_priority_max(SCHED_FIFO);
        if (maxFifo > 0)
        {
            rtParam.sched_priority = maxFifo;
            if (::pthread_setschedparam(nativeThread, SCHED_FIFO, &rtParam) == 0)
            {
                return ThreadResult::Success;
            }
        }
    }
    else
    {
        // Ensure non-TimeCritical threads use SCHED_OTHER if previously elevated to SCHED_FIFO
        int currentPolicy = SCHED_OTHER;
        struct sched_param currentParam{};
        if (::pthread_getschedparam(nativeThread, &currentPolicy, &currentParam) == 0 && currentPolicy != SCHED_OTHER)
        {
            currentParam.sched_priority = 0;
            (void)::pthread_setschedparam(nativeThread, SCHED_OTHER, &currentParam);
        }
    }

    // Apply per-thread kernel nice level via the thread's kernel TID (PRIO_PROCESS accepts Linux kernel tid)
    if (::setpriority(PRIO_PROCESS, static_cast<id_t>(m_threadId), targetNice) == 0)
    {
        return ThreadResult::Success;
    }

    // Unprivileged Linux users without CAP_SYS_NICE receive EACCES/EPERM when setting negative nice values
    // or lowering nice values; verify the thread is alive and succeed as best-effort user-space priority.
    if (errno == EACCES || errno == EPERM)
    {
        return (::pthread_kill(nativeThread, 0) == 0) ? ThreadResult::Success : ThreadResult::PriorityFailed;
    }

    return ThreadResult::PriorityFailed;
}

ThreadId Thread::currentThreadId() noexcept
{
    thread_local ThreadId s_cachedTid = 0ULL;
    if (s_cachedTid == 0ULL) [[unlikely]]
    {
        s_cachedTid = static_cast<ThreadId>(::syscall(SYS_gettid));
    }
    return s_cachedTid;
}

gp::UInt32 Thread::getProcessConcurrency() noexcept
{
    cpu_set_t cpuset{};
    CPU_ZERO(&cpuset);

    if (::sched_getaffinity(0, sizeof(cpu_set_t), &cpuset) == 0)
    {
        const int count = CPU_COUNT(&cpuset);
        if (count > 0)
        {
            return static_cast<gp::UInt32>(count);
        }
    }

    return getHardwareConcurrency();
}

}   // namespace gp::platform::linux
