// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreTypes.hpp"
#include "Templates/Concepts/ComparisonConcepts.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"
#include "Templates/Concepts/ObjectConcepts.hpp"
#include "Templates/Core/Utility.hpp"

namespace GP
{

/// @brief NullOpt tag type for constructing empty optionals and comparisons.
struct FNullOptT
{
    explicit constexpr FNullOptT() = default;
};

/// @brief NullOpt constant for constructing empty optionals and comparisons.
inline constexpr FNullOptT NullOpt{};

/// @brief Optional value container, holds a T or nothing.
/// @note No heap allocation. The value lives in aligned inline storage. Use HasValue() / operator bool to check
/// presence before access.
/// @tparam T Contained type.
template <typename T>
class TOptional
{
    static_assert(!CReference<T>, "TOptional cannot hold a reference; use TOptional<T*> instead");
    static_assert(CDestructible<T>, "TOptional type must be destructible");

public:
    using ValueType = T;

private:
    GP_ALIGN(alignof(T)) Byte m_storage[sizeof(T)];
    bool m_hasValue = false;

public:
    /// @brief Constructs an empty optional.
    constexpr TOptional() noexcept
        : m_hasValue(false)
    {}

    /// @brief Constructs an empty optional from NullOpt.
    /// @param NullOpt NullOpt tag.
    constexpr TOptional(FNullOptT) noexcept
        : m_hasValue(false)
    {}

    /// @brief Copy-constructs an optional holding @p value.
    /// @param value Value to store.
    constexpr TOptional(const T& value) noexcept(noexcept(T(value)))
        : m_hasValue(true)
    {
        ::new (&m_storage) T(value);
    }

    /// @brief Move-constructs an optional holding @p value.
    /// @param value Value to move.
    constexpr TOptional(T&& value) noexcept(noexcept(T(GP::Move(value))))
        : m_hasValue(true)
    {
        ::new (&m_storage) T(GP::Move(value));
    }

    /// @brief Constructs a new value in-place.
    /// @tparam Args Argument types.
    /// @param args Arguments forwarded to T's constructor.
    template <typename... Args>
    requires CConstructibleFrom<T, Args...>
    explicit constexpr TOptional(Args&&... args) noexcept(noexcept(T(GP::Forward<Args>(args)...)))
        : m_hasValue(true)
    {
        ::new (&m_storage) T(GP::Forward<Args>(args)...);
    }

    /// @brief Copy-constructs from another optional.
    /// @param other Optional to copy from.
    TOptional(const TOptional& other)
        : m_hasValue(other.m_hasValue)
    {
        if (m_hasValue) { ::new (&m_storage) T(other.Value()); }
    }

    /// @brief Move-constructs from another optional.
    /// @param other Optional to move from.
    TOptional(TOptional&& other) noexcept(noexcept(T(GP::Move(other.Value()))))
        : m_hasValue(other.m_hasValue)
    {
        if (m_hasValue)
        {
            ::new (&m_storage) T(GP::Move(other.Value()));
            other.Reset();
        }
    }

    /// @brief Destroys the contained value (if any).
    ~TOptional() { Reset(); }

public:
    /// @brief Assigns NullOpt, destroying any contained value.
    /// @param NullOpt NullOpt tag.
    /// @return Reference to this optional.
    TOptional& operator=(FNullOptT) noexcept
    {
        Reset();
        return *this;
    }

    /// @brief Copy-assigns from another optional.
    /// @param other Optional to copy from.
    /// @return Reference to this optional.
    TOptional& operator=(const TOptional& other)
    {
        if (this == &other) { return *this; }
        if (m_hasValue && other.m_hasValue) { Value() = other.Value(); }
        else if (!m_hasValue && other.m_hasValue)
        {
            ::new (&m_storage) T(other.Value());
            m_hasValue = true;
        }
        else if (m_hasValue) { Reset(); }
        return *this;
    }

    /// @brief Move-assigns from another optional.
    /// @param other Optional to move from.
    /// @return Reference to this optional.
    TOptional& operator=(TOptional&& other) noexcept(noexcept(T(GP::Move(other.Value()))))
    {
        if (this == &other) { return *this; }
        if (m_hasValue && other.m_hasValue)
        {
            Value() = GP::Move(other.Value());
            other.Reset();
        }
        else if (!m_hasValue && other.m_hasValue)
        {
            ::new (&m_storage) T(GP::Move(other.Value()));
            m_hasValue = true;
            other.Reset();
        }
        else if (m_hasValue) { Reset(); }
        return *this;
    }

    /// @brief Copy- or move-assigns a new value, constructing it if the optional was empty.
    /// @tparam U Type of the value to assign (deduced).
    /// @param value Value to assign.
    /// @return Reference to this optional.
    template <typename U = T>
    requires CConstructibleFrom<T, U&&> && (!CSameAs<std::remove_cvref_t<U>, TOptional>)TOptional& operator=(U&& value)
    {
        if (m_hasValue) { Value() = GP::Forward<U>(value); }
        else
        {
            ::new (&m_storage) T(GP::Forward<U>(value));
            m_hasValue = true;
        }
        return *this;
    }

    /// @brief Checks if the optional contains a value.
    /// @param other Optional to compare with.
    /// @return True if both optionals are empty or both contain equal values, false otherwise.
    GP_NODISCARD bool operator==(const TOptional& other) const noexcept requires CEqualityComparable<T>
    {
        if (m_hasValue != other.m_hasValue) { return false; }
        return !m_hasValue || Value() == other.Value();
    }

    /// @brief Checks if the optional is empty.
    /// @param NullOpt NullOpt tag.
    /// @return True if this optional is empty, false otherwise.
    GP_NODISCARD bool operator==(FNullOptT) const noexcept { return !m_hasValue; }

    /// @brief Checks if the optional is not empty.
    /// @param NullOpt NullOpt tag.
    /// @return True if this optional has a value, false otherwise.
    GP_NODISCARD bool operator!=(FNullOptT) const noexcept { return m_hasValue; }

    /// @brief Checks if the optional contains a value equal to @p value.
    /// @param value Value to compare with.
    /// @return True if the optional has a value and it is equal to @p value, false otherwise.
    GP_NODISCARD bool operator==(const T& value) const noexcept requires CEqualityComparable<T>
    {
        return m_hasValue && Value() == value;
    }

    /// @brief Checks if the optional does not contain a value equal to @p value.
    /// @param value Value to compare with.
    /// @return True if the optional is empty or its value differs from @p value, false otherwise.
    GP_NODISCARD bool operator!=(const T& value) const noexcept requires CEqualityComparable<T>
    {
        return !(*this == value);
    }

    /// @brief Checks if this optional is not equal to another optional.
    /// @param other Optional to compare with.
    /// @return True if the optionals differ in value presence or contained value, false otherwise.
    GP_NODISCARD bool operator!=(const TOptional& other) const noexcept requires CEqualityComparable<T>
    {
        return !(*this == other);
    }

    /// @brief Dereferences the optional to access the contained value.
    /// @return Reference to the contained value.
    GP_NODISCARD constexpr T& operator*() & noexcept { return Value(); }

    /// @brief Dereferences the optional to access the contained value.
    /// @return Reference to the contained value.
    GP_NODISCARD constexpr const T& operator*() const& noexcept { return Value(); }

    /// @brief Dereferences the optional to access the contained value.
    /// @return Rvalue reference to the contained value.
    GP_NODISCARD constexpr T&& operator*() && noexcept { return GP::Move(Value()); }

    /// @brief Accesses members of the contained value.
    /// @return Pointer to the contained value.
    GP_NODISCARD constexpr T* operator->() noexcept { return &Value(); }

    /// @brief Accesses members of the contained value.
    /// @return Pointer to the contained value.
    GP_NODISCARD constexpr const T* operator->() const noexcept { return &Value(); }

    /// @brief Explicitly converts to bool to check for value presence.
    /// @return True if the optional has a value, false otherwise.
    GP_NODISCARD constexpr explicit operator bool() const noexcept { return m_hasValue; }

public:
    /// @brief Checks if the optional contains a value.
    /// @return True if the optional has a value, false otherwise.
    GP_NODISCARD constexpr bool HasValue() const noexcept { return m_hasValue; }

    /// @brief Accesses the contained value, asserting if there is none.
    /// @return Reference to the contained value.
    GP_NODISCARD constexpr T& Value() & noexcept
    {
        GP_ASSERT(m_hasValue);
        return *reinterpret_cast<T*>(&m_storage);
    }

    /// @brief Accesses the contained value, asserting if there is none.
    /// @return Reference to the contained value.
    GP_NODISCARD constexpr const T& Value() const& noexcept
    {
        GP_ASSERT(m_hasValue);
        return *reinterpret_cast<const T*>(&m_storage);
    }

    /// @brief Accesses the contained value, asserting if there is none.
    /// @return Rvalue reference to the contained value.
    GP_NODISCARD constexpr T&& Value() && noexcept
    {
        GP_ASSERT(m_hasValue);
        return GP::Move(*reinterpret_cast<T*>(&m_storage));
    }

    /// @brief Returns the contained value if present, otherwise returns @p defaultValue.
    /// @param defaultValue Value to return if the optional is empty.
    /// @return The contained value or @p defaultValue.
    GP_NODISCARD constexpr T ValueOr(T&& defaultValue) const& noexcept
    {
        return m_hasValue ? Value() : GP::Move(defaultValue);
    }

    /// @brief Returns the contained value if present, otherwise returns @p defaultValue.
    /// @param defaultValue Value to return if the optional is empty.
    /// @return The contained value or @p defaultValue.
    GP_NODISCARD constexpr T ValueOr(T&& defaultValue) && noexcept
    {
        return m_hasValue ? GP::Move(Value()) : GP::Move(defaultValue);
    }

    /// @brief Destroys the contained value if present, leaving the optional empty.
    void Reset() noexcept
    {
        if (m_hasValue)
        {
            if constexpr (!CTriviallyDestructible<T>) { reinterpret_cast<T*>(&m_storage)->~T(); }
            m_hasValue = false;
        }
    }

    /// @brief Constructs a new value in-place, destroying any existing value.
    /// @tparam Args Argument types for constructing the new value.
    /// @param args Arguments forwarded to T's constructor.
    /// @return Reference to the newly constructed value.
    template <typename... Args>
    T& Emplace(Args&&... args)
    {
        Reset();
        ::new (&m_storage) T(GP::Forward<Args>(args)...);
        m_hasValue = true;
        return Value();
    }

    /// @brief Swaps the contents of this optional with another optional.
    /// @param other Optional to swap with.
    void Swap(TOptional& other) noexcept(noexcept(T(GP::Move(GP::Declval<T&>()))))
    {
        if (m_hasValue && other.m_hasValue) { GP::Swap(Value(), other.Value()); }
        else if (!m_hasValue && other.m_hasValue)
        {
            ::new (&m_storage) T(GP::Move(other.Value()));
            m_hasValue = true;
            other.Reset();
        }
        else if (m_hasValue && !other.m_hasValue)
        {
            ::new (&other.m_storage) T(GP::Move(Value()));
            other.m_hasValue = true;
            Reset();
        }
    }
};

/// @brief Creates a TOptional with an in-place constructed value.
/// @tparam T Value type.
/// @tparam Args Constructor argument types.
/// @param args Arguments forwarded to T's constructor.
/// @return TOptional<T> with a live value.
template <typename T, typename... Args>
GP_NODISCARD constexpr TOptional<T> MakeOptional(Args&&... args) noexcept(noexcept(T(GP::Forward<Args>(args)...)))
{
    return TOptional<T>(GP::Forward<Args>(args)...);
}

}   // namespace GP
