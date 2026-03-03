// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/Utility/Iterator.hpp"
#include "Container/Utility/ReverseIterator.hpp"
#include "CoreTypes.hpp"

namespace GP
{

/// @brief Non-owning, bounds-safe view over a contiguous sequence of elements.
///
/// TArrayView<T> references externally-owned memory [data, data + size). It neither allocates
/// nor frees storage. The caller is responsible for ensuring the underlying buffer outlives any
/// TArrayView that refers to it.
///
/// Use TArrayView<const T> for read-only access. Functions that previously accepted
/// const TArray<T>& can accept TArrayView<const T> to remove the header dependency on TArray.
///
/// @tparam T Element type. May be const-qualified for read-only views.
template <typename T>
class TArrayView
{
public:
    using ValueType = T;
    using SizeType = SizeT;
    using DiffType = SSizeT;
    using Pointer = T*;
    using ConstPointer = const T*;
    using Reference = T&;
    using ConstReference = const T&;
    using Iterator = TIterator<T>;
    using ConstIterator = TConstIterator<T>;
    using ReverseIterator = TReverseIteratorT<T>;
    using ConstReverseIterator = TConstReverseIterator<T>;

public:
    /// @brief Sentinel value representing "not found" in search operations.
    static constexpr SizeT NPOS = static_cast<SizeT>(-1);

private:
    T* m_data{ nullptr };
    SizeT m_size{ 0 };

public:
    /// @brief Constructs an empty view (null data, zero size).
    constexpr TArrayView() noexcept = default;

    /// @brief Constructs a view over [data, data + size).
    /// @param data Pointer to the first element.
    /// @param size Number of elements in the view.
    constexpr TArrayView(T* data, SizeT size) noexcept
        : m_data(data)
        , m_size(size)
    {}

    /// @brief Constructs a view from a C-style fixed-size array.
    /// @tparam N Array extent inferred from the argument.
    /// @param arr Reference to the raw array.
    template <SizeT N>
    constexpr TArrayView(T (&arr)[N]) noexcept
        : m_data(arr)
        , m_size(N)
    {}

public:
    /// @brief Accesses element at `index` without bounds checking.
    /// @param index Zero-based element index.
    /// @return Mutable reference to the element.
    GP_NODISCARD constexpr Reference operator[](SizeT index) noexcept
    {
        // GP_ASSERT(index < m_size);
        return m_data[index];
    }

    /// @brief Accesses element at `index` without bounds checking (const overload).
    /// @param index Zero-based element index.
    /// @return Const reference to the element.
    GP_NODISCARD constexpr ConstReference operator[](SizeT index) const noexcept
    {
        // GP_ASSERT(index < m_size);
        return m_data[index];
    }

public:
    /// @brief Returns the number of elements covered by this view.
    GP_NODISCARD constexpr SizeT Size() const noexcept { return m_size; }

    /// @brief Returns true when the view covers zero elements.
    GP_NODISCARD constexpr bool IsEmpty() const noexcept { return m_size == 0; }

    /// @brief Returns a mutable pointer to the first element.
    GP_NODISCARD constexpr Pointer Data() noexcept { return m_data; }

    /// @brief Returns a const pointer to the first element.
    GP_NODISCARD constexpr ConstPointer Data() const noexcept { return m_data; }

    /// @brief Returns a mutable reference to the first element.
    GP_NODISCARD constexpr Reference Front() noexcept
    {
        // GP_ASSERT(!IsEmpty());
        return m_data[0];
    }

    /// @brief Returns a const reference to the first element.
    GP_NODISCARD constexpr ConstReference Front() const noexcept
    {
        // GP_ASSERT(!IsEmpty());
        return m_data[0];
    }

    /// @brief Returns a mutable reference to the last element.
    GP_NODISCARD constexpr Reference Back() noexcept
    {
        // GP_ASSERT(!IsEmpty());
        return m_data[m_size - 1];
    }

    /// @brief Returns a const reference to the last element.
    GP_NODISCARD constexpr ConstReference Back() const noexcept
    {
        // GP_ASSERT(!IsEmpty());
        return m_data[m_size - 1];
    }

    /// @brief Returns a sub-view starting at `offset` with at most `length` elements.
    ///        The length is clamped to the number of elements remaining after `offset`.
    /// @param offset Start index within this view.
    /// @param length Maximum number of elements in the sub-view.
    /// @return A new TArrayView covering [offset, offset + clamped_length).
    GP_NODISCARD constexpr TArrayView SubView(SizeT offset, SizeT length = NPOS) const noexcept
    {
        // GP_ASSERT(offset <= m_size);
        const SizeT avail = (offset <= m_size) ? (m_size - offset) : SizeT(0);
        return TArrayView(m_data + offset, avail < length ? avail : length);
    }

    /// @brief Returns a mutable iterator to the first element.
    GP_NODISCARD constexpr Iterator begin() noexcept { return Iterator(m_data); }

    /// @brief Returns a mutable iterator to one-past-last.
    GP_NODISCARD constexpr Iterator end() noexcept { return Iterator(m_data + m_size); }

    /// @brief Returns a const iterator to the first element.
    GP_NODISCARD constexpr ConstIterator begin() const noexcept { return ConstIterator(m_data); }

    /// @brief Returns a const iterator to one-past-last.
    GP_NODISCARD constexpr ConstIterator end() const noexcept { return ConstIterator(m_data + m_size); }

    /// @brief Returns a const iterator to the first element.
    GP_NODISCARD constexpr ConstIterator cbegin() const noexcept { return begin(); }

    /// @brief Returns a const iterator to one-past-last.
    GP_NODISCARD constexpr ConstIterator cend() const noexcept { return end(); }

    /// @brief Returns a reverse iterator to the last element.
    GP_NODISCARD constexpr ReverseIterator rbegin() noexcept { return ReverseIterator(end()); }

    /// @brief Returns a reverse iterator to one-before-first.
    GP_NODISCARD constexpr ReverseIterator rend() noexcept { return ReverseIterator(begin()); }

    /// @brief Returns a const reverse iterator to the last element.
    GP_NODISCARD constexpr ConstReverseIterator rbegin() const noexcept { return ConstReverseIterator(cend()); }

    /// @brief Returns a const reverse iterator to one-before-first.
    GP_NODISCARD constexpr ConstReverseIterator rend() const noexcept { return ConstReverseIterator(cbegin()); }
};

}   // namespace GP
