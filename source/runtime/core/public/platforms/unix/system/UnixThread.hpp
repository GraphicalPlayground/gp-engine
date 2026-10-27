// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/generic/system/GenericThread.hpp"

namespace gp::platform::unix
{

/// @brief Shared POSIX (pthreads) implementation of an OS hardware thread.
/// @details Emulates Win32 CREATE_SUSPENDED semantics using a stack-allocated two-way mutex/condvar
///          barrier during start(), allowing CPU affinity and scheduling priority to be configured
///          and validated before the child thread executes user code.
/// @note Lifecycle Policy: Callers must explicitly join() or detach() before destruction.
///       Destroying a joinable thread triggers a debug trap in debug builds and performs a fallback
///       join() when called from a thread other than this one.
class GP_CORE_API Thread : public gp::platform::generic::Thread
{
public:
    /// @brief Opaque representation of a POSIX pthread_t handle to avoid leaking <pthread.h> into public headers.
    using NativeHandle = void*;

protected:
    NativeHandle m_handle = nullptr;
    ThreadId m_threadId = 0ULL;
    bool m_isJoinable = false;

public:
    /// @brief Constructs an empty, non-joinable POSIX thread object.
    [[nodiscard]] constexpr Thread() noexcept = default;

    /// @brief Destroys the POSIX thread object.
    /// @note Triggers a debug break in debug builds if still joinable, and performs a fallback join()
    ///       if invoked from a different thread.
    ~Thread() noexcept;

    /// @brief Non-copyable to enforce unique ownership of the underlying pthread_t resource.
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    /// @brief Move constructor transferring ownership of the POSIX thread handle and ID.
    /// @param[in,out] other Source thread object to move from (left in a non-joinable state).
    [[nodiscard]] Thread(Thread&& other) noexcept;

    /// @brief Move assignment operator transferring ownership of the POSIX thread handle and ID.
    /// @param[in,out] other Source thread object to move from (left in a non-joinable state).
    /// @return Reference to this thread instance.
    Thread& operator=(Thread&& other) noexcept;

public:
    /// @brief Spawns a POSIX thread and blocks until startup parameters and scheduling policies are applied.
    /// @param[in] desc Configuration parameters for spawning the thread.
    /// @return ThreadResult::Success on success, or a specific error code detailing the failure stage.
    [[nodiscard]] ThreadResult start(const ThreadDesc& desc) noexcept;

    /// @brief Blocks the calling thread until this thread finishes execution and reclaims POSIX resources.
    /// @param[out] outExitCode Optional pointer to receive the 32-bit exit code returned by the thread entry function.
    /// @return ThreadResult::Success on completion, ThreadResult::NotJoinable if inactive,
    ///         ThreadResult::SelfJoinError if called from this thread, or ThreadResult::WaitFailed on pthread_join
    ///         error.
    [[nodiscard]] ThreadResult join(gp::UInt32* outExitCode = nullptr) noexcept;

    /// @brief Detaches the underlying POSIX thread so OS resources are reclaimed automatically upon exit.
    /// @return ThreadResult::Success if detached, or ThreadResult::NotJoinable if inactive.
    [[nodiscard]] ThreadResult detach() noexcept;

    /// @brief Sets the CPU core affinity mask for the thread (unsupported on generic POSIX without OS extensions).
    /// @param[in] affinityMask Non-zero bitmask of allowed logical processors.
    /// @return ThreadResult::NotJoinable if inactive, ThreadResult::InvalidArguments if mask is 0 or kNoAffinityMask,
    ///         or ThreadResult::UnsupportedPlatform on generic POSIX targets.
    [[nodiscard]] ThreadResult setAffinity(gp::UInt64 affinityMask) noexcept;

    /// @brief Sets the scheduling priority of the thread using standard POSIX pthread_setschedparam.
    /// @param[in] priority Desired hardware scheduling priority.
    /// @return ThreadResult::Success if updated, ThreadResult::NotJoinable if inactive,
    ///         or ThreadResult::PriorityFailed if the OS rejected the scheduling parameter.
    [[nodiscard]] ThreadResult setPriority(ThreadPriority priority) noexcept;

    /// @brief Checks whether this instance owns an active, joinable POSIX thread.
    /// @return True if joinable, false otherwise.
    [[nodiscard]] constexpr bool isJoinable() const noexcept
    {
        return m_isJoinable;
    }

    /// @brief Retrieves the 64-bit OS thread identifier associated with this instance.
    /// @return The 64-bit thread identifier, or 0ULL if not joinable.
    [[nodiscard]] constexpr ThreadId getId() const noexcept
    {
        return m_threadId;
    }

    /// @brief Retrieves the underlying pthread_t cast to an opaque NativeHandle.
    /// @return Opaque native handle, or nullptr if not joinable.
    [[nodiscard]] constexpr NativeHandle getNativeHandle() const noexcept
    {
        return m_isJoinable ? m_handle : nullptr;
    }

    /// @brief Suspends the calling thread for at least the specified duration using an EINTR-safe nanosleep loop.
    /// @param[in] milliseconds Minimum duration to sleep in milliseconds.
    static void sleep(gp::UInt32 milliseconds) noexcept;

    /// @brief Yields the remainder of the calling thread's time slice via POSIX sched_yield().
    static void yieldExecution() noexcept;

    /// @brief Issues an architecture-specific processor pause/yield hint for spin-wait loops.
    static void spinWait() noexcept;

    /// @brief Retrieves the 64-bit OS thread identifier of the calling thread.
    /// @return The 64-bit identifier of the currently executing thread.
    [[nodiscard]] static ThreadId currentThreadId() noexcept;

    /// @brief Queries the total number of online logical CPU cores via sysconf(_SC_NPROCESSORS_ONLN).
    /// @return Total logical processor count (at least 1u).
    [[nodiscard]] static gp::UInt32 getHardwareConcurrency() noexcept;

    /// @brief Queries the number of logical CPU cores available to the current process.
    /// @return Process-available logical processor count (at least 1u).
    [[nodiscard]] static gp::UInt32 getProcessConcurrency() noexcept;

protected:
    /// @brief Platform customization callbacks injected by derived OS classes (LinuxThread, AppleThread) during
    /// start().
    struct StartupHooks
    {
        /// @brief Optional pre-creation hook to configure platform-specific pthread_attr_t properties (e.g. Darwin
        /// QoS).
        ThreadResult (*configureAttributes)(void* attrPtr, const ThreadDesc& desc) noexcept = nullptr;

        /// @brief Child-side hook executed inside the trampoline to assign the debugger thread name and query ThreadId.
        ThreadId (*initializeChildThread)(const char* name) noexcept = nullptr;

        /// @brief Parent-side hook executed while the child is parked on the startup barrier to bind CPU affinity.
        ThreadResult (*applyAffinity)(Thread& thread, gp::UInt64 affinityMask) noexcept = nullptr;

        /// @brief Parent-side hook executed while the child is parked on the startup barrier to bind scheduling
        /// priority.
        ThreadResult (*applyPriority)(Thread& thread, ThreadPriority priority) noexcept = nullptr;
    };

    /// @brief Executes the two-way condition-variable startup handoff using the provided platform hooks.
    /// @param[in] desc Thread creation descriptor.
    /// @param[in] hooks Platform-specific hooks for attribute setup, naming, affinity, and priority.
    /// @return ThreadResult::Success on success, or the specific failure code.
    [[nodiscard]] ThreadResult startWithHooks(const ThreadDesc& desc, const StartupHooks& hooks) noexcept;
};

}   // namespace gp::platform::unix
