// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/Utility/Iterator.hpp"
#include "Container/Utility/ReverseIterator.hpp"
#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include "Math/Scalar/Min.hpp"
#include "Templates/Concepts/IteratorConcepts.hpp"
#include "Templates/Concepts/TypeConcepts.hpp"
#include <compare>
#include <cstring>

namespace GP
{

/// @brief A non-owning reference to a contiguous sequence of characters.
/// @tparam T The character type (e.g., char, wchar_t, char16_t).
/// @warning This class does not manage the lifetime of the underlying data. Ensure the source string outlives the view
///          to avoid dangling pointers. String views are NOT guaranteed to be null-terminated.
template <CCharacter T>
class TStringView
{
public:
    using ValueType = T;
    using SizeType = SizeT;
    using DiffType = SSizeT;
    using Pointer = const T*;
    using ConstPointer = const T*;
    using Reference = const T&;
    using ConstReference = const T&;
    using Iterator = TConstIterator<T>;
    using ConstIterator = TConstIterator<T>;
    using ReverseIterator = TConstReverseIterator<T>;
    using ConstReverseIterator = TConstReverseIterator<T>;

public:
    static constexpr SizeType NPOS = static_cast<SizeType>(-1);   //<! Sentinel value representing "not found".

private:
    ConstPointer m_data{ nullptr };   //<! Pointer to the first character in the string view.
    SizeType m_size{ 0 };             //<! The number of characters in the string view.

public:
    constexpr TStringView() noexcept = default;

    /// @brief Constructs from a raw pointer.
    /// @note This constructor has O(n) complexity as it calls strlen/wcslen.
    /// @param str Null-terminated character array.
    constexpr TStringView(const T* str) noexcept
        : m_data(str)
        , m_size(str ? ::strlen(str) : 0)
    {}

    constexpr TStringView(const T* str, SizeType length) noexcept
        : m_data(str)
        , m_size(length)
    {}

    /// @note Allow implicit construction from compatible string-like types (e.g., TString) that provide Data() and
    ///       Size() methods.
    template <typename TStr>
    requires CContiguousRange<TStr> && CSizedRange<TStr> constexpr TStringView(const TStr& str) noexcept
        : m_data(str.Data())
        , m_size(str.Size())
    {}

public:
    ///
    /// @section Operators
    ///

    GP_NODISCARD constexpr ConstReference operator[](SizeType index) const noexcept
    {
        // GP_ASSERT(i < m_size);
        return m_data[index];
    }

    GP_NODISCARD constexpr bool operator==(TStringView other) const noexcept
    {
        return m_size == other.m_size && ::memcmp(m_data, other.m_data, m_size * sizeof(T)) == 0;
    }

    GP_NODISCARD constexpr bool operator!=(TStringView other) const noexcept { return !(*this == other); }

    GP_NODISCARD constexpr auto operator<=>(TStringView other) const noexcept
    {
        if (int cmp = ::memcmp(m_data, other.m_data, GP::Min(m_size, other.m_size) * sizeof(T)); cmp != 0)
        {
            return cmp < 0 ? std::strong_ordering::less : std::strong_ordering::greater;
        }
        return m_size <=> other.m_size;
    }

public:
    ///
    /// @section Capacity
    ///

    // clang-format off

    GP_NODISCARD constexpr SizeType Size() const noexcept { return m_size; }
    GP_NODISCARD constexpr bool IsEmpty() const noexcept { return m_size == 0; }

    // clang-format on

    ///
    /// @section Element Access
    ///

    GP_NODISCARD constexpr ConstReference At(SizeType index) const noexcept
    {
        GP_ASSERT(index < m_size);
        return m_data[index];
    }

    GP_NODISCARD constexpr ConstReference Front() const noexcept
    {
        GP_ASSERT(!IsEmpty());
        return m_data[0];
    }

    GP_NODISCARD constexpr ConstReference Back() const noexcept
    {
        GP_ASSERT(!IsEmpty());
        return m_data[m_size - 1];
    }

    GP_NODISCARD constexpr ConstPointer Data() const noexcept { return m_data; }

    ///
    /// @section Iterators
    ///

    // clang-format off

    GP_NODISCARD constexpr Iterator begin() const noexcept { return Iterator(m_data); }
    GP_NODISCARD constexpr Iterator end() const noexcept { return Iterator(m_data + m_size); }
    GP_NODISCARD constexpr ConstIterator cbegin() const noexcept { return begin(); }
    GP_NODISCARD constexpr ConstIterator cend() const noexcept { return end(); }
    GP_NODISCARD constexpr ReverseIterator rbegin() const noexcept { return ReverseIterator(end()); }
    GP_NODISCARD constexpr ReverseIterator rend() const noexcept { return ReverseIterator(begin()); }
    GP_NODISCARD constexpr ConstReverseIterator crbegin() const noexcept { return rbegin(); }
    GP_NODISCARD constexpr ConstReverseIterator crend() const noexcept { return rend(); }

    // clang-format on

    ///
    /// @section Sub-views
    ///

    /// @brief Returns a view of the first N characters.
    GP_NODISCARD constexpr TStringView First(SizeType count) const noexcept
    {
        GP_ASSERT(count <= m_size);
        return TStringView(m_data, count);
    }

    /// @brief Returns a view of the last N characters.
    GP_NODISCARD constexpr TStringView Last(SizeType count) const noexcept
    {
        GP_ASSERT(count <= m_size);
        return TStringView(m_data + m_size - count, count);
    }

    /// @brief Returns a sub-view starting at pos with the given length.
    /// @note If len is NPOS or greater than the remaining size, the result is clamped to the end.
    GP_NODISCARD constexpr TStringView Substr(SizeType pos = 0, SizeType len = NPOS) const noexcept
    {
        GP_ASSERT(pos <= m_size);
        SizeType n = len == NPOS ? m_size - pos : GP::Min(len, m_size - pos);
        return TStringView(m_data + pos, n);
    }

    ///
    /// @section Lookups
    ///

    /// @brief Searches for a character. Complexity: O(n).
    /// @return Index of the character or NPOS if not found.
    GP_NODISCARD constexpr SizeType Find(T ch, SizeType pos = 0) const noexcept
    {
        for (SizeType i = pos; i < m_size; ++i)
        {
            if (m_data[i] == ch) { return i; }
        }
        return NPOS;
    }

    /// @brief Searches for a sub-view. Complexity: O(n * m).
    /// @return Start index of the sub-view or NPOS if not found.
    GP_NODISCARD constexpr SizeType Find(TStringView sv, SizeType pos = 0) const noexcept
    {
        if (sv.m_size == 0) { return pos <= m_size ? pos : NPOS; }
        if (pos + sv.m_size > m_size) { return NPOS; }
        for (SizeType i = pos; i <= m_size - sv.m_size; ++i)
        {
            if (::memcmp(m_data + i, sv.m_data, sv.m_size) == 0) { return i; }
        }
        return NPOS;
    }

    GP_NODISCARD constexpr SizeType RFind(T ch, SizeType pos = NPOS) const noexcept
    {
        if (m_size == 0) { return NPOS; }
        SizeType start = pos == NPOS ? m_size - 1 : GP::Min(pos, m_size - 1);
        for (DiffType i = static_cast<DiffType>(start); i >= 0; --i)
        {
            if (m_data[i] == ch) { return static_cast<SizeType>(i); }
        }
        return NPOS;
    }

    GP_NODISCARD constexpr bool StartsWith(TStringView prefix) const noexcept
    {
        return m_size >= prefix.m_size && ::memcmp(m_data, prefix.m_data, prefix.m_size) == 0;
    }

    GP_NODISCARD constexpr bool StartsWith(T ch) const noexcept { return !IsEmpty() && m_data[0] == ch; }

    GP_NODISCARD constexpr bool EndsWith(TStringView suffix) const noexcept
    {
        return m_size >= suffix.m_size && ::memcmp(m_data + m_size - suffix.m_size, suffix.m_data, suffix.m_size) == 0;
    }

    GP_NODISCARD constexpr bool EndsWith(T ch) const noexcept { return !IsEmpty() && m_data[m_size - 1] == ch; }

    GP_NODISCARD constexpr bool Contains(TStringView sv) const noexcept { return Find(sv) != NPOS; }

    GP_NODISCARD constexpr bool Contains(T ch) const noexcept { return Find(ch) != NPOS; }
};

}   // namespace GP
