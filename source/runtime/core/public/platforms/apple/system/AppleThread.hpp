// File: source/runtime/core/public/platforms/apple/system/AppleThread.hpp
// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/unix/system/UnixThread.hpp"

namespace gp::platform::apple
{

/// @brief Shared Apple (Darwin / XNU / Mach) hardware thread implementation for macOS, iOS, tvOS, and visionOS.
/// @details Specializes gp::platform::unix::Thread with Darwin Quality of Service (QoS) classes for Apple Silicon
///          Performance (P) and Efficiency (E) core routing, Mach THREAD_PRECEDENCE_POLICY runtime priority
///          adjustments, pthread_threadid_np kernel thread IDs, and sysctl logical CPU queries.
class GP_CORE_API Thread : public gp::platform::unix::Thread
{
public:
    /// @brief Constructs an empty, non-joinable Apple thread object.
    [[nodiscard]] constexpr Thread() noexcept = default;

    /// @brief Destroys the Apple thread object.
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
    /// @brief Spawns a Darwin/Mach thread configured with Apple QoS classes and Mach scheduling policies.
    /// @param[in] desc Configuration parameters for spawning the thread.
    /// @return ThreadResult::Success on success, or a specific error code detailing the failure stage.
    [[nodiscard]] ThreadResult start(const ThreadDesc& desc) noexcept;

    /// @brief Sets the Mach thread affinity tag (supported on x86_64 Darwin; returns UnsupportedPlatform on Apple
    /// Silicon).
    /// @param[in] affinityMask Non-zero 64-bit bitmask of allowed logical CPU cores.
    /// @return ThreadResult::Success if updated, ThreadResult::NotJoinable if inactive,
    ///         ThreadResult::InvalidArguments if mask is 0 or kNoAffinityMask,
    ///         ThreadResult::AffinityFailed if no set bit maps to a valid hardware core,
    ///         or ThreadResult::UnsupportedPlatform on Apple Silicon where XNU disables affinity tags.
    [[nodiscard]] ThreadResult setAffinity(gp::UInt64 affinityMask) noexcept;

    /// @brief Updates the thread's scheduling priority via Darwin QoS (if self) or Mach THREAD_PRECEDENCE_POLICY.
    /// @param[in] priority Target scheduling priority level.
    /// @return ThreadResult::Success if updated, ThreadResult::NotJoinable if inactive,
    ///         ThreadResult::InvalidArguments if priority is invalid, or ThreadResult::PriorityFailed on Mach error.
    [[nodiscard]] ThreadResult setPriority(ThreadPriority priority) noexcept;

    /// @brief Retrieves the 64-bit XNU kernel thread ID via pthread_threadid_np, cached in TLS.
    /// @return The 64-bit Darwin thread identifier of the calling thread.
    [[nodiscard]] static ThreadId currentThreadId() noexcept;

    /// @brief Queries the number of active logical CPU cores available to the process via
    /// sysctlbyname("hw.logicalcpu").
    /// @return Process-available logical processor count (at least 1u).
    [[nodiscard]] static gp::UInt32 getProcessConcurrency() noexcept;
};

}   // namespace gp::platform::apple
