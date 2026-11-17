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

/// @brief RAII guard that saves a variable's value on entry and restores it when leaving the scope.
/// @details Keeps scoped state mutations safe across early returns. Nested guards restore in reverse declaration
///          order (LIFO). Copy and move operations are deleted so the guard stays bound to the target variable's scope.
/// @tparam T Stored target variable type. Must be assignable on exit.
template <typename T>
class GuardValue final
{
private:
    T& m_target;
    T m_oldValue;

public:
    /// @brief Saves @p target without changing its current value, restoring that snapshot on scope exit.
    /// @param[in,out] target Variable whose current value is saved and later restored.
    explicit constexpr GuardValue(T& target) noexcept(std::is_nothrow_copy_constructible_v<T>)
        requires std::copy_constructible<T>
        : m_target(target)
        , m_oldValue(target)
    {}

    /// @brief Saves @p target by move (or copy), assigns @p newValue immediately, and restores the saved value on exit.
    /// @details Supports move-only target types by moving the original value into internal storage right before
    ///          assigning @p newValue to @p target.
    /// @tparam ValueType Convertible value type assigned to @p target.
    /// @param[in,out] target Variable to temporarily overwrite.
    /// @param[in] newValue Temporary value held for the lifetime of this guard.
    template <typename ValueType>
    requires std::constructible_from<T, T&&> && std::assignable_from<T&, ValueType&&> constexpr GuardValue(
        T& target, ValueType&& newValue
    ) noexcept(std::is_nothrow_constructible_v<T, T&&> && std::is_nothrow_assignable_v<T&, ValueType&&>)
        : m_target(target)
        , m_oldValue(std::move(target))
    {
        m_target = std::forward<ValueType>(newValue);
    }

    GuardValue(const GuardValue&) = delete;
    GuardValue(GuardValue&&) = delete;
    GuardValue& operator=(const GuardValue&) = delete;
    GuardValue& operator=(GuardValue&&) = delete;

    /// @brief Restores the original value back into the target variable.
    constexpr ~GuardValue() noexcept(std::is_nothrow_assignable_v<T&, T&&>)
    {
        m_target = std::move(m_oldValue);
    }

public:
    /// @brief Reads the original value captured when the guard was constructed.
    /// @return Read-only reference to the saved value.
    [[nodiscard]] constexpr const T& originalValue() const noexcept
    {
        return m_oldValue;
    }
};

template <typename T>
GuardValue(T&) -> GuardValue<T>;

template <typename T, typename ValueType>
GuardValue(T&, ValueType&&) -> GuardValue<T>;

}   // namespace gp
