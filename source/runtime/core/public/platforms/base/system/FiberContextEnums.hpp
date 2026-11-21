// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::platform
{

/// @brief Explicit status codes for low-level platform fiber context operations.
enum class [[nodiscard]] FiberContextResult : gp::UInt8
{
    /// @brief Operation completed successfully.
    Success = 0,

    /// @brief One or more configuration arguments (e.g., null entry point) are invalid.
    InvalidArguments,

    /// @brief The FiberContext instance already owns an active or converted context.
    AlreadyInitialized,

    /// @brief The FiberContext instance (or target context) has not been initialized.
    NotInitialized,

    /// @brief The calling OS thread has already been converted into a thread-anchor fiber context.
    AlreadyThreadFiber,

    /// @brief The calling OS thread is not currently converted into a thread-anchor fiber context.
    NotAThreadFiber,

    /// @brief Virtual memory or OS fiber stack allocation failed.
    AllocationFailed,

    /// @brief Hardware guard page protection (`mprotect` / `VirtualProtect`) failed.
    GuardPageFailed,

    /// @brief Underlying OS fiber creation (`CreateFiberEx` or stack frame initialization) failed.
    CreationFailed,

    /// @brief Context switch failed because the source context is not the currently executing context on this thread.
    SwitchFailed,

    /// @brief Attempted to switch a fiber context directly to itself.
    SelfSwitchError,

    /// @brief Attempted to destroy a child fiber context while currently executing on its stack.
    CannotDestroyActiveContext,

    /// @brief Fiber contexts are not supported on the current target platform.
    UnsupportedPlatform
};

/// @brief Bitmask flags controlling stack protection and floating-point register preservation for fiber contexts.
enum class FiberStackFlags : gp::UInt8
{
    /// @brief No special flags; uses minimal stack and register configuration.
    None = 0x00,

    /// @brief Reserves an inaccessible hardware guard page at the stack floor to trap stack overflows immediately.
    EnableGuardPage = 0x01,

    /// @brief Saves and restores extended non-volatile floating-point/SIMD state (`FIBER_FLAG_FLOAT_SWITCH` on Win32,
    ///        `MXCSR`/`FPU CW` on x86_64, `d8-d15`/`FPCR`/`FPSR` on ARM64).
    PreserveFloatingPointState = 0x02,

    /// @brief Standard production configuration enabling both hardware guard pages and full floating-point state save.
    Default = EnableGuardPage | PreserveFloatingPointState
};

}   // namespace gp::platform

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::platform::FiberStackFlags);
