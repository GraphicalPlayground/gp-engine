// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "platforms/base/system/FiberContextDesc.hpp"
#include "platforms/base/system/FiberContextEnums.hpp"
#include "platforms/base/system/FiberContextTypes.hpp"
#include "platforms/generic/system/GenericFiberContext.hpp"

namespace gp::platform::unix
{

/// @brief POSIX (`Linux`, `macOS`, `Apple`) user-mode cooperative execution context.
/// @details Bypasses deprecated POSIX `<ucontext.h>` (`swapcontext` issues a `sigprocmask` kernel syscall on every
///          switch) in favor of a zero-syscall, ~15-instruction ABI register-frame switch (`System V AMD64` on `x86_64`
///          and `AAPCS64` on `ARM64`). Virtual stack memory is allocated via `mmap` with an optional `PROT_NONE`
///          hardware guard page at the stack floor (`mprotect`) to immediately trap stack overflows.
class GP_CORE_API FiberContext : public gp::platform::generic::FiberContext
{
private:
    NativeHandle m_stackPointer{ nullptr };
    void* m_allocationBase{ nullptr };
    gp::USize m_allocationSize{ 0u };
    gp::USize m_usableStackSize{ 0u };
    FiberContextEntryFunction m_entry{ nullptr };
    void* m_userData{ nullptr };
    gp::UInt64 m_contextId{ 0ULL };
    bool m_isThreadAnchor{ false };

public:
    /// @brief Constructs an uninitialized POSIX fiber context.
    [[nodiscard]] constexpr FiberContext() noexcept = default;

    /// @brief Destroys the POSIX fiber context and unmaps its virtual stack (or restores the thread anchor).
    ~FiberContext() noexcept;

    FiberContext(const FiberContext&) = delete;
    FiberContext& operator=(const FiberContext&) = delete;

    /// @brief Move constructor transferring ownership of the virtual stack and saved stack pointer.
    /// @param[in,out] other Source fiber context to move from (left in an uninitialized state).
    [[nodiscard]] FiberContext(FiberContext&& other) noexcept;

    /// @brief Move assignment operator transferring ownership of the virtual stack and saved stack pointer.
    /// @param[in,out] other Source fiber context to move from (left in an uninitialized state).
    /// @return Reference to this fiber context instance.
    FiberContext& operator=(FiberContext&& other) noexcept;

public:
    /// @copydoc gp::platform::generic::FiberContext::create()
    /// @note Maps a virtual stack via `mmap`, applies an optional `PROT_NONE` guard page (`mprotect`), and initializes
    ///       the initial `System V AMD64` or `AAPCS64` callee-saved register frame at the top of the stack.
    [[nodiscard]] FiberContextResult create(const FiberContextDesc& desc) noexcept;

    /// @copydoc gp::platform::generic::FiberContext::destroy()
    /// @note Unmaps the virtual stack (`munmap`) or restores a converted thread-anchor context.
    [[nodiscard]] FiberContextResult destroy() noexcept;

    /// @copydoc gp::platform::generic::FiberContext::convertCurrentThread()
    /// @note Registers a unique thread-anchor context ID on the calling POSIX thread capable of saving its `RSP`/`SP`.
    [[nodiscard]] FiberContextResult
        convertCurrentThread(const FiberStackFlags flags = FiberStackFlags::Default) noexcept;

    /// @copydoc gp::platform::generic::FiberContext::restoreCurrentThread()
    [[nodiscard]] FiberContextResult restoreCurrentThread() noexcept;

    /// @copydoc gp::platform::generic::FiberContext::switchTo()
    /// @note Saves callee-saved general-purpose and SIMD/FP control registers onto the current stack and switches
    ///       `RSP`/`SP` to `target` in user space with zero kernel syscalls.
    [[nodiscard]] FiberContextResult switchTo(FiberContext& target) noexcept;

    /// @copydoc gp::platform::generic::FiberContext::isValid()
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return m_contextId != 0ULL;
    }

    /// @copydoc gp::platform::generic::FiberContext::isThreadAnchor()
    [[nodiscard]] constexpr bool isThreadAnchor() const noexcept
    {
        return m_isThreadAnchor;
    }

    /// @copydoc gp::platform::generic::FiberContext::isCurrentlyExecuting()
    [[nodiscard]] bool isCurrentlyExecuting() const noexcept;

    /// @copydoc gp::platform::generic::FiberContext::getStackSize()
    [[nodiscard]] constexpr gp::USize getStackSize() const noexcept
    {
        return m_usableStackSize;
    }

    /// @copydoc gp::platform::generic::FiberContext::getNativeHandle()
    [[nodiscard]] constexpr NativeHandle getNativeHandle() const noexcept
    {
        return m_stackPointer;
    }

    /// @copydoc gp::platform::generic::FiberContext::isCurrentThreadConverted()
    [[nodiscard]] static bool isCurrentThreadConverted() noexcept;
};

}   // namespace gp::platform::unix
