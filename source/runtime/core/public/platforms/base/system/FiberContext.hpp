// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "platforms/base/Platform.hpp"                   // IWYU pragma: keep
#include "platforms/base/system/FiberContextDesc.hpp"    // IWYU pragma: export
#include "platforms/base/system/FiberContextEnums.hpp"   // IWYU pragma: export
#include "platforms/base/system/FiberContextTypes.hpp"   // IWYU pragma: export

#if GP_PLATFORM_WINDOWS
    #include "platforms/windows/system/WindowsFiberContext.hpp"
#elif GP_PLATFORM_LINUX
    #include "platforms/linux/system/LinuxFiberContext.hpp"
#elif GP_PLATFORM_MACOS
    #include "platforms/macos/system/MacOSFiberContext.hpp"
#elif GP_PLATFORM_APPLE
    #include "platforms/apple/system/AppleFiberContext.hpp"
#elif GP_PLATFORM_UNIX
    #include "platforms/unix/system/UnixFiberContext.hpp"
#else
    #include "platforms/generic/system/GenericFiberContext.hpp"
#endif

namespace gp::platform
{

#if GP_PLATFORM_WINDOWS
using FiberContext = gp::platform::windows::FiberContext;
#elif GP_PLATFORM_LINUX
using FiberContext = gp::platform::linux::FiberContext;
#elif GP_PLATFORM_MACOS
using FiberContext = gp::platform::macos::FiberContext;
#elif GP_PLATFORM_APPLE
using FiberContext = gp::platform::apple::FiberContext;
#elif GP_PLATFORM_UNIX
using FiberContext = gp::platform::unix::FiberContext;
#else
using FiberContext = gp::platform::generic::FiberContext;
#endif

/// @brief RAII scope guard that converts the calling OS hardware thread into a root `FiberContext` anchor on
///        construction and restores it back to a standard OS thread on destruction
///        ([ADR-0049](../../../../../../docs/adr/ADR-0049.md)).
class ScopedThreadFiberContext final
{
private:
    FiberContext m_anchorContext{};
    FiberContextResult m_result{ FiberContextResult::NotInitialized };

public:
    /// @brief Converts the calling OS thread into a thread-anchor `FiberContext`.
    /// @param[in] flags Register preservation flags (`FiberStackFlags::Default` preserves SIMD/FP state).
    explicit ScopedThreadFiberContext(const FiberStackFlags flags = FiberStackFlags::Default) noexcept
        : m_result(m_anchorContext.convertCurrentThread(flags))
    {}

    /// @brief Restores the calling OS thread from its thread-anchor `FiberContext` if conversion succeeded.
    ~ScopedThreadFiberContext() noexcept
    {
        if (m_anchorContext.isValid())
        {
            (void)m_anchorContext.restoreCurrentThread();
        }
    }

    ScopedThreadFiberContext(const ScopedThreadFiberContext&) = delete;
    ScopedThreadFiberContext& operator=(const ScopedThreadFiberContext&) = delete;
    ScopedThreadFiberContext(ScopedThreadFiberContext&&) = delete;
    ScopedThreadFiberContext& operator=(ScopedThreadFiberContext&&) = delete;

public:
    /// @brief Checks whether the calling thread was successfully converted into a thread-anchor `FiberContext`.
    /// @return True if `m_result == FiberContextResult::Success` and the anchor context is valid.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return m_result == FiberContextResult::Success && m_anchorContext.isValid();
    }

    /// @brief Retrieves the status code returned by `convertCurrentThread()`.
    /// @return Conversion `FiberContextResult`.
    [[nodiscard]] constexpr FiberContextResult getResult() const noexcept
    {
        return m_result;
    }

    /// @brief Retrieves a mutable reference to the underlying thread-anchor `FiberContext`.
    /// @return Reference to the anchor `FiberContext`.
    [[nodiscard]] constexpr FiberContext& getContext() noexcept
    {
        return m_anchorContext;
    }

    /// @brief Retrieves a const reference to the underlying thread-anchor `FiberContext`.
    /// @return Const reference to the anchor `FiberContext`.
    [[nodiscard]] constexpr const FiberContext& getContext() const noexcept
    {
        return m_anchorContext;
    }
};

}   // namespace gp::platform
