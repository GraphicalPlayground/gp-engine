// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/unix/system/UnixThread.hpp"

namespace gp::platform::linux
{

/// @brief Linux-specific OS hardware thread implementation.
/// @details Specializes gp::platform::unix::Thread with Linux kernel TIDs (SYS_gettid),
///          cpu_set_t core affinity masks (pthread_setaffinity_np / sched_getaffinity),
///          16-byte TASK_COMM_LEN thread naming, and per-thread kernel nice/SCHED_FIFO priorities.
class GP_CORE_API Thread : public gp::platform::unix::Thread
{
public:
    /// @brief Constructs an empty, non-joinable Linux thread object.
    [[nodiscard]] constexpr Thread() noexcept = default;

    /// @brief Destroys the Linux thread object.
    ~Thread() noexcept = default;

    /// @brief Non-copyable.
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    /// @brief Move constructor.
    /// @param[in,out] other Source thread object to move from.
    [[nodiscard]] Thread(Thread&& other) noexcept = default;

    /// @brief Move assignment operator.
    /// @param[in,out] other Source thread object to move from.
    /// @return Reference to this thread instance.
    Thread& operator=(Thread&& other) noexcept = default;

public:
    /// @brief Spawns a Linux thread with kernel TID resolution, cpu_set_t affinity, and nice/SCHED_FIFO priority.
    /// @param[in] desc Configuration parameters for spawning the thread.
    /// @return ThreadResult::Success on success, or a specific error code detailing the failure stage.
    [[nodiscard]] ThreadResult start(const ThreadDesc& desc) noexcept;

    /// @brief Binds the thread to the logical CPU cores specified in affinityMask via pthread_setaffinity_np.
    /// @param[in] affinityMask Non-zero 64-bit bitmask of allowed logical CPU cores.
    /// @return ThreadResult::Success if updated, ThreadResult::NotJoinable if inactive,
    ///         ThreadResult::InvalidArguments if mask is 0 or kNoAffinityMask, or ThreadResult::AffinityFailed on
    ///         error.
    [[nodiscard]] ThreadResult setAffinity(gp::UInt64 affinityMask) noexcept;

    /// @brief Sets the Linux scheduling priority via per-thread nice levels (Lowest..Highest) or SCHED_FIFO
    /// (TimeCritical).
    /// @param[in] priority Target scheduling priority level.
    /// @return ThreadResult::Success if updated, ThreadResult::NotJoinable if inactive,
    ///         ThreadResult::InvalidArguments if priority is out of range, or ThreadResult::PriorityFailed on error.
    [[nodiscard]] ThreadResult setPriority(ThreadPriority priority) noexcept;

    /// @brief Retrieves the Linux kernel thread ID (SYS_gettid) of the calling thread, cached in TLS.
    /// @return The 64-bit Linux kernel TID of the currently executing thread.
    [[nodiscard]] static ThreadId currentThreadId() noexcept;

    /// @brief Queries the number of logical CPU cores available to the current process via sched_getaffinity.
    /// @return Process-available logical processor count (at least 1u).
    [[nodiscard]] static gp::UInt32 getProcessConcurrency() noexcept;
};

}   // namespace gp::platform::linux
