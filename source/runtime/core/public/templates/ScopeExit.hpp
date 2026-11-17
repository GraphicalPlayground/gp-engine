// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <concepts>
#include <type_traits>
#include <utility>

namespace gp
{

/// @brief Invokes a callable object when the guard leaves scope unless cancelled via `release()`.
/// @details Runs cleanup deterministically along early returns and normal block exits without heap allocation or
///          virtual dispatch. Multiple guards in the same scope execute in reverse declaration order (LIFO), matching
///          standard C++ stack destruction.
/// @tparam Callback Invocable type invoked with zero arguments at scope exit.
template <typename Callback>
requires std::invocable<Callback&> class ScopeExit final
{
private:
    GP_NO_UNIQUE_ADDRESS Callback m_callback;
    bool m_active = true;

public:
    /// @brief Constructs an active guard from a callable object.
    /// @tparam Fn Forwarded callable type.
    /// @param[in] fn Callable to store and run at scope exit.
    template <typename Fn>
    requires(!std::same_as<std::remove_cvref_t<Fn>, ScopeExit>) && std::constructible_from<Callback, Fn>
    explicit constexpr ScopeExit(Fn&& fn) noexcept(std::is_nothrow_constructible_v<Callback, Fn>)
        : m_callback(std::forward<Fn>(fn))
        , m_active(true)
    {}

    /// @brief Transfers ownership of the pending cleanup callback to a new guard and disarms @p other.
    /// @param[in,out] other Source guard whose active state is transferred and cleared.
    constexpr ScopeExit(ScopeExit&& other) noexcept(std::is_nothrow_move_constructible_v<Callback>)
        : m_callback(std::move(other.m_callback))
        , m_active(other.m_active)
    {
        other.release();
    }

    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;
    ScopeExit& operator=(ScopeExit&&) = delete;

    /// @brief Runs the stored callback if the guard is still active.
    constexpr ~ScopeExit() noexcept(noexcept(m_callback()))
    {
        if (m_active)
        {
            m_callback();
        }
    }

public:
    /// @brief Disarms the guard so its destructor does nothing when the scope ends.
    constexpr void release() noexcept
    {
        m_active = false;
    }

    /// @brief Reports whether the guard will run its callback on destruction.
    /// @return `true` if the callback is still armed, `false` after `release()` or move-from.
    [[nodiscard]] constexpr bool isActive() const noexcept
    {
        return m_active;
    }
};

template <typename Callback>
ScopeExit(Callback) -> ScopeExit<Callback>;

/// @brief Creates a `ScopeExit` guard, deducing and decaying the callable type.
/// @tparam Callback Callable type invoked with no arguments.
/// @param[in] callback Callable object or lambda to execute on scope exit.
/// @return Armed `ScopeExit` owning the decayed callable.
template <typename Callback>
requires std::invocable<std::decay_t<Callback>&> [[nodiscard]] constexpr ScopeExit<std::decay_t<Callback>>
    makeScopeExit(Callback&& callback) noexcept(std::is_nothrow_constructible_v<std::decay_t<Callback>, Callback>)
{
    return ScopeExit<std::decay_t<Callback>>(std::forward<Callback>(callback));
}

namespace detail
{

struct ScopeExitTag final
{};

template <typename Callback>
requires std::invocable<std::decay_t<Callback>&> [[nodiscard]] constexpr ScopeExit<std::decay_t<Callback>> operator+(
    ScopeExitTag, Callback&& callback
) noexcept(std::is_nothrow_constructible_v<std::decay_t<Callback>, Callback>)
{
    return ScopeExit<std::decay_t<Callback>>(std::forward<Callback>(callback));
}

}   // namespace detail

}   // namespace gp

#if defined(__COUNTER__)
    /// @brief Generates a translation-unit-unique identifier using the compiler counter.
    #define GP_ANONYMOUS_VARIABLE(prefix) GP_JOIN(prefix, __COUNTER__)
#else
    /// @brief Generates a line-scoped identifier when `__COUNTER__` is unavailable.
    #define GP_ANONYMOUS_VARIABLE(prefix) GP_JOIN(prefix, __LINE__)
#endif

/// @brief Registers a block of statements to execute automatically when the enclosing scope exits.
/// @details Captures referenced local variables by reference (`[&]`). Place a semicolon after the closing brace:
/// @code
/// GP_ON_SCOPE_EXIT
/// {
///     closeHandle(handle);
/// };
/// @endcode
#define GP_ON_SCOPE_EXIT \
    const auto GP_ANONYMOUS_VARIABLE(_gpScopeExit_) = ::gp::detail::ScopeExitTag{} + [&]() noexcept
