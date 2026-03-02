// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/Utility/Iterator.hpp"
#include "CoreTypes.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"

namespace GP
{

/// @brief A reverse iterator adaptor that takes a bidirectional or random access iterator and provides reverse
///        traversal capabilities. The reverse iterator maintains an underlying base iterator and implements the
///        standard iterator interface by reversing the direction of traversal and adjusting the dereference behavior
///        accordingly.
/// @tparam TIter The type of the underlying iterator that this reverse iterator wraps. Must satisfy the requirements of
///         a bidirectional or random access iterator.
template <typename TIter>
class TReverseIterator
{
public:
    using IteratorCategory = typename TIter::IteratorCategory;
    using ValueType = typename TIter::ValueType;
    using Pointer = typename TIter::Pointer;
    using Reference = typename TIter::Reference;
    using DifferenceType = typename TIter::DifferenceType;

private:
    TIter m_base;   //<! The underlying iterator that this reverse iterator wraps.

public:
    /// @brief Default constructor, creates a reverse iterator that does not point to any element.
    constexpr TReverseIterator() noexcept = default;

    /// @brief Constructor for creating a reverse iterator from a base iterator.
    /// @param base The base iterator to wrap.
    constexpr explicit TReverseIterator(TIter base) noexcept
        : m_base(base)
    {}

    /// @brief Copy constructor for the same type.
    /// @tparam UIt The type of the other reverse iterator (must be the same as TIter).
    /// @param other The other reverse iterator to copy from.
    template <typename UIt>
    requires CConvertibleTo<UIt, TIter> constexpr TReverseIterator(const TReverseIterator<UIt>& other) noexcept
        : m_base(other.GetBase())
    {}

public:
    /// @brief Dereference operator to access the element pointed to by the reverse iterator.
    /// @return A reference to the element pointed to by the reverse iterator.
    GP_NODISCARD constexpr Reference operator*() const noexcept
    {
        TIter tmp = m_base;
        return *--tmp;
    }

    /// @brief Arrow operator to access members of the element pointed to by the reverse iterator.
    /// @return A pointer to the element pointed to by the reverse iterator.
    GP_NODISCARD constexpr Pointer operator->() const noexcept
    {
        TIter tmp = m_base;
        --tmp;
        return tmp.GetPtr();
    }

    /// @brief Subscript operator to access elements at an offset from the reverse iterator's current position.
    /// @param n The offset from the current position (can be positive or negative).
    /// @return A reference to the element at the specified offset from the current position.
    /// @warning This operator does not perform bounds checking. The caller must ensure that the resulting position is
    ///          valid.
    GP_NODISCARD constexpr Reference operator[](DifferenceType n) const noexcept { return m_base[-n - 1]; }

    /// @brief Pre-increment operator to advance the reverse iterator to the next element (which is actually the
    ///        previous element
    /// @return A reference to the reverse iterator after it has been advanced.
    constexpr TReverseIterator& operator++() noexcept
    {
        --m_base;
        return *this;
    }

    /// @brief Post-increment operator to advance the reverse iterator to the next element (which is actually the
    ///        previous element), returning the original position.
    /// @return A copy of the reverse iterator before it was advanced.
    constexpr TReverseIterator operator++(int) noexcept
    {
        TReverseIterator tmp(*this);
        --m_base;
        return tmp;
    }

    /// @brief Pre-decrement operator to move the reverse iterator to the previous element (which is actually the next
    ///        element).
    /// @return A reference to the reverse iterator after it has been moved.
    constexpr TReverseIterator& operator--() noexcept
    {
        ++m_base;
        return *this;
    }

    /// @brief Post-decrement operator to move the reverse iterator to the previous element (which is actually the next
    ///        element), returning the original position.
    /// @return A copy of the reverse iterator before it was moved.
    constexpr TReverseIterator operator--(int) noexcept
    {
        TReverseIterator tmp(*this);
        ++m_base;
        return tmp;
    }

    /// @brief Compound assignment operator to advance the reverse iterator by a specified number of elements (which
    ///        actually moves it back).
    /// @param n The number of elements to advance the reverse iterator (can be positive or negative).
    /// @return A reference to the reverse iterator after it has been advanced.
    constexpr TReverseIterator& operator+=(DifferenceType n) noexcept
    {
        m_base -= n;
        return *this;
    }

    /// @brief Compound assignment operator to move the reverse iterator back by a specified number of elements (which
    ///        actually advances it).
    /// @param n The number of elements to move the reverse iterator back (can be positive or negative).
    /// @return A reference to the reverse iterator after it has been moved back.
    constexpr TReverseIterator& operator-=(DifferenceType n) noexcept
    {
        m_base += n;
        return *this;
    }

    /// @brief Binary addition operator to create a new reverse iterator advanced by a specified number of elements
    ///        (which actually moves it back).
    /// @param n The number of elements to advance the reverse iterator (can be positive or negative).
    /// @return A new reverse iterator that is advanced by the specified number of elements from the current position.
    GP_NODISCARD constexpr TReverseIterator operator+(DifferenceType n) const noexcept
    {
        return TReverseIterator(m_base - n);
    }

    /// @brief Binary subtraction operator to create a new reverse iterator moved back by a specified number of elements
    ///        (which actually advances it).
    /// @param n The number of elements to move the reverse iterator back (can be positive or negative).
    /// @return A new reverse iterator that is moved back by the specified number of elements from the current position.
    GP_NODISCARD constexpr TReverseIterator operator-(DifferenceType n) const noexcept
    {
        return TReverseIterator(m_base + n);
    }

    /// @brief Binary addition operator to create a new reverse iterator advanced by a specified number of elements
    ///        (which actually moves it back), with the reverse iterator on the right-hand side.
    /// @param n The number of elements to advance the reverse iterator (can be positive or negative).
    /// @param it The reverse iterator to advance (must be the same type as the current reverse iterator).
    /// @return A new reverse iterator that is advanced by the specified number of elements from the position of the
    ///         provided reverse iterator.
    GP_NODISCARD friend constexpr TReverseIterator operator+(DifferenceType n, const TReverseIterator& it) noexcept
    {
        return TReverseIterator(it.m_base - n);
    }

    /// @brief Binary subtraction operator to calculate the distance between two reverse iterators.
    /// @param other The other reverse iterator to subtract from the current reverse iterator (must be the same type as
    ///              the current reverse iterator).
    /// @return The distance between the two reverse iterators, calculated as the difference between their base
    ///         iterators in reverse order.
    GP_NODISCARD constexpr DifferenceType operator-(const TReverseIterator& other) const noexcept
    {
        return other.m_base - m_base;
    }

    /// @brief Equality operator to compare two reverse iterators for equality.
    /// @param other The other reverse iterator to compare with (must be the same type as the current reverse iterator).
    /// @return True if the reverse iterators point to the same position, false otherwise.
    GP_NODISCARD constexpr bool operator==(const TReverseIterator& other) const noexcept
    {
        return m_base == other.m_base;
    }

    /// @brief Inequality operator to compare two reverse iterators for inequality.
    /// @param other The other reverse iterator to compare with (must be the same type as the current reverse iterator).
    /// @return True if the reverse iterators point to different positions, false if they point to the same position.
    GP_NODISCARD constexpr bool operator!=(const TReverseIterator& other) const noexcept
    {
        return m_base != other.m_base;
    }

    /// @brief Relational operators to compare the positions of two reverse iterators.
    /// @param other The other reverse iterator to compare with (must be the same type as the current reverse iterator).
    /// @return True if the current reverse iterator points to a position after the other reverse iterator, false
    ///         otherwise.
    GP_NODISCARD constexpr bool operator<(const TReverseIterator& other) const noexcept
    {
        return m_base > other.m_base;
    }

    /// @brief Less-than-or-equal operator to compare the positions of two reverse iterators.
    /// @param other The other reverse iterator to compare with (must be the same type as the current reverse iterator).
    /// @return True if the current reverse iterator points to a position after or the same as the other reverse
    ///         iterator, false otherwise.
    GP_NODISCARD constexpr bool operator<=(const TReverseIterator& other) const noexcept
    {
        return m_base >= other.m_base;
    }

    /// @brief Greater-than operator to compare the positions of two reverse iterators.
    /// @param other The other reverse iterator to compare with (must be the same type as the current reverse iterator).
    /// @return True if the current reverse iterator points to a position before the other reverse iterator, false
    ///         otherwise.
    GP_NODISCARD constexpr bool operator>(const TReverseIterator& other) const noexcept
    {
        return m_base < other.m_base;
    }

    /// @brief Greater-than-or-equal operator to compare the positions of two reverse iterators.
    /// @param other The other reverse iterator to compare with (must be the same type as the current reverse iterator).
    /// @return True if the current reverse iterator points to a position before or the same as the other reverse
    ///         iterator, false otherwise.
    GP_NODISCARD constexpr bool operator>=(const TReverseIterator& other) const noexcept
    {
        return m_base <= other.m_base;
    }

public:
    /// @brief Accessor to retrieve the underlying base iterator from the reverse iterator.
    /// @return The underlying base iterator that this reverse iterator wraps.
    GP_NODISCARD constexpr TIter GetBase() const noexcept { return m_base; }
};

/// @brief Convenience type alias for a reverse iterator that wraps a TIterator. Provides reverse traversal capabilities
///        for a TIterator.
template <typename T>
using TReverseIteratorT = TReverseIterator<TIterator<T>>;

/// @brief Convenience type alias for a const reverse iterator that wraps a TIterator. Provides reverse traversal
///        capabilities for a TIterator with a const-qualified value type.
template <typename T>
using TConstReverseIterator = TReverseIterator<TIterator<const T>>;

}   // namespace GP
