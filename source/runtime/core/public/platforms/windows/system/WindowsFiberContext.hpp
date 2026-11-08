// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "platforms/base/system/FiberContextDesc.hpp"
#include "platforms/base/system/FiberContextEnums.hpp"
#include "platforms/base/system/FiberContextTypes.hpp"
#include "platforms/generic/system/GenericFiberContext.hpp"

namespace gp::platform::windows
{

/// @brief Win32 implementation of a user-mode cooperative execution context.
/// @details Wraps Win32 Fiber primitives (`CreateFiberEx`, `ConvertThreadToFiberEx`, `ConvertFiberToThread`,
///          `SwitchToFiber`, and `DeleteFiber`) with `FIBER_FLAG_FLOAT_SWITCH` enabled by default.
///          Using the native Win32 Fiber subsystem on Windows guarantees 100% compatibility with Windows 11 Hardware-
///          Enforced Stack Protection (Intel CET / AMD Shadow Stacks), Win64 Thread Environment Block (`NT_TIB`) stack
///          bounds (`StackBase`, `StackLimit`, `DeallocationStack`), `_chkstk` probes, Structured Exception Handling
///          (SEH), and Visual Studio's Parallel Stacks debugger without leaking `<windows.h>` into public headers.
class GP_CORE_API FiberContext : public gp::platform::generic::FiberContext
{
private:
    NativeHandle m_handle{ nullptr };
    FiberContextEntryFunction m_entry{ nullptr };
    void* m_userData{ nullptr };
    gp::USize m_stackSize{ 0u };
    bool m_isThreadAnchor{ false };
    bool m_ownsThreadConversion{ false };

public:
    /// @brief Constructs an uninitialized Win32 fiber context.
    [[nodiscard]] constexpr FiberContext() noexcept = default;

    /// @brief Destroys the fiber context, releasing its Win32 fiber stack or restoring the thread anchor if owned.
    ~FiberContext() noexcept;

    FiberContext(const FiberContext&) = delete;
    FiberContext& operator=(const FiberContext&) = delete;

    /// @brief Move constructor transferring ownership of the underlying Win32 fiber handle.
    /// @param[in,out] other Source fiber context to move from (left in an uninitialized state).
    [[nodiscard]] FiberContext(FiberContext&& other) noexcept;

    /// @brief Move assignment operator transferring ownership of the underlying Win32 fiber handle.
    /// @param[in,out] other Source fiber context to move from (left in an uninitialized state).
    /// @return Reference to this fiber context instance.
    FiberContext& operator=(FiberContext&& other) noexcept;

public:
    /// @copydoc gp::platform::generic::FiberContext::create()
    /// @note Allocates a Win32 fiber stack via `CreateFiberEx` configured with `FIBER_FLAG_FLOAT_SWITCH` when
    ///       `FiberStackFlags::PreserveFloatingPointState` is set.
    [[nodiscard]] FiberContextResult create(const FiberContextDesc& desc) noexcept;

    /// @copydoc gp::platform::generic::FiberContext::destroy()
    /// @note Attempting to destroy a child fiber while currently executing on its stack returns
    ///       `FiberContextResult::CannotDestroyActiveContext` to prevent `DeleteFiber` from terminating the OS thread.
    [[nodiscard]] FiberContextResult destroy() noexcept;

    /// @copydoc gp::platform::generic::FiberContext::convertCurrentThread()
    /// @note Invokes `ConvertThreadToFiberEx` on the calling Win32 hardware thread, or adopts an existing Win32 fiber
    ///       if the host thread was already converted externally.
    [[nodiscard]] FiberContextResult
        convertCurrentThread(const FiberStackFlags flags = FiberStackFlags::Default) noexcept;

    /// @copydoc gp::platform::generic::FiberContext::restoreCurrentThread()
    /// @note Invokes `ConvertFiberToThread` if this instance performed the thread conversion.
    [[nodiscard]] FiberContextResult restoreCurrentThread() noexcept;

    /// @copydoc gp::platform::generic::FiberContext::switchTo()
    /// @note Invokes `SwitchToFiber(target.m_handle)`, atomically swapping the Win64 `NT_TIB` stack limits and shadow
    ///       stack pointer.
    [[nodiscard]] FiberContextResult switchTo(FiberContext& target) noexcept;

    /// @copydoc gp::platform::generic::FiberContext::isValid()
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return m_handle != nullptr;
    }

    /// @copydoc gp::platform::generic::FiberContext::isThreadAnchor()
    [[nodiscard]] constexpr bool isThreadAnchor() const noexcept
    {
        return m_isThreadAnchor;
    }

    /// @copydoc gp::platform::generic::FiberContext::isCurrentlyExecuting()
    /// @note Returns true when `IsThreadAFiber() != FALSE` and `GetCurrentFiber() == m_handle`.
    [[nodiscard]] bool isCurrentlyExecuting() const noexcept;

    /// @copydoc gp::platform::generic::FiberContext::getStackSize()
    [[nodiscard]] constexpr gp::USize getStackSize() const noexcept
    {
        return m_stackSize;
    }

    /// @copydoc gp::platform::generic::FiberContext::getNativeHandle()
    [[nodiscard]] constexpr NativeHandle getNativeHandle() const noexcept
    {
        return m_handle;
    }

    /// @copydoc gp::platform::generic::FiberContext::isCurrentThreadConverted()
    [[nodiscard]] static bool isCurrentThreadConverted() noexcept;
};

}   // namespace gp::platform::windows
