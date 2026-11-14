// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "platforms/base/system/FiberContextDesc.hpp"
#include "platforms/base/system/FiberContextEnums.hpp"
#include "platforms/base/system/FiberContextTypes.hpp"   // IWYU pragma: export

namespace gp::platform::generic
{

/// @brief Stateless base contract and unsupported-platform fallback for user-mode execution contexts.
/// @details Provides the complete public `FiberContext` interface returning `FiberContextResult::UnsupportedPlatform`
///          when compiled on a target without a dedicated OS or CPU-architecture context implementation.
class FiberContext
{
public:
    /// @brief Opaque representation of an OS or ABI execution context handle.
    using NativeHandle = void*;

public:
    /// @brief Constructs an uninitialized fiber context.
    [[nodiscard]] constexpr FiberContext() noexcept = default;

    /// @brief Destroys the generic fiber context fallback.
    constexpr ~FiberContext() noexcept = default;

    FiberContext(const FiberContext&) = delete;
    FiberContext& operator=(const FiberContext&) = delete;

    /// @brief Move constructor.
    [[nodiscard]] constexpr FiberContext(FiberContext&&) noexcept = default;

    /// @brief Move assignment operator.
    /// @return Reference to this fiber context instance.
    constexpr FiberContext& operator=(FiberContext&&) noexcept = default;

public:
    /// @brief Allocates a stack and initializes a new execution context ready to enter `desc.entry(desc.userData)`.
    /// @param[in] desc Configuration parameters for the execution context.
    /// @return Always returns `FiberContextResult::UnsupportedPlatform` on generic fallback targets.
    [[nodiscard]] constexpr FiberContextResult create([[maybe_unused]] const FiberContextDesc& desc) noexcept
    {
        return FiberContextResult::UnsupportedPlatform;
    }

    /// @brief Destroys the execution context and releases its virtual stack memory (or restores the thread anchor).
    /// @return Always returns `FiberContextResult::UnsupportedPlatform` on generic fallback targets.
    [[nodiscard]] constexpr FiberContextResult destroy() noexcept
    {
        return FiberContextResult::UnsupportedPlatform;
    }

    /// @brief Converts the currently executing OS hardware thread into a root thread-anchor fiber context.
    /// @param[in] flags Optional stack/register preservation flags (`PreserveFloatingPointState`).
    /// @return Always returns `FiberContextResult::UnsupportedPlatform` on generic fallback targets.
    [[nodiscard]] constexpr FiberContextResult
        convertCurrentThread([[maybe_unused]] const FiberStackFlags flags = FiberStackFlags::Default) noexcept
    {
        return FiberContextResult::UnsupportedPlatform;
    }

    /// @brief Restores a thread-anchor fiber context back to a normal OS thread.
    /// @return Always returns `FiberContextResult::UnsupportedPlatform` on generic fallback targets.
    [[nodiscard]] constexpr FiberContextResult restoreCurrentThread() noexcept
    {
        return FiberContextResult::UnsupportedPlatform;
    }

    /// @brief Saves the current CPU register state into this context and resumes execution on `target`.
    /// @param[in,out] target Destination execution context to switch into.
    /// @return Always returns `FiberContextResult::UnsupportedPlatform` on generic fallback targets.
    [[nodiscard]] constexpr FiberContextResult switchTo([[maybe_unused]] FiberContext& target) noexcept
    {
        return FiberContextResult::UnsupportedPlatform;
    }

    /// @brief Checks whether this instance owns an initialized child context or converted thread anchor.
    /// @return Always returns `false` on generic fallback targets.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return false;
    }

    /// @brief Checks whether this instance represents a converted OS thread anchor rather than an allocated stack.
    /// @return Always returns `false` on generic fallback targets.
    [[nodiscard]] constexpr bool isThreadAnchor() const noexcept
    {
        return false;
    }

    /// @brief Checks whether this context is the actively executing context on the calling OS thread.
    /// @return Always returns `false` on generic fallback targets.
    [[nodiscard]] constexpr bool isCurrentlyExecuting() const noexcept
    {
        return false;
    }

    /// @brief Queries the usable stack size in bytes reserved for this context (0 for thread anchors).
    /// @return Always returns `0u` on generic fallback targets.
    [[nodiscard]] constexpr gp::USize getStackSize() const noexcept
    {
        return 0u;
    }

    /// @brief Retrieves the underlying platform handle or stack pointer as an opaque `void*`.
    /// @return Always returns `nullptr` on generic fallback targets.
    [[nodiscard]] constexpr NativeHandle getNativeHandle() const noexcept
    {
        return nullptr;
    }

    /// @brief Queries whether the calling OS hardware thread has been converted into a thread-anchor fiber context.
    /// @return Always returns `false` on generic fallback targets.
    [[nodiscard]] static constexpr bool isCurrentThreadConverted() noexcept
    {
        return false;
    }
};

}   // namespace gp::platform::generic
