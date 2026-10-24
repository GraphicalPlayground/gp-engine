// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::platform
{

/// @brief 64-bit OS-level thread identifier type, sized to prevent truncation across Win32 and 64-bit POSIX targets.
using ThreadId = gp::UInt64;

/// @brief Function pointer signature for OS thread entry points.
/// @param[in] userData User-defined context pointer passed via ThreadDesc.
/// @return Thread exit status code.
using ThreadEntryFunction = gp::UInt32 (*)(void* userData);

/// @brief Explicit status codes for low-level OS thread operations.
enum class [[nodiscard]] ThreadResult : unsigned char
{
    Success = 0,
    InvalidArguments,
    AlreadyRunning,
    StackSizeOverflow,
    CreationFailed,
    AffinityFailed,
    PriorityFailed,
    ResumeFailed,
    NotJoinable,
    SelfJoinError,
    WaitFailed,
    UnsupportedPlatform
};

/// @brief Hardware scheduling priority for engine threads.
/// @note TimeCritical should be reserved strictly for real-time audio or hardware pacing threads;
///       misuse can starve OS kernel threads and cause priority inversion.
enum class ThreadPriority : unsigned char
{
    Lowest,
    BelowNormal,
    Normal,
    AboveNormal,
    Highest,
    TimeCritical
};

/// @brief Sentinel constant indicating the OS default thread stack size should be used.
inline constexpr gp::UInt64 kDefaultStackSize = 0ULL;

/// @brief Sentinel constant indicating no explicit CPU core affinity mask is applied.
inline constexpr gp::UInt64 kNoAffinityMask = ~0ULL;

/// @brief Configuration parameters for spawning an OS thread.
struct ThreadDesc
{
    /// @brief UTF-8 thread name surfaced to debuggers and profilers (truncated to 63 characters).
    const char* name = "GP_Worker";

    /// @brief Mandatory thread entry point function.
    ThreadEntryFunction entry = nullptr;

    /// @brief Opaque user context pointer forwarded to the thread entry function.
    void* userData = nullptr;

    /// @brief Stack size in bytes, or kDefaultStackSize to use the OS default.
    gp::UInt64 stackSize = kDefaultStackSize;

    /// @brief Bitmask of allowed logical CPU cores, or kNoAffinityMask for unrestricted scheduling.
    gp::UInt64 affinityMask = kNoAffinityMask;

    /// @brief Initial hardware scheduling priority for the thread.
    ThreadPriority priority = ThreadPriority::Normal;

    /// @brief If true, start() aborts and terminates the suspended thread if affinity or priority configuration fails.
    bool failOnConfigError = false;
};

}   // namespace gp::platform

namespace gp::platform::generic
{

/// @brief Stateless base contract and unsupported-platform fallback (zero vtable / EBO-friendly).
/// @details Provides the complete public Thread interface returning ThreadResult::UnsupportedPlatform
///          when compiled on a target without a dedicated OS thread implementation.
class Thread
{
public:
    /// @brief Constructs an unstarted, non-joinable thread object.
    [[nodiscard]] constexpr Thread() noexcept = default;

    /// @brief Destroys the stateless generic thread fallback.
    constexpr ~Thread() noexcept = default;

    /// @brief Non-copyable to enforce unique ownership of OS thread resources.
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    /// @brief Move constructor.
    [[nodiscard]] constexpr Thread(Thread&&) noexcept = default;

    /// @brief Move assignment operator.
    /// @return Reference to this thread instance.
    constexpr Thread& operator=(Thread&&) noexcept = default;

public:
    /// @brief Fallback thread creation entry point for unsupported platforms.
    /// @param[in] desc Configuration parameters for spawning the thread.
    /// @return Always returns ThreadResult::UnsupportedPlatform.
    [[nodiscard]] constexpr ThreadResult start([[maybe_unused]] const ThreadDesc& desc) noexcept
    {
        return ThreadResult::UnsupportedPlatform;
    }

    /// @brief Fallback thread join operation for unsupported platforms.
    /// @param[out] outExitCode Optional pointer to receive the thread's exit code.
    /// @return Always returns ThreadResult::UnsupportedPlatform.
    [[nodiscard]] constexpr ThreadResult join([[maybe_unused]] gp::UInt32* outExitCode = nullptr) noexcept
    {
        return ThreadResult::UnsupportedPlatform;
    }

    /// @brief Fallback thread detach operation for unsupported platforms.
    /// @return Always returns ThreadResult::UnsupportedPlatform.
    [[nodiscard]] constexpr ThreadResult detach() noexcept
    {
        return ThreadResult::UnsupportedPlatform;
    }

    /// @brief Fallback CPU affinity configuration for unsupported platforms.
    /// @param[in] affinityMask Bitmask of allowed logical CPU cores.
    /// @return Always returns ThreadResult::UnsupportedPlatform.
    [[nodiscard]] constexpr ThreadResult setAffinity([[maybe_unused]] gp::UInt64 affinityMask) noexcept
    {
        return ThreadResult::UnsupportedPlatform;
    }

    /// @brief Fallback scheduling priority configuration for unsupported platforms.
    /// @param[in] priority Desired hardware scheduling priority.
    /// @return Always returns ThreadResult::UnsupportedPlatform.
    [[nodiscard]] constexpr ThreadResult setPriority([[maybe_unused]] ThreadPriority priority) noexcept
    {
        return ThreadResult::UnsupportedPlatform;
    }

    /// @brief Checks whether this instance owns an active, joinable OS thread.
    /// @return Always returns false on unsupported platforms.
    [[nodiscard]] constexpr bool isJoinable() const noexcept
    {
        return false;
    }

    /// @brief Retrieves the OS thread identifier associated with this instance.
    /// @return Always returns 0ULL on unsupported platforms.
    [[nodiscard]] constexpr ThreadId getId() const noexcept
    {
        return 0ULL;
    }

    /// @brief Suspends the calling thread for at least the specified duration (no-op on unsupported platforms).
    /// @param[in] milliseconds Minimum duration to sleep in milliseconds.
    static constexpr void sleep([[maybe_unused]] gp::UInt32 milliseconds) noexcept
    {}

    /// @brief Yields the remainder of the calling thread's time slice to the OS scheduler (no-op on unsupported
    /// platforms).
    static constexpr void yieldExecution() noexcept
    {}

    /// @brief Issues a processor hint for spin-wait loops (no-op on unsupported platforms).
    static constexpr void spinWait() noexcept
    {}

    /// @brief Retrieves the OS thread identifier of the calling thread.
    /// @return Always returns 0ULL on unsupported platforms.
    [[nodiscard]] static constexpr ThreadId currentThreadId() noexcept
    {
        return 0ULL;
    }

    /// @brief Queries the total number of logical CPU cores present on the hardware.
    /// @return Always returns 1u on unsupported platforms.
    [[nodiscard]] static constexpr gp::UInt32 getHardwareConcurrency() noexcept
    {
        return 1u;
    }

    /// @brief Queries the number of logical CPU cores available to the current process.
    /// @return Always returns 1u on unsupported platforms.
    [[nodiscard]] static constexpr gp::UInt32 getProcessConcurrency() noexcept
    {
        return 1u;
    }
};

}   // namespace gp::platform::generic
