// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/generic/system/Thread.hpp"

namespace gp::platform::windows
{

/// @brief Win32 implementation of an OS hardware thread.
class Thread : public gp::platform::generic::Thread
{
public:
    /// @brief Default constructor.
    Thread() noexcept = default;

    /// @brief Destructor that ensures the thread is joined or detached before destruction.
    ~Thread() noexcept;

    /// @brief Non-copyable
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    /// @brief Move constructor for transferring ownership of the OS thread handle.
    /// @param[in] other The thread to move from.
    Thread(Thread&& other) noexcept;

    /// @brief Move assignment operator for transferring ownership of the OS thread handle.
    /// @param[in] other The thread to move from.
    Thread& operator=(Thread&& other) noexcept;

public:
    /// @brief Spawns the OS thread immediately using the provided configuration.
    [[nodiscard]] bool start(const ThreadDesc& desc) noexcept;

    /// @brief Blocks the calling thread until this thread finishes execution.
    /// @param[in] outExitCode Optional pointer to receive the thread's return code.
    void join(gp::UInt32* outExitCode = nullptr) noexcept;

    /// @brief Detaches the OS handle so the thread cleans up automatically upon exit.
    void detach() noexcept;

    /// @brief Updates the CPU core affinity bitmask of a running thread.
    /// @param[in] affinityMask Bitmask of allowed CPU cores (~0ULL = any core).
    /// @return True if the affinity was successfully updated, false otherwise.
    bool setAffinity(gp::UInt64 affinityMask) noexcept;

    /// @brief Updates the scheduling priority of a running thread.
    /// @param[in] priority The new scheduling priority to set.
    /// @return True if the priority was successfully updated, false otherwise.
    bool setPriority(ThreadPriority priority) noexcept;

    /// @brief Suspends the calling thread for at least the specified milliseconds.
    /// @param[in] milliseconds The minimum number of milliseconds to sleep.
    static void sleep(gp::UInt32 milliseconds) noexcept;

    /// @brief Yields execution of the current time slice to another ready thread.
    static void yieldExecution() noexcept;

    /// @brief Executes a CPU pause instruction (e.g., _mm_pause) for spin-wait loops.
    static void spinWait() noexcept;

    /// @brief Returns the OS thread ID of the calling thread.
    /// @return The thread ID of the calling thread.
    [[nodiscard]] static ThreadId currentThreadId() noexcept;

    /// @brief Returns the number of logical CPU cores available on the system.
    /// @return The number of logical CPU cores.
    [[nodiscard]] static gp::UInt32 getHardwareConcurrency() noexcept;
};

}   // namespace gp::platform::windows
