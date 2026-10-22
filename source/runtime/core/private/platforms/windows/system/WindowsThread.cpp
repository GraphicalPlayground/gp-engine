// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/windows/system/WindowsThread.hpp"

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <Windows.h>

// Include after Windows.h to avoid conflicts with min/max macros.
#include <intrin.h>
#include <process.h>

namespace gp::platform::windows
{

namespace
{

/// @brief Maximum stack size supported by Win32 _beginthreadex (32-bit gp::UInt32 limit).
constexpr gp::UInt64 kMaxWin32StackSize = 0xFFFFFFFFULL;

/// @brief Fixed buffer capacity (including null terminator) for stack-copied thread names.
constexpr gp::UInt32 kMaxThreadNameBytes = 64u;

/// @brief Stack-allocated handoff packet passed from Thread::start() to threadTrampoline().
/// @details Eliminates heap allocations during thread creation while guaranteeing that caller-owned
///          strings and descriptors cannot dangle before the child thread begins execution.
struct ThreadStartPayload
{
    ThreadEntryFunction entry = nullptr;
    void* userData = nullptr;
    HANDLE handoffEvent = nullptr;
    char name[kMaxThreadNameBytes] = {};
};

/// @brief Copies a null-terminated UTF-8 thread name into a fixed-size buffer with guaranteed null termination.
/// @param[out] dest Destination fixed-size character array.
/// @param[in] src Source null-terminated string (may be nullptr).
constexpr void copyThreadName(char (&dest)[kMaxThreadNameBytes], const char* src) noexcept
{
    if (src == nullptr)
    {
        dest[0] = '\0';
        return;
    }

    gp::UInt32 i = 0u;
    for (; i + 1u < kMaxThreadNameBytes && src[i] != '\0'; ++i)
    {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

/// @brief Assigns the UTF-8 debugger thread description on the calling Win32 thread.
/// @details Converts the UTF-8 name into a stack-allocated UTF-16 buffer and invokes SetThreadDescription
///          so the thread name surfaces in Visual Studio, PIX, RenderDoc, and Tracy.
/// @param[in] name Null-terminated UTF-8 thread name string.
void setCurrentThreadDebuggerName(const char* name) noexcept
{
    if (name == nullptr || name[0] == '\0')
    {
        return;
    }

    wchar_t wideName[kMaxThreadNameBytes] = {};
    const int converted =
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wideName, static_cast<int>(kMaxThreadNameBytes));

    if (converted > 0)
    {
        // Best-effort debugger metadata for PIX, Tracy, RenderDoc, and Visual Studio
        (void)::SetThreadDescription(::GetCurrentThread(), wideName);
    }
}

/// @brief Maps an engine ThreadPriority enumerator to the corresponding Win32 thread priority constant.
/// @param[in] priority Engine thread scheduling priority.
/// @return Win32 THREAD_PRIORITY_* constant.
[[nodiscard]] constexpr int toWin32Priority(ThreadPriority priority) noexcept
{
    switch (priority)
    {
    case ThreadPriority::Lowest:
        return THREAD_PRIORITY_LOWEST;
    case ThreadPriority::BelowNormal:
        return THREAD_PRIORITY_BELOW_NORMAL;
    case ThreadPriority::Normal:
        return THREAD_PRIORITY_NORMAL;
    case ThreadPriority::AboveNormal:
        return THREAD_PRIORITY_ABOVE_NORMAL;
    case ThreadPriority::Highest:
        return THREAD_PRIORITY_HIGHEST;
    case ThreadPriority::TimeCritical:
        return THREAD_PRIORITY_TIME_CRITICAL;
    default:
        return THREAD_PRIORITY_NORMAL;
    }
}

/// @brief Counts the number of set bits (population count) in a pointer-sized affinity bitmask.
/// @param[in] bitmask Affinity mask to evaluate.
/// @return Number of set bits in @p bitmask.
[[nodiscard]] constexpr gp::UInt32 countSetBits(ULONG_PTR bitmask) noexcept
{
    gp::UInt32 count = 0u;
    while (bitmask != 0u)
    {
        bitmask &= (bitmask - 1u);
        ++count;
    }
    return count;
}

/// @brief Win32 _beginthreadex entry trampoline that completes startup handoff before invoking user code.
/// @details Copies the entry function and user data onto the child thread's stack, sets the debugger thread
///          name, signals the parent's manual-reset handoff event, and invokes the user entry point.
/// @param[in] rawPayload Pointer to the parent stack-allocated ThreadStartPayload.
/// @return Exit code returned by the user ThreadEntryFunction.
gp::UInt32 __stdcall threadTrampoline(void* rawPayload) noexcept
{
    auto* payload = static_cast<ThreadStartPayload*>(rawPayload);

    // Copy startup parameters into child thread local stack
    const ThreadEntryFunction entry = payload->entry;
    void* const userData = payload->userData;
    setCurrentThreadDebuggerName(payload->name);

    // Signal the parent thread that the stack-allocated payload can be destroyed
    ::SetEvent(payload->handoffEvent);

    return entry(userData);
}

}   // namespace

Thread::~Thread() noexcept
{
    if (isJoinable())
    {
        // In a AAA engine, destroying an unjoined thread is a lifecycle violation.
        // Assert in debug builds; fall back to join() only if not executing on this thread.
#if defined(_DEBUG)
        __debugbreak();
#endif

        if (currentThreadId() != m_threadId)
        {
            (void)join();
        }
    }
}

Thread::Thread(Thread&& other) noexcept
    : m_handle(other.m_handle)
    , m_threadId(other.m_threadId)
{
    other.m_handle = nullptr;
    other.m_threadId = 0ULL;
}

Thread& Thread::operator=(Thread&& other) noexcept
{
    if (this != &other)
    {
        if (isJoinable())
        {
#if defined(_DEBUG)
            __debugbreak();
#endif

            if (currentThreadId() != m_threadId)
            {
                (void)join();
            }
        }

        m_handle = other.m_handle;
        m_threadId = other.m_threadId;
        other.m_handle = nullptr;
        other.m_threadId = 0ULL;
    }
    return *this;
}

ThreadResult Thread::start(const ThreadDesc& desc) noexcept
{
    if (isJoinable())
    {
        return ThreadResult::AlreadyRunning;
    }

    if (desc.entry == nullptr)
    {
        return ThreadResult::InvalidArguments;
    }

    if (desc.stackSize > kMaxWin32StackSize)
    {
        return ThreadResult::StackSizeOverflow;
    }

    // Scoped kernel event avoids heap allocation and avoids parent CPU busy-spinning
    const HANDLE handoffEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (handoffEvent == nullptr)
    {
        return ThreadResult::CreationFailed;
    }

    ThreadStartPayload payload{};
    payload.entry = desc.entry;
    payload.userData = desc.userData;
    payload.handoffEvent = handoffEvent;
    copyThreadName(payload.name, desc.name);

    gp::UInt32 winThreadId = 0u;
    const uintptr_t rawHandle = ::_beginthreadex(
        nullptr, static_cast<gp::UInt32>(desc.stackSize), &threadTrampoline, &payload, CREATE_SUSPENDED, &winThreadId
    );

    if (rawHandle == 0u)
    {
        ::CloseHandle(handoffEvent);
        return ThreadResult::CreationFailed;
    }

    m_handle = reinterpret_cast<NativeHandle>(rawHandle);
    m_threadId = static_cast<ThreadId>(winThreadId);

    // Apply affinity if explicitly specified (kNoAffinityMask = OS default)
    if (desc.affinityMask != kNoAffinityMask)
    {
        const ThreadResult affinityRes = setAffinity(desc.affinityMask);
        if (affinityRes != ThreadResult::Success && desc.failOnConfigError)
        {
            // Safe to terminate: thread is still suspended and has not entered threadTrampoline
            ::TerminateThread(static_cast<HANDLE>(m_handle), 0);
            ::CloseHandle(static_cast<HANDLE>(m_handle));
            ::CloseHandle(handoffEvent);
            m_handle = nullptr;
            m_threadId = 0ULL;
            return affinityRes;
        }
    }

    if (desc.priority != ThreadPriority::Normal)
    {
        const ThreadResult priorityRes = setPriority(desc.priority);
        if (priorityRes != ThreadResult::Success && desc.failOnConfigError)
        {
            ::TerminateThread(static_cast<HANDLE>(m_handle), 0);
            ::CloseHandle(static_cast<HANDLE>(m_handle));
            ::CloseHandle(handoffEvent);
            m_handle = nullptr;
            m_threadId = 0ULL;
            return priorityRes;
        }
    }

    if (::ResumeThread(static_cast<HANDLE>(m_handle)) == static_cast<DWORD>(-1))
    {
        ::TerminateThread(static_cast<HANDLE>(m_handle), 0);
        ::CloseHandle(static_cast<HANDLE>(m_handle));
        ::CloseHandle(handoffEvent);
        m_handle = nullptr;
        m_threadId = 0ULL;
        return ThreadResult::ResumeFailed;
    }

    // Wait on either handoff completion OR premature thread termination
    HANDLE waitHandles[2] = { handoffEvent, static_cast<HANDLE>(m_handle) };
    const DWORD waitStatus = ::WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
    ::CloseHandle(handoffEvent);

    if (waitStatus != WAIT_OBJECT_0 && waitStatus != (WAIT_OBJECT_0 + 1))
    {
        return ThreadResult::WaitFailed;
    }

    return ThreadResult::Success;
}

ThreadResult Thread::join(gp::UInt32* outExitCode) noexcept
{
    if (!isJoinable())
    {
        return ThreadResult::NotJoinable;
    }

    if (currentThreadId() == m_threadId)
    {
        return ThreadResult::SelfJoinError;
    }

    const HANDLE winHandle = static_cast<HANDLE>(m_handle);
    if (::WaitForSingleObject(winHandle, INFINITE) != WAIT_OBJECT_0)
    {
        // Do not close or clear m_handle on wait failure so caller can diagnose or retry
        return ThreadResult::WaitFailed;
    }

    if (outExitCode != nullptr)
    {
        DWORD exitCode = 0;
        if (::GetExitCodeThread(winHandle, &exitCode) != FALSE)
        {
            *outExitCode = static_cast<gp::UInt32>(exitCode);
        }
    }

    ::CloseHandle(winHandle);
    m_handle = nullptr;
    m_threadId = 0ULL;
    return ThreadResult::Success;
}

ThreadResult Thread::detach() noexcept
{
    if (!isJoinable())
    {
        return ThreadResult::NotJoinable;
    }

    ::CloseHandle(static_cast<HANDLE>(m_handle));
    m_handle = nullptr;
    m_threadId = 0ULL;
    return ThreadResult::Success;
}

ThreadResult Thread::setAffinity(gp::UInt64 affinityMask) noexcept
{
    if (!isJoinable())
    {
        return ThreadResult::NotJoinable;
    }

    if (affinityMask == 0ULL || affinityMask == kNoAffinityMask)
    {
        return ThreadResult::InvalidArguments;
    }

    const DWORD_PTR prevMask =
        ::SetThreadAffinityMask(static_cast<HANDLE>(m_handle), static_cast<DWORD_PTR>(affinityMask));

    return (prevMask != 0u) ? ThreadResult::Success : ThreadResult::AffinityFailed;
}

ThreadResult Thread::setPriority(ThreadPriority priority) noexcept
{
    if (!isJoinable())
    {
        return ThreadResult::NotJoinable;
    }

    const BOOL ok = ::SetThreadPriority(static_cast<HANDLE>(m_handle), toWin32Priority(priority));

    return (ok != FALSE) ? ThreadResult::Success : ThreadResult::PriorityFailed;
}

void Thread::sleep(gp::UInt32 milliseconds) noexcept
{
    ::Sleep(static_cast<DWORD>(milliseconds));
}

void Thread::yieldExecution() noexcept
{
    (void)::SwitchToThread();
}

void Thread::spinWait() noexcept
{
    ::_mm_pause();
}

ThreadId Thread::currentThreadId() noexcept
{
    return static_cast<ThreadId>(::GetCurrentThreadId());
}

gp::UInt32 Thread::getHardwareConcurrency() noexcept
{
    SYSTEM_INFO sysInfo{};
    ::GetSystemInfo(&sysInfo);
    return (sysInfo.dwNumberOfProcessors > 0u) ? static_cast<gp::UInt32>(sysInfo.dwNumberOfProcessors) : 1u;
}

gp::UInt32 Thread::getProcessConcurrency() noexcept
{
    DWORD_PTR processMask = 0u;
    DWORD_PTR systemMask = 0u;
    if (::GetProcessAffinityMask(::GetCurrentProcess(), &processMask, &systemMask) != FALSE)
    {
        const gp::UInt32 count = countSetBits(processMask);
        if (count > 0u)
        {
            return count;
        }
    }
    return getHardwareConcurrency();
}

}   // namespace gp::platform::windows
