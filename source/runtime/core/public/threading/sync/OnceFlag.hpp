// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Functional.hpp"
#include "CoreMinimal.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/sync/MutexEnums.hpp"     // IWYU pragma: export
#include "threading/sync/MutexForward.hpp"   // IWYU pragma: export
#include <utility>

namespace gp::thread
{

/// @brief 4-byte, `constexpr`-constructible one-time initialization flag (`gp::thread::OnceFlag`) ([ADR-0058]).
/// @details Uses a 4-state atomic + futex protocol (`NotStarted = 0`, `Running = 1`, `RunningWithWaiters = 2`,
///          `Completed = 3`) modeled after Ulrich Drepper's futex `pthread_once` and Unreal Engine's low-level
///          initialization guards:
///          - **Single-Load Fast Path:** Once initialized, `isCompleted()` / `callOnce()` executes a single inline
///            `Acquire` load (`m_state.load(Acquire) == Completed`) with zero atomic RMW instructions and zero
///            cross-DLL calls.
///          - **Zero-Allocation Contention Parking:** If multiple threads race into `callOnce()` simultaneously,
///            exactly one thread transitions `NotStarted -> Running` and executes the callable, while contending
///            threads transition `Running -> RunningWithWaiters` and park on `m_state` via `Futex::waitOnAddress()`
///            until the initializer transitions `m_state` to `Completed` and invokes `Futex::wakeAll()`.
class GP_CORE_API OnceFlag final
{
public:
    /// @brief Constructs an unexecuted `OnceFlag` (`OnceState::NotStarted`).
    [[nodiscard]] constexpr OnceFlag() noexcept = default;

    /// @brief Trivially destructible; requires no OS kernel cleanup.
    constexpr ~OnceFlag() noexcept = default;

    OnceFlag(const OnceFlag&) = delete;
    OnceFlag& operator=(const OnceFlag&) = delete;
    OnceFlag(OnceFlag&&) = delete;
    OnceFlag& operator=(OnceFlag&&) = delete;

public:
    /// @brief Checks whether the one-time initialization associated with this flag has already completed.
    /// @param[in] order Memory ordering constraint for the atomic load (defaults to `MemoryOrder::Acquire`).
    /// @return True if `getState(order) == OnceState::Completed`, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        isCompleted(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return m_state.load(order) == static_cast<gp::UInt32>(OnceState::Completed);
    }

    /// @brief Queries the current lifecycle state of this `OnceFlag`.
    /// @param[in] order Memory ordering constraint for the atomic load (defaults to `MemoryOrder::Acquire`).
    /// @return Current `OnceState` enumerator.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr OnceState
        getState(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return static_cast<OnceState>(m_state.load(order));
    }

    /// @brief Resets this `OnceFlag` back to `OnceState::NotStarted`.
    /// @warning Not thread-safe with concurrent `callOnce()` invocations; intended strictly for subsystem
    /// teardown/reinit
    ///          cycles and deterministic unit testing.
    constexpr void reset() noexcept
    {
        m_state.store(static_cast<gp::UInt32>(OnceState::NotStarted), gp::MemoryOrder::Release);
    }

public:
    /// @brief Internal fast/slow-path state machine entry point used by `gp::thread::callOnce` and `GP_CALL_ONCE`.
    /// @details Returns `true` if the calling execution context won the initialization race (`NotStarted -> Running`)
    ///          and must now execute the one-time callable followed by `completeExecution()`. Returns `false` if the
    ///          callable was already completed (or after waiting for a concurrent runner to finish).
    /// @return True if the caller must run the initialization callable, false if initialization is already complete.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool tryBeginExecution() noexcept
    {
        if (isCompleted(gp::MemoryOrder::Acquire)) [[likely]]
        {
            return false;
        }
        if consteval
        {
            m_state.store(static_cast<gp::UInt32>(OnceState::Running), gp::MemoryOrder::Relaxed);
            return true;
        }
        else
        {
            return tryBeginExecutionSlow();
        }
    }

    /// @brief Publishes completion of the one-time initialization (`OnceState::Completed`) and wakes any parked
    ///        threads if `OnceState::RunningWithWaiters` was observed.
    GP_FORCEINLINE_HINT constexpr void completeExecution() noexcept
    {
        if consteval
        {
            m_state.store(static_cast<gp::UInt32>(OnceState::Completed), gp::MemoryOrder::Release);
        }
        else
        {
            const gp::UInt32 prev =
                m_state.exchange(static_cast<gp::UInt32>(OnceState::Completed), gp::MemoryOrder::Release);
            GP_ASSERT(
                prev == static_cast<gp::UInt32>(OnceState::Running) ||
                    prev == static_cast<gp::UInt32>(OnceState::RunningWithWaiters),
                "OnceFlag::completeExecution() called when OnceFlag was not in Running state."
            );
            if (prev == static_cast<gp::UInt32>(OnceState::RunningWithWaiters)) [[unlikely]]
            {
                completeExecutionSlow();
            }
        }
    }

    /// @brief Aborts an active `Running` or `RunningWithWaiters` initialization back to `OnceState::NotStarted` and
    ///        wakes any parked threads so another caller can retry ([ADR-0058]).
    /// @details Used by `gp::thread::tryCallOnce()` when a fallible initialization callback returns `false`.
    GP_FORCEINLINE_HINT constexpr void cancelExecution() noexcept
    {
        if consteval
        {
            m_state.store(static_cast<gp::UInt32>(OnceState::NotStarted), gp::MemoryOrder::Release);
        }
        else
        {
            const gp::UInt32 prev =
                m_state.exchange(static_cast<gp::UInt32>(OnceState::NotStarted), gp::MemoryOrder::Release);
            GP_ASSERT(
                prev == static_cast<gp::UInt32>(OnceState::Running) ||
                    prev == static_cast<gp::UInt32>(OnceState::RunningWithWaiters),
                "OnceFlag::cancelExecution() called when OnceFlag was not in Running state."
            );
            if (prev == static_cast<gp::UInt32>(OnceState::RunningWithWaiters)) [[unlikely]]
            {
                completeExecutionSlow();
            }
        }
    }

private:
    [[nodiscard]] GP_COLD bool tryBeginExecutionSlow() noexcept;
    GP_COLD void completeExecutionSlow() noexcept;

private:
    Atomic<gp::UInt32> m_state{ static_cast<gp::UInt32>(OnceState::NotStarted) };
};

static_assert(sizeof(OnceFlag) == 4u, "gp::thread::OnceFlag must be exactly 4 bytes.");
static_assert(alignof(OnceFlag) == 4u, "gp::thread::OnceFlag must be 4-byte aligned.");

/// @brief Executes `callable(args...)` exactly once across all threads and fibers sharing `flag` ([ADR-0058]).
/// @details If `flag.isCompleted()` is already true on entry, returns immediately after a single `Acquire` load.
///          Otherwise, the first thread to claim `flag` runs `callable` and publishes its side effects with `Release`
///          ordering while any concurrent callers wait until completion.
/// @warning Recursive re-entry into `callOnce` on the same `OnceFlag` from inside `callable` is a fatal deadlock bug.
/// @tparam InCallable Callable type invocable with `InArgs...`.
/// @tparam InArgs Argument types forwarded to `callable`.
/// @param[in,out] flag Shared `OnceFlag` guarding one-time execution.
/// @param[in] callable Initialization function or lambda to invoke once.
/// @param[in] args Optional arguments forwarded to `callable`.
template <typename InCallable, typename... InArgs>
requires gp::concepts::IsInvocable<InCallable, InArgs...>
GP_FORCEINLINE_HINT constexpr void callOnce(OnceFlag& flag, InCallable&& callable, InArgs&&... args) noexcept
{
    if (flag.tryBeginExecution()) [[unlikely]]
    {
        std::forward<InCallable>(callable)(std::forward<InArgs>(args)...);
        flag.completeExecution();
    }
}

/// @brief Executes a fallible `bool`-returning predicate `callable(args...)` under `flag`, marking `flag` completed
///        only if `callable` returns `true` ([ADR-0058]).
/// @details Because `gp-engine` compiles with `-fno-exceptions`, fallible one-time subsystem initialization returns
///          `bool` rather than throwing. If `callable` returns `false`, `tryCallOnce` calls `flag.cancelExecution()` to
///          reset `flag` back to `OnceState::NotStarted`, wakes any waiting threads, and returns `false` so a
///          subsequent caller can retry.
/// @tparam InCallable Predicate type returning `bool` when invoked with `InArgs...`.
/// @tparam InArgs Argument types forwarded to `callable`.
/// @param[in,out] flag Shared `OnceFlag` guarding one-time execution.
/// @param[in] callable Fallible initialization function returning `true` on success or `false` on failure.
/// @param[in] args Optional arguments forwarded to `callable`.
/// @return True if `flag` is now `OnceState::Completed`, false if `callable` failed and reset `flag`.
template <typename InCallable, typename... InArgs>
requires gp::concepts::IsPredicate<InCallable, InArgs...> [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
    tryCallOnce(OnceFlag& flag, InCallable&& callable, InArgs&&... args) noexcept
{
    if (flag.tryBeginExecution()) [[unlikely]]
    {
        if (std::forward<InCallable>(callable)(std::forward<InArgs>(args)...))
        {
            flag.completeExecution();
            return true;
        }
        flag.cancelExecution();
        return false;
    }
    return true;
}

}   // namespace gp::thread

namespace gp
{

using thread::callOnce;
using thread::tryCallOnce;

}   // namespace gp

/// @brief Executes a callable (with optional arguments) exactly once for the given `gp::thread::OnceFlag` instance
///        ([ADR-0058]).
/// @details Short-circuits in user space via an inline `Acquire` load before evaluating or constructing callable
///          arguments on subsequent calls after initialization has completed.
/// @param[in,out] flag `gp::thread::OnceFlag` (or `gp::OnceFlag`) lvalue.
/// @param[in] callable Callable object, lambda, or function pointer to execute once.
#define GP_CALL_ONCE(flag, callable, ...)                                                  \
    do                                                                                     \
    {                                                                                      \
        ::gp::thread::OnceFlag& _gpOnceFlagRef = (flag);                                   \
        if (!_gpOnceFlagRef.isCompleted(::gp::MemoryOrder::Acquire)) [[unlikely]]          \
        {                                                                                  \
            ::gp::thread::callOnce(_gpOnceFlagRef, (callable)__VA_OPT__(, ) __VA_ARGS__);  \
        }                                                                                  \
    } while (false)
