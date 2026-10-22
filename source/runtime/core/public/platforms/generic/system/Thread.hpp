// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::platform
{

using ThreadId = unsigned int;
using NativeThreadHandle = void*;
using ThreadEntryFunction = unsigned int (*)(void* userData);

/// @brief Hardware scheduling priority for engine threads.
enum class ThreadPriority : unsigned char
{
    Lowest,
    BelowNormal,
    Normal,
    AboveNormal,
    Highest,
    TimeCritical   // Reserved for real-time audio or frame-pacing threads
};

/// @brief Configuration parameters for spawning an OS thread.
struct ThreadDesc
{
    const char* name = "Unnamed Thread";
    ThreadEntryFunction entry = nullptr;
    void* userData = nullptr;
    unsigned long long stackSize = 0;          // 0 = OS default
    unsigned long long affinityMask = ~0ULL;   // Bitmask of allowed CPU cores (~0ULL = any core)
    ThreadPriority priority = ThreadPriority::Normal;
};

}   // namespace gp::platform

namespace gp::platform::generic
{

/// @brief Base OS thread layout and fallback contract (compile-time polymorphism, no vtable).
class Thread
{
protected:
    NativeThreadHandle m_handle{ nullptr };
    ThreadId m_threadId{ 0 };

public:
    /// @brief Default constructor and destructor.
    Thread() noexcept = default;
    ~Thread() noexcept = default;

    /// @brief Non-copyable
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

public:
    /// @brief Returns true if the thread is currently active and joinable.
    /// @note A thread is joinable if it has been started and has not yet been joined or detached.
    /// @return True if the thread is joinable, false otherwise.
    [[nodiscard]] bool isJoinable() const noexcept
    {
        return m_handle != nullptr;
    }

    /// @brief Returns the OS-level thread identifier.
    /// @note The thread ID is unique for the lifetime of the thread, but may be reused after the thread has terminated.
    /// @return The thread ID.
    [[nodiscard]] ThreadId getId() const noexcept
    {
        return m_threadId;
    }

    /// @brief Returns the underlying native OS handle (HANDLE on Windows, pthread_t on POSIX).
    /// @return The native thread handle.
    [[nodiscard]] NativeThreadHandle getNativeHandle() const noexcept
    {
        return m_handle;
    }
};

}   // namespace gp::platform::generic
