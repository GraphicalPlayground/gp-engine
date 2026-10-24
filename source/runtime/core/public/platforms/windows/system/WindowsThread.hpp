// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/generic/system/GenericThread.hpp"

namespace gp::platform::windows
{

/// @brief Win32 implementation of an OS hardware thread.
/// @details Wraps a native Win32 thread handle created via _beginthreadex with zero-allocation startup handoff.
/// @note Lifecycle Policy: Callers must explicitly join() or detach() before destruction.
///       Destroying a joinable thread triggers an assertion in debug builds and attempts
///       a fallback join() if not executing on the thread itself.
class Thread : public gp::platform::generic::Thread
{
public:
    /// @brief Opaque representation of a Win32 HANDLE to avoid leaking <Windows.h> into public headers.
    using NativeHandle = void*;

private:
    NativeHandle m_handle = nullptr;
    ThreadId m_threadId = 0ULL;

public:
    /// @brief Constructs an empty, non-joinable thread object.
    [[nodiscard]] constexpr Thread() noexcept = default;

    /// @brief Destroys the thread object.
    /// @note Asserts in debug builds if the thread is still joinable, and performs a fallback join()
    ///       when called from a thread other than this one.
    ~Thread() noexcept;

    /// @brief Non-copyable to enforce unique ownership of the underlying Win32 HANDLE.
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    /// @brief Move constructor transferring ownership of the native Win32 thread handle and ID.
    /// @param[in,out] other The source thread object to move from (left in a non-joinable state).
    [[nodiscard]] Thread(Thread&& other) noexcept;

    /// @brief Move assignment operator transferring ownership of the native Win32 thread handle and ID.
    /// @note If this instance is currently joinable, triggers a debug break and attempts a fallback join()
    ///       before acquiring ownership from @p other.
    /// @param[in,out] other The source thread object to move from (left in a non-joinable state).
    /// @return Reference to this thread instance.
    Thread& operator=(Thread&& other) noexcept;

public:
    /// @brief Spawns the OS thread and blocks until startup parameters are safely handed off.
    /// @details Creates the Win32 thread in a suspended state, applies optional CPU affinity and priority,
    ///          resumes execution, and waits on a scoped manual-reset event until the child thread copies
    ///          its stack-allocated startup payload.
    /// @param[in] desc Configuration parameters for the new thread.
    /// @return ThreadResult::Success on success, or a specific error code detailing the failure stage.
    [[nodiscard]] ThreadResult start(const ThreadDesc& desc) noexcept;

    /// @brief Blocks the calling thread until this thread finishes execution and closes the native handle.
    /// @note If waiting fails, the native handle is preserved so the caller can diagnose or retry.
    /// @param[out] outExitCode Optional pointer to receive the thread's exit code upon completion.
    /// @return ThreadResult::Success on completion, ThreadResult::NotJoinable if not running,
    ///         ThreadResult::SelfJoinError if called from this thread, or ThreadResult::WaitFailed on OS wait error.
    [[nodiscard]] ThreadResult join(gp::UInt32* outExitCode = nullptr) noexcept;

    /// @brief Releases ownership of the native OS handle so the thread cleans up automatically upon exit.
    /// @warning Detached threads must not access engine subsystems during or after engine shutdown.
    /// @return ThreadResult::Success if detached, or ThreadResult::NotJoinable if the thread is not active.
    [[nodiscard]] ThreadResult detach() noexcept;

    /// @brief Sets the CPU affinity mask within the thread's current Windows processor group.
    /// @param[in] affinityMask Non-zero bitmask of allowed logical processors (must not be 0 or kNoAffinityMask).
    /// @return ThreadResult::Success if updated, ThreadResult::NotJoinable if inactive,
    ///         ThreadResult::InvalidArguments if the mask is invalid, or ThreadResult::AffinityFailed on Win32 error.
    [[nodiscard]] ThreadResult setAffinity(gp::UInt64 affinityMask) noexcept;

    /// @brief Sets the OS scheduling priority of the thread.
    /// @param[in] priority Target scheduling priority level.
    /// @return ThreadResult::Success if updated, ThreadResult::NotJoinable if inactive,
    ///         or ThreadResult::PriorityFailed on Win32 error.
    [[nodiscard]] ThreadResult setPriority(ThreadPriority priority) noexcept;

    /// @brief Returns true if this object owns a valid, joinable OS thread handle.
    /// @note A thread that has finished executing its entry function remains joinable until join() or detach() is
    /// called.
    /// @return True if joinable, false otherwise.
    [[nodiscard]] constexpr bool isJoinable() const noexcept
    {
        return m_handle != nullptr;
    }

    /// @brief Returns the OS thread ID, or 0 if not joinable.
    /// @return The 64-bit thread identifier, or 0ULL if not active.
    [[nodiscard]] constexpr ThreadId getId() const noexcept
    {
        return m_threadId;
    }

    /// @brief Returns the underlying Win32 HANDLE as an opaque void*.
    /// @return The native Win32 thread handle, or nullptr if not joinable.
    [[nodiscard]] constexpr NativeHandle getNativeHandle() const noexcept
    {
        return m_handle;
    }

    /// @brief Suspends execution of the calling thread for at least the specified duration.
    /// @param[in] milliseconds Minimum duration to sleep in milliseconds.
    static void sleep(gp::UInt32 milliseconds) noexcept;

    /// @brief Yields the remainder of the calling thread's time slice to another ready thread on the current processor.
    static void yieldExecution() noexcept;

    /// @brief Issues an architecture-specific pause instruction (_mm_pause) for spin-wait loops.
    static void spinWait() noexcept;

    /// @brief Retrieves the OS thread identifier of the calling thread.
    /// @return The 64-bit identifier of the currently executing thread.
    [[nodiscard]] static ThreadId currentThreadId() noexcept;

    /// @brief Returns total logical processors present on the system.
    /// @return Total logical processor count (at least 1u).
    [[nodiscard]] static gp::UInt32 getHardwareConcurrency() noexcept;

    /// @brief Returns logical processors available to the current process (respecting OS affinity masks).
    /// @details Queries GetProcessAffinityMask and counts set bits, falling back to getHardwareConcurrency() on
    /// failure.
    /// @return Process-available logical processor count (at least 1u).
    [[nodiscard]] static gp::UInt32 getProcessConcurrency() noexcept;
};

}   // namespace gp::platform::windows
