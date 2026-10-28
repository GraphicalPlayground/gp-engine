// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/unix/system/UnixThread.hpp"
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#if defined(__linux__)
    #include <sys/syscall.h>
#endif

namespace gp::platform::unix
{

namespace detail
{

/// @brief Maximum stack size limit (4 GiB - 1) matching WindowsThread to enforce consistent cross-platform bounds.
constexpr gp::UInt64 kMaxPosixStackSize = 0xFFFFFFFFULL;

/// @brief Fixed buffer capacity (including null terminator) for stack-copied thread names.
constexpr gp::UInt32 kMaxThreadNameBytes = 64u;

/// @brief Nanoseconds per millisecond conversion constant.
constexpr gp::UInt64 kNanosecondsPerMillisecond = 1'000'000ULL;

/// @brief Milliseconds per second conversion constant.
constexpr gp::UInt32 kMillisecondsPerSecond = 1'000u;

static_assert(sizeof(pthread_t) <= sizeof(Thread::NativeHandle), "pthread_t exceeds NativeHandle pointer width!");

/// @brief Converts a POSIX pthread_t into an opaque NativeHandle without UB.
/// @param[in] thread POSIX thread handle.
/// @return Opaque pointer representation.
[[nodiscard]] Thread::NativeHandle toNativeHandle(const pthread_t thread) noexcept
{
    Thread::NativeHandle handle = nullptr;
    __builtin_memcpy(&handle, &thread, sizeof(pthread_t));
    return handle;
}

/// @brief Extracts a POSIX pthread_t from an opaque NativeHandle without UB.
/// @param[in] handle Opaque pointer representation.
/// @return Restored POSIX thread handle.
[[nodiscard]] pthread_t fromNativeHandle(const Thread::NativeHandle handle) noexcept
{
    pthread_t thread{};
    __builtin_memcpy(&thread, &handle, sizeof(pthread_t));
    return thread;
}

/// @brief Triggers a hardware debugger breakpoint in debug builds when a lifecycle invariant is violated.
void triggerDebugTrap() noexcept
{
#if !defined(NDEBUG) || defined(_DEBUG)
    __builtin_trap();
#endif
}

/// @brief Copies a null-terminated UTF-8 thread name into a fixed-size stack buffer.
/// @param[out] dest Destination fixed-size character array.
/// @param[in] src Source null-terminated string (may be nullptr).
constexpr void copyThreadName(char (&dest)[kMaxThreadNameBytes], const char* const src) noexcept
{
    if (src == nullptr)
    {
        dest[0] = '\0';
        return;
    }

    gp::UInt32 i = 0u;
    for (; (i + 1u) < kMaxThreadNameBytes && src[i] != '\0'; ++i)
    {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

/// @brief Synchronization stages for the two-way parent/child startup barrier.
enum class HandoffStage : gp::UInt8
{
    WaitingForChild = 0,
    ChildReady,
    ParentResume,
    ParentAbort,
    ChildAcknowledged
};

/// @brief Stack-allocated startup synchronization packet shared between startWithHooks() and threadTrampoline().
struct ThreadStartPayload
{
    pthread_mutex_t mutex{};
    pthread_cond_t cond{};
    ThreadEntryFunction entry = nullptr;
    void* userData = nullptr;
    ThreadId (*initializeChildThread)(const char* name) noexcept = nullptr;
    ThreadId childThreadId = 0ULL;
    HandoffStage stage = HandoffStage::WaitingForChild;
    char name[kMaxThreadNameBytes] = {};
};

/// @brief Default generic Unix child initializer that sets the thread name and queries currentThreadId().
/// @param[in] name Null-terminated UTF-8 thread name.
/// @return The calling child thread's 64-bit ThreadId.
[[nodiscard]] ThreadId defaultInitializeChild(const char* const name) noexcept
{
    if (name != nullptr && name[0] != '\0')
    {
#if defined(__APPLE__)
        (void)::pthread_setname_np(name);
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
        char shortName[16] = {};
        gp::UInt32 i = 0u;
        for (; i < 15u && name[i] != '\0'; ++i)
        {
            shortName[i] = name[i];
        }
        shortName[i] = '\0';
        (void)::pthread_setname_np(::pthread_self(), shortName);
#endif
    }
    return Thread::currentThreadId();
}

/// @brief Default generic Unix affinity hook.
/// @param[in,out] thread Target thread instance.
/// @param[in] affinityMask Desired core bitmask.
/// @return Result of thread.setAffinity(affinityMask).
[[nodiscard]] ThreadResult defaultApplyAffinity(Thread& thread, const gp::UInt64 affinityMask) noexcept
{
    return thread.setAffinity(affinityMask);
}

/// @brief Default generic Unix priority hook.
/// @param[in,out] thread Target thread instance.
/// @param[in] priority Desired thread scheduling priority.
/// @return Result of thread.setPriority(priority).
[[nodiscard]] ThreadResult defaultApplyPriority(Thread& thread, const ThreadPriority priority) noexcept
{
    return thread.setPriority(priority);
}

/// @brief POSIX thread entry point that executes the two-way barrier handoff before invoking user code.
/// @param[in] rawPayload Pointer to the parent's stack-allocated ThreadStartPayload.
/// @return Exit code cast to void* for retrieval by pthread_join().
void* threadTrampoline(void* const rawPayload) noexcept
{
    ThreadStartPayload* const payload = static_cast<ThreadStartPayload*>(rawPayload);

    // Copy startup arguments onto the child thread's stack frame
    const ThreadEntryFunction entry = payload->entry;
    void* const userData = payload->userData;
    char localName[kMaxThreadNameBytes] = {};
    copyThreadName(localName, payload->name);
    ThreadId (*const initChildFn)(const char*) noexcept = payload->initializeChildThread;

    // Apply debugger thread name and query kernel ThreadId
    const ThreadId resolvedId = (initChildFn != nullptr) ? initChildFn(localName) : defaultInitializeChild(localName);

    // Notify parent that child metadata is ready and wait for parent configuration (affinity/priority)
    (void)::pthread_mutex_lock(&payload->mutex);
    payload->childThreadId = resolvedId;
    payload->stage = HandoffStage::ChildReady;
    (void)::pthread_cond_signal(&payload->cond);

    while (payload->stage == HandoffStage::ChildReady)
    {
        (void)::pthread_cond_wait(&payload->cond, &payload->mutex);
    }

    const bool shouldAbort = (payload->stage == HandoffStage::ParentAbort);

    // Acknowledge completion so the parent can safely destroy the stack mutex and condvar
    payload->stage = HandoffStage::ChildAcknowledged;
    (void)::pthread_cond_signal(&payload->cond);
    (void)::pthread_mutex_unlock(&payload->mutex);

    if (shouldAbort)
    {
        return nullptr;
    }

    const gp::UInt32 exitCode = entry(userData);
    return reinterpret_cast<void*>(static_cast<uintptr_t>(exitCode));
}

}   // namespace detail

Thread::~Thread() noexcept
{
    if (isJoinable())
    {
        detail::triggerDebugTrap();

        if (!::pthread_equal(::pthread_self(), detail::fromNativeHandle(m_handle)))
        {
            (void)join();
        }
    }
}

Thread::Thread(Thread&& other) noexcept
    : m_handle(other.m_handle)
    , m_threadId(other.m_threadId)
    , m_isJoinable(other.m_isJoinable)
{
    other.m_handle = nullptr;
    other.m_threadId = 0ULL;
    other.m_isJoinable = false;
}

Thread& Thread::operator=(Thread&& other) noexcept
{
    if (this != &other)
    {
        if (isJoinable())
        {
            detail::triggerDebugTrap();

            if (!::pthread_equal(::pthread_self(), detail::fromNativeHandle(m_handle)))
            {
                (void)join();
            }
        }

        m_handle = other.m_handle;
        m_threadId = other.m_threadId;
        m_isJoinable = other.m_isJoinable;

        other.m_handle = nullptr;
        other.m_threadId = 0ULL;
        other.m_isJoinable = false;
    }
    return *this;
}

ThreadResult Thread::start(const ThreadDesc& desc) noexcept
{
    const StartupHooks defaultHooks{ .configureAttributes = nullptr,
                                     .initializeChildThread = &detail::defaultInitializeChild,
                                     .applyAffinity = &detail::defaultApplyAffinity,
                                     .applyPriority = &detail::defaultApplyPriority };
    return startWithHooks(desc, defaultHooks);
}

ThreadResult Thread::startWithHooks(const ThreadDesc& desc, const StartupHooks& hooks) noexcept
{
    if (isJoinable())
    {
        return ThreadResult::AlreadyRunning;
    }

    if (desc.entry == nullptr)
    {
        return ThreadResult::InvalidArguments;
    }

    if (desc.stackSize > detail::kMaxPosixStackSize)
    {
        return ThreadResult::StackSizeOverflow;
    }

    pthread_attr_t attr{};
    if (::pthread_attr_init(&attr) != 0)
    {
        return ThreadResult::CreationFailed;
    }

    (void)::pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    if (desc.stackSize != kDefaultStackSize)
    {
        const size_t minStack = static_cast<size_t>(PTHREAD_STACK_MIN);
        const size_t requestedStack = static_cast<size_t>(desc.stackSize);
        const size_t effectiveStack = (requestedStack < minStack) ? minStack : requestedStack;

        if (::pthread_attr_setstacksize(&attr, effectiveStack) != 0)
        {
            (void)::pthread_attr_destroy(&attr);
            return ThreadResult::StackSizeOverflow;
        }
    }

    if (hooks.configureAttributes != nullptr)
    {
        const ThreadResult attrRes = hooks.configureAttributes(&attr, desc);
        if (attrRes != ThreadResult::Success && desc.failOnConfigError)
        {
            (void)::pthread_attr_destroy(&attr);
            return attrRes;
        }
    }

    detail::ThreadStartPayload payload{};
    if (::pthread_mutex_init(&payload.mutex, nullptr) != 0)
    {
        (void)::pthread_attr_destroy(&attr);
        return ThreadResult::CreationFailed;
    }

    if (::pthread_cond_init(&payload.cond, nullptr) != 0)
    {
        (void)::pthread_mutex_destroy(&payload.mutex);
        (void)::pthread_attr_destroy(&attr);
        return ThreadResult::CreationFailed;
    }

    payload.entry = desc.entry;
    payload.userData = desc.userData;
    payload.initializeChildThread =
        (hooks.initializeChildThread != nullptr) ? hooks.initializeChildThread : &detail::defaultInitializeChild;
    detail::copyThreadName(payload.name, desc.name);

    pthread_t nativeThread{};
    const int createErr = ::pthread_create(&nativeThread, &attr, &detail::threadTrampoline, &payload);
    (void)::pthread_attr_destroy(&attr);

    if (createErr != 0)
    {
        (void)::pthread_cond_destroy(&payload.cond);
        (void)::pthread_mutex_destroy(&payload.mutex);
        return ThreadResult::CreationFailed;
    }

    // Wait for child thread to copy startup payload and publish its kernel ThreadId
    (void)::pthread_mutex_lock(&payload.mutex);
    while (payload.stage == detail::HandoffStage::WaitingForChild)
    {
        (void)::pthread_cond_wait(&payload.cond, &payload.mutex);
    }

    m_handle = detail::toNativeHandle(nativeThread);
    m_threadId = payload.childThreadId;
    m_isJoinable = true;

    ThreadResult configFailure = ThreadResult::Success;

    // Apply affinity while child is parked on the barrier
    if (desc.affinityMask != kNoAffinityMask && hooks.applyAffinity != nullptr)
    {
        const ThreadResult affinityRes = hooks.applyAffinity(*this, desc.affinityMask);
        if (affinityRes != ThreadResult::Success && desc.failOnConfigError)
        {
            configFailure = affinityRes;
        }
    }

    // Apply priority while child is parked on the barrier
    if (configFailure == ThreadResult::Success && desc.priority != ThreadPriority::Normal &&
        hooks.applyPriority != nullptr)
    {
        const ThreadResult priorityRes = hooks.applyPriority(*this, desc.priority);
        if (priorityRes != ThreadResult::Success && desc.failOnConfigError)
        {
            configFailure = priorityRes;
        }
    }

    // Signal child to either resume into user entry or abort immediately
    payload.stage = (configFailure == ThreadResult::Success) ? detail::HandoffStage::ParentResume
                                                             : detail::HandoffStage::ParentAbort;
    (void)::pthread_cond_signal(&payload.cond);

    // Wait for child to acknowledge before destroying stack synchronization primitives
    while (payload.stage != detail::HandoffStage::ChildAcknowledged)
    {
        (void)::pthread_cond_wait(&payload.cond, &payload.mutex);
    }
    (void)::pthread_mutex_unlock(&payload.mutex);

    (void)::pthread_cond_destroy(&payload.cond);
    (void)::pthread_mutex_destroy(&payload.mutex);

    if (configFailure != ThreadResult::Success)
    {
        (void)::pthread_join(nativeThread, nullptr);
        m_handle = nullptr;
        m_threadId = 0ULL;
        m_isJoinable = false;
        return configFailure;
    }

    return ThreadResult::Success;
}

ThreadResult Thread::join(gp::UInt32* const outExitCode) noexcept
{
    if (!isJoinable())
    {
        return ThreadResult::NotJoinable;
    }

    const pthread_t nativeThread = detail::fromNativeHandle(m_handle);
    if (::pthread_equal(::pthread_self(), nativeThread) != 0)
    {
        return ThreadResult::SelfJoinError;
    }

    void* rawReturn = nullptr;
    if (::pthread_join(nativeThread, &rawReturn) != 0)
    {
        return ThreadResult::WaitFailed;
    }

    if (outExitCode != nullptr)
    {
        *outExitCode = static_cast<gp::UInt32>(reinterpret_cast<uintptr_t>(rawReturn));
    }

    m_handle = nullptr;
    m_threadId = 0ULL;
    m_isJoinable = false;
    return ThreadResult::Success;
}

ThreadResult Thread::detach() noexcept
{
    if (!isJoinable())
    {
        return ThreadResult::NotJoinable;
    }

    const pthread_t nativeThread = detail::fromNativeHandle(m_handle);
    if (::pthread_detach(nativeThread) != 0)
    {
        return ThreadResult::WaitFailed;
    }

    m_handle = nullptr;
    m_threadId = 0ULL;
    m_isJoinable = false;
    return ThreadResult::Success;
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

    return ThreadResult::UnsupportedPlatform;
}

ThreadResult Thread::setPriority(const ThreadPriority priority) noexcept
{
    if (!isJoinable())
    {
        return ThreadResult::NotJoinable;
    }

    const pthread_t nativeThread = detail::fromNativeHandle(m_handle);
    int policy = SCHED_OTHER;
    struct sched_param param{};

    if (::pthread_getschedparam(nativeThread, &policy, &param) != 0)
    {
        return ThreadResult::PriorityFailed;
    }

    const int minPrio = ::sched_get_priority_min(policy);
    const int maxPrio = ::sched_get_priority_max(policy);
    if (minPrio == -1 || maxPrio == -1)
    {
        return ThreadResult::PriorityFailed;
    }

    const int span = maxPrio - minPrio;
    switch (priority)
    {
    case ThreadPriority::Lowest:
        param.sched_priority = minPrio;
        break;
    case ThreadPriority::BelowNormal:
        param.sched_priority = minPrio + (span / 4);
        break;
    case ThreadPriority::Normal:
        param.sched_priority = minPrio + (span / 2);
        break;
    case ThreadPriority::AboveNormal:
        param.sched_priority = minPrio + ((span * 3) / 4);
        break;
    case ThreadPriority::Highest:
    case ThreadPriority::TimeCritical:
        param.sched_priority = maxPrio;
        break;
    default:
        return ThreadResult::InvalidArguments;
    }

    if (::pthread_setschedparam(nativeThread, policy, &param) != 0)
    {
        return ThreadResult::PriorityFailed;
    }

    return ThreadResult::Success;
}

void Thread::sleep(const gp::UInt32 milliseconds) noexcept
{
    struct timespec req{};
    req.tv_sec = static_cast<time_t>(milliseconds / detail::kMillisecondsPerSecond);
    req.tv_nsec = static_cast<long>(
        static_cast<gp::UInt64>(milliseconds % detail::kMillisecondsPerSecond) * detail::kNanosecondsPerMillisecond
    );

    struct timespec rem{};
    while (::nanosleep(&req, &rem) == -1 && errno == EINTR)
    {
        req = rem;
    }
}

void Thread::yieldExecution() noexcept
{
    (void)::sched_yield();
}

void Thread::spinWait() noexcept
{
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
    __asm__ __volatile__("yield" ::: "memory");
#else
    __asm__ __volatile__("" ::: "memory");
#endif
}

ThreadId Thread::currentThreadId() noexcept
{
#if defined(__linux__)
    thread_local ThreadId s_cachedTid = 0ULL;
    if (s_cachedTid == 0ULL) [[unlikely]]
    {
        s_cachedTid = static_cast<ThreadId>(::syscall(SYS_gettid));
    }
    return s_cachedTid;
#elif defined(__APPLE__)
    thread_local ThreadId s_cachedTid = 0ULL;
    if (s_cachedTid == 0ULL) [[unlikely]]
    {
        gp::UInt64 tid = 0ULL;
        (void)::pthread_threadid_np(nullptr, &tid);
        s_cachedTid = static_cast<ThreadId>(tid);
    }
    return s_cachedTid;
#else
    return static_cast<ThreadId>(reinterpret_cast<uintptr_t>(::pthread_self()));
#endif
}

gp::UInt32 Thread::getHardwareConcurrency() noexcept
{
    const long cores = ::sysconf(_SC_NPROCESSORS_ONLN);
    return (cores > 0L) ? static_cast<gp::UInt32>(cores) : 1u;
}

gp::UInt32 Thread::getProcessConcurrency() noexcept
{
    return getHardwareConcurrency();
}

}   // namespace gp::platform::unix
