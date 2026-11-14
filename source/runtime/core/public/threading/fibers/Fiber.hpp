// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/allocators/PagedLinearAllocator.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/FiberContext.hpp"
#include "platforms/base/system/Thread.hpp"    // IWYU pragma: keep
#include "threading/atomics/Atomic.hpp"
#include "threading/fibers/FiberDesc.hpp"      // IWYU pragma: export
#include "threading/fibers/FiberEnums.hpp"     // IWYU pragma: export
#include "threading/fibers/FiberForward.hpp"   // IWYU pragma: export
#include "threading/fibers/FiberTypes.hpp"     // IWYU pragma: export

namespace gp::thread
{

/// @brief Engine-level cooperative execution fiber owning a platform `FiberContext` and an elastic
///        `PagedLinearAllocator`.
/// @details Designed as the foundational Layer 1 primitive for `gp-engine`'s $M:N$ work-stealing job system:
///          - **Two-Tier Stack Provisioning:** Supports `Standard64KiB` (`64 KiB`) and `Large512KiB` (`512 KiB`) stacks
///            modeled after Naughty Dog's fiber pool (GDC 2015).
///          - **Zero-Allocation Persistent Task Re-Arming (`bindTask`):** Completed fibers yield back to `returnFiber`
///            inside a persistent trampoline loop and can be re-armed for thousands of subsequent jobs via `bindTask()`
///            without reallocating virtual stack memory or recreating OS fiber handles.
///          - **Automatic `FiberScratchArena` Context Switching:** Every `switchTo()` call automatically binds the
///            incoming fiber's `PagedLinearAllocator` to the executing worker thread via
///            `FiberScratchArena::bindFiberArena()` and increments the thread's fiber-switch epoch counter so
///            `FiberScratchAllocator` / `ScopedFiberScratchMark` remain 100% safe across core migrations while
///            illegal `ScopedScratchMark` (`ThreadScratchAllocator`) yields are mechanically caught.
class GP_CORE_API Fiber final
{
private:
    gp::platform::FiberContext m_context{};
    gp::memory::PagedLinearAllocator m_scratchArena{};
    FiberEntryFunction m_entry{ nullptr };
    void* m_userData{ nullptr };
    Fiber* m_returnFiber{ nullptr };
    const char* m_name{ "gp::Fiber" };
    gp::UInt64 m_completedTaskCount{ 0ULL };
    gp::platform::ThreadId m_anchorThreadId{ 0ULL };
    FiberId m_id{ kInvalidFiberId };
    FiberState m_state{ FiberState::Uninitialized };
    FiberStackTier m_stackTier{ FiberStackTier::Standard64KiB };
    bool m_resetScratchOnCompletion{ true };
    Atomic<gp::UInt8> m_isStackActive{ 0u };

public:
    /// @brief Constructs an uninitialized fiber (`FiberState::Uninitialized`, 0 bytes of leased memory).
    [[nodiscard]] Fiber() noexcept = default;

    /// @brief Destroys the fiber, releasing its `PagedLinearAllocator` pages and underlying `FiberContext`.
    ~Fiber() noexcept;

    Fiber(const Fiber&) = delete;
    Fiber& operator=(const Fiber&) = delete;

    /// @brief Move constructor transferring ownership of the platform context and scratch arena.
    /// @param[in,out] other Source fiber to move from (left in `FiberState::Uninitialized`).
    [[nodiscard]] Fiber(Fiber&& other) noexcept;

    /// @brief Move assignment operator transferring ownership of the platform context and scratch arena.
    /// @param[in,out] other Source fiber to move from (left in `FiberState::Uninitialized`).
    /// @return Reference to this fiber instance.
    Fiber& operator=(Fiber&& other) noexcept;

public:
    /// @section Lifecycle & Thread-to-Fiber Anchoring

    /// @brief Allocates the underlying platform stack context and configures the fiber's scratch arena.
    /// @param[in] desc Configuration parameters (`name`, `id`, `entry`, `stackTier`, `returnFiber`, etc.).
    /// @return `FiberResult::Success` on creation, or an explicit error code.
    [[nodiscard]] FiberResult create(const FiberDesc& desc) noexcept;

    /// @brief Releases all leased scratch pages and destroys the underlying platform `FiberContext` (or restores a
    ///        thread-anchor fiber).
    /// @note Cannot be called on a child fiber that is currently executing on its own stack.
    /// @return `FiberResult::Success` on cleanup, or an error code.
    [[nodiscard]] FiberResult destroy() noexcept;

    /// @brief Converts the calling OS hardware thread into a root `FiberState::ThreadAnchor` fiber.
    /// @details A worker thread must convert itself into a thread-anchor fiber before it can switch into pooled child
    ///          fibers and later return to its original OS stack.
    /// @param[in] id Diagnostic identifier for the thread anchor (defaults to `kThreadAnchorFiberId`).
    /// @param[in] name UTF-8 debug name for the thread anchor.
    /// @return `FiberResult::Success` on conversion, or `FiberResult::AlreadyThreadFiber` if the calling thread is
    ///         already converted.
    [[nodiscard]] FiberResult convertCurrentThread(
        const FiberId id = kThreadAnchorFiberId, const char* name = "gp::ThreadAnchorFiber"
    ) noexcept;

    /// @brief Restores a converted thread-anchor fiber back to a standard OS hardware thread.
    /// @return `FiberResult::Success` if restored, or an error code if not currently executing on this thread anchor.
    [[nodiscard]] FiberResult restoreCurrentThread() noexcept;

public:
    /// @section Task Binding & Cooperative Context Switching

    /// @brief Binds or re-arms a stateless task entry function on an `Idle`, `Ready`, or `Completed` child fiber
    ///        without reallocating its stack or platform context.
    /// @param[in] entry Mandatory stateless task function pointer `void (*)(void* userData)`.
    /// @param[in] userData Opaque caller-managed task payload pointer.
    /// @param[in] returnFiber Optional fiber to switch back to upon task completion (if `nullptr`, retains the
    ///                        previously configured `m_returnFiber`).
    /// @return `FiberResult::Success` if armed (`FiberState::Ready`), or `FiberResult::BusyRunningOrSuspended` if the
    ///         fiber is currently mid-execution.
    [[nodiscard]] FiberResult
        bindTask(FiberEntryFunction entry, void* userData = nullptr, Fiber* returnFiber = nullptr) noexcept;

    /// @brief Updates the target fiber to which this fiber automatically switches when its bound task returns.
    /// @param[in] returnFiber Pointer to the return/scheduler fiber.
    void setReturnFiber(Fiber* const returnFiber) noexcept
    {
        m_returnFiber = returnFiber;
    }

    /// @brief Retrieves the fiber to which this fiber switches when its bound task returns.
    /// @return Pointer to the configured return fiber, or `nullptr`.
    [[nodiscard]] constexpr Fiber* getReturnFiber() const noexcept
    {
        return m_returnFiber;
    }

    /// @brief Cooperatively suspends this fiber, binds `target`'s `PagedLinearAllocator` to `FiberScratchArena`,
    ///        advances the thread switch epoch, and switches execution to `target`.
    /// @note Must be invoked from the currently executing fiber on the calling OS thread (`isCurrentlyExecuting()`).
    /// @param[in,out] target Destination fiber (`Ready`, `Suspended`, or `ThreadAnchor`) to switch into.
    /// @return `FiberResult::Success` when execution later resumes on this fiber, or an explicit error code.
    [[nodiscard]] FiberResult switchTo(Fiber& target) noexcept;

    /// @brief Cooperatively switches execution back to this fiber's configured `m_returnFiber`.
    /// @return `FiberResult::Success` when resumed, or `FiberResult::InvalidArguments` if `m_returnFiber == nullptr`.
    [[nodiscard]] FiberResult yieldToReturnFiber() noexcept;

public:
    /// @section Fiber Scratchpad Arena ([ADR-0048])

    /// @brief Retrieves a mutable reference to this fiber's elastic `PagedLinearAllocator`.
    /// @return Reference to `m_scratchArena`.
    [[nodiscard]] constexpr gp::memory::PagedLinearAllocator& getScratchArena() noexcept
    {
        return m_scratchArena;
    }

    /// @brief Retrieves a const reference to this fiber's elastic `PagedLinearAllocator`.
    /// @return Const reference to `m_scratchArena`.
    [[nodiscard]] constexpr const gp::memory::PagedLinearAllocator& getScratchArena() const noexcept
    {
        return m_scratchArena;
    }

    /// @brief Releases all leased pages in this fiber's `PagedLinearAllocator` back to `PagePool` in O(1) time.
    void resetScratchArena() noexcept
    {
        m_scratchArena.reset();
    }

    /// @brief Checks whether this fiber automatically resets its `PagedLinearAllocator` upon task completion.
    /// @return True if `m_scratchArena.reset()` is invoked automatically when a bound task returns.
    [[nodiscard]] constexpr bool shouldResetScratchOnCompletion() const noexcept
    {
        return m_resetScratchOnCompletion;
    }

    /// @brief Configures whether this fiber automatically resets its `PagedLinearAllocator` upon task completion.
    /// @param[in] shouldReset True to automatically reset on task return, false to retain leased pages across tasks.
    constexpr void setResetScratchOnCompletion(const bool shouldReset) noexcept
    {
        m_resetScratchOnCompletion = shouldReset;
    }

public:
    /// @section State, Diagnostics & Thread-Local Queries

    /// @brief Checks whether this fiber owns a valid platform execution context or thread anchor.
    /// @return True if initialized, false otherwise.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return m_context.isValid();
    }

    /// @brief Checks whether this fiber is a converted OS hardware thread anchor (`FiberState::ThreadAnchor`).
    /// @return True if this instance is a thread anchor, false otherwise.
    [[nodiscard]] constexpr bool isThreadAnchor() const noexcept
    {
        return m_state == FiberState::ThreadAnchor;
    }

    /// @brief Checks whether this fiber is allocated and waiting in `FiberState::Idle` with no task bound.
    /// @return True if `m_state == FiberState::Idle`.
    [[nodiscard]] constexpr bool isIdle() const noexcept
    {
        return m_state == FiberState::Idle;
    }

    /// @brief Checks whether this fiber has a bound task ready to start execution (`FiberState::Ready`).
    /// @return True if `m_state == FiberState::Ready`.
    [[nodiscard]] constexpr bool isReady() const noexcept
    {
        return m_state == FiberState::Ready;
    }

    /// @brief Checks whether this fiber is currently marked as `FiberState::Running`.
    /// @return True if `m_state == FiberState::Running`.
    [[nodiscard]] constexpr bool isRunning() const noexcept
    {
        return m_state == FiberState::Running;
    }

    /// @brief Checks whether this fiber yielded mid-task and is awaiting resumption (`FiberState::Suspended`).
    /// @return True if `m_state == FiberState::Suspended`.
    [[nodiscard]] constexpr bool isSuspended() const noexcept
    {
        return m_state == FiberState::Suspended;
    }

    /// @brief Checks whether this fiber's most recent task has run to completion (`FiberState::Completed`).
    /// @return True if `m_state == FiberState::Completed`.
    [[nodiscard]] constexpr bool isCompleted() const noexcept
    {
        return m_state == FiberState::Completed;
    }

    /// @brief Checks whether a non-null task entry function is currently bound to this fiber.
    /// @return True if `m_entry != nullptr`.
    [[nodiscard]] constexpr bool hasBoundTask() const noexcept
    {
        return m_entry != nullptr;
    }

    /// @brief Checks whether this fiber is the actively executing fiber on the calling OS worker thread.
    /// @return True if `getCurrentFiber() == this` and the underlying platform context is active.
    [[nodiscard]] bool isCurrentlyExecuting() const noexcept;

    /// @brief Checks whether this fiber's hardware call stack is currently occupied by a CPU core ([ADR-0050]).
    /// @details Remains true throughout task execution and during the outgoing register save inside `switchTo()`,
    ///          transitioning to false with `MemoryOrder::Release` only after the outgoing CPU core has switched onto
    ///          its target fiber's stack.
    /// @return True if a CPU core is currently executing on or vacating this fiber's stack.
    [[nodiscard]] constexpr bool isStackActive() const noexcept
    {
        return m_isStackActive.load(gp::MemoryOrder::Acquire) != 0u;
    }

    /// @brief Queries the current lifecycle state of the fiber.
    /// @return Current `FiberState`.
    [[nodiscard]] constexpr FiberState getState() const noexcept
    {
        return m_state;
    }

    /// @brief Queries the 32-bit identifier assigned to this fiber.
    /// @return Fiber identifier (`kInvalidFiberId` if uninitialized).
    [[nodiscard]] constexpr FiberId getId() const noexcept
    {
        return m_id;
    }

    /// @brief Queries the UTF-8 debug name assigned to this fiber.
    /// @return Non-null UTF-8 C-string.
    [[nodiscard]] constexpr const char* getName() const noexcept
    {
        return m_name;
    }

    /// @brief Queries the opaque user context pointer currently bound to this fiber's task.
    /// @return Bound `userData` pointer, or `nullptr` if none is bound.
    [[nodiscard]] constexpr void* getUserData() const noexcept
    {
        return m_userData;
    }

    /// @brief Queries the stack provisioning tier configured for this fiber.
    /// @return `FiberStackTier` enumerator.
    [[nodiscard]] constexpr FiberStackTier getStackTier() const noexcept
    {
        return m_stackTier;
    }

    /// @brief Queries the usable virtual stack size in bytes reserved for this fiber (0 for thread anchors).
    /// @return Stack size in bytes.
    [[nodiscard]] constexpr gp::USize getStackSize() const noexcept
    {
        return m_context.getStackSize();
    }

    /// @brief Queries the total number of tasks that have run to completion on this fiber since `create()`.
    /// @return Completed task count.
    [[nodiscard]] constexpr gp::UInt64 getCompletedTaskCount() const noexcept
    {
        return m_completedTaskCount;
    }

    /// @brief Retrieves a mutable reference to the underlying platform `FiberContext`.
    /// @return Reference to `m_context`.
    [[nodiscard]] constexpr gp::platform::FiberContext& getPlatformContext() noexcept
    {
        return m_context;
    }

    /// @brief Retrieves a const reference to the underlying platform `FiberContext`.
    /// @return Const reference to `m_context`.
    [[nodiscard]] constexpr const gp::platform::FiberContext& getPlatformContext() const noexcept
    {
        return m_context;
    }

    /// @brief Retrieves a pointer to the `gp::thread::Fiber` currently executing on the calling OS thread.
    /// @return Pointer to the active `Fiber` (either a child fiber or a `ThreadAnchor`), or `nullptr` if the calling
    ///         OS thread has not been converted to a fiber.
    [[nodiscard]] static Fiber* getCurrentFiber() noexcept;

    /// @brief Queries the `FiberId` of the fiber currently executing on the calling OS thread.
    /// @return Active `FiberId`, or `kInvalidFiberId` if the calling thread is not executing in a fiber.
    [[nodiscard]] static FiberId getCurrentFiberId() noexcept;

    /// @brief Checks whether the calling OS thread is currently executing inside any `gp::thread::Fiber` (including a
    ///        converted `ThreadAnchor`).
    /// @return True if `getCurrentFiber() != nullptr`.
    [[nodiscard]] static bool isExecutingInFiber() noexcept;

    /// @brief Checks whether the calling OS thread is currently executing inside a child task `Fiber` (excluding root
    ///        `ThreadAnchor` fibers).
    /// @return True if `getCurrentFiber() != nullptr && !getCurrentFiber()->isThreadAnchor()`.
    [[nodiscard]] static bool isExecutingInChildFiber() noexcept;

    /// @brief Resolves the stack reservation size in bytes for a given `FiberStackTier`.
    /// @param[in] tier Stack tier enumerator.
    /// @param[in] customBytes Custom byte size used when `tier == FiberStackTier::Custom`.
    /// @return Clamped stack size in bytes (at least `gp::platform::kMinFiberStackSize`).
    [[nodiscard]] static constexpr gp::USize resolveStackTierBytes(
        const FiberStackTier tier, const gp::USize customBytes = kStandardFiberStackBytes
    ) noexcept
    {
        switch (tier)
        {
        case FiberStackTier::Standard64KiB:
            return kStandardFiberStackBytes;
        case FiberStackTier::Large512KiB:
            return kLargeFiberStackBytes;
        case FiberStackTier::Custom:
            // falls through
        default:
            return (customBytes < gp::platform::kMinFiberStackSize) ? gp::platform::kMinFiberStackSize : customBytes;
        }
    }

private:
    static void fiberLoopTrampoline(void* unusedUserData) noexcept;
};

/// @brief RAII scope guard that converts the calling OS worker thread into a root `gp::thread::Fiber` anchor on
///        construction and restores it back to a normal OS thread on destruction.
class GP_CORE_API ScopedThreadFiber final
{
private:
    Fiber m_anchorFiber{};
    FiberResult m_result{ FiberResult::NotInitialized };

public:
    /// @brief Converts the calling OS thread into a root thread-anchor `Fiber`.
    /// @param[in] id Optional identifier for the thread anchor (defaults to `kThreadAnchorFiberId`).
    /// @param[in] name Optional UTF-8 debug label for the thread anchor.
    explicit ScopedThreadFiber(
        const FiberId id = kThreadAnchorFiberId, const char* name = "gp::ThreadAnchorFiber"
    ) noexcept
        : m_result(m_anchorFiber.convertCurrentThread(id, name))
    {}

    /// @brief Restores the calling OS thread from its root thread-anchor `Fiber` if conversion succeeded.
    ~ScopedThreadFiber() noexcept
    {
        if (m_anchorFiber.isValid())
        {
            (void)m_anchorFiber.restoreCurrentThread();
        }
    }

    ScopedThreadFiber(const ScopedThreadFiber&) = delete;
    ScopedThreadFiber& operator=(const ScopedThreadFiber&) = delete;
    ScopedThreadFiber(ScopedThreadFiber&&) = delete;
    ScopedThreadFiber& operator=(ScopedThreadFiber&&) = delete;

public:
    /// @brief Checks whether the calling OS thread was successfully converted into a thread-anchor `Fiber`.
    /// @return True if `m_result == FiberResult::Success` and the anchor fiber is valid.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return m_result == FiberResult::Success && m_anchorFiber.isValid();
    }

    /// @brief Queries the status code returned by `convertCurrentThread()`.
    /// @return `FiberResult` status code.
    [[nodiscard]] constexpr FiberResult getResult() const noexcept
    {
        return m_result;
    }

    /// @brief Retrieves a mutable reference to the underlying thread-anchor `Fiber`.
    /// @return Reference to `m_anchorFiber`.
    [[nodiscard]] constexpr Fiber& getFiber() noexcept
    {
        return m_anchorFiber;
    }

    /// @brief Retrieves a const reference to the underlying thread-anchor `Fiber`.
    /// @return Const reference to `m_anchorFiber`.
    [[nodiscard]] constexpr const Fiber& getFiber() const noexcept
    {
        return m_anchorFiber;
    }
};

}   // namespace gp::thread
