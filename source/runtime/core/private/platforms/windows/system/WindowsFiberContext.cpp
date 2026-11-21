// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/windows/system/WindowsFiberContext.hpp"
#include "platforms/windows/WindowsHeaderWrapper.hpp"   // IWYU pragma: keep

namespace gp::platform::windows
{

namespace detail
{

/// @brief Thread-local handoff state populated immediately prior to `SwitchToFiber` for newly starting fibers.
/// @details Because `SwitchToFiber` transfers control synchronously on the same OS hardware thread, passing the entry
///          point and user data via thread-local storage avoids heap allocations and allows `FiberContext` instances
///          to be moved freely in memory before or after `create()`.
struct PendingFiberStart
{
    FiberContextEntryFunction entry{ nullptr };
    void* userData{ nullptr };
};

static thread_local PendingFiberStart tl_pendingStart{};
static thread_local void* tl_registeredAnchorHandle{ nullptr };

static GP_FORCEINLINE void compilerMemoryBarrier() noexcept
{
#if defined(__clang__) || defined(__GNUC__)
    __asm__ volatile("" ::: "memory");
#else
    _ReadWriteBarrier();
#endif
}

static GP_FORCENOINLINE void
    setPendingFiberStartTLS(const FiberContextEntryFunction entry, void* const userData) noexcept
{
    compilerMemoryBarrier();
    tl_pendingStart.entry = entry;
    tl_pendingStart.userData = userData;
    compilerMemoryBarrier();
}

[[nodiscard]] static GP_FORCENOINLINE PendingFiberStart consumePendingFiberStartTLS() noexcept
{
    compilerMemoryBarrier();
    const PendingFiberStart startInfo = tl_pendingStart;
    tl_pendingStart.entry = nullptr;
    tl_pendingStart.userData = nullptr;
    compilerMemoryBarrier();
    return startInfo;
}

[[nodiscard]] static GP_FORCENOINLINE void* getRegisteredThreadAnchorTLS() noexcept
{
    compilerMemoryBarrier();
    return tl_registeredAnchorHandle;
}

static GP_FORCENOINLINE void setRegisteredThreadAnchorTLS(void* const handle) noexcept
{
    compilerMemoryBarrier();
    tl_registeredAnchorHandle = handle;
    compilerMemoryBarrier();
}

/// @brief Win32 `LPFIBER_START_ROUTINE` entry trampoline.
/// @param[in] unusedParameter Unused `lpParameter` (startup arguments are latched from `consumePendingFiberStartTLS`).
static void __stdcall win32FiberTrampoline([[maybe_unused]] void* const unusedParameter) noexcept
{
    const PendingFiberStart startInfo = consumePendingFiberStartTLS();
    const FiberContextEntryFunction entry = startInfo.entry;
    void* const userData = startInfo.userData;

    GP_ASSERT(entry != nullptr, "Win32 FiberContext trampoline entered with a null entry function.");
    if (entry != nullptr) [[likely]]
    {
        entry(userData);
    }

    // A raw FiberContext entry function must never return without switching back to a caller context.
    GP_ASSERT(
        false,
        "gp::platform::windows::FiberContext entry function returned to the bottom of the fiber stack without " "switching back to a caller or thread-anchor context."
    );
}

}   // namespace detail

FiberContext::~FiberContext() noexcept
{
    if (isValid())
    {
        (void)destroy();
    }
}

FiberContext::FiberContext(FiberContext&& other) noexcept
    : m_handle(other.m_handle)
    , m_entry(other.m_entry)
    , m_userData(other.m_userData)
    , m_stackSize(other.m_stackSize)
    , m_isThreadAnchor(other.m_isThreadAnchor)
    , m_ownsThreadConversion(other.m_ownsThreadConversion)
{
    other.m_handle = nullptr;
    other.m_entry = nullptr;
    other.m_userData = nullptr;
    other.m_stackSize = 0u;
    other.m_isThreadAnchor = false;
    other.m_ownsThreadConversion = false;
}

FiberContext& FiberContext::operator=(FiberContext&& other) noexcept
{
    if (this != &other)
    {
        if (isValid())
        {
            (void)destroy();
        }

        m_handle = other.m_handle;
        m_entry = other.m_entry;
        m_userData = other.m_userData;
        m_stackSize = other.m_stackSize;
        m_isThreadAnchor = other.m_isThreadAnchor;
        m_ownsThreadConversion = other.m_ownsThreadConversion;

        other.m_handle = nullptr;
        other.m_entry = nullptr;
        other.m_userData = nullptr;
        other.m_stackSize = 0u;
        other.m_isThreadAnchor = false;
        other.m_ownsThreadConversion = false;
    }

    return *this;
}

FiberContextResult FiberContext::create(const FiberContextDesc& desc) noexcept
{
    if (isValid())
    {
        return FiberContextResult::AlreadyInitialized;
    }

    if (desc.entry == nullptr || desc.stackReserveSize > kMaxFiberStackSize)
    {
        return FiberContextResult::InvalidArguments;
    }

    const gp::USize requestedReserve = (desc.stackReserveSize == 0u) ? kDefaultFiberStackSize : desc.stackReserveSize;
    if (desc.stackCommitSize > requestedReserve)
    {
        return FiberContextResult::InvalidArguments;
    }

    const gp::USize clampedReserve = (requestedReserve < kMinFiberStackSize) ? kMinFiberStackSize : requestedReserve;
    const gp::USize alignedReserve = (clampedReserve + (kFiberStackAlignment - 1u)) & ~(kFiberStackAlignment - 1u);
    const gp::USize rawCommitSize = (desc.stackCommitSize == 0u) ? alignedReserve : desc.stackCommitSize;
    const gp::USize commitSize = (rawCommitSize < kMinFiberStackSize) ? kMinFiberStackSize : rawCommitSize;

    const DWORD winFlags = ((desc.flags & FiberStackFlags::PreserveFloatingPointState) != FiberStackFlags::None)
                               ? FIBER_FLAG_FLOAT_SWITCH
                               : 0u;

    void* const fiberHandle = ::CreateFiberEx(
        static_cast<SIZE_T>(commitSize),
        static_cast<SIZE_T>(alignedReserve),
        winFlags,
        &detail::win32FiberTrampoline,
        nullptr
    );

    if (fiberHandle == nullptr)
    {
        return FiberContextResult::CreationFailed;
    }

    m_handle = fiberHandle;
    m_entry = desc.entry;
    m_userData = desc.userData;
    m_stackSize = alignedReserve;
    m_isThreadAnchor = false;
    m_ownsThreadConversion = false;

    return FiberContextResult::Success;
}

FiberContextResult FiberContext::destroy() noexcept
{
    if (!isValid())
    {
        return FiberContextResult::NotInitialized;
    }

    if (m_isThreadAnchor)
    {
        return restoreCurrentThread();
    }

    // Never invoke ::DeleteFiber on the currently executing fiber, as Win32 immediately calls ::ExitThread().
    if (isCurrentlyExecuting())
    {
        return FiberContextResult::CannotDestroyActiveContext;
    }

    ::DeleteFiber(m_handle);
    m_handle = nullptr;
    m_entry = nullptr;
    m_userData = nullptr;
    m_stackSize = 0u;
    m_isThreadAnchor = false;
    m_ownsThreadConversion = false;

    return FiberContextResult::Success;
}

FiberContextResult FiberContext::convertCurrentThread(const FiberStackFlags flags) noexcept
{
    if (isValid())
    {
        return FiberContextResult::AlreadyInitialized;
    }

    if (detail::getRegisteredThreadAnchorTLS() != nullptr)
    {
        return FiberContextResult::AlreadyThreadFiber;
    }

    if (::IsThreadAFiber() != FALSE)
    {
        void* const existingFiber = ::GetCurrentFiber();
        if (existingFiber == nullptr)
        {
            return FiberContextResult::CreationFailed;
        }

        m_handle = existingFiber;
        m_entry = nullptr;
        m_userData = nullptr;
        m_stackSize = 0u;
        m_isThreadAnchor = true;
        m_ownsThreadConversion = false;
        detail::setRegisteredThreadAnchorTLS(existingFiber);
        return FiberContextResult::Success;
    }

    const DWORD winFlags =
        ((flags & FiberStackFlags::PreserveFloatingPointState) != FiberStackFlags::None) ? FIBER_FLAG_FLOAT_SWITCH : 0u;

    void* const convertedHandle = ::ConvertThreadToFiberEx(nullptr, winFlags);
    if (convertedHandle == nullptr)
    {
        return FiberContextResult::CreationFailed;
    }

    m_handle = convertedHandle;
    m_entry = nullptr;
    m_userData = nullptr;
    m_stackSize = 0u;
    m_isThreadAnchor = true;
    m_ownsThreadConversion = true;
    detail::setRegisteredThreadAnchorTLS(convertedHandle);

    return FiberContextResult::Success;
}

FiberContextResult FiberContext::restoreCurrentThread() noexcept
{
    if (!isValid())
    {
        return FiberContextResult::NotInitialized;
    }

    if (!m_isThreadAnchor)
    {
        return FiberContextResult::NotAThreadFiber;
    }

    if (!isCurrentlyExecuting())
    {
        return FiberContextResult::CannotDestroyActiveContext;
    }

    if (m_ownsThreadConversion)
    {
        if (::ConvertFiberToThread() == FALSE)
        {
            return FiberContextResult::SwitchFailed;
        }
    }

    if (detail::getRegisteredThreadAnchorTLS() == m_handle)
    {
        detail::setRegisteredThreadAnchorTLS(nullptr);
    }

    m_handle = nullptr;
    m_entry = nullptr;
    m_userData = nullptr;
    m_stackSize = 0u;
    m_isThreadAnchor = false;
    m_ownsThreadConversion = false;

    return FiberContextResult::Success;
}

FiberContextResult FiberContext::switchTo(FiberContext& target) noexcept
{
    if (!isValid() || !target.isValid())
    {
        return FiberContextResult::NotInitialized;
    }

    if (this == &target || m_handle == target.m_handle)
    {
        return FiberContextResult::SelfSwitchError;
    }

    if (!isCurrentlyExecuting())
    {
        return FiberContextResult::SwitchFailed;
    }

    if (!target.m_isThreadAnchor)
    {
        detail::setPendingFiberStartTLS(target.m_entry, target.m_userData);
    }

    ::SwitchToFiber(target.m_handle);

    return FiberContextResult::Success;
}

bool FiberContext::isCurrentlyExecuting() const noexcept
{
    if (!isValid() || ::IsThreadAFiber() == FALSE)
    {
        return false;
    }

    return ::GetCurrentFiber() == m_handle;
}

bool FiberContext::isCurrentThreadConverted() noexcept
{
    return detail::getRegisteredThreadAnchorTLS() != nullptr || (::IsThreadAFiber() != FALSE);
}

}   // namespace gp::platform::windows
