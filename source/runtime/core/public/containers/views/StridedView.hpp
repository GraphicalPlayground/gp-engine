// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/ContainerForward.hpp"
#include "containers/views/VectorView.hpp"
#include "CoreMinimal.hpp"
#include "platforms/base/Platform.hpp"
#include <compare>
#include <iterator>
#include <type_traits>

namespace gp::container
{

namespace detail
{

/// @brief Resolves the address of element `index` from `base` using signed byte stride `strideInBytes`.
/// @tparam InT Element type (may be `const`).
/// @param[in] base Pointer to element `0`.
/// @param[in] strideInBytes Signed byte offset between consecutive elements.
/// @param[in] index Signed logical element index.
/// @return Pointer to the element at `index`.
template <typename InT>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr InT* resolveStridedAddress(
    InT* const base, const gp::ISize strideInBytes, const gp::ISize index
) noexcept
{
    if (base == nullptr || index == 0 || strideInBytes == 0)
    {
        return base;
    }

    if (std::is_constant_evaluated())
    {
        const gp::ISize elemSize = static_cast<gp::ISize>(sizeof(InT));
        if ((strideInBytes % elemSize) == 0)
        {
            return base + (index * (strideInBytes / elemSize));
        }
    }

    using ByteType = std::conditional_t<concepts::IsConst<InT>, const gp::UInt8, gp::UInt8>;
    return reinterpret_cast<InT*>(reinterpret_cast<ByteType*>(base) + (index * strideInBytes));
}

}   // namespace detail

/// @brief Random-access iterator over a byte-strided sequence (`gp::StridedView`).
/// @details Stores `{ m_base, m_strideInBytes, m_index }` so iterator subtraction (`it2 - it1`), ordering (`<=>`),
///          zero-stride broadcasting (`m_strideInBytes == 0`), and negative-stride reverse traversal
///          (`m_strideInBytes < 0`) execute in O(1) without integer division.
/// @tparam InT Element type (may be `const`-qualified).
template <typename InT>
class StridedIterator
{
public:
    using iterator_concept = std::random_access_iterator_tag;
    using iterator_category = std::random_access_iterator_tag;
    using value_type = std::remove_cv_t<InT>;
    using difference_type = gp::ISize;
    using pointer = InT*;
    using reference = InT&;

    using ElementType = InT;
    using ValueType = value_type;
    using DifferenceType = difference_type;
    using Pointer = pointer;
    using Reference = reference;

    template <typename InOtherT>
    friend class StridedIterator;

public:
    /// @brief Default constructor.
    [[nodiscard]] constexpr StridedIterator() noexcept = default;

    /// @brief Constructs a strided iterator at logical `index` relative to `base`.
    /// @param[in] base Pointer to element `0` of the strided view.
    /// @param[in] strideInBytes Signed byte stride between consecutive elements.
    /// @param[in] index Logical element index.
    [[nodiscard]] constexpr StridedIterator(
        Pointer const base, const DifferenceType strideInBytes, const DifferenceType index
    ) noexcept
        : m_base(base)
        , m_strideInBytes(strideInBytes)
        , m_index(index)
    {}

    /// @brief Implicit qualification conversion from mutable `StridedIterator<InOtherT>` to const iterator.
    /// @tparam InOtherT Source element type.
    /// @param[in] other Source iterator.
    template <typename InOtherT>
    requires(!concepts::IsSameAs<InOtherT, InT> && std::is_convertible_v<InOtherT (*)[], InT (*)[]>)
    [[nodiscard]] constexpr StridedIterator(const StridedIterator<InOtherT>& other) noexcept
        : m_base(other.m_base)
        , m_strideInBytes(other.m_strideInBytes)
        , m_index(other.m_index)
    {}

    /// @brief Dereferences the iterator to access the current element.
    /// @return Reference to the element at `m_index`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Reference operator*() const noexcept
    {
        return *detail::resolveStridedAddress(m_base, m_strideInBytes, m_index);
    }

    /// @brief Member-access operator for the current element.
    /// @return Pointer to the element at `m_index`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Pointer operator->() const noexcept
    {
        return detail::resolveStridedAddress(m_base, m_strideInBytes, m_index);
    }

    /// @brief Random-access subscript operator relative to the current iterator position.
    /// @param[in] offset Signed element offset from `m_index`.
    /// @return Reference to the element at `m_index + offset`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Reference operator[](const DifferenceType offset) const noexcept
    {
        return *detail::resolveStridedAddress(m_base, m_strideInBytes, m_index + offset);
    }

    /// @brief Pre-increment operator.
    /// @return Reference to `*this`.
    constexpr StridedIterator& operator++() noexcept
    {
        ++m_index;
        return *this;
    }

    /// @brief Post-increment operator.
    /// @return Copy of the iterator prior to incrementing.
    [[nodiscard]] constexpr StridedIterator operator++(int) noexcept
    {
        const StridedIterator copy(*this);
        ++m_index;
        return copy;
    }

    /// @brief Pre-decrement operator.
    /// @return Reference to `*this`.
    constexpr StridedIterator& operator--() noexcept
    {
        --m_index;
        return *this;
    }

    /// @brief Post-decrement operator.
    /// @return Copy of the iterator prior to decrementing.
    [[nodiscard]] constexpr StridedIterator operator--(int) noexcept
    {
        const StridedIterator copy(*this);
        --m_index;
        return copy;
    }

    /// @brief Advances the iterator by `offset` elements.
    /// @param[in] offset Signed element count to advance.
    /// @return Reference to `*this`.
    constexpr StridedIterator& operator+=(const DifferenceType offset) noexcept
    {
        m_index += offset;
        return *this;
    }

    /// @brief Moves the iterator backward by `offset` elements.
    /// @param[in] offset Signed element count to subtract.
    /// @return Reference to `*this`.
    constexpr StridedIterator& operator-=(const DifferenceType offset) noexcept
    {
        m_index -= offset;
        return *this;
    }

    /// @brief Returns an iterator advanced by `offset` elements.
    /// @param[in] offset Signed element offset.
    /// @return New `StridedIterator`.
    [[nodiscard]] constexpr StridedIterator operator+(const DifferenceType offset) const noexcept
    {
        return StridedIterator(m_base, m_strideInBytes, m_index + offset);
    }

    /// @brief Commutative addition of `offset + it`.
    /// @param[in] offset Signed element offset.
    /// @param[in] it Source iterator.
    /// @return New `StridedIterator`.
    [[nodiscard]] friend constexpr StridedIterator
        operator+(const DifferenceType offset, const StridedIterator& it) noexcept
    {
        return StridedIterator(it.m_base, it.m_strideInBytes, it.m_index + offset);
    }

    /// @brief Returns an iterator moved backward by `offset` elements.
    /// @param[in] offset Signed element offset.
    /// @return New `StridedIterator`.
    [[nodiscard]] constexpr StridedIterator operator-(const DifferenceType offset) const noexcept
    {
        return StridedIterator(m_base, m_strideInBytes, m_index - offset);
    }

    /// @brief Computes the signed element distance between two iterators over the same strided view.
    /// @tparam InOtherT Right-hand iterator element type.
    /// @param[in] rhs Iterator to subtract.
    /// @return `m_index - rhs.m_index`.
    template <typename InOtherT>
    requires(concepts::IsSameAs<std::remove_cv_t<InOtherT>, ValueType>)
    [[nodiscard]] constexpr DifferenceType operator-(const StridedIterator<InOtherT>& rhs) const noexcept
    {
        return m_index - rhs.m_index;
    }

    /// @brief Equality comparison between two strided iterators.
    /// @tparam InOtherT Right-hand iterator element type.
    /// @param[in] rhs Iterator to compare against.
    /// @return True if both iterators refer to the same base and logical index.
    template <typename InOtherT>
    requires(concepts::IsSameAs<std::remove_cv_t<InOtherT>, ValueType>)
    [[nodiscard]] constexpr bool operator==(const StridedIterator<InOtherT>& rhs) const noexcept
    {
        return m_index == rhs.m_index && m_base == rhs.m_base;
    }

    /// @brief Three-way ordering comparison between two strided iterators.
    /// @tparam InOtherT Right-hand iterator element type.
    /// @param[in] rhs Iterator to compare against.
    /// @return `std::strong_ordering` of the logical indices.
    template <typename InOtherT>
    requires(concepts::IsSameAs<std::remove_cv_t<InOtherT>, ValueType>)
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const StridedIterator<InOtherT>& rhs) const noexcept
    {
        return m_index <=> rhs.m_index;
    }

private:
    Pointer m_base{ nullptr };
    DifferenceType m_strideInBytes{ 0 };
    DifferenceType m_index{ 0 };
};

/// @brief Non-owning, 24-byte byte-strided sequence view standardized on `gp::USize` and `gp::ISize` (ADR-0056).
/// @details Bridges Array-of-Structures (AoS) and Structure-of-Arrays (SoA) data streams without heap allocations.
///          Supports contiguous strides (`sizeof(InT)`), interleaved AoS member strides (`> sizeof(InT)`),
///          zero-stride constant broadcasting (`0`), and negative-stride reverse traversal (`< 0`).
/// @tparam InT The viewed element type (may be `const`-qualified for read-only views).
template <typename InT>
class StridedView
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
    using Iterator = StridedIterator<InT>;
    using ConstIterator = StridedIterator<const ValueType>;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

    /// @brief Sentinel index returned when a search operation fails to find a matching element.
    static constexpr SizeType npos = static_cast<SizeType>(-1);

public:
    /// @brief Default constructor. Initializes an empty strided view (`nullptr`, size `0`, stride `sizeof(InT)`).
    [[nodiscard]] constexpr StridedView() noexcept = default;

    /// @brief Constructs an empty strided view from `nullptr`.
    [[nodiscard]] constexpr StridedView(std::nullptr_t) noexcept
        : m_data(nullptr)
        , m_size(0u)
        , m_strideInBytes(static_cast<DifferenceType>(sizeof(InT)))
    {}

    /// @brief Constructs a strided view from a first-element pointer, element count, and explicit signed byte stride.
    /// @param[in] data Pointer to the first element (may be `nullptr` if `count == 0`).
    /// @param[in] count Number of elements in the view.
    /// @param[in] strideInBytes Signed byte offset between consecutive elements (must be a multiple of `alignof(InT)`).
    [[nodiscard]] constexpr StridedView(
        Pointer const data,
        const SizeType count,
        const DifferenceType strideInBytes = static_cast<DifferenceType>(sizeof(InT))
    ) noexcept
        : m_data(data)
        , m_size(count)
        , m_strideInBytes(strideInBytes)
    {
        GP_ASSERT(count == 0u || data != nullptr, "StridedView constructed with nullptr and non-zero count.");
        GP_ASSERT(
            (strideInBytes % static_cast<DifferenceType>(alignof(InT))) == 0,
            "StridedView strideInBytes must be an integer multiple of alignof(InT)."
        );
    }

    /// @brief Implicitly constructs a contiguous strided view from a `VectorView`.
    /// @tparam InOtherT Source view element type.
    /// @param[in] view Source contiguous view.
    template <typename InOtherT>
    requires(std::is_convertible_v<InOtherT (*)[], InT (*)[]>)
    [[nodiscard]] constexpr StridedView(const VectorView<InOtherT>& view) noexcept
        : m_data(view.data())
        , m_size(view.size())
        , m_strideInBytes(static_cast<DifferenceType>(sizeof(InT)))
    {}

    /// @brief Implicitly constructs a contiguous strided view from a C-style array `arr[InN]`.
    /// @tparam InOtherT Array element type.
    /// @tparam InN Number of elements in `arr`.
    /// @param[in] arr Reference to the C array.
    template <typename InOtherT, gp::USize InN>
    requires(std::is_convertible_v<InOtherT (*)[], InT (*)[]>)
    [[nodiscard]] constexpr StridedView(InOtherT (&arr)[InN]) noexcept
        : m_data(arr)
        , m_size(InN)
        , m_strideInBytes(static_cast<DifferenceType>(sizeof(InT)))
    {}

    /// @brief Implicitly constructs a contiguous strided view from a mutable `gp::Array`.
    /// @tparam InOtherT Array element type.
    /// @tparam InN Array size.
    /// @tparam InAlign Array alignment.
    /// @param[in] arr Source `gp::Array`.
    template <typename InOtherT, gp::USize InN, gp::USize InAlign>
    requires(std::is_convertible_v<InOtherT (*)[], InT (*)[]>)
    [[nodiscard]] constexpr StridedView(Array<InOtherT, InN, InAlign>& arr) noexcept
        : m_data(arr.data())
        , m_size(arr.size())
        , m_strideInBytes(static_cast<DifferenceType>(sizeof(InT)))
    {}

    /// @brief Implicitly constructs a contiguous read-only strided view from a `const gp::Array`.
    /// @tparam InOtherT Array element type.
    /// @tparam InN Array size.
    /// @tparam InAlign Array alignment.
    /// @param[in] arr Source `const gp::Array`.
    template <typename InOtherT, gp::USize InN, gp::USize InAlign>
    requires(std::is_convertible_v<const InOtherT (*)[], InT (*)[]>)
    [[nodiscard]] constexpr StridedView(const Array<InOtherT, InN, InAlign>& arr) noexcept
        : m_data(arr.data())
        , m_size(arr.size())
        , m_strideInBytes(static_cast<DifferenceType>(sizeof(InT)))
    {}

    /// @brief Implicitly constructs a contiguous strided view from a mutable `gp::Vector`.
    /// @tparam InOtherT Vector element type.
    /// @tparam InAllocator Vector allocator type.
    /// @param[in] vec Source `gp::Vector`.
    template <typename InOtherT, typename InAllocator>
    requires(std::is_convertible_v<InOtherT (*)[], InT (*)[]>)
    [[nodiscard]] constexpr StridedView(Vector<InOtherT, InAllocator>& vec) noexcept
        : m_data(vec.data())
        , m_size(vec.size())
        , m_strideInBytes(static_cast<DifferenceType>(sizeof(InT)))
    {}

    /// @brief Implicitly constructs a contiguous read-only strided view from a `const gp::Vector`.
    /// @tparam InOtherT Vector element type.
    /// @tparam InAllocator Vector allocator type.
    /// @param[in] vec Source `const gp::Vector`.
    template <typename InOtherT, typename InAllocator>
    requires(std::is_convertible_v<const InOtherT (*)[], InT (*)[]>)
    [[nodiscard]] constexpr StridedView(const Vector<InOtherT, InAllocator>& vec) noexcept
        : m_data(vec.data())
        , m_size(vec.size())
        , m_strideInBytes(static_cast<DifferenceType>(sizeof(InT)))
    {}

    /// @brief Implicit qualification conversion from `StridedView<InOtherT>` (e.g. `StridedView<T>` to
    ///        `StridedView<const T>`).
    /// @tparam InOtherT Source view element type.
    /// @param[in] other Source strided view.
    template <typename InOtherT>
    requires(!concepts::IsSameAs<InOtherT, InT> && std::is_convertible_v<InOtherT (*)[], InT (*)[]>)
    [[nodiscard]] constexpr StridedView(const StridedView<InOtherT>& other) noexcept
        : m_data(other.data())
        , m_size(other.size())
        , m_strideInBytes(other.stride())
    {}

    /// @brief Copy constructor and copy assignment operator (trivially copyable).
    [[nodiscard]] constexpr StridedView(const StridedView&) noexcept = default;
    constexpr StridedView& operator=(const StridedView&) noexcept = default;

    /// @brief Creates a zero-stride broadcast view that yields `singleElement` across `repeatCount` iterations.
    /// @param[in] singleElement Reference to the single element to repeat.
    /// @param[in] repeatCount Number of virtual elements in the view.
    /// @return `StridedView<InT>` with `stride() == 0`.
    [[nodiscard]] static constexpr StridedView broadcast(Reference singleElement, const SizeType repeatCount) noexcept
    {
        return StridedView(&singleElement, repeatCount, 0);
    }

public:
    /// @brief Unchecked strided element access by index.
    /// @param[in] index Zero-based logical index (`index < size()`).
    /// @return Reference to the element at `m_data + index * m_strideInBytes`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Reference operator[](const SizeType index) const noexcept
    {
        GP_ASSERT(index < m_size, "StridedView::operator[] index out of bounds.");
        return *detail::resolveStridedAddress(m_data, m_strideInBytes, static_cast<DifferenceType>(index));
    }

    /// @brief Bounds-checked strided element access by index.
    /// @param[in] index Zero-based logical index (`index < size()`).
    /// @return Reference to the element at `m_data + index * m_strideInBytes`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Reference at(const SizeType index) const noexcept
    {
        GP_ASSERT(index < m_size, "StridedView::at index out of bounds.");
        return *detail::resolveStridedAddress(m_data, m_strideInBytes, static_cast<DifferenceType>(index));
    }

    /// @brief Returns a reference to the first element in the view.
    /// @return Reference to `(*this)[0]`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Reference front() const noexcept
    {
        GP_ASSERT(m_size > 0u, "StridedView::front called on an empty view.");
        return *m_data;
    }

    /// @brief Returns a reference to the last element in the view.
    /// @return Reference to `(*this)[size() - 1]`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Reference back() const noexcept
    {
        GP_ASSERT(m_size > 0u, "StridedView::back called on an empty view.");
        return (*this)[m_size - 1u];
    }

    /// @brief Returns a pointer to the first element (`index == 0`) in the view.
    /// @return Pointer to element `0`, or `nullptr` if empty.
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

    /// @brief Returns the signed byte stride between consecutive elements.
    /// @return Byte stride (`sizeof(InT)` when contiguous, `0` when broadcasting, `< 0` when reversed).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr DifferenceType stride() const noexcept
    {
        return m_strideInBytes;
    }

    /// @brief Checks whether the view is empty (`size() == 0`).
    /// @return True if empty, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isEmpty() const noexcept
    {
        return m_size == 0u;
    }

    /// @brief Checks whether the elements are tightly packed in forward contiguous order (`stride() == sizeof(InT)`).
    /// @return True if contiguous, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isContiguous() const noexcept
    {
        return m_strideInBytes == static_cast<DifferenceType>(sizeof(InT));
    }

    /// @brief Checks whether `index` lies within `[0, size())`.
    /// @param[in] index Index to test.
    /// @return True if `index < size()`, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isValidIndex(const SizeType index) const noexcept
    {
        return index < m_size;
    }

public:
    /// @brief Returns a random-access iterator to the first element.
    /// @return `Iterator(m_data, m_strideInBytes, 0)`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Iterator begin() const noexcept
    {
        return Iterator(m_data, m_strideInBytes, 0);
    }

    /// @brief Returns a random-access iterator one past the last element.
    /// @return `Iterator(m_data, m_strideInBytes, static_cast<DifferenceType>(m_size))`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Iterator end() const noexcept
    {
        return Iterator(m_data, m_strideInBytes, static_cast<DifferenceType>(m_size));
    }

    /// @brief Returns a const random-access iterator to the first element.
    /// @return `ConstIterator(m_data, m_strideInBytes, 0)`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstIterator cbegin() const noexcept
    {
        return ConstIterator(m_data, m_strideInBytes, 0);
    }

    /// @brief Returns a const random-access iterator one past the last element.
    /// @return `ConstIterator(m_data, m_strideInBytes, static_cast<DifferenceType>(m_size))`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstIterator cend() const noexcept
    {
        return ConstIterator(m_data, m_strideInBytes, static_cast<DifferenceType>(m_size));
    }

    /// @brief Returns a reverse iterator to the last element.
    /// @return `ReverseIterator(end())`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ReverseIterator rbegin() const noexcept
    {
        return ReverseIterator(end());
    }

    /// @brief Returns a reverse iterator one before the first element.
    /// @return `ReverseIterator(begin())`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ReverseIterator rend() const noexcept
    {
        return ReverseIterator(begin());
    }

    /// @brief Returns a const reverse iterator to the last element.
    /// @return `ConstReverseIterator(cend())`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstReverseIterator crbegin() const noexcept
    {
        return ConstReverseIterator(cend());
    }

    /// @brief Returns a const reverse iterator one before the first element.
    /// @return `ConstReverseIterator(cbegin())`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstReverseIterator crend() const noexcept
    {
        return ConstReverseIterator(cbegin());
    }

public:
    /// @brief Returns a sub-view starting at `offset` with `count` elements (preserving `stride()`).
    /// @param[in] offset Starting element index (`offset <= size()`).
    /// @param[in] count Number of elements to include (defaults to `npos`).
    /// @return Strided sub-view `[offset, offset + effectiveCount)`.
    [[nodiscard]] constexpr StridedView slice(const SizeType offset, const SizeType count = npos) const noexcept
    {
        GP_ASSERT(offset <= m_size, "StridedView::slice offset out of bounds.");
        const SizeType remaining = m_size - offset;
        const SizeType effectiveCount = (count == npos) ? remaining : count;
        GP_ASSERT(effectiveCount <= remaining, "StridedView::slice count exceeds remaining elements.");
        if (effectiveCount == 0u)
        {
            return StridedView(nullptr, 0u, m_strideInBytes);
        }
        return StridedView(
            detail::resolveStridedAddress(m_data, m_strideInBytes, static_cast<DifferenceType>(offset)),
            effectiveCount,
            m_strideInBytes
        );
    }

    /// @brief Returns a strided sub-view of the first `count` elements.
    /// @param[in] count Number of leading elements (`count <= size()`).
    /// @return Strided sub-view `[0, count)`.
    [[nodiscard]] constexpr StridedView first(const SizeType count) const noexcept
    {
        GP_ASSERT(count <= m_size, "StridedView::first count exceeds view size.");
        return slice(0u, count);
    }

    /// @brief Returns a strided sub-view of the last `count` elements.
    /// @param[in] count Number of trailing elements (`count <= size()`).
    /// @return Strided sub-view `[size() - count, size())`.
    [[nodiscard]] constexpr StridedView last(const SizeType count) const noexcept
    {
        GP_ASSERT(count <= m_size, "StridedView::last count exceeds view size.");
        return slice(m_size - count, count);
    }

    /// @brief Returns a new strided view with the first `count` elements removed.
    /// @param[in] count Number of leading elements to drop (`count <= size()`).
    /// @return Strided sub-view `[count, size())`.
    [[nodiscard]] constexpr StridedView dropFirst(const SizeType count = 1u) const noexcept
    {
        GP_ASSERT(count <= m_size, "StridedView::dropFirst count exceeds view size.");
        return slice(count, m_size - count);
    }

    /// @brief Returns a new strided view with the last `count` elements removed.
    /// @param[in] count Number of trailing elements to drop (`count <= size()`).
    /// @return Strided sub-view `[0, size() - count)`.
    [[nodiscard]] constexpr StridedView dropLast(const SizeType count = 1u) const noexcept
    {
        GP_ASSERT(count <= m_size, "StridedView::dropLast count exceeds view size.");
        return slice(0u, m_size - count);
    }

    /// @brief Returns a new strided view that visits every `step`-th element (`stride() * step`).
    /// @param[in] step Positive element step (`step >= 1`).
    /// @return Sub-sampled `StridedView`.
    [[nodiscard]] constexpr StridedView everyNth(const SizeType step) const noexcept
    {
        GP_ASSERT(step > 0u, "StridedView::everyNth step must be greater than zero.");
        if (m_size == 0u)
        {
            return *this;
        }
        const SizeType newCount = ((m_size - 1u) / step) + 1u;
        return StridedView(m_data, newCount, m_strideInBytes * static_cast<DifferenceType>(step));
    }

    /// @brief Returns a reversed `StridedView` in O(1) by pointing to the last element and negating `stride()`.
    /// @return Reversed `StridedView<InT>`.
    [[nodiscard]] constexpr StridedView reverseStrided() const noexcept
    {
        if (m_size <= 1u)
        {
            return *this;
        }
        Pointer const lastPtr =
            detail::resolveStridedAddress(m_data, m_strideInBytes, static_cast<DifferenceType>(m_size - 1u));
        return StridedView(lastPtr, m_size, -m_strideInBytes);
    }

    /// @brief Projects this view onto a data member `memberPtr` of `InT` (AoS-to-SoA field projection).
    /// @tparam InMember Member field type.
    /// @tparam InStructBase Struct or base class type owning `memberPtr`.
    /// @param[in] memberPtr Pointer-to-data-member `&InStructBase::field`.
    /// @return `StridedView` over the projected member field with the same element count and byte stride.
    template <typename InMember, typename InStructBase>
    requires(std::is_base_of_v<InStructBase, ValueType>)
    [[nodiscard]] constexpr auto project(InMember InStructBase::* const memberPtr) const noexcept
    {
        using QualifiedMember = std::conditional_t<concepts::IsConst<InT>, const InMember, InMember>;
        GP_ASSERT(memberPtr != nullptr, "StridedView::project called with a null member pointer.");
        if (m_size == 0u || m_data == nullptr)
        {
            return StridedView<QualifiedMember>(
                nullptr, 0u, static_cast<DifferenceType>(sizeof(QualifiedMember))
            );
        }
        QualifiedMember* const firstFieldPtr = &(m_data->*memberPtr);
        return StridedView<QualifiedMember>(firstFieldPtr, m_size, m_strideInBytes);
    }

public:
    /// @brief Finds the first occurrence of `value` in the strided view.
    /// @param[in] value Value to search for.
    /// @return Iterator to the first matching element, or `end()` if not found.
    [[nodiscard]] constexpr Iterator find(const ValueType& value) const noexcept
    {
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if ((*this)[i] == value)
            {
                return Iterator(m_data, m_strideInBytes, static_cast<DifferenceType>(i));
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
            if (predicate((*this)[i]))
            {
                return Iterator(m_data, m_strideInBytes, static_cast<DifferenceType>(i));
            }
        }
        return end();
    }

    /// @brief Finds the first index of `value` in the strided view.
    /// @param[in] value Value to search for.
    /// @return Zero-based logical index of the first match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType indexOf(const ValueType& value) const noexcept
    {
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if ((*this)[i] == value)
            {
                return i;
            }
        }
        return npos;
    }

    /// @brief Finds the first index of an element satisfying `predicate`.
    /// @tparam InPredicate Unary predicate `(ConstReference) -> bool`.
    /// @param[in] predicate Search condition.
    /// @return Zero-based logical index of the first match, or `npos` if not found.
    template <typename InPredicate>
    [[nodiscard]] constexpr SizeType indexOfIf(InPredicate predicate) const noexcept
    {
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (predicate((*this)[i]))
            {
                return i;
            }
        }
        return npos;
    }

    /// @brief Finds the last index of `value` in the strided view.
    /// @param[in] value Value to search for.
    /// @return Zero-based logical index of the last match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType lastIndexOf(const ValueType& value) const noexcept
    {
        for (SizeType i = m_size; i > 0u; --i)
        {
            if ((*this)[i - 1u] == value)
            {
                return i - 1u;
            }
        }
        return npos;
    }

    /// @brief Checks whether the strided view contains at least one element equal to `value`.
    /// @param[in] value Value to search for.
    /// @return True if found, false otherwise.
    [[nodiscard]] constexpr bool contains(const ValueType& value) const noexcept
    {
        return indexOf(value) != npos;
    }

    /// @brief Checks whether the strided view contains at least one element satisfying `predicate`.
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
            if ((*this)[i] == value)
            {
                ++matches;
            }
        }
        return matches;
    }

    /// @brief Assigns `value` to every element in a mutable strided view.
    /// @param[in] value Value to assign to each strided slot.
    constexpr void fill(const ValueType& value) const noexcept requires(!concepts::IsConst<InT>)
    {
        for (SizeType i = 0u; i < m_size; ++i)
        {
            (*this)[i] = value;
        }
    }

public:
    /// @brief Element-wise equality comparison between two compatible `StridedView` instances.
    /// @tparam InOtherT Right-hand view element type.
    /// @param[in] rhs View to compare against.
    /// @return True if both views have the same size and equal elements at each logical index.
    template <typename InOtherT>
    requires(concepts::IsSameAs<std::remove_cv_t<InOtherT>, ValueType>)
    [[nodiscard]] constexpr bool operator==(const StridedView<InOtherT>& rhs) const noexcept
    {
        if (m_size != rhs.size())
        {
            return false;
        }

        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (!((*this)[i] == rhs[i]))
            {
                return false;
            }
        }
        return true;
    }

    /// @brief Lexicographical three-way comparison between two compatible `StridedView` instances.
    /// @tparam InOtherT Right-hand view element type.
    /// @param[in] rhs View to compare against.
    /// @return Three-way comparison ordering result.
    template <typename InOtherT>
    requires(concepts::IsSameAs<std::remove_cv_t<InOtherT>, ValueType> && std::three_way_comparable<ValueType>)
    [[nodiscard]] constexpr auto operator<=>(const StridedView<InOtherT>& rhs) const noexcept
    {
        const SizeType minLen = m_size < rhs.size() ? m_size : rhs.size();
        for (SizeType i = 0u; i < minLen; ++i)
        {
            const auto cmp = (*this)[i] <=> rhs[i];
            if (cmp != 0)
            {
                return cmp;
            }
        }
        return m_size <=> rhs.size();
    }

private:
    Pointer m_data{ nullptr };
    SizeType m_size{ 0u };
    DifferenceType m_strideInBytes{ static_cast<DifferenceType>(sizeof(InT)) };
};

/// @brief Projects a `StridedView` onto a data member `memberPtr` of its element struct.
/// @tparam InStruct Source struct type (may be `const`).
/// @tparam InMember Member field type.
/// @tparam InStructBase Struct or base class owning `memberPtr`.
/// @param[in] view Source strided view.
/// @param[in] memberPtr Pointer-to-data-member `&InStructBase::field`.
/// @return `StridedView` over the projected member field.
template <typename InStruct, typename InMember, typename InStructBase>
requires(std::is_base_of_v<InStructBase, std::remove_cv_t<InStruct>>)
[[nodiscard]] constexpr auto
    makeStridedView(const StridedView<InStruct>& view, InMember InStructBase::* const memberPtr) noexcept
{
    return view.project(memberPtr);
}

/// @brief Projects a `VectorView` onto a data member `memberPtr` of its element struct.
/// @tparam InStruct Source struct type (may be `const`).
/// @tparam InMember Member field type.
/// @tparam InStructBase Struct or base class owning `memberPtr`.
/// @param[in] view Source contiguous view.
/// @param[in] memberPtr Pointer-to-data-member `&InStructBase::field`.
/// @return `StridedView` over the projected member field.
template <typename InStruct, typename InMember, typename InStructBase>
requires(std::is_base_of_v<InStructBase, std::remove_cv_t<InStruct>>)
[[nodiscard]] constexpr auto
    makeStridedView(const VectorView<InStruct>& view, InMember InStructBase::* const memberPtr) noexcept
{
    return StridedView<InStruct>(view).project(memberPtr);
}

/// @brief Projects a mutable `gp::Vector` onto a data member `memberPtr` of its element struct.
/// @tparam InStruct Source struct type.
/// @tparam InAllocator Vector allocator type.
/// @tparam InMember Member field type.
/// @tparam InStructBase Struct or base class owning `memberPtr`.
/// @param[in] vec Source mutable vector.
/// @param[in] memberPtr Pointer-to-data-member `&InStructBase::field`.
/// @return Mutable `StridedView<InMember>` over the projected member field.
template <typename InStruct, typename InAllocator, typename InMember, typename InStructBase>
requires(std::is_base_of_v<InStructBase, std::remove_cv_t<InStruct>>)
[[nodiscard]] constexpr auto
    makeStridedView(Vector<InStruct, InAllocator>& vec, InMember InStructBase::* const memberPtr) noexcept
{
    return StridedView<InStruct>(vec).project(memberPtr);
}

/// @brief Projects a `const gp::Vector` onto a data member `memberPtr` of its element struct.
/// @tparam InStruct Source struct type.
/// @tparam InAllocator Vector allocator type.
/// @tparam InMember Member field type.
/// @tparam InStructBase Struct or base class owning `memberPtr`.
/// @param[in] vec Source const vector.
/// @param[in] memberPtr Pointer-to-data-member `&InStructBase::field`.
/// @return Read-only `StridedView<const InMember>` over the projected member field.
template <typename InStruct, typename InAllocator, typename InMember, typename InStructBase>
requires(std::is_base_of_v<InStructBase, std::remove_cv_t<InStruct>>)
[[nodiscard]] constexpr auto
    makeStridedView(const Vector<InStruct, InAllocator>& vec, InMember InStructBase::* const memberPtr) noexcept
{
    return StridedView<const InStruct>(vec).project(memberPtr);
}

/// @brief Projects a mutable `gp::Array` onto a data member `memberPtr` of its element struct.
/// @tparam InStruct Source struct type.
/// @tparam InN Array size.
/// @tparam InAlign Array alignment.
/// @tparam InMember Member field type.
/// @tparam InStructBase Struct or base class owning `memberPtr`.
/// @param[in] arr Source mutable array.
/// @param[in] memberPtr Pointer-to-data-member `&InStructBase::field`.
/// @return Mutable `StridedView<InMember>` over the projected member field.
template <typename InStruct, gp::USize InN, gp::USize InAlign, typename InMember, typename InStructBase>
requires(std::is_base_of_v<InStructBase, std::remove_cv_t<InStruct>>)
[[nodiscard]] constexpr auto
    makeStridedView(Array<InStruct, InN, InAlign>& arr, InMember InStructBase::* const memberPtr) noexcept
{
    return StridedView<InStruct>(arr).project(memberPtr);
}

/// @brief Projects a `const gp::Array` onto a data member `memberPtr` of its element struct.
/// @tparam InStruct Source struct type.
/// @tparam InN Array size.
/// @tparam InAlign Array alignment.
/// @tparam InMember Member field type.
/// @tparam InStructBase Struct or base class owning `memberPtr`.
/// @param[in] arr Source const array.
/// @param[in] memberPtr Pointer-to-data-member `&InStructBase::field`.
/// @return Read-only `StridedView<const InMember>` over the projected member field.
template <typename InStruct, gp::USize InN, gp::USize InAlign, typename InMember, typename InStructBase>
requires(std::is_base_of_v<InStructBase, std::remove_cv_t<InStruct>>)
[[nodiscard]] constexpr auto
    makeStridedView(const Array<InStruct, InN, InAlign>& arr, InMember InStructBase::* const memberPtr) noexcept
{
    return StridedView<const InStruct>(arr).project(memberPtr);
}

/// @brief Projects a C-style array `arr[InN]` onto a data member `memberPtr` of its element struct.
/// @tparam InStruct Source struct type (may be `const`).
/// @tparam InN Number of elements in `arr`.
/// @tparam InMember Member field type.
/// @tparam InStructBase Struct or base class owning `memberPtr`.
/// @param[in] arr Source C array.
/// @param[in] memberPtr Pointer-to-data-member `&InStructBase::field`.
/// @return `StridedView` over the projected member field.
template <typename InStruct, gp::USize InN, typename InMember, typename InStructBase>
requires(std::is_base_of_v<InStructBase, std::remove_cv_t<InStruct>>)
[[nodiscard]] constexpr auto
    makeStridedView(InStruct (&arr)[InN], InMember InStructBase::* const memberPtr) noexcept
{
    return StridedView<InStruct>(arr).project(memberPtr);
}

}   // namespace gp::container

namespace gp
{

using container::makeStridedView;
using container::StridedIterator;

}   // namespace gp

namespace gp::detail
{

/// @brief Helper variable template to determine if a type is a `gp::StridedView`.
template <typename InT>
constexpr bool IsStridedViewV = false;

/// @brief Specialization for `gp::container::StridedView` (`gp::StridedView`) types.
template <typename InT>
constexpr bool IsStridedViewV<gp::container::StridedView<InT>> = true;

}   // namespace gp::detail

namespace gp::concepts
{

/// @brief Concept to check if a type is any instantiation of `gp::StridedView`.
/// @tparam InT The type to check.
template <typename InT>
concept IsStridedView = gp::detail::IsStridedViewV<std::remove_cvref_t<InT>>;

}   // namespace gp::concepts
