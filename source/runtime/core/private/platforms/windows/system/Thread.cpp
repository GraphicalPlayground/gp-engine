// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/windows/system/Thread.hpp"

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

namespace detail
{

/// @brief Structure used to pass parameters to the thread entry function.
struct ThreadStartPayload
{
    ThreadEntryFunction entry = nullptr;
    void* userData = nullptr;
    const char* name = nullptr;
    volatile long handoffComplete = 0;
};

/// @brief Sets the name of the current thread for debugging purposes.
/// @param[in] name The name to assign to the current thread.
void setCurrentThreadDebuggerName(const char* name) noexcept
{
    if (name == nullptr || name[0] == '\0')
    {
        return;
    }

    // Stack-allocated UTF-16 conversion (zero heap allocations)
    wchar_t wideName[64] = {};
    const int converted = ::MultiByteToWideChar(CP_UTF8, 0, name, -1, wideName, 63);
    if (converted > 0)
    {
        // SetThreadDescription is available on Windows 10 1607+ and surfaces in PIX, Tracy, and VS
        ::SetThreadDescription(::GetCurrentThread(), wideName);
    }
}

/// @brief Converts a gp::platform::generic::ThreadPriority to the corresponding Win32 thread priority constant.
/// @param[in] priority The gp::platform::generic::ThreadPriority value to convert.
/// @return The corresponding Win32 thread priority constant.
int toWin32Priority(ThreadPriority priority) noexcept
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

/// @brief Trampoline function that serves as the entry point for the OS thread.
/// @param[in] rawPayload Pointer to the ThreadStartPayload structure containing the entry function and user data.
/// @return The exit code returned by the thread entry function.
gp::UInt32 __stdcall threadTrampoline(void* rawPayload) noexcept
{
    auto* payload = static_cast<ThreadStartPayload*>(rawPayload);

    // Copy startup parameters onto the child thread's own stack
    const ThreadEntryFunction entry = payload->entry;
    void* const userData = payload->userData;
    setCurrentThreadDebuggerName(payload->name);

    // Signal the parent thread that the stack payload is no longer needed
    ::_InterlockedExchange(&payload->handoffComplete, 1);

    return entry(userData);
}

}   // namespace detail

Thread::~Thread() noexcept
{
    // Enforce clean lifecycle: join if still active at destruction
    if (isJoinable())
    {
        join();
    }
}

Thread::Thread(Thread&& other) noexcept
{
    m_handle = other.m_handle;
    m_threadId = other.m_threadId;
    other.m_handle = nullptr;
    other.m_threadId = 0;
}

Thread& Thread::operator=(Thread&& other) noexcept
{
    if (this != &other)
    {
        if (isJoinable())
        {
            join();
        }
        m_handle = other.m_handle;
        m_threadId = other.m_threadId;
        other.m_handle = nullptr;
        other.m_threadId = 0;
    }
    return *this;
}

bool Thread::start(const ThreadDesc& desc) noexcept
{
    if (isJoinable() || desc.entry == nullptr)
    {
        return false;
    }

    detail::ThreadStartPayload payload{};
    payload.entry = desc.entry;
    payload.userData = desc.userData;
    payload.name = desc.name;
    payload.handoffComplete = 0;

    gp::UInt32 winThreadId = 0;
    const uintptr_t handle = ::_beginthreadex(
        nullptr,
        static_cast<gp::UInt32>(desc.stackSize),
        &detail::threadTrampoline,
        &payload,
        CREATE_SUSPENDED,
        &winThreadId
    );

    if (handle == 0)
    {
        return false;
    }

    m_handle = reinterpret_cast<NativeThreadHandle>(handle);
    m_threadId = static_cast<ThreadId>(winThreadId);

    // Apply core affinity and priority before waking the thread
    if (desc.affinityMask != ~0ULL && desc.affinityMask != 0ULL)
    {
        setAffinity(desc.affinityMask);
    }

    if (desc.priority != ThreadPriority::Normal)
    {
        setPriority(desc.priority);
    }

    ::ResumeThread(static_cast<HANDLE>(m_handle));

    // Wait for the child thread to copy the stack payload before start() returns
    while (::_InterlockedCompareExchange(&payload.handoffComplete, 0, 0) == 0)
    {
        ::_mm_pause();
    }

    return true;
}

void Thread::join(gp::UInt32* outExitCode) noexcept
{
    if (!isJoinable())
    {
        return;
    }

    const HANDLE winHandle = static_cast<HANDLE>(m_handle);
    ::WaitForSingleObject(winHandle, INFINITE);

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
    m_threadId = 0;
}

void Thread::detach() noexcept
{
    if (!isJoinable())
    {
        return;
    }

    ::CloseHandle(static_cast<HANDLE>(m_handle));
    m_handle = nullptr;
    m_threadId = 0;
}

bool Thread::setAffinity(gp::UInt64 affinityMask) noexcept
{
    if (!isJoinable() || affinityMask == 0ULL)
    {
        return false;
    }

    const DWORD_PTR previous =
        ::SetThreadAffinityMask(static_cast<HANDLE>(m_handle), static_cast<DWORD_PTR>(affinityMask));
    return previous != 0;
}

bool Thread::setPriority(ThreadPriority priority) noexcept
{
    if (!isJoinable())
    {
        return false;
    }

    return ::SetThreadPriority(static_cast<HANDLE>(m_handle), detail::toWin32Priority(priority)) != FALSE;
}

void Thread::sleep(gp::UInt32 milliseconds) noexcept
{
    ::Sleep(static_cast<DWORD>(milliseconds));
}

void Thread::yieldExecution() noexcept
{
    ::SwitchToThread();
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
    SYSTEM_INFO sys_info{};
    ::GetSystemInfo(&sys_info);
    return sys_info.dwNumberOfProcessors > 0 ? static_cast<gp::UInt32>(sys_info.dwNumberOfProcessors) : 1u;
}

}   // namespace gp::platform::windows
