// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/fibers/Fiber.hpp"
#include "memory/allocators/FiberScratchAllocator.hpp"
#include <utility>

namespace gp::thread
{

namespace detail
{

static thread_local Fiber* tl_currentFiber{ nullptr };

static GP_FORCEINLINE void compilerMemoryBarrier() noexcept
{
#if defined(__clang__) || defined(__GNUC__)
    __asm__ volatile("" ::: "memory");
#else
    _ReadWriteBarrier();
#endif
}

/// @brief Reads the calling OS hardware thread's active `Fiber*` slot.
/// @note Marked `GP_FORCENOINLINE` with a compiler memory barrier so the compiler (`clang-cl` / MSVC `/O2` / LTO)
///       never hoists or caches the thread's TLS base pointer (`gs:[0x58]` / `fs:[0x0]`) in a callee-saved register
///       across a cooperative fiber switch and core migration.
[[nodiscard]] static GP_FORCENOINLINE Fiber* getCurrentFiberTLS() noexcept
{
    compilerMemoryBarrier();
    return tl_currentFiber;
}

/// @brief Updates the calling OS hardware thread's active `Fiber*` slot.
/// @note Marked `GP_FORCENOINLINE` with a compiler memory barrier to prevent TLS address caching across fiber context
///       switches.
static GP_FORCENOINLINE void setCurrentFiberTLS(Fiber* const fiber) noexcept
{
    compilerMemoryBarrier();
    tl_currentFiber = fiber;
    compilerMemoryBarrier();
}

[[nodiscard]] static constexpr FiberResult mapContextError(const gp::platform::FiberContextResult contextRes) noexcept
{
    switch (contextRes)
    {
    case gp::platform::FiberContextResult::Success:
        return FiberResult::Success;
    case gp::platform::FiberContextResult::InvalidArguments:
        return FiberResult::InvalidArguments;
    case gp::platform::FiberContextResult::AlreadyInitialized:
        return FiberResult::AlreadyInitialized;
    case gp::platform::FiberContextResult::NotInitialized:
        return FiberResult::NotInitialized;
    case gp::platform::FiberContextResult::AlreadyThreadFiber:
        return FiberResult::AlreadyThreadFiber;
    case gp::platform::FiberContextResult::NotAThreadFiber:
        return FiberResult::NotAThreadFiber;
    case gp::platform::FiberContextResult::SelfSwitchError:
        return FiberResult::SelfSwitchError;
    case gp::platform::FiberContextResult::CannotDestroyActiveContext:
        return FiberResult::CannotDestroyActiveFiber;
    case gp::platform::FiberContextResult::UnsupportedPlatform:
        return FiberResult::UnsupportedPlatform;
    case gp::platform::FiberContextResult::AllocationFailed:
        // falls through
    case gp::platform::FiberContextResult::GuardPageFailed:
        // falls through
    case gp::platform::FiberContextResult::CreationFailed:
        // falls through
    case gp::platform::FiberContextResult::SwitchFailed:
        // falls through
    default:
        return FiberResult::ContextError;
    }
}

}   // namespace detail

Fiber::~Fiber() noexcept
{
    if (isValid())
    {
        (void)destroy();
    }
}

Fiber::Fiber(Fiber&& other) noexcept
    : m_context(std::move(other.m_context))
    , m_scratchArena(std::move(other.m_scratchArena))
    , m_entry(other.m_entry)
    , m_userData(other.m_userData)
    , m_returnFiber(other.m_returnFiber)
    , m_name(other.m_name)
    , m_completedTaskCount(other.m_completedTaskCount)
    , m_id(other.m_id)
    , m_state(other.m_state)
    , m_stackTier(other.m_stackTier)
    , m_resetScratchOnCompletion(other.m_resetScratchOnCompletion)
{
    if (detail::getCurrentFiberTLS() == &other)
    {
        detail::setCurrentFiberTLS(this);
    }

    if (gp::memory::FiberScratchArena::getBoundFiberArena() == &other.m_scratchArena)
    {
        (void)gp::memory::FiberScratchArena::bindFiberArena(&m_scratchArena);
    }

    other.m_entry = nullptr;
    other.m_userData = nullptr;
    other.m_returnFiber = nullptr;
    other.m_name = "gp::Fiber";
    other.m_completedTaskCount = 0ULL;
    other.m_id = kInvalidFiberId;
    other.m_state = FiberState::Uninitialized;
    other.m_stackTier = FiberStackTier::Standard64KiB;
    other.m_resetScratchOnCompletion = true;
}

Fiber& Fiber::operator=(Fiber&& other) noexcept
{
    if (this != &other)
    {
        if (isValid())
        {
            (void)destroy();
        }

        const bool wasCurrentFiber = (detail::getCurrentFiberTLS() == &other);
        const bool wasBoundArena = (gp::memory::FiberScratchArena::getBoundFiberArena() == &other.m_scratchArena);

        m_context = std::move(other.m_context);
        m_scratchArena = std::move(other.m_scratchArena);
        m_entry = other.m_entry;
        m_userData = other.m_userData;
        m_returnFiber = other.m_returnFiber;
        m_name = other.m_name;
        m_completedTaskCount = other.m_completedTaskCount;
        m_id = other.m_id;
        m_state = other.m_state;
        m_stackTier = other.m_stackTier;
        m_resetScratchOnCompletion = other.m_resetScratchOnCompletion;

        if (wasCurrentFiber)
        {
            detail::setCurrentFiberTLS(this);
        }

        if (wasBoundArena)
        {
            (void)gp::memory::FiberScratchArena::bindFiberArena(&m_scratchArena);
        }

        other.m_entry = nullptr;
        other.m_userData = nullptr;
        other.m_returnFiber = nullptr;
        other.m_name = "gp::Fiber";
        other.m_completedTaskCount = 0ULL;
        other.m_id = kInvalidFiberId;
        other.m_state = FiberState::Uninitialized;
        other.m_stackTier = FiberStackTier::Standard64KiB;
        other.m_resetScratchOnCompletion = true;
    }

    return *this;
}

FiberResult Fiber::create(const FiberDesc& desc) noexcept
{
    if (isValid())
    {
        return FiberResult::AlreadyInitialized;
    }

    if (desc.id == kInvalidFiberId)
    {
        return FiberResult::InvalidArguments;
    }

    if (desc.stackTier == FiberStackTier::Custom && desc.customStackSizeBytes > gp::platform::kMaxFiberStackSize)
    {
        return FiberResult::InvalidArguments;
    }

    const char* const resolvedName = (desc.name != nullptr && desc.name[0] != '\0') ? desc.name : "gp::Fiber";
    const gp::USize stackBytes = resolveStackTierBytes(desc.stackTier, desc.customStackSizeBytes);

    gp::platform::FiberStackFlags stackFlags = gp::platform::FiberStackFlags::PreserveFloatingPointState;
    if (desc.enableGuardPage)
    {
        stackFlags |= gp::platform::FiberStackFlags::EnableGuardPage;
    }

    gp::platform::FiberContextDesc contextDesc{};
    contextDesc.entry = &Fiber::fiberLoopTrampoline;
    contextDesc.userData = nullptr;
    contextDesc.stackReserveSize = stackBytes;
    contextDesc.stackCommitSize = stackBytes;
    contextDesc.flags = stackFlags;

    const gp::platform::FiberContextResult contextRes = m_context.create(contextDesc);
    if (contextRes != gp::platform::FiberContextResult::Success)
    {
        return detail::mapContextError(contextRes);
    }

    if (desc.scratchPagePool != nullptr)
    {
        (void)m_scratchArena.initialize(desc.scratchPagePool, nullptr, true, resolvedName);
    }
    else
    {
        m_scratchArena.shutdown();
    }

    m_entry = desc.entry;
    m_userData = (desc.entry != nullptr) ? desc.userData : nullptr;
    m_returnFiber = desc.returnFiber;
    m_name = resolvedName;
    m_completedTaskCount = 0ULL;
    m_id = desc.id;
    m_state = (desc.entry != nullptr) ? FiberState::Ready : FiberState::Idle;
    m_stackTier = desc.stackTier;
    m_resetScratchOnCompletion = desc.resetScratchOnTaskCompletion;

    return FiberResult::Success;
}

FiberResult Fiber::destroy() noexcept
{
    if (!isValid())
    {
        return FiberResult::NotInitialized;
    }

    if (m_state == FiberState::ThreadAnchor)
    {
        return restoreCurrentThread();
    }

    if (isCurrentlyExecuting())
    {
        return FiberResult::CannotDestroyActiveFiber;
    }

    if (m_state == FiberState::Running)
    {
        return FiberResult::BusyRunningOrSuspended;
    }

    if (gp::memory::FiberScratchArena::getBoundFiberArena() == &m_scratchArena)
    {
        (void)gp::memory::FiberScratchArena::bindFiberArena(nullptr);
    }

    m_scratchArena.shutdown();

    const gp::platform::FiberContextResult contextRes = m_context.destroy();
    if (contextRes != gp::platform::FiberContextResult::Success)
    {
        return detail::mapContextError(contextRes);
    }

    m_entry = nullptr;
    m_userData = nullptr;
    m_returnFiber = nullptr;
    m_name = "gp::Fiber";
    m_completedTaskCount = 0ULL;
    m_id = kInvalidFiberId;
    m_state = FiberState::Uninitialized;
    m_stackTier = FiberStackTier::Standard64KiB;
    m_resetScratchOnCompletion = true;

    return FiberResult::Success;
}

FiberResult Fiber::convertCurrentThread(const FiberId id, const char* const name) noexcept
{
    if (isValid())
    {
        return FiberResult::AlreadyInitialized;
    }

    if (id == kInvalidFiberId)
    {
        return FiberResult::InvalidArguments;
    }

    if (detail::getCurrentFiberTLS() != nullptr)
    {
        return FiberResult::AlreadyThreadFiber;
    }

    const gp::platform::FiberContextResult contextRes =
        m_context.convertCurrentThread(gp::platform::FiberStackFlags::Default);
    if (contextRes != gp::platform::FiberContextResult::Success)
    {
        return detail::mapContextError(contextRes);
    }

    m_scratchArena.shutdown();
    m_entry = nullptr;
    m_userData = nullptr;
    m_returnFiber = nullptr;
    m_name = (name != nullptr && name[0] != '\0') ? name : "gp::ThreadAnchorFiber";
    m_completedTaskCount = 0ULL;
    m_id = id;
    m_state = FiberState::ThreadAnchor;
    m_stackTier = FiberStackTier::Custom;
    m_resetScratchOnCompletion = false;

    detail::setCurrentFiberTLS(this);
    return FiberResult::Success;
}

FiberResult Fiber::restoreCurrentThread() noexcept
{
    if (!isValid())
    {
        return FiberResult::NotInitialized;
    }

    if (m_state != FiberState::ThreadAnchor)
    {
        return FiberResult::NotAThreadFiber;
    }

    if (!isCurrentlyExecuting())
    {
        return FiberResult::CannotDestroyActiveFiber;
    }

    if (gp::memory::FiberScratchArena::getBoundFiberArena() != nullptr)
    {
        (void)gp::memory::FiberScratchArena::bindFiberArena(nullptr);
    }

    m_scratchArena.shutdown();

    const gp::platform::FiberContextResult contextRes = m_context.restoreCurrentThread();
    if (contextRes != gp::platform::FiberContextResult::Success)
    {
        return detail::mapContextError(contextRes);
    }

    if (detail::getCurrentFiberTLS() == this)
    {
        detail::setCurrentFiberTLS(nullptr);
    }

    m_entry = nullptr;
    m_userData = nullptr;
    m_returnFiber = nullptr;
    m_name = "gp::Fiber";
    m_completedTaskCount = 0ULL;
    m_id = kInvalidFiberId;
    m_state = FiberState::Uninitialized;
    m_stackTier = FiberStackTier::Standard64KiB;
    m_resetScratchOnCompletion = true;

    return FiberResult::Success;
}

FiberResult Fiber::bindTask(const FiberEntryFunction entry, void* const userData, Fiber* const returnFiber) noexcept
{
    if (!isValid())
    {
        return FiberResult::NotInitialized;
    }

    if (m_state == FiberState::ThreadAnchor)
    {
        return FiberResult::InvalidArguments;
    }

    if (m_state == FiberState::Running || m_state == FiberState::Suspended)
    {
        return FiberResult::BusyRunningOrSuspended;
    }

    if (entry == nullptr)
    {
        return FiberResult::InvalidArguments;
    }

    m_entry = entry;
    m_userData = userData;
    if (returnFiber != nullptr)
    {
        m_returnFiber = returnFiber;
    }
    m_state = FiberState::Ready;

    return FiberResult::Success;
}

FiberResult Fiber::switchTo(Fiber& target) noexcept
{
    if (!isValid() || !target.isValid())
    {
        return FiberResult::NotInitialized;
    }

    if (this == &target)
    {
        return FiberResult::SelfSwitchError;
    }

    if (!isCurrentlyExecuting())
    {
        return FiberResult::CallerNotInFiber;
    }

    if (target.m_state == FiberState::Running)
    {
        return FiberResult::BusyRunningOrSuspended;
    }

    if (target.m_state != FiberState::ThreadAnchor && target.m_state != FiberState::Ready &&
        target.m_state != FiberState::Suspended)
    {
        return FiberResult::NoTaskBound;
    }

    if (m_state == FiberState::Running)
    {
        m_state = FiberState::Suspended;
    }

    if (target.m_state != FiberState::ThreadAnchor)
    {
        target.m_state = FiberState::Running;
    }

    // Update the calling OS worker thread's active fiber pointer and bind the target fiber's PagedLinearAllocator
    // (or unbind to nullptr when returning to a root ThreadAnchor). bindFiberArena also advances the worker thread's
    // switch epoch so any illegal ScopedScratchMark (ThreadScratchAllocator) spanning this switch asserts.
    detail::setCurrentFiberTLS(&target);
    gp::memory::PagedLinearAllocator* const targetArena = target.isThreadAnchor() ? nullptr : &target.m_scratchArena;
    (void)gp::memory::FiberScratchArena::bindFiberArena(targetArena);

    const gp::platform::FiberContextResult switchRes = m_context.switchTo(target.m_context);
    if (switchRes != gp::platform::FiberContextResult::Success) [[unlikely]]
    {
        // Roll back state and thread-local bindings if the underlying platform switch rejected the transition.
        detail::setCurrentFiberTLS(this);
        (void)gp::memory::FiberScratchArena::bindFiberArena(isThreadAnchor() ? nullptr : &m_scratchArena);
        if (m_state == FiberState::Suspended)
        {
            m_state = FiberState::Running;
        }
        return detail::mapContextError(switchRes);
    }

    return FiberResult::Success;
}

FiberResult Fiber::yieldToReturnFiber() noexcept
{
    if (m_returnFiber == nullptr)
    {
        return FiberResult::InvalidArguments;
    }

    return switchTo(*m_returnFiber);
}

bool Fiber::isCurrentlyExecuting() const noexcept
{
    return isValid() && (detail::getCurrentFiberTLS() == this) && m_context.isCurrentlyExecuting();
}

Fiber* Fiber::getCurrentFiber() noexcept
{
    return detail::getCurrentFiberTLS();
}

FiberId Fiber::getCurrentFiberId() noexcept
{
    const Fiber* const current = detail::getCurrentFiberTLS();
    return (current != nullptr) ? current->m_id : kInvalidFiberId;
}

bool Fiber::isExecutingInFiber() noexcept
{
    return detail::getCurrentFiberTLS() != nullptr;
}

bool Fiber::isExecutingInChildFiber() noexcept
{
    const Fiber* const current = detail::getCurrentFiberTLS();
    return (current != nullptr) && !current->isThreadAnchor();
}

void Fiber::fiberLoopTrampoline([[maybe_unused]] void* const unusedUserData) noexcept
{
    for (;;)
    {
        Fiber* const startingSelf = detail::getCurrentFiberTLS();
        GP_ASSERT(
            startingSelf != nullptr,
            "gp::thread::Fiber trampoline entered without an active thread-local fiber binding."
        );

        const FiberEntryFunction taskEntry = startingSelf->m_entry;
        void* const taskData = startingSelf->m_userData;
        startingSelf->m_state = FiberState::Running;

        GP_ASSERT(taskEntry != nullptr, "gp::thread::Fiber entered trampoline with a null task entry function.");
        if (taskEntry != nullptr) [[likely]]
        {
            taskEntry(taskData);
        }

        // Re-query the active fiber slot because the fiber may have yielded inside taskEntry() and migrated to a
        // different OS worker thread before completing.
        Fiber* const completedSelf = detail::getCurrentFiberTLS();
        GP_ASSERT(
            completedSelf != nullptr, "gp::thread::Fiber task completed without an active thread-local fiber binding."
        );

        if (completedSelf->m_resetScratchOnCompletion)
        {
            completedSelf->m_scratchArena.reset();
        }

        completedSelf->m_entry = nullptr;
        completedSelf->m_userData = nullptr;
        ++completedSelf->m_completedTaskCount;
        completedSelf->m_state = FiberState::Completed;

        Fiber* const returnTarget = completedSelf->m_returnFiber;
        GP_ASSERT(
            returnTarget != nullptr && returnTarget->isValid(),
            "gp::thread::Fiber task completed without a valid returnFiber configured. Always provide a returnFiber " "(such as the worker thread's ScopedThreadFiber anchor) or yield explicitly."
        );

        [[maybe_unused]] const FiberResult switchRes = completedSelf->switchTo(*returnTarget);
        GP_ASSERT(
            switchRes == FiberResult::Success,
            "gp::thread::Fiber failed to switch back to returnFiber upon task completion."
        );
    }
}

}   // namespace gp::thread
