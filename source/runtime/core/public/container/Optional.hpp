// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include <concepts>
#include <type_traits>

namespace gp
{

/// @brief NullOpt tag type for constructing empty optionals and comparisons.
struct NullOptT
{
    explicit constexpr NullOptT() = default;
};

/// @brief NullOpt constant for constructing empty optionals and comparisons.
inline constexpr NullOptT nullOpt{};

/// @brief Optional value container, holds a T or nothing.
/// @note No heap allocation. The value lives in aligned inline storage. Use hasValue() / operator bool to check
/// presence before access.
/// @tparam T Contained type.
template <typename T>
class Optional
{
    static_assert(std::is_destructible_v<T>, "T must be destructible");
    static_assert(!std::is_reference_v<T>, "T must not be a reference type");

public:
    using ValueType = T;

private:
    GP_ALIGN(alignof(T)) Byte m_storage[sizeof(T)];
    bool m_hasValue = false;

public:
    /// @brief Constructs an empty optional.
    constexpr Optional() noexcept
        : m_hasValue(false)
    {}

    /// @brief Constructs an empty optional from NullOpt.
    /// @param[in] NullOpt NullOpt tag.
    constexpr Optional(NullOptT) noexcept
        : m_hasValue(false)
    {}

    /// @brief Copy-constructs an optional holding @p value.
    /// @param[in] value Value to store.
    constexpr Optional(const T& value) noexcept(noexcept(T(value)))
        : m_hasValue(true)
    {
        ::new (&m_storage) T(value);
    }

    /// @brief Move-constructs an optional holding @p value.
    /// @param[in] value Value to move.
    constexpr Optional(T&& value) noexcept(noexcept(T(std::move(value))))
        : m_hasValue(true)
    {
        ::new (&m_storage) T(std::move(value));
    }

    /// @brief Constructs a new value in-place.
    /// @tparam Args Argument types.
    /// @param[in] args Arguments forwarded to T's constructor.
    template <typename... Args>
    requires std::is_constructible_v<T, Args...>
    explicit constexpr Optional(Args&&... args) noexcept(noexcept(T(std::forward<Args>(args)...)))
        : m_hasValue(true)
    {
        ::new (&m_storage) T(std::forward<Args>(args)...);
    }

    /// @brief Copy-constructs from another optional.
    /// @param[in] other Optional to copy from.
    constexpr Optional(const Optional& other)
        : m_hasValue(other.m_hasValue)
    {
        if (m_hasValue) { ::new (&m_storage) T(other.value()); }
    }

    /// @brief Move-constructs from another optional.
    /// @param[in] other Optional to move from.
    constexpr Optional(Optional&& other) noexcept(noexcept(T(std::move(other.value()))))
        : m_hasValue(other.m_hasValue)
    {
        if (m_hasValue)
        {
            ::new (&m_storage) T(std::move(other.value()));
            other.reset();
        }
    }

    /// @brief Destroys the contained value (if any).
    ~Optional() { reset(); }

    /// @brief Assigns NullOpt, destroying any contained value.
    /// @param[in] NullOpt NullOpt tag.
    /// @return Reference to this optional.
    Optional& operator=(NullOptT) noexcept
    {
        reset();
        return *this;
    }

    /// @brief Copy-assigns from another optional.
    /// @param[in] other Optional to copy from.
    /// @return Reference to this optional.
    Optional& operator=(const Optional& other)
    {
        if (this == &other) { return *this; }
        if (m_hasValue && other.m_hasValue) { value() = other.value(); }
        else if (!m_hasValue && other.m_hasValue)
        {
            ::new (&m_storage) T(other.value());
            m_hasValue = true;
        }
        else if (m_hasValue) { reset(); }
        return *this;
    }

    /// @brief Move-assigns from another optional.
    /// @param[in] other Optional to move from.
    /// @return Reference to this optional.
    Optional& operator=(Optional&& other) noexcept(noexcept(T(std::move(other.value()))))
    {
        if (this == &other) { return *this; }
        if (m_hasValue && other.m_hasValue)
        {
            value() = std::move(other.value());
            other.reset();
        }
        else if (!m_hasValue && other.m_hasValue)
        {
            ::new (&m_storage) T(std::move(other.value()));
            m_hasValue = true;
            other.reset();
        }
        else if (m_hasValue) { reset(); }
        return *this;
    }

    /// @brief Copy- or move-assigns a new value, constructing it if the optional was empty.
    /// @tparam U Type of the value to assign (deduced).
    /// @param[in] inValue Value to assign.
    /// @return Reference to this optional.
    template <typename U = T>
    requires std::is_constructible_v<T, U&&> && (!std::is_same_v<std::remove_cvref_t<U>, Optional>)Optional&
        operator=(U&& inValue)
    {
        if (m_hasValue) { value() = std::forward<U>(inValue); }
        else
        {
            ::new (&m_storage) T(std::forward<U>(inValue));
            m_hasValue = true;
        }
        return *this;
    }

public:
    /// @brief Checks if the optional contains a value.
    /// @param[in] other Optional to compare with.
    /// @return True if both optionals are empty or both contain equal values, false otherwise.
    GP_NODISCARD bool operator==(const Optional& other) const noexcept requires std::equality_comparable<T>
    {
        if (m_hasValue != other.m_hasValue) { return false; }
        return !m_hasValue || value() == other.value();
    }

    /// @brief Checks if the optional is empty.
    /// @param[in] NullOpt NullOpt tag.
    /// @return True if this optional is empty, false otherwise.
    GP_NODISCARD bool operator==(NullOptT) const noexcept { return !m_hasValue; }

    /// @brief Checks if the optional is not empty.
    /// @param[in] NullOpt NullOpt tag.
    /// @return True if this optional has a value, false otherwise.
    GP_NODISCARD bool operator!=(NullOptT) const noexcept { return m_hasValue; }

    /// @brief Checks if the optional contains a value equal to @p inValue.
    /// @param[in] inValue Value to compare with.
    /// @return True if the optional has a value and it is equal to @p inValue, false otherwise.
    GP_NODISCARD bool operator==(const T& inValue) const noexcept requires std::equality_comparable<T>
    {
        return m_hasValue && value() == inValue;
    }

    /// @brief Checks if the optional does not contain a value equal to @p value.
    /// @param[in] value Value to compare with.
    /// @return True if the optional is empty or its value differs from @p value, false otherwise.
    GP_NODISCARD bool operator!=(const T& value) const noexcept requires std::equality_comparable<T>
    {
        return !(*this == value);
    }

    /// @brief Checks if this optional is not equal to another optional.
    /// @param[in] other Optional to compare with.
    /// @return True if the optionals differ in value presence or contained value, false otherwise.
    GP_NODISCARD bool operator!=(const Optional& other) const noexcept requires std::equality_comparable<T>
    {
        return !(*this == other);
    }

    /// @brief Dereferences the optional to access the contained value.
    /// @return Reference to the contained value.
    GP_NODISCARD constexpr T& operator*() & noexcept { return value(); }

    /// @brief Dereferences the optional to access the contained value.
    /// @return Reference to the contained value.
    GP_NODISCARD constexpr const T& operator*() const& noexcept { return value(); }

    /// @brief Dereferences the optional to access the contained value.
    /// @return Rvalue reference to the contained value.
    GP_NODISCARD constexpr T&& operator*() && noexcept { return std::move(value()); }

    /// @brief Accesses members of the contained value.
    /// @return Pointer to the contained value.
    GP_NODISCARD constexpr T* operator->() noexcept { return &value(); }

    /// @brief Accesses members of the contained value.
    /// @return Pointer to the contained value.
    GP_NODISCARD constexpr const T* operator->() const noexcept { return &value(); }

    /// @brief Explicitly converts to bool to check for value presence.
    /// @return True if the optional has a value, false otherwise.
    GP_NODISCARD constexpr explicit operator bool() const noexcept { return m_hasValue; }

public:
    /// @brief Checks if the optional contains a value.
    /// @return True if the optional has a value, false otherwise.
    GP_NODISCARD constexpr bool hasValue() const noexcept { return m_hasValue; }

    /// @brief Accesses the contained value, asserting if there is none.
    /// @return Reference to the contained value.
    GP_NODISCARD constexpr T& value() & noexcept
    {
        GP_ASSERT(m_hasValue);
        return *reinterpret_cast<T*>(&m_storage);
    }

    /// @brief Accesses the contained value, asserting if there is none.
    /// @return Reference to the contained value.
    GP_NODISCARD constexpr const T& value() const& noexcept
    {
        GP_ASSERT(m_hasValue);
        return *reinterpret_cast<const T*>(&m_storage);
    }

    /// @brief Accesses the contained value, asserting if there is none.
    /// @return Rvalue reference to the contained value.
    GP_NODISCARD constexpr T&& value() && noexcept
    {
        GP_ASSERT(m_hasValue);
        return std::move(*reinterpret_cast<T*>(&m_storage));
    }

    /// @brief Returns the contained value if present, otherwise returns @p defaultValue.
    /// @param[in] defaultValue Value to return if the optional is empty.
    /// @return The contained value or @p defaultValue.
    GP_NODISCARD constexpr T valueOr(T&& defaultValue) const& noexcept
    {
        return m_hasValue ? value() : std::move(defaultValue);
    }

    /// @brief Returns the contained value if present, otherwise returns @p defaultValue.
    /// @param[in] defaultValue Value to return if the optional is empty.
    /// @return The contained value or @p defaultValue.
    GP_NODISCARD constexpr T valueOr(T&& defaultValue) && noexcept
    {
        return m_hasValue ? std::move(value()) : std::move(defaultValue);
    }

    /// @brief Destroys the contained value if present, leaving the optional empty.
    void reset() noexcept
    {
        if (m_hasValue)
        {
            if constexpr (!std::is_trivially_destructible_v<T>) { reinterpret_cast<T*>(&m_storage)->~T(); }
            m_hasValue = false;
        }
    }

    /// @brief Constructs a new value in-place, destroying any existing value.
    /// @tparam Args Argument types for constructing the new value.
    /// @param[in] args Arguments forwarded to T's constructor.
    /// @return Reference to the newly constructed value.
    template <typename... Args>
    T& emplace(Args&&... args)
    {
        reset();
        ::new (&m_storage) T(std::forward<Args>(args)...);
        m_hasValue = true;
        return value();
    }

    /// @brief Swaps the contents of this optional with another optional.
    /// @param[in] other Optional to swap with.
    void swap(Optional& other) noexcept(noexcept(T(std::move(std::declval<T&>()))))
    {
        if (m_hasValue && other.m_hasValue) { std::swap(value(), other.value()); }
        else if (!m_hasValue && other.m_hasValue)
        {
            ::new (&m_storage) T(std::move(other.value()));
            m_hasValue = true;
            other.reset();
        }
        else if (m_hasValue && !other.m_hasValue)
        {
            ::new (&other.m_storage) T(std::move(value()));
            other.m_hasValue = true;
            reset();
        }
    }
};

/// @brief Creates a Optional with an in-place constructed value.
/// @tparam T Value type.
/// @tparam Args Constructor argument types.
/// @param args Arguments forwarded to T's constructor.
/// @return Optional<T> with a live value.
template <typename T, typename... Args>
GP_NODISCARD constexpr Optional<T> makeOptional(Args&&... args) noexcept(noexcept(T(std::forward<Args>(args)...)))
{
    return Optional<T>(std::forward<Args>(args)...);
}

}   // namespace gp
