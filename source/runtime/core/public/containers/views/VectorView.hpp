// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/ContainerForward.hpp"
#include "CoreMinimal.hpp"
#include "platforms/base/Platform.hpp"
#include <algorithm>
#include <compare>
#include <initializer_list>
#include <iterator>
#include <type_traits>

namespace gp::container
{

/// @brief Non-owning, 16-byte contiguous sequence view standardized on `gp::USize` (ADR-0056).
/// @details Decouples engine APIs from concrete container allocators (`gp::Vector`, `gp::InlineVector`,
///          `gp::FixedVector`, `gp::ScratchVector`, `gp::PolymorphicVector`, `gp::Array`, and C arrays).
///          Supports both mutable views (`gp::VectorView<T>`) and read-only views (`gp::ConstVectorView<T>` =
///          `gp::VectorView<const T>`). Trivially copyable and passed in two 64-bit registers (`{ m_data, m_size }`).
/// @tparam InT The element type (may be `const`-qualified for read-only views).
template <typename InT>
class VectorView
{
public:
    using ElementType = InT;
    using ValueType = std::remove_cv_t<InT>;
    using SizeType = gp::USize;
    using DifferenceType = gp::ISize;
    using Pointer = InT*;
    using ConstPointer = const ValueType*;
    using Reference = InT&;
    using ConstReference = const ValueType&;
    using Iterator = Pointer;
    using ConstIterator = ConstPointer;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

    /// @brief Sentinel index returned when a search operation fails to find a matching element.
    static constexpr SizeType npos = static_cast<SizeType>(-1);

public:
    /// @brief Default constructor. Initializes an empty view (`nullptr`, size `0`).
    [[nodiscard]] constexpr VectorView() noexcept = default;

    /// @brief Constructs an empty view from `nullptr`.
    [[nodiscard]] constexpr VectorView(std::nullptr_t) noexcept
        : m_data(nullptr)
        , m_size(0u)
    {}

    /// @brief Constructs a view over `count` contiguous elements starting at `data`.
    /// @param[in] data Pointer to the first element (may be `nullptr` if `count == 0`).
    /// @param[in] count Number of elements in the view.
    [[nodiscard]] constexpr VectorView(const Pointer data, const SizeType count) noexcept
        : m_data(data)
        , m_size(count)
    {
        GP_ASSERT(count == 0u || data != nullptr, "VectorView constructed with nullptr and non-zero count.");
    }

    /// @brief Constructs a view over the contiguous pointer range `[first, last)`.
    /// @param[in] first Pointer to the first element.
    /// @param[in] last Pointer one past the last element (`last >= first`).
    [[nodiscard]] constexpr VectorView(const Pointer first, const Pointer last) noexcept
        : m_data(first)
        , m_size((first != nullptr && last != nullptr && last >= first) ? static_cast<SizeType>(last - first) : 0u)
    {
        GP_ASSERT(first <= last, "VectorView range constructor requires first <= last.");
    }

    /// @brief Constructs a view over a native C-style array `arr[InN]`.
    /// @tparam InOtherT Array element type (must be qualification-convertible to `InT`).
    /// @tparam InN Number of elements in the C array.
    /// @param[in] arr Reference to the C array.
    template <typename InOtherT, gp::USize InN>
    requires(std::is_convertible_v<InOtherT (*)[], InT (*)[]>)
    [[nodiscard]] constexpr VectorView(InOtherT (&arr)[InN]) noexcept
        : m_data(arr)
        , m_size(InN)
    {}

    /// @brief Constructs a mutable or const view from a mutable `gp::Array`.
    /// @tparam InOtherT Array element type.
    /// @tparam InN Array element count.
    /// @tparam InAlign Array alignment.
    /// @param[in] arr Source `gp::Array`.
    template <typename InOtherT, gp::USize InN, gp::USize InAlign>
    requires(std::is_convertible_v<InOtherT (*)[], InT (*)[]>)
    [[nodiscard]] constexpr VectorView(Array<InOtherT, InN, InAlign>& arr) noexcept
        : m_data(arr.data())
        , m_size(arr.size())
    {}

    /// @brief Constructs a read-only view from a `const gp::Array`.
    /// @tparam InOtherT Array element type.
    /// @tparam InN Array element count.
    /// @tparam InAlign Array alignment.
    /// @param[in] arr Source `const gp::Array`.
    template <typename InOtherT, gp::USize InN, gp::USize InAlign>
    requires(std::is_convertible_v<const InOtherT (*)[], InT (*)[]>)
    [[nodiscard]] constexpr VectorView(const Array<InOtherT, InN, InAlign>& arr) noexcept
        : m_data(arr.data())
        , m_size(arr.size())
    {}

    /// @brief Constructs a mutable or const view from a mutable `gp::Vector` with any allocator.
    /// @tparam InOtherT Vector element type.
    /// @tparam InAllocator Vector allocator type.
    /// @param[in] vec Source `gp::Vector`.
    template <typename InOtherT, typename InAllocator>
    requires(std::is_convertible_v<InOtherT (*)[], InT (*)[]>)
    [[nodiscard]] constexpr VectorView(Vector<InOtherT, InAllocator>& vec) noexcept
        : m_data(vec.data())
        , m_size(vec.size())
    {}

    /// @brief Constructs a read-only view from a `const gp::Vector` with any allocator.
    /// @tparam InOtherT Vector element type.
    /// @tparam InAllocator Vector allocator type.
    /// @param[in] vec Source `const gp::Vector`.
    template <typename InOtherT, typename InAllocator>
    requires(std::is_convertible_v<const InOtherT (*)[], InT (*)[]>)
    [[nodiscard]] constexpr VectorView(const Vector<InOtherT, InAllocator>& vec) noexcept
        : m_data(vec.data())
        , m_size(vec.size())
    {}

    /// @brief Implicit qualification conversion from `VectorView<InOtherT>` (e.g. `VectorView<T>` to
    ///        `VectorView<const T>`).
    /// @tparam InOtherT Source view element type.
    /// @param[in] other Source view.
    template <typename InOtherT>
    requires(!concepts::IsSameAs<InOtherT, InT> && std::is_convertible_v<InOtherT (*)[], InT (*)[]>)
    [[nodiscard]] constexpr VectorView(const VectorView<InOtherT>& other) noexcept
        : m_data(other.data())
        , m_size(other.size())
    {}

    /// @brief Constructs a read-only view from an `std::initializer_list<ValueType>` (enabled only when `InT` is
    ///        `const`).
    /// @param[in] init Initializer list.
    [[nodiscard]] constexpr VectorView(const std::initializer_list<ValueType> init) noexcept
        requires concepts::IsConst<InT>
        : m_data(init.begin())
        , m_size(init.size())
    {}

    /// @brief Copy constructor and copy assignment operator (trivially copyable).
    [[nodiscard]] constexpr VectorView(const VectorView&) noexcept = default;
    constexpr VectorView& operator=(const VectorView&) noexcept = default;

public:
    /// @brief Unchecked element access by index.
    /// @param[in] index Zero-based index (`index < size()`).
    /// @return Reference to `m_data[index]`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Reference operator[](const SizeType index) const noexcept
    {
        GP_ASSERT(index < m_size, "VectorView::operator[] index out of bounds.");
        return m_data[index];
    }

    /// @brief Bounds-checked element access by index.
    /// @param[in] index Zero-based index (`index < size()`).
    /// @return Reference to `m_data[index]`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Reference at(const SizeType index) const noexcept
    {
        GP_ASSERT(index < m_size, "VectorView::at index out of bounds.");
        return m_data[index];
    }

    /// @brief Returns a reference to the first element in the view.
    /// @return Reference to `m_data[0]`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Reference front() const noexcept
    {
        GP_ASSERT(m_size > 0u, "VectorView::front called on an empty view.");
        return m_data[0u];
    }

    /// @brief Returns a reference to the last element in the view.
    /// @return Reference to `m_data[size() - 1]`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Reference back() const noexcept
    {
        GP_ASSERT(m_size > 0u, "VectorView::back called on an empty view.");
        return m_data[m_size - 1u];
    }

    /// @brief Accesses an element indexed backward from the end of the view (`0` = last element).
    /// @param[in] indexFromEnd Offset backward from `size() - 1`.
    /// @return Reference to `m_data[size() - 1 - indexFromEnd]`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Reference lastElement(const SizeType indexFromEnd = 0u) const noexcept
    {
        GP_ASSERT(indexFromEnd < m_size, "VectorView::lastElement indexFromEnd out of bounds.");
        return m_data[m_size - 1u - indexFromEnd];
    }

    /// @brief Returns a pointer to the underlying contiguous element array.
    /// @return Pointer to the first element, or `nullptr` if empty.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Pointer data() const noexcept
    {
        return m_data;
    }

    /// @brief Returns the number of elements in the view.
    /// @return Element count.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType size() const noexcept
    {
        return m_size;
    }

    /// @brief Returns the total byte size spanned by the view (`size() * sizeof(InT)`).
    /// @return Byte count.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType sizeInBytes() const noexcept
    {
        return m_size * sizeof(InT);
    }

    /// @brief Checks whether the view is empty (`size() == 0`).
    /// @return True if empty, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isEmpty() const noexcept
    {
        return m_size == 0u;
    }

    /// @brief Checks whether `index` lies within `[0, size())`.
    /// @param[in] index Index to test.
    /// @return True if `index < size()`, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isValidIndex(const SizeType index) const noexcept
    {
        return index < m_size;
    }

public:
    /// @brief Returns an iterator to the beginning of the view.
    /// @return Pointer to the first element.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Iterator begin() const noexcept
    {
        return m_data;
    }

    /// @brief Returns an iterator one past the last element of the view.
    /// @return Pointer to `m_data + m_size`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Iterator end() const noexcept
    {
        return m_data + m_size;
    }

    /// @brief Returns a const iterator to the beginning of the view.
    /// @return Const pointer to the first element.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstIterator cbegin() const noexcept
    {
        return m_data;
    }

    /// @brief Returns a const iterator one past the last element of the view.
    /// @return Const pointer to `m_data + m_size`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstIterator cend() const noexcept
    {
        return m_data + m_size;
    }

    /// @brief Returns a reverse iterator to the last element of the view.
    /// @return `ReverseIterator(end())`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ReverseIterator rbegin() const noexcept
    {
        return ReverseIterator(end());
    }

    /// @brief Returns a reverse iterator one before the first element of the view.
    /// @return `ReverseIterator(begin())`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ReverseIterator rend() const noexcept
    {
        return ReverseIterator(begin());
    }

    /// @brief Returns a const reverse iterator to the last element of the view.
    /// @return `ConstReverseIterator(cend())`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstReverseIterator crbegin() const noexcept
    {
        return ConstReverseIterator(cend());
    }

    /// @brief Returns a const reverse iterator one before the first element of the view.
    /// @return `ConstReverseIterator(cbegin())`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstReverseIterator crend() const noexcept
    {
        return ConstReverseIterator(cbegin());
    }

public:
    /// @brief Returns a sub-view starting at `offset` with `count` elements (or until the end if `count == npos`).
    /// @param[in] offset Starting element index (`offset <= size()`).
    /// @param[in] count Number of elements to include (defaults to `npos`).
    /// @return Sub-view spanning `[offset, offset + effectiveCount)`.
    [[nodiscard]] constexpr VectorView slice(const SizeType offset, const SizeType count = npos) const noexcept
    {
        GP_ASSERT(offset <= m_size, "VectorView::slice offset out of bounds.");
        const SizeType remaining = m_size - offset;
        const SizeType effectiveCount = (count == npos) ? remaining : count;
        GP_ASSERT(effectiveCount <= remaining, "VectorView::slice count exceeds remaining elements.");
        return VectorView(m_data + offset, effectiveCount);
    }

    /// @brief Returns a sub-view of the first `count` elements.
    /// @param[in] count Number of leading elements (`count <= size()`).
    /// @return Sub-view `[0, count)`.
    [[nodiscard]] constexpr VectorView first(const SizeType count) const noexcept
    {
        GP_ASSERT(count <= m_size, "VectorView::first count exceeds view size.");
        return VectorView(m_data, count);
    }

    /// @brief Returns a sub-view of the last `count` elements.
    /// @param[in] count Number of trailing elements (`count <= size()`).
    /// @return Sub-view `[size() - count, size())`.
    [[nodiscard]] constexpr VectorView last(const SizeType count) const noexcept
    {
        GP_ASSERT(count <= m_size, "VectorView::last count exceeds view size.");
        return VectorView(m_data + (m_size - count), count);
    }

    /// @brief Returns a new view with the first `count` elements removed.
    /// @param[in] count Number of leading elements to drop (`count <= size()`).
    /// @return Sub-view `[count, size())`.
    [[nodiscard]] constexpr VectorView dropFirst(const SizeType count = 1u) const noexcept
    {
        GP_ASSERT(count <= m_size, "VectorView::dropFirst count exceeds view size.");
        return VectorView(m_data + count, m_size - count);
    }

    /// @brief Returns a new view with the last `count` elements removed.
    /// @param[in] count Number of trailing elements to drop (`count <= size()`).
    /// @return Sub-view `[0, size() - count)`.
    [[nodiscard]] constexpr VectorView dropLast(const SizeType count = 1u) const noexcept
    {
        GP_ASSERT(count <= m_size, "VectorView::dropLast count exceeds view size.");
        return VectorView(m_data, m_size - count);
    }

    /// @brief Advances the start of this view in-place by `count` elements.
    /// @param[in] count Number of leading elements to remove (`count <= size()`).
    constexpr void removePrefix(const SizeType count) noexcept
    {
        GP_ASSERT(count <= m_size, "VectorView::removePrefix count exceeds view size.");
        m_data += count;
        m_size -= count;
    }

    /// @brief Shrinks the end of this view in-place by `count` elements.
    /// @param[in] count Number of trailing elements to remove (`count <= size()`).
    constexpr void removeSuffix(const SizeType count) noexcept
    {
        GP_ASSERT(count <= m_size, "VectorView::removeSuffix count exceeds view size.");
        m_size -= count;
    }

    /// @brief Reinterprets the viewed sequence as a read-only byte view (`VectorView<const gp::UInt8>`).
    /// @return Read-only byte view of length `sizeInBytes()`.
    [[nodiscard]] VectorView<const gp::UInt8> asBytes() const noexcept
    {
        return VectorView<const gp::UInt8>(reinterpret_cast<const gp::UInt8*>(m_data), sizeInBytes());
    }

    /// @brief Reinterprets the viewed sequence as a writable byte view (`VectorView<gp::UInt8>`).
    /// @return Mutable byte view of length `sizeInBytes()`.
    [[nodiscard]] VectorView<gp::UInt8> asWritableBytes() const noexcept requires(!concepts::IsConst<InT>)
    {
        return VectorView<gp::UInt8>(reinterpret_cast<gp::UInt8*>(m_data), sizeInBytes());
    }

public:
    /// @brief Finds the first occurrence of `value` in the view.
    /// @param[in] value Value to search for.
    /// @return Iterator to the first matching element, or `end()` if not found.
    [[nodiscard]] constexpr Iterator find(const ValueType& value) const noexcept
    {
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (m_data[i] == value)
            {
                return m_data + i;
            }
        }
        return end();
    }

    /// @brief Finds the first element satisfying `predicate`.
    /// @tparam InPredicate Unary predicate `(ConstReference) -> bool`.
    /// @param[in] predicate Search condition.
    /// @return Iterator to the first matching element, or `end()` if not found.
    template <typename InPredicate>
    [[nodiscard]] constexpr Iterator findIf(InPredicate predicate) const noexcept
    {
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (predicate(m_data[i]))
            {
                return m_data + i;
            }
        }
        return end();
    }

    /// @brief Finds the last occurrence of `value` in the view.
    /// @param[in] value Value to search for.
    /// @return Iterator to the last matching element, or `end()` if not found.
    [[nodiscard]] constexpr Iterator findLast(const ValueType& value) const noexcept
    {
        for (SizeType i = m_size; i > 0u; --i)
        {
            if (m_data[i - 1u] == value)
            {
                return m_data + (i - 1u);
            }
        }
        return end();
    }

    /// @brief Finds the first index of `value` in the view.
    /// @param[in] value Value to search for.
    /// @return Zero-based index of the first match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType indexOf(const ValueType& value) const noexcept
    {
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (m_data[i] == value)
            {
                return i;
            }
        }
        return npos;
    }

    /// @brief Finds the first index of an element satisfying `predicate`.
    /// @tparam InPredicate Unary predicate `(ConstReference) -> bool`.
    /// @param[in] predicate Search condition.
    /// @return Zero-based index of the first match, or `npos` if not found.
    template <typename InPredicate>
    [[nodiscard]] constexpr SizeType indexOfIf(InPredicate predicate) const noexcept
    {
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (predicate(m_data[i]))
            {
                return i;
            }
        }
        return npos;
    }

    /// @brief Finds the last index of `value` in the view.
    /// @param[in] value Value to search for.
    /// @return Zero-based index of the last match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType lastIndexOf(const ValueType& value) const noexcept
    {
        for (SizeType i = m_size; i > 0u; --i)
        {
            if (m_data[i - 1u] == value)
            {
                return i - 1u;
            }
        }
        return npos;
    }

    /// @brief Finds the last index of an element satisfying `predicate`.
    /// @tparam InPredicate Unary predicate `(ConstReference) -> bool`.
    /// @param[in] predicate Search condition.
    /// @return Zero-based index of the last match, or `npos` if not found.
    template <typename InPredicate>
    [[nodiscard]] constexpr SizeType lastIndexOfIf(InPredicate predicate) const noexcept
    {
        for (SizeType i = m_size; i > 0u; --i)
        {
            if (predicate(m_data[i - 1u]))
            {
                return i - 1u;
            }
        }
        return npos;
    }

    /// @brief Checks whether the view contains at least one element equal to `value`.
    /// @param[in] value Value to search for.
    /// @return True if found, false otherwise.
    [[nodiscard]] constexpr bool contains(const ValueType& value) const noexcept
    {
        return indexOf(value) != npos;
    }

    /// @brief Checks whether the view contains at least one element satisfying `predicate`.
    /// @tparam InPredicate Unary predicate `(ConstReference) -> bool`.
    /// @param[in] predicate Search condition.
    /// @return True if found, false otherwise.
    template <typename InPredicate>
    [[nodiscard]] constexpr bool containsIf(InPredicate predicate) const noexcept
    {
        return indexOfIf(predicate) != npos;
    }

    /// @brief Counts the number of elements equal to `value`.
    /// @param[in] value Value to count.
    /// @return Number of matching elements.
    [[nodiscard]] constexpr SizeType count(const ValueType& value) const noexcept
    {
        SizeType matches = 0u;
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (m_data[i] == value)
            {
                ++matches;
            }
        }
        return matches;
    }

    /// @brief Counts the number of elements satisfying `predicate`.
    /// @tparam InPredicate Unary predicate `(ConstReference) -> bool`.
    /// @param[in] predicate Search condition.
    /// @return Number of matching elements.
    template <typename InPredicate>
    [[nodiscard]] constexpr SizeType countIf(InPredicate predicate) const noexcept
    {
        SizeType matches = 0u;
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (predicate(m_data[i]))
            {
                ++matches;
            }
        }
        return matches;
    }

    /// @brief Checks whether this view begins with all elements of `prefix`.
    /// @tparam InOtherT Prefix element type.
    /// @param[in] prefix Prefix sequence to match.
    /// @return True if `size() >= prefix.size()` and leading elements compare equal.
    template <typename InOtherT>
    requires(concepts::IsSameAs<std::remove_cv_t<InOtherT>, ValueType>)
    [[nodiscard]] constexpr bool startsWith(const VectorView<InOtherT>& prefix) const noexcept
    {
        if (prefix.size() > m_size)
        {
            return false;
        }
        return first(prefix.size()) == prefix;
    }

    /// @brief Checks whether this view ends with all elements of `suffix`.
    /// @tparam InOtherT Suffix element type.
    /// @param[in] suffix Suffix sequence to match.
    /// @return True if `size() >= suffix.size()` and trailing elements compare equal.
    template <typename InOtherT>
    requires(concepts::IsSameAs<std::remove_cv_t<InOtherT>, ValueType>)
    [[nodiscard]] constexpr bool endsWith(const VectorView<InOtherT>& suffix) const noexcept
    {
        if (suffix.size() > m_size)
        {
            return false;
        }
        return last(suffix.size()) == suffix;
    }

    /// @brief Returns the index of the first element `>= value` in a sorted view, or `size()` if all are smaller.
    /// @param[in] value Value to search for.
    /// @return Lower-bound insertion index in `[0, size()]`.
    [[nodiscard]] constexpr SizeType lowerBound(const ValueType& value) const noexcept
    {
        const ConstIterator it = std::ranges::lower_bound(cbegin(), cend(), value);
        return static_cast<SizeType>(it - cbegin());
    }

    /// @brief Returns the index of the first element `> value` in a sorted view, or `size()` if none are greater.
    /// @param[in] value Value to search for.
    /// @return Upper-bound insertion index in `[0, size()]`.
    [[nodiscard]] constexpr SizeType upperBound(const ValueType& value) const noexcept
    {
        const ConstIterator it = std::ranges::upper_bound(cbegin(), cend(), value);
        return static_cast<SizeType>(it - cbegin());
    }

    /// @brief Performs O(log N) binary search for `value` in a sorted view.
    /// @param[in] value Value to search for.
    /// @return Index of a matching element, or `npos` if not found.
    [[nodiscard]] constexpr SizeType binarySearch(const ValueType& value) const noexcept
    {
        const SizeType idx = lowerBound(value);
        if (idx < m_size && (m_data[idx] == value))
        {
            return idx;
        }
        return npos;
    }

    /// @brief Fills all elements in a mutable view with `value`.
    /// @param[in] value Value to assign to each element.
    constexpr void fill(const ValueType& value) const noexcept requires(!concepts::IsConst<InT>)
    {
        for (SizeType i = 0u; i < m_size; ++i)
        {
            m_data[i] = value;
        }
    }

public:
    /// @brief Element-wise equality comparison between two compatible `VectorView` instances.
    /// @tparam InOtherT Right-hand view element type.
    /// @param[in] rhs View to compare against.
    /// @return True if both views have the same size and equal elements.
    template <typename InOtherT>
    requires(concepts::IsSameAs<std::remove_cv_t<InOtherT>, ValueType>)
    [[nodiscard]] constexpr bool operator==(const VectorView<InOtherT>& rhs) const noexcept
    {
        if (m_size != rhs.size())
        {
            return false;
        }

        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (!(m_data[i] == rhs[i]))
            {
                return false;
            }
        }
        return true;
    }

    /// @brief Lexicographical three-way comparison between two compatible `VectorView` instances.
    /// @tparam InOtherT Right-hand view element type.
    /// @param[in] rhs View to compare against.
    /// @return Three-way comparison ordering result.
    template <typename InOtherT>
    requires(concepts::IsSameAs<std::remove_cv_t<InOtherT>, ValueType> && std::three_way_comparable<ValueType>)
    [[nodiscard]] constexpr auto operator<=>(const VectorView<InOtherT>& rhs) const noexcept
    {
        return std::lexicographical_compare_three_way(cbegin(), cend(), rhs.cbegin(), rhs.cend());
    }

private:
    Pointer m_data{ nullptr };
    SizeType m_size{ 0u };
};

/// @brief Deduction guides for `VectorView`.
template <typename InT, gp::USize InN>
VectorView(InT (&)[InN]) -> VectorView<InT>;

template <typename InT, gp::USize InN, gp::USize InAlign>
VectorView(Array<InT, InN, InAlign>&) -> VectorView<InT>;

template <typename InT, gp::USize InN, gp::USize InAlign>
VectorView(const Array<InT, InN, InAlign>&) -> VectorView<const InT>;

template <typename InT, typename InAllocator>
VectorView(Vector<InT, InAllocator>&) -> VectorView<InT>;

template <typename InT, typename InAllocator>
VectorView(const Vector<InT, InAllocator>&) -> VectorView<const InT>;

}   // namespace gp::container

namespace gp::detail
{

/// @brief Helper variable template to determine if a type is a `gp::VectorView`.
template <typename InT>
constexpr bool IsVectorViewV = false;

/// @brief Specialization for `gp::container::VectorView` (`gp::VectorView`) types.
template <typename InT>
constexpr bool IsVectorViewV<gp::container::VectorView<InT>> = true;

}   // namespace gp::detail

namespace gp::concepts
{

/// @brief Concept to check if a type is any instantiation of `gp::VectorView`.
/// @tparam InT The type to check.
template <typename InT>
concept IsVectorView = gp::detail::IsVectorViewV<std::remove_cvref_t<InT>>;

}   // namespace gp::concepts
