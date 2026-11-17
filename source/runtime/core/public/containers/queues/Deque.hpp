// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "containers/ContainerForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "maths/base/Scalar.hpp"
#include "memory/allocators/AllocatorConcepts.hpp"
#include "memory/allocators/HeapAllocator.hpp"
#include "memory/Memory.hpp"
#include "memory/MemoryBase.hpp"
#include "memory/utils/MemoryOps.hpp"
#include <compare>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

namespace gp
{

namespace container::detail
{

/// @brief Random-access iterator walking logical positions `[m_head, m_head + m_size]` over a power-of-two ring.
/// @tparam InElementType Element value type (`T` or `const T`).
template <typename InElementType>
class DequeIterator
{
public:
    using iterator_category = std::random_access_iterator_tag;
    using iterator_concept = std::random_access_iterator_tag;
    using value_type = std::remove_const_t<InElementType>;
    using difference_type = gp::ISize;
    using pointer = InElementType*;
    using reference = InElementType&;

public:
    /// @brief Constructs an unassociated null iterator.
    constexpr DequeIterator() noexcept = default;

    /// @brief Constructs an iterator at logical ring cursor `cursor`.
    /// @param[in] buffer Backing ring array pointer.
    /// @param[in] mask Power-of-two wraparound mask (`capacity - 1`, or `0` when empty).
    /// @param[in] cursor Unmasked logical cursor starting at `head`.
    constexpr DequeIterator(pointer buffer, const gp::USize mask, const gp::USize cursor) noexcept
        : m_buffer(buffer)
        , m_mask(mask)
        , m_cursor(cursor)
    {}

    /// @brief Implicit conversion from mutable `DequeIterator<T>` to `DequeIterator<const T>`.
    /// @tparam InOtherT Mutable element type matching `value_type`.
    /// @param[in] other Source mutable iterator.
    template <typename InOtherT>
    requires(std::is_const_v<InElementType> && std::is_same_v<InOtherT, value_type>)
    constexpr DequeIterator(const DequeIterator<InOtherT>& other) noexcept
        : m_buffer(other.m_buffer)
        , m_mask(other.m_mask)
        , m_cursor(other.m_cursor)
    {}

public:
    /// @brief Dereferences the element at the current cursor.
    /// @return Reference to the ring slot at `m_cursor & m_mask`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr reference operator*() const noexcept
    {
        return m_buffer[m_cursor & m_mask];
    }

    /// @brief Returns a pointer to the element at the current cursor.
    /// @return Pointer to the ring slot at `m_cursor & m_mask`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr pointer operator->() const noexcept
    {
        return m_buffer + (m_cursor & m_mask);
    }

    /// @brief Random-access element indexing relative to this iterator.
    /// @param[in] offset Signed element offset from the current cursor.
    /// @return Reference to the offset element.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr reference operator[](const difference_type offset) const noexcept
    {
        const gp::USize target = static_cast<gp::USize>(static_cast<difference_type>(m_cursor) + offset);
        return m_buffer[target & m_mask];
    }

    /// @brief Pre-increments the cursor by one element.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr DequeIterator& operator++() noexcept
    {
        ++m_cursor;
        return *this;
    }

    /// @brief Post-increments the cursor by one element.
    /// @return Copy of the iterator prior to incrementing.
    GP_FORCEINLINE_HINT constexpr DequeIterator operator++(int) noexcept
    {
        const DequeIterator copy(*this);
        ++m_cursor;
        return copy;
    }

    /// @brief Pre-decrements the cursor by one element.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr DequeIterator& operator--() noexcept
    {
        --m_cursor;
        return *this;
    }

    /// @brief Post-decrements the cursor by one element.
    /// @return Copy of the iterator prior to decrementing.
    GP_FORCEINLINE_HINT constexpr DequeIterator operator--(int) noexcept
    {
        const DequeIterator copy(*this);
        --m_cursor;
        return copy;
    }

    /// @brief Advances the cursor forward by `offset` elements.
    /// @param[in] offset Signed element step.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr DequeIterator& operator+=(const difference_type offset) noexcept
    {
        m_cursor = static_cast<gp::USize>(static_cast<difference_type>(m_cursor) + offset);
        return *this;
    }

    /// @brief Moves the cursor backward by `offset` elements.
    /// @param[in] offset Signed element step.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr DequeIterator& operator-=(const difference_type offset) noexcept
    {
        m_cursor = static_cast<gp::USize>(static_cast<difference_type>(m_cursor) - offset);
        return *this;
    }

    /// @brief Returns an iterator advanced by `offset`.
    /// @param[in] it Base iterator.
    /// @param[in] offset Signed element offset.
    /// @return Shifted iterator copy.
    [[nodiscard]] friend constexpr DequeIterator operator+(DequeIterator it, const difference_type offset) noexcept
    {
        it += offset;
        return it;
    }

    /// @brief Returns an iterator advanced by `offset` (commuted operand order).
    /// @param[in] offset Signed element offset.
    /// @param[in] it Base iterator.
    /// @return Shifted iterator copy.
    [[nodiscard]] friend constexpr DequeIterator operator+(const difference_type offset, DequeIterator it) noexcept
    {
        it += offset;
        return it;
    }

    /// @brief Returns an iterator stepped backward by `offset`.
    /// @param[in] it Base iterator.
    /// @param[in] offset Signed element offset.
    /// @return Shifted iterator copy.
    [[nodiscard]] friend constexpr DequeIterator operator-(DequeIterator it, const difference_type offset) noexcept
    {
        it -= offset;
        return it;
    }

    /// @brief Computes the signed logical distance between two iterators on the same deque.
    /// @param[in] lhs Left-hand iterator.
    /// @param[in] rhs Right-hand iterator.
    /// @return Number of elements from `rhs` to `lhs`.
    [[nodiscard]] friend constexpr difference_type
        operator-(const DequeIterator& lhs, const DequeIterator& rhs) noexcept
    {
        return static_cast<difference_type>(lhs.m_cursor) - static_cast<difference_type>(rhs.m_cursor);
    }

    /// @brief Compares two iterators for position equality.
    /// @param[in] other Right-hand iterator.
    /// @return True if both refer to the same buffer cursor.
    [[nodiscard]] constexpr bool operator==(const DequeIterator& other) const noexcept
    {
        return m_cursor == other.m_cursor && m_buffer == other.m_buffer;
    }

    /// @brief Three-way comparison ordering iterators by logical cursor position.
    /// @param[in] other Right-hand iterator.
    /// @return `std::strong_ordering` of the logical cursors.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const DequeIterator& other) const noexcept
    {
        return m_cursor <=> other.m_cursor;
    }

    /// @brief Returns an opaque pointer to the underlying ring buffer for debug ownership checks.
    /// @return Backing ring buffer pointer, or `nullptr` if empty or unassociated.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr const void* buffer() const noexcept
    {
        return m_buffer;
    }

private:
    template <typename InOtherT>
    friend class DequeIterator;

    pointer m_buffer{ nullptr };
    gp::USize m_mask{ 0u };
    gp::USize m_cursor{ 0u };
};

}   // namespace container::detail

/// @brief Double-ended circular ring queue backed by a power-of-two contiguous buffer.
/// @details Keeps all elements in a single contiguous allocation with capacity always 0 or a power of two. Logical
///          index `i` maps to `m_data[(m_head + i) & (m_capacity - 1)]`, replacing integer division with a single
///          bitwise AND on push, pop, indexing, and random-access iteration. When the buffer fills, elements relocate
///          into linearized order at offset 0.
/// @note Not thread-safe; synchronize external access if shared across worker threads, or use `gp::SPSCRingBuffer` /
///       `gp::thread::WorkStealingDeque` for lock-free concurrent queues.
/// @tparam InT Element type stored in the deque (cannot be const or a reference).
/// @tparam InAllocator Engine memory allocator satisfying `gp::concepts::IsAllocator`.
template <typename InT, typename InAllocator>
class Deque
{
    static_assert(!gp::concepts::IsConst<InT>, "Deque element type cannot be const.");
    static_assert(!gp::concepts::IsReference<InT>, "Deque element type cannot be a reference.");
    static_assert(gp::concepts::IsAllocator<InAllocator>, "Deque requires InAllocator to satisfy IsAllocator.");

private:
    template <typename InOtherT, typename InOtherAllocator>
    friend class Deque;

public:
    using ValueType = InT;
    using AllocatorType = InAllocator;
    using SizeType = gp::USize;
    using DifferenceType = gp::ISize;
    using Reference = ValueType&;
    using ConstReference = const ValueType&;
    using Pointer = ValueType*;
    using ConstPointer = const ValueType*;
    using Iterator = container::detail::DequeIterator<ValueType>;
    using ConstIterator = container::detail::DequeIterator<const ValueType>;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

    /// @brief Contiguous mutable slice descriptor returned by `getSpans()` for zero-copy ring traversal.
    struct Span
    {
        Pointer data{ nullptr };
        SizeType count{ 0u };
    };

    /// @brief Contiguous read-only slice descriptor returned by `getSpans()` for zero-copy ring reads.
    struct ConstSpan
    {
        ConstPointer data{ nullptr };
        SizeType count{ 0u };
    };

    /// @brief Sentinel value returned by search methods when an element is not found.
    static constexpr SizeType npos = static_cast<SizeType>(-1);

    /// @brief Minimum non-zero power-of-two capacity allocated on first growth.
    static constexpr SizeType kDefaultInitialCapacity = 4u;

    /// @brief Byte alignment requested from `m_allocator` for the backing ring buffer.
    static constexpr gp::UInt32 kAllocAlignment = static_cast<gp::UInt32>(
        alignof(InT) > gp::memory::kMinimumAlignment ? alignof(InT) : gp::memory::kMinimumAlignment
    );

public:
    /// @brief Constructs an empty deque with zero allocations.
    constexpr Deque() noexcept = default;

    /// @brief Constructs an empty deque bound to a copy of `allocator`.
    /// @param[in] allocator Allocator instance to copy.
    constexpr explicit Deque(const InAllocator& allocator) noexcept requires std::is_copy_constructible_v<InAllocator>
        : m_allocator(allocator)
    {}

    /// @brief Constructs an empty deque by moving `allocator`.
    /// @param[in,out] allocator Allocator instance to move from.
    constexpr explicit Deque(InAllocator&& allocator) noexcept requires std::is_move_constructible_v<InAllocator>
        : m_allocator(std::move(allocator))
    {}

    /// @brief Constructs a deque containing `count` value-initialized elements.
    /// @param[in] count Initial number of elements.
    explicit Deque(const SizeType count) noexcept
    {
        if (count > 0u)
        {
            reserve(count);
            if (m_data != nullptr) [[likely]]
            {
                gp::memory::valueConstructItems<InT>(m_data, count);
                m_size = count;
            }
        }
    }

    /// @brief Constructs a deque containing `count` value-initialized elements using `allocator`.
    /// @param[in] count Initial number of elements.
    /// @param[in] allocator Allocator instance to copy.
    Deque(const SizeType count, const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_allocator(allocator)
    {
        if (count > 0u)
        {
            reserve(count);
            if (m_data != nullptr) [[likely]]
            {
                gp::memory::valueConstructItems<InT>(m_data, count);
                m_size = count;
            }
        }
    }

    /// @brief Constructs a deque containing `count` copies of `value`.
    /// @param[in] count Number of elements to insert.
    /// @param[in] value Fill value copied into each slot.
    Deque(const SizeType count, const InT& value) noexcept
    {
        assign(count, value);
    }

    /// @brief Constructs a deque containing `count` copies of `value` using `allocator`.
    /// @param[in] count Number of elements to insert.
    /// @param[in] value Fill value copied into each slot.
    /// @param[in] allocator Allocator instance to copy.
    Deque(const SizeType count, const InT& value, const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_allocator(allocator)
    {
        assign(count, value);
    }

    /// @brief Constructs a deque from an iterator range `[first, last)`.
    /// @tparam InInputIt Input iterator type yielding elements convertible to `InT`.
    /// @param[in] first Start of the source range.
    /// @param[in] last End of the source range.
    template <typename InInputIt>
    requires(!gp::concepts::IsIntegral<InInputIt>) Deque(InInputIt first, InInputIt last) noexcept
    {
        assign(first, last);
    }

    /// @brief Constructs a deque from an iterator range `[first, last)` using `allocator`.
    /// @tparam InInputIt Input iterator type yielding elements convertible to `InT`.
    /// @param[in] first Start of the source range.
    /// @param[in] last End of the source range.
    /// @param[in] allocator Allocator instance to copy.
    template <typename InInputIt>
    requires(!gp::concepts::IsIntegral<InInputIt> && std::is_copy_constructible_v<InAllocator>)
    Deque(InInputIt first, InInputIt last, const InAllocator& allocator) noexcept
        : m_allocator(allocator)
    {
        assign(first, last);
    }

    /// @brief Constructs a deque from an initializer list.
    /// @param[in] init Initializer list of elements pushed front-to-back.
    Deque(const std::initializer_list<InT> init) noexcept
    {
        assign(init);
    }

    /// @brief Constructs a deque from an initializer list using `allocator`.
    /// @param[in] init Initializer list of elements pushed front-to-back.
    /// @param[in] allocator Allocator instance to copy.
    Deque(const std::initializer_list<InT> init, const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_allocator(allocator)
    {
        assign(init);
    }

    /// @brief Copy constructor when `InAllocator` is copy-constructible.
    /// @param[in] other Source deque to copy.
    Deque(const Deque& other) noexcept requires std::is_copy_constructible_v<InAllocator>
        : m_allocator(other.m_allocator)
    {
        copyElementsFrom(other);
    }

    /// @brief Copy constructor when `InAllocator` is non-copyable (such as `InlineAllocator`).
    /// @param[in] other Source deque to copy into a default-constructed allocator.
    Deque(const Deque& other) noexcept
        requires(!std::is_copy_constructible_v<InAllocator> && std::is_default_constructible_v<InAllocator>)
    {
        copyElementsFrom(other);
    }

    /// @brief Cross-allocator copy constructor from any `Deque` sharing element type `InT`.
    /// @tparam InOtherAllocator Source deque's allocator type.
    /// @param[in] other Source deque to copy elements from.
    template <gp::concepts::IsAllocator InOtherAllocator>
    requires(!gp::concepts::IsSameAs<InAllocator, InOtherAllocator>)
    explicit Deque(const Deque<InT, InOtherAllocator>& other) noexcept
    {
        copyElementsFrom(other);
    }

    /// @brief Move constructor when `InAllocator` is move-constructible.
    /// @details Steals the backing buffer in O(1) if the moved allocator compares equal; otherwise moves elements into
    ///          `m_allocator` and frees `other`.
    /// @param[in,out] other Source deque left empty after the move.
    Deque(Deque&& other) noexcept requires std::is_move_constructible_v<InAllocator>
        : m_allocator(std::move(other.m_allocator))
    {
        if (m_allocator == other.m_allocator)
        {
            stealFrom(other);
        }
        else
        {
            moveElementsFrom(other);
            other.reset();
        }
    }

    /// @brief Move constructor when `InAllocator` is non-movable (such as `InlineAllocator`).
    /// @param[in,out] other Source deque left empty after the move.
    Deque(Deque&& other) noexcept
        requires(!std::is_move_constructible_v<InAllocator> && std::is_default_constructible_v<InAllocator>)
    {
        moveElementsFrom(other);
        other.reset();
    }

    /// @brief Destroys all active elements and returns the ring buffer to `m_allocator`.
    ~Deque() noexcept
    {
        reset();
    }

public:
    /// @brief Copy assignment operator.
    /// @param[in] other Source deque to copy from.
    /// @return Reference to `*this`.
    Deque& operator=(const Deque& other) noexcept
    {
        if (this != &other)
        {
            assign(other.begin(), other.end());
        }
        return *this;
    }

    /// @brief Move assignment operator.
    /// @param[in,out] other Source deque left empty after the move.
    /// @return Reference to `*this`.
    Deque& operator=(Deque&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        if (m_allocator == other.m_allocator)
        {
            reset();
            stealFrom(other);
            return *this;
        }

        clear();
        moveElementsFrom(other);
        other.reset();
        return *this;
    }

    /// @brief Initializer-list assignment operator.
    /// @param[in] init Initializer list replacing the current contents.
    /// @return Reference to `*this`.
    Deque& operator=(const std::initializer_list<InT> init) noexcept
    {
        assign(init);
        return *this;
    }

    /// @brief Replaces contents with `count` copies of `value`.
    /// @param[in] count Number of copies to store.
    /// @param[in] value Element value to replicate.
    void assign(const SizeType count, const InT& value) noexcept
    {
        // Stage before clearing in case `value` aliases an element currently inside `*this`.
        const InT copyValue(value);
        clear();
        if (count == 0u)
        {
            return;
        }
        reserve(count);
        if (m_data == nullptr) [[unlikely]]
        {
            return;
        }
        for (SizeType i = 0u; i < count; ++i)
        {
            ::new (static_cast<void*>(m_data + i)) InT(copyValue);
        }
        m_size = count;
    }

    /// @brief Replaces contents with elements from `[first, last)`.
    /// @tparam InInputIt Input iterator yielding elements convertible to `InT`.
    /// @param[in] first Start of source range.
    /// @param[in] last End of source range.
    template <typename InInputIt>
    requires(!gp::concepts::IsIntegral<InInputIt>) void assign(InInputIt first, InInputIt last) noexcept
    {
        clear();
        if constexpr (std::forward_iterator<InInputIt>)
        {
            const DifferenceType dist = std::distance(first, last);
            if (dist > 0)
            {
                reserve(static_cast<SizeType>(dist));
            }
        }
        for (; first != last; ++first)
        {
            emplaceBack(*first);
        }
    }

    /// @brief Replaces contents with elements from an initializer list.
    /// @param[in] init Initializer list of elements.
    void assign(const std::initializer_list<InT> init) noexcept
    {
        assign(init.begin(), init.end());
    }

public:
    /// @brief Accesses the element at logical `index` (`0` is `front()`).
    /// @param[in] index Zero-based logical offset (`< size()`).
    /// @return Mutable reference to the element.
    [[nodiscard]] GP_FORCEINLINE_HINT Reference operator[](const SizeType index) noexcept
    {
        GP_ASSERT(index < m_size && "Deque index out of bounds.");
        return m_data[physicalIndex(index)];
    }

    /// @brief Accesses the element at logical `index` (`const` overload).
    /// @param[in] index Zero-based logical offset (`< size()`).
    /// @return Const reference to the element.
    [[nodiscard]] GP_FORCEINLINE_HINT ConstReference operator[](const SizeType index) const noexcept
    {
        GP_ASSERT(index < m_size && "Deque index out of bounds.");
        return m_data[physicalIndex(index)];
    }

    /// @brief Accesses the element at logical `index` with debug bounds assertion.
    /// @param[in] index Zero-based logical offset (`< size()`).
    /// @return Mutable reference to the element.
    [[nodiscard]] GP_FORCEINLINE_HINT Reference at(const SizeType index) noexcept
    {
        GP_ASSERT(index < m_size && "Deque index out of bounds.");
        return m_data[physicalIndex(index)];
    }

    /// @brief Accesses the element at logical `index` with debug bounds assertion (`const` overload).
    /// @param[in] index Zero-based logical offset (`< size()`).
    /// @return Const reference to the element.
    [[nodiscard]] GP_FORCEINLINE_HINT ConstReference at(const SizeType index) const noexcept
    {
        GP_ASSERT(index < m_size && "Deque index out of bounds.");
        return m_data[physicalIndex(index)];
    }

    /// @brief Returns a pointer to the element at logical `index`, or `nullptr` if out of bounds.
    /// @param[in] index Zero-based logical offset.
    /// @return Pointer to the element if `index < size()`, otherwise `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT Pointer tryAt(const SizeType index) noexcept
    {
        return (index < m_size) ? (m_data + physicalIndex(index)) : nullptr;
    }

    /// @brief Returns a const pointer to the element at logical `index`, or `nullptr` if out of bounds.
    /// @param[in] index Zero-based logical offset.
    /// @return Const pointer to the element if `index < size()`, otherwise `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT ConstPointer tryAt(const SizeType index) const noexcept
    {
        return (index < m_size) ? (m_data + physicalIndex(index)) : nullptr;
    }

    /// @brief Accesses the first (front) element in the deque.
    /// @return Mutable reference to the front element.
    [[nodiscard]] GP_FORCEINLINE_HINT Reference front() noexcept
    {
        GP_ASSERT(m_size > 0u && "Deque::front() called on an empty deque.");
        return m_data[m_head];
    }

    /// @brief Accesses the first (front) element in the deque (`const` overload).
    /// @return Const reference to the front element.
    [[nodiscard]] GP_FORCEINLINE_HINT ConstReference front() const noexcept
    {
        GP_ASSERT(m_size > 0u && "Deque::front() called on an empty deque.");
        return m_data[m_head];
    }

    /// @brief Accesses the last (back) element in the deque.
    /// @return Mutable reference to the back element.
    [[nodiscard]] GP_FORCEINLINE_HINT Reference back() noexcept
    {
        GP_ASSERT(m_size > 0u && "Deque::back() called on an empty deque.");
        return m_data[physicalIndex(m_size - 1u)];
    }

    /// @brief Accesses the last (back) element in the deque (`const` overload).
    /// @return Const reference to the back element.
    [[nodiscard]] GP_FORCEINLINE_HINT ConstReference back() const noexcept
    {
        GP_ASSERT(m_size > 0u && "Deque::back() called on an empty deque.");
        return m_data[physicalIndex(m_size - 1u)];
    }

    /// @brief Returns a pointer to the front element, or `nullptr` if the deque is empty.
    /// @return Pointer to the front element or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT Pointer tryFront() noexcept
    {
        return (m_size > 0u) ? (m_data + m_head) : nullptr;
    }

    /// @brief Returns a const pointer to the front element, or `nullptr` if the deque is empty.
    /// @return Const pointer to the front element or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT ConstPointer tryFront() const noexcept
    {
        return (m_size > 0u) ? (m_data + m_head) : nullptr;
    }

    /// @brief Returns a pointer to the back element, or `nullptr` if the deque is empty.
    /// @return Pointer to the back element or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT Pointer tryBack() noexcept
    {
        return (m_size > 0u) ? (m_data + physicalIndex(m_size - 1u)) : nullptr;
    }

    /// @brief Returns a const pointer to the back element, or `nullptr` if the deque is empty.
    /// @return Const pointer to the back element or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT ConstPointer tryBack() const noexcept
    {
        return (m_size > 0u) ? (m_data + physicalIndex(m_size - 1u)) : nullptr;
    }

public:
    /// @brief Constructs an element in place at the back of the deque.
    /// @tparam InArgs Constructor argument types forwarded to `InT`.
    /// @param[in,out] args Constructor arguments.
    /// @return Reference to the newly constructed back element.
    template <typename... InArgs>
    requires std::is_constructible_v<InT, InArgs...>
    Reference emplaceBack(InArgs&&... args) noexcept(std::is_nothrow_constructible_v<InT, InArgs...>)
    {
        if (m_size == m_capacity) [[unlikely]]
        {
            if constexpr (std::is_move_constructible_v<InT>)
            {
                InT staged(std::forward<InArgs>(args)...);
                growForCapacity(m_size + 1u);
                const SizeType writeIdx = physicalIndex(m_size);
                ::new (static_cast<void*>(m_data + writeIdx)) InT(std::move(staged));
                ++m_size;
                return m_data[writeIdx];
            }
            else
            {
                growForCapacity(m_size + 1u);
            }
        }

        const SizeType writeIdx = physicalIndex(m_size);
        ::new (static_cast<void*>(m_data + writeIdx)) InT(std::forward<InArgs>(args)...);
        ++m_size;
        return m_data[writeIdx];
    }

    /// @brief Appends a copy of `value` to the back of the deque.
    /// @param[in] value Element value to copy.
    void pushBack(const InT& value) noexcept
    {
        (void)emplaceBack(value);
    }

    /// @brief Appends `value` to the back of the deque by move.
    /// @param[in,out] value Element value to move from.
    void pushBack(InT&& value) noexcept
    {
        (void)emplaceBack(std::move(value));
    }

    /// @brief Constructs an element in place at the front of the deque.
    /// @tparam InArgs Constructor argument types forwarded to `InT`.
    /// @param[in,out] args Constructor arguments.
    /// @return Reference to the newly constructed front element.
    template <typename... InArgs>
    requires std::is_constructible_v<InT, InArgs...>
    Reference emplaceFront(InArgs&&... args) noexcept(std::is_nothrow_constructible_v<InT, InArgs...>)
    {
        if (m_size == m_capacity) [[unlikely]]
        {
            if constexpr (std::is_move_constructible_v<InT>)
            {
                InT staged(std::forward<InArgs>(args)...);
                growForCapacity(m_size + 1u);
                const SizeType newHead = (m_head - 1u) & mask();
                ::new (static_cast<void*>(m_data + newHead)) InT(std::move(staged));
                m_head = newHead;
                ++m_size;
                return m_data[m_head];
            }
            else
            {
                growForCapacity(m_size + 1u);
            }
        }

        const SizeType newHead = (m_head - 1u) & mask();
        ::new (static_cast<void*>(m_data + newHead)) InT(std::forward<InArgs>(args)...);
        m_head = newHead;
        ++m_size;
        return m_data[m_head];
    }

    /// @brief Prepends a copy of `value` to the front of the deque.
    /// @param[in] value Element value to copy.
    void pushFront(const InT& value) noexcept
    {
        (void)emplaceFront(value);
    }

    /// @brief Prepends `value` to the front of the deque by move.
    /// @param[in,out] value Element value to move from.
    void pushFront(InT&& value) noexcept
    {
        (void)emplaceFront(std::move(value));
    }

    /// @brief Removes and destroys the back element of the deque.
    void popBack() noexcept
    {
        GP_ASSERT(m_size > 0u && "Deque::popBack() called on an empty deque.");
        const SizeType backIdx = physicalIndex(m_size - 1u);
        gp::memory::destructItem(m_data + backIdx);
        --m_size;
        if (m_size == 0u)
        {
            m_head = 0u;
        }
    }

    /// @brief Removes and destroys the front element of the deque.
    void popFront() noexcept
    {
        GP_ASSERT(m_size > 0u && "Deque::popFront() called on an empty deque.");
        gp::memory::destructItem(m_data + m_head);
        --m_size;
        m_head = (m_size == 0u) ? 0u : ((m_head + 1u) & mask());
    }

    /// @brief Moves the back element into `outValue` and pops it if the deque is non-empty.
    /// @param[out] outValue Receives the popped back element on success.
    /// @return True if an element was popped, false if the deque was empty.
    [[nodiscard]] bool tryPopBack(InT& outValue) noexcept
    {
        if (m_size == 0u)
        {
            return false;
        }
        const SizeType backIdx = physicalIndex(m_size - 1u);
        outValue = std::move(m_data[backIdx]);
        gp::memory::destructItem(m_data + backIdx);
        --m_size;
        if (m_size == 0u)
        {
            m_head = 0u;
        }
        return true;
    }

    /// @brief Moves the front element into `outValue` and pops it if the deque is non-empty.
    /// @param[out] outValue Receives the popped front element on success.
    /// @return True if an element was popped, false if the deque was empty.
    [[nodiscard]] bool tryPopFront(InT& outValue) noexcept
    {
        if (m_size == 0u)
        {
            return false;
        }
        outValue = std::move(m_data[m_head]);
        gp::memory::destructItem(m_data + m_head);
        --m_size;
        m_head = (m_size == 0u) ? 0u : ((m_head + 1u) & mask());
        return true;
    }

    /// @brief Inserts a new element constructed from `args...` at logical `index`, shifting the shorter half.
    /// @tparam InArgs Constructor argument types forwarded to `InT`.
    /// @param[in] index Logical insertion offset in `[0, size()]`.
    /// @param[in,out] args Constructor arguments for the inserted element.
    /// @return Iterator pointing to the inserted element.
    template <typename... InArgs>
    requires std::is_constructible_v<InT, InArgs...> Iterator emplace(const SizeType index, InArgs&&... args) noexcept
    {
        GP_ASSERT(index <= m_size && "Deque::emplace() index out of bounds.");
        if (index == 0u)
        {
            (void)emplaceFront(std::forward<InArgs>(args)...);
            return begin();
        }
        if (index == m_size)
        {
            (void)emplaceBack(std::forward<InArgs>(args)...);
            return begin() + static_cast<DifferenceType>(m_size - 1u);
        }

        if constexpr (std::is_move_constructible_v<InT> && std::is_move_assignable_v<InT>)
        {
            InT staged(std::forward<InArgs>(args)...);
            if (m_size == m_capacity)
            {
                growForCapacity(m_size + 1u);
            }

            const SizeType currentMask = mask();
            if (index <= (m_size / 2u))
            {
                // Shift `[0, index)` one slot toward `m_head - 1` so the shorter prefix moves.
                const SizeType newHead = (m_head - 1u) & currentMask;
                ::new (static_cast<void*>(m_data + newHead)) InT(std::move(m_data[m_head]));
                for (SizeType i = 0u; i + 1u < index; ++i)
                {
                    m_data[(m_head + i) & currentMask] = std::move(m_data[(m_head + i + 1u) & currentMask]);
                }
                m_data[(m_head + index - 1u) & currentMask] = std::move(staged);
                m_head = newHead;
                ++m_size;
            }
            else
            {
                // Shift `[index, m_size)` one slot toward the tail so the shorter suffix moves.
                const SizeType tailSlot = (m_head + m_size) & currentMask;
                ::new (static_cast<void*>(m_data + tailSlot))
                    InT(std::move(m_data[(m_head + m_size - 1u) & currentMask]));
                for (SizeType i = m_size - 1u; i > index; --i)
                {
                    m_data[(m_head + i) & currentMask] = std::move(m_data[(m_head + i - 1u) & currentMask]);
                }
                m_data[(m_head + index) & currentMask] = std::move(staged);
                ++m_size;
            }

            return begin() + static_cast<DifferenceType>(index);
        }
        else
        {
            GP_ASSERT(false && "Deque::emplace() at an interior index requires a movable element type.");
            return end();
        }
    }

    /// @brief Inserts a new element constructed from `args...` before iterator `pos`.
    /// @tparam InArgs Constructor argument types forwarded to `InT`.
    /// @param[in] pos Position in `[begin(), end()]`.
    /// @param[in,out] args Constructor arguments for the inserted element.
    /// @return Iterator pointing to the inserted element.
    template <typename... InArgs>
    requires std::is_constructible_v<InT, InArgs...>
    Iterator emplace(const ConstIterator pos, InArgs&&... args) noexcept
    {
        GP_ASSERT(pos.buffer() == m_data && "Deque::emplace() iterator belongs to a different deque.");
        const DifferenceType offset = pos - cbegin();
        GP_ASSERT(offset >= 0 && static_cast<SizeType>(offset) <= m_size && "Deque::emplace() iterator out of range.");
        return emplace(static_cast<SizeType>(offset), std::forward<InArgs>(args)...);
    }

    /// @brief Inserts a copy of `value` at logical `index`.
    /// @param[in] index Logical insertion offset in `[0, size()]`.
    /// @param[in] value Element value to copy.
    /// @return Iterator pointing to the inserted element.
    Iterator insert(const SizeType index, const InT& value) noexcept
    {
        return emplace(index, value);
    }

    /// @brief Inserts `value` at logical `index` by move.
    /// @param[in] index Logical insertion offset in `[0, size()]`.
    /// @param[in,out] value Element value to move from.
    /// @return Iterator pointing to the inserted element.
    Iterator insert(const SizeType index, InT&& value) noexcept
    {
        return emplace(index, std::move(value));
    }

    /// @brief Inserts a copy of `value` before iterator `pos`.
    /// @param[in] pos Insertion position in `[begin(), end()]`.
    /// @param[in] value Element value to copy.
    /// @return Iterator pointing to the inserted element.
    Iterator insert(const ConstIterator pos, const InT& value) noexcept
    {
        return emplace(pos, value);
    }

    /// @brief Inserts `value` before iterator `pos` by move.
    /// @param[in] pos Insertion position in `[begin(), end()]`.
    /// @param[in,out] value Element value to move from.
    /// @return Iterator pointing to the inserted element.
    Iterator insert(const ConstIterator pos, InT&& value) noexcept
    {
        return emplace(pos, std::move(value));
    }

    /// @brief Removes the element at logical `index`, shifting whichever side is shorter.
    /// @param[in] index Zero-based logical offset (`< size()`).
    /// @return Iterator to the element that followed the erased slot, or `end()`.
    Iterator erase(const SizeType index) noexcept
    {
        GP_ASSERT(index < m_size && "Deque::erase() index out of bounds.");
        if (index == 0u)
        {
            popFront();
            return begin();
        }
        if (index == (m_size - 1u))
        {
            popBack();
            return end();
        }

        const SizeType currentMask = mask();
        if (index < (m_size / 2u))
        {
            for (SizeType i = index; i > 0u; --i)
            {
                m_data[(m_head + i) & currentMask] = std::move(m_data[(m_head + i - 1u) & currentMask]);
            }
            gp::memory::destructItem(m_data + m_head);
            m_head = (m_head + 1u) & currentMask;
            --m_size;
        }
        else
        {
            for (SizeType i = index; (i + 1u) < m_size; ++i)
            {
                m_data[(m_head + i) & currentMask] = std::move(m_data[(m_head + i + 1u) & currentMask]);
            }
            gp::memory::destructItem(m_data + ((m_head + m_size - 1u) & currentMask));
            --m_size;
        }

        return begin() + static_cast<DifferenceType>(index);
    }

    /// @brief Removes the element referenced by `pos`.
    /// @param[in] pos Valid element iterator in `[begin(), end())`.
    /// @return Iterator to the element following `pos`, or `end()`.
    Iterator erase(const ConstIterator pos) noexcept
    {
        GP_ASSERT(pos.buffer() == m_data && "Deque::erase() iterator belongs to a different deque.");
        const DifferenceType offset = pos - cbegin();
        GP_ASSERT(offset >= 0 && static_cast<SizeType>(offset) < m_size && "Deque::erase() iterator out of range.");
        return erase(static_cast<SizeType>(offset));
    }

    /// @brief Resizes the deque to contain `newSize` elements, default-constructing any appended elements.
    /// @param[in] newSize Target logical element count.
    void resize(const SizeType newSize) noexcept
    {
        while (m_size > newSize)
        {
            popBack();
        }
        if (newSize > m_capacity)
        {
            reserve(newSize);
        }
        if (m_capacity < newSize) [[unlikely]]
        {
            return;
        }
        while (m_size < newSize)
        {
            const SizeType writeIdx = physicalIndex(m_size);
            ::new (static_cast<void*>(m_data + writeIdx)) InT();
            ++m_size;
        }
    }

    /// @brief Resizes the deque to contain `newSize` elements, copying `fillValue` into any appended slots.
    /// @param[in] newSize Target logical element count.
    /// @param[in] fillValue Value copied into newly appended elements.
    void resize(const SizeType newSize, const InT& fillValue) noexcept
    {
        if (newSize <= m_size)
        {
            while (m_size > newSize)
            {
                popBack();
            }
            return;
        }

        const InT copyValue(fillValue);
        reserve(newSize);
        if (m_capacity < newSize) [[unlikely]]
        {
            return;
        }
        while (m_size < newSize)
        {
            const SizeType writeIdx = physicalIndex(m_size);
            ::new (static_cast<void*>(m_data + writeIdx)) InT(copyValue);
            ++m_size;
        }
    }

public:
    /// @brief Returns the active element count.
    /// @return Number of stored elements.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType size() const noexcept
    {
        return m_size;
    }

    /// @brief Returns the byte size of active elements (`size() * sizeof(InT)`).
    /// @return Active payload size in bytes.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType sizeInBytes() const noexcept
    {
        return m_size * sizeof(InT);
    }

    /// @brief Returns the current power-of-two capacity in elements.
    /// @return Allocated capacity (`0` or a power of two).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType capacity() const noexcept
    {
        return m_capacity;
    }

    /// @brief Returns the maximum element capacity representable without overflow.
    /// @return Largest power-of-two element count fitting in `SizeType` bytes.
    [[nodiscard]] constexpr SizeType maxSize() const noexcept
    {
        return std::numeric_limits<SizeType>::max() / sizeof(InT);
    }

    /// @brief Checks whether the deque contains zero elements.
    /// @return True if `size() == 0`, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isEmpty() const noexcept
    {
        return m_size == 0u;
    }

    /// @brief Ensures capacity is at least `minCapacity` (rounded up to the next power of two).
    /// @param[in] minCapacity Minimum required element capacity.
    void reserve(const SizeType minCapacity) noexcept
    {
        if (minCapacity <= m_capacity)
        {
            return;
        }

        const SizeType targetCapacity = normalizeCapacity(minCapacity);
        reallocateBuffer(targetCapacity);
    }

    /// @brief Shrinks the ring allocation to the smallest power of two fitting `size()`, or frees it if empty.
    void shrinkToFit() noexcept
    {
        if (m_size == 0u)
        {
            reset();
            return;
        }

        const SizeType targetCapacity = normalizeCapacity(m_size);
        if (targetCapacity < m_capacity)
        {
            reallocateBuffer(targetCapacity);
        }
    }

    /// @brief Destroys all elements while preserving the allocated ring buffer capacity.
    void clear() noexcept
    {
        if (m_size > 0u)
        {
            if constexpr (!gp::concepts::IsTriviallyDestructible<InT>)
            {
                const SizeType currentMask = mask();
                for (SizeType i = 0u; i < m_size; ++i)
                {
                    gp::memory::destructItem(m_data + ((m_head + i) & currentMask));
                }
            }
            m_size = 0u;
        }
        m_head = 0u;
    }

    /// @brief Destroys all elements and returns the ring buffer to `m_allocator`.
    void reset() noexcept
    {
        clear();
        if (m_data != nullptr)
        {
            m_allocator.deallocate(m_data, m_capacity * sizeof(InT), kAllocAlignment);
            m_data = nullptr;
        }
        m_capacity = 0u;
        m_head = 0u;
    }

    /// @brief Checks whether active elements occupy a single contiguous memory span without wrapping.
    /// @return True if empty or `m_head + m_size <= m_capacity`, false if split across the buffer end.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isLinear() const noexcept
    {
        return m_size == 0u || (m_head + m_size) <= m_capacity;
    }

    /// @brief Arranges all active elements contiguously starting at index `0` (`m_head == 0`).
    /// @details Shifts un-wrapped rings left in place. When wrapped for an `IsRelocatable` element type, rearranges
    ///          the ring completely in place with zero heap allocation and zero reads from uninitialized gap slots.
    void linearize() noexcept
    {
        if (m_size == 0u)
        {
            m_head = 0u;
            return;
        }

        if (m_head == 0u)
        {
            return;
        }

        if (isLinear())
        {
            if constexpr (gp::concepts::IsRelocatable<InT> || std::is_move_constructible_v<InT>)
            {
                gp::memory::relocateConstructItems<InT, InT>(m_data, m_data + m_head, m_size);
                m_head = 0u;
            }
            else
            {
                GP_ASSERT(false && "Deque::linearize() cannot move non-movable element type.");
            }
            return;
        }

        if constexpr (gp::concepts::IsRelocatable<InT>)
        {
            const SizeType tailSpace = m_capacity - m_head;
            const SizeType firstCount = (m_size <= tailSpace) ? m_size : tailSpace;
            const SizeType secondCount = m_size - firstCount;
            const SizeType gap = m_capacity - m_size;

            if (gap == 0u)
            {
                rotateRelocatableLeft(m_data, m_capacity, m_head);
                m_head = 0u;
            }
            else if (m_size <= gap)
            {
                linearizeWrappedWithLargeGap(firstCount, secondCount);
            }
            else
            {
                gp::memory::moveMemory(m_data + gap, m_data, secondCount * sizeof(InT));
                rotateRelocatableLeft(m_data + gap, m_size, secondCount);
                gp::memory::moveMemory(m_data, m_data + gap, m_size * sizeof(InT));
                m_head = 0u;
            }
            return;
        }

        reallocateBuffer(m_capacity);
    }

    /// @brief Outputs up to two contiguous mutable memory spans covering all elements in logical order.
    /// @details Lets streaming I/O or GPU upload helpers consume the ring with zero copying or linearization.
    /// @param[out] outFirst Span covering elements starting at `m_head` up to buffer end.
    /// @param[out] outSecond Span covering wrapped elements starting at `m_data`, or empty if un-wrapped.
    void getSpans(Span& outFirst, Span& outSecond) noexcept
    {
        if (m_size == 0u)
        {
            outFirst = Span{};
            outSecond = Span{};
            return;
        }

        const SizeType tailSpace = m_capacity - m_head;
        const SizeType firstCount = (m_size <= tailSpace) ? m_size : tailSpace;
        const SizeType secondCount = m_size - firstCount;

        outFirst = Span{ m_data + m_head, firstCount };
        outSecond = (secondCount > 0u) ? Span{ m_data, secondCount } : Span{};
    }

    /// @brief Outputs up to two contiguous read-only memory spans covering all elements in logical order.
    /// @param[out] outFirst Span covering elements starting at `m_head` up to buffer end.
    /// @param[out] outSecond Span covering wrapped elements starting at `m_data`, or empty if un-wrapped.
    void getSpans(ConstSpan& outFirst, ConstSpan& outSecond) const noexcept
    {
        if (m_size == 0u)
        {
            outFirst = ConstSpan{};
            outSecond = ConstSpan{};
            return;
        }

        const SizeType tailSpace = m_capacity - m_head;
        const SizeType firstCount = (m_size <= tailSpace) ? m_size : tailSpace;
        const SizeType secondCount = m_size - firstCount;

        outFirst = ConstSpan{ m_data + m_head, firstCount };
        outSecond = (secondCount > 0u) ? ConstSpan{ m_data, secondCount } : ConstSpan{};
    }

public:
    /// @brief Returns a mutable random-access iterator to the front element.
    /// @return Iterator to the start of the deque.
    [[nodiscard]] constexpr Iterator begin() noexcept
    {
        return Iterator(m_data, mask(), m_head);
    }

    /// @brief Returns a const random-access iterator to the front element.
    /// @return Const iterator to the start of the deque.
    [[nodiscard]] constexpr ConstIterator begin() const noexcept
    {
        return ConstIterator(m_data, mask(), m_head);
    }

    /// @brief Returns a mutable random-access iterator one past the back element.
    /// @return End iterator.
    [[nodiscard]] constexpr Iterator end() noexcept
    {
        return Iterator(m_data, mask(), m_head + m_size);
    }

    /// @brief Returns a const random-access iterator one past the back element.
    /// @return Const end iterator.
    [[nodiscard]] constexpr ConstIterator end() const noexcept
    {
        return ConstIterator(m_data, mask(), m_head + m_size);
    }

    /// @brief Returns a const iterator to the front element.
    /// @return Const start iterator.
    [[nodiscard]] constexpr ConstIterator cbegin() const noexcept
    {
        return begin();
    }

    /// @brief Returns a const iterator one past the back element.
    /// @return Const end iterator.
    [[nodiscard]] constexpr ConstIterator cend() const noexcept
    {
        return end();
    }

    /// @brief Returns a mutable reverse iterator starting at the back element.
    /// @return Reverse start iterator.
    [[nodiscard]] constexpr ReverseIterator rbegin() noexcept
    {
        return ReverseIterator(end());
    }

    /// @brief Returns a const reverse iterator starting at the back element.
    /// @return Const reverse start iterator.
    [[nodiscard]] constexpr ConstReverseIterator rbegin() const noexcept
    {
        return ConstReverseIterator(end());
    }

    /// @brief Returns a mutable reverse iterator one before the front element.
    /// @return Reverse end iterator.
    [[nodiscard]] constexpr ReverseIterator rend() noexcept
    {
        return ReverseIterator(begin());
    }

    /// @brief Returns a const reverse iterator one before the front element.
    /// @return Const reverse end iterator.
    [[nodiscard]] constexpr ConstReverseIterator rend() const noexcept
    {
        return ConstReverseIterator(begin());
    }

    /// @brief Returns a const reverse iterator starting at the back element.
    /// @return Const reverse start iterator.
    [[nodiscard]] constexpr ConstReverseIterator crbegin() const noexcept
    {
        return ConstReverseIterator(end());
    }

    /// @brief Returns a const reverse iterator one before the front element.
    /// @return Const reverse end iterator.
    [[nodiscard]] constexpr ConstReverseIterator crend() const noexcept
    {
        return ConstReverseIterator(begin());
    }

public:
    /// @brief Finds the first element equal to `value`.
    /// @param[in] value Target value to match.
    /// @return Iterator to the matching element, or `end()` if absent.
    [[nodiscard]] Iterator find(const InT& value) noexcept requires gp::concepts::IsEqualityComparable<InT>
    {
        const SizeType currentMask = mask();
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (m_data[(m_head + i) & currentMask] == value)
            {
                return Iterator(m_data, currentMask, m_head + i);
            }
        }
        return end();
    }

    /// @brief Finds the first element equal to `value` (`const` overload).
    /// @param[in] value Target value to match.
    /// @return Const iterator to the matching element, or `end()` if absent.
    [[nodiscard]] ConstIterator find(const InT& value) const noexcept requires gp::concepts::IsEqualityComparable<InT>
    {
        const SizeType currentMask = mask();
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (m_data[(m_head + i) & currentMask] == value)
            {
                return ConstIterator(m_data, currentMask, m_head + i);
            }
        }
        return end();
    }

    /// @brief Finds the first element satisfying unary predicate `predicate`.
    /// @tparam InPredicate Predicate type satisfying `gp::concepts::IsPredicate<InPredicate, const InT&>`.
    /// @param[in,out] predicate Predicate tested against each element in front-to-back order.
    /// @return Mutable iterator to the first matching element, or `end()` if none match.
    template <typename InPredicate>
    requires gp::concepts::IsPredicate<InPredicate, const InT&>
    [[nodiscard]] Iterator findIf(InPredicate&& predicate) noexcept
    {
        const SizeType currentMask = mask();
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (predicate(m_data[(m_head + i) & currentMask]))
            {
                return Iterator(m_data, currentMask, m_head + i);
            }
        }
        return end();
    }

    /// @brief Finds the first element satisfying unary predicate `predicate` (`const` overload).
    /// @tparam InPredicate Predicate type satisfying `gp::concepts::IsPredicate<InPredicate, const InT&>`.
    /// @param[in,out] predicate Predicate tested against each element in front-to-back order.
    /// @return Const iterator to the first matching element, or `end()` if none match.
    template <typename InPredicate>
    requires gp::concepts::IsPredicate<InPredicate, const InT&>
    [[nodiscard]] ConstIterator findIf(InPredicate&& predicate) const noexcept
    {
        const SizeType currentMask = mask();
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (predicate(m_data[(m_head + i) & currentMask]))
            {
                return ConstIterator(m_data, currentMask, m_head + i);
            }
        }
        return end();
    }

    /// @brief Finds the logical index of the first element equal to `value`.
    /// @param[in] value Target value to match.
    /// @return Logical index in `[0, size())`, or `npos` if absent.
    [[nodiscard]] SizeType indexOf(const InT& value) const noexcept requires gp::concepts::IsEqualityComparable<InT>
    {
        const SizeType currentMask = mask();
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (m_data[(m_head + i) & currentMask] == value)
            {
                return i;
            }
        }
        return npos;
    }

    /// @brief Checks whether any element compares equal to `value`.
    /// @param[in] value Target value to match.
    /// @return True if present, false otherwise.
    [[nodiscard]] bool contains(const InT& value) const noexcept requires gp::concepts::IsEqualityComparable<InT>
    {
        return indexOf(value) != npos;
    }

    /// @brief Counts elements comparing equal to `value`.
    /// @param[in] value Target value to match.
    /// @return Number of matching elements.
    [[nodiscard]] SizeType count(const InT& value) const noexcept requires gp::concepts::IsEqualityComparable<InT>
    {
        SizeType matches = 0u;
        const SizeType currentMask = mask();
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (m_data[(m_head + i) & currentMask] == value)
            {
                ++matches;
            }
        }
        return matches;
    }

    /// @brief Streams all elements in logical front-to-back order plus trailing length framing into `hasher`.
    /// @tparam InHasher Concrete streaming hasher type (`gp::hashing::hash()` / `hashOf()`).
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        const SizeType currentMask = mask();
        for (SizeType i = 0u; i < m_size; ++i)
        {
            hasher.append(m_data[(m_head + i) & currentMask]);
        }
        hasher.append(static_cast<gp::UInt64>(m_size));
    }

    /// @brief Returns a mutable reference to the bound allocator instance.
    /// @return Reference to `m_allocator`.
    [[nodiscard]] constexpr InAllocator& getAllocator() noexcept
    {
        return m_allocator;
    }

    /// @brief Returns a const reference to the bound allocator instance.
    /// @return Const reference to `m_allocator`.
    [[nodiscard]] constexpr const InAllocator& getAllocator() const noexcept
    {
        return m_allocator;
    }

    /// @brief Exchanges contents with `other` in O(1) when allocators are equal, or via element moves otherwise.
    /// @param[in,out] other Target deque to swap with.
    void swap(Deque& other) noexcept
    {
        if (this == &other)
        {
            return;
        }

        if (m_allocator == other.m_allocator)
        {
            std::swap(m_data, other.m_data);
            std::swap(m_head, other.m_head);
            std::swap(m_size, other.m_size);
            std::swap(m_capacity, other.m_capacity);
            return;
        }

        Deque temp(std::move(*this));
        *this = std::move(other);
        other = std::move(temp);
    }

public:
    /// @brief Compares two deques (including across allocator types) for element-wise equality.
    /// @tparam InOtherAllocator Right-hand deque's allocator type.
    /// @param[in] other Deque to compare against.
    /// @return True if sizes and every corresponding element compare equal, false otherwise.
    template <gp::concepts::IsAllocator InOtherAllocator>
    requires gp::concepts::IsEqualityComparable<InT>
    [[nodiscard]] bool operator==(const Deque<InT, InOtherAllocator>& other) const noexcept
    {
        if (m_size != other.m_size)
        {
            return false;
        }

        const SizeType maskA = mask();
        const SizeType maskB = other.mask();
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (!(m_data[(m_head + i) & maskA] == other.m_data[(other.m_head + i) & maskB]))
            {
                return false;
            }
        }
        return true;
    }

    /// @brief Performs lexicographical three-way comparison against another deque.
    /// @tparam InOtherAllocator Right-hand deque's allocator type.
    /// @param[in] other Deque to compare against.
    /// @return Three-way comparison ordering result matching `InT`.
    template <gp::concepts::IsAllocator InOtherAllocator, typename InElemT = InT>
    requires gp::concepts::IsThreeWayComparable<InElemT> [[nodiscard]] std::compare_three_way_result_t<InElemT>
        operator<=>(const Deque<InT, InOtherAllocator>& other) const noexcept
    {
        const SizeType commonSize = (m_size < other.m_size) ? m_size : other.m_size;
        const SizeType maskA = mask();
        const SizeType maskB = other.mask();
        for (SizeType i = 0u; i < commonSize; ++i)
        {
            const std::compare_three_way_result_t<InElemT> cmp =
                (m_data[(m_head + i) & maskA] <=> other.m_data[(other.m_head + i) & maskB]);
            if (cmp != 0)
            {
                return cmp;
            }
        }
        return m_size <=> other.m_size;
    }

private:
    /// @brief Returns the bitmask `m_capacity - 1` (or `0` when unallocated).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType mask() const noexcept
    {
        return (m_capacity > 0u) ? (m_capacity - 1u) : 0u;
    }

    /// @brief Converts a logical offset `i` in `[0, m_capacity)` into its wrapped ring slot index.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType physicalIndex(const SizeType logicalIndex) const noexcept
    {
        return (m_head + logicalIndex) & mask();
    }

    /// @brief Normalizes a non-zero element requirement up to the nearest valid power of two.
    [[nodiscard]] SizeType normalizeCapacity(const SizeType minCapacity) const noexcept
    {
        if (minCapacity == 0u)
        {
            return 0u;
        }
        const SizeType clamped = (minCapacity < kDefaultInitialCapacity) ? kDefaultInitialCapacity : minCapacity;
        const SizeType rounded = gp::math::roundUpToPowerOfTwo(clamped);
        GP_ASSERT(rounded != 0u && rounded <= maxSize() && "Deque capacity overflow.");
        return rounded;
    }

    /// @brief Grows the ring capacity to fit at least `minRequired` elements.
    void growForCapacity(const SizeType minRequired) noexcept
    {
        const SizeType nextCap = (m_capacity == 0u) ? normalizeCapacity(minRequired) : (m_capacity * 2u);
        const SizeType targetCap = (nextCap >= minRequired) ? nextCap : normalizeCapacity(minRequired);
        reallocateBuffer(targetCap);
    }

    /// @brief Reallocates or linearizes the ring buffer to hold exactly `targetCapacity` slots at `m_head = 0`.
    void reallocateBuffer(const SizeType targetCapacity) noexcept
    {
        GP_ASSERT(targetCapacity >= m_size && gp::math::isPowerOfTwo(targetCapacity));
        const SizeType newBytes = targetCapacity * sizeof(InT);

        if (m_data == nullptr)
        {
            const Pointer freshData = static_cast<Pointer>(m_allocator.allocate(newBytes, kAllocAlignment));
            GP_ASSERT(freshData != nullptr && "Deque allocation failed.");
            if (freshData == nullptr) [[unlikely]]
            {
                return;
            }
            m_data = freshData;
            m_capacity = targetCapacity;
            m_head = 0u;
            return;
        }

        if (m_size == 0u)
        {
            m_allocator.deallocate(m_data, m_capacity * sizeof(InT), kAllocAlignment);
            m_data = static_cast<Pointer>(m_allocator.allocate(newBytes, kAllocAlignment));
            GP_ASSERT(m_data != nullptr && "Deque allocation failed.");
            m_capacity = (m_data != nullptr) ? targetCapacity : 0u;
            m_head = 0u;
            return;
        }

        // Fast path for relocatable buffers: linearize in place and resize via reallocate() so LinearAllocator and
        // InlineAllocator expand or shrink without spilling or stranding blocks.
        if constexpr (gp::concepts::IsRelocatable<InT>)
        {
            const SizeType oldCap = m_capacity;
            const bool wrapped = (m_head + m_size) > oldCap;
            const SizeType tailSpace = oldCap - m_head;
            const SizeType firstCount = (m_size <= tailSpace) ? m_size : tailSpace;
            const SizeType secondCount = m_size - firstCount;

            if (targetCapacity > oldCap)
            {
                const Pointer resized = static_cast<Pointer>(
                    m_allocator.reallocate(m_data, oldCap * sizeof(InT), newBytes, kAllocAlignment)
                );
                GP_ASSERT(resized != nullptr && "Deque reallocation failed.");
                if (resized == nullptr) [[unlikely]]
                {
                    return;
                }
                m_data = resized;
                m_capacity = targetCapacity;

                if (wrapped)
                {
                    // `targetCapacity >= 2 * oldCap` and `secondCount < oldCap`, so `[oldCap, oldCap + secondCount)`
                    // never overlaps `[0, oldCap)`.
                    gp::memory::moveMemory(m_data + oldCap, m_data, secondCount * sizeof(InT));
                }
                if (m_head > 0u)
                {
                    gp::memory::moveMemory(m_data, m_data + m_head, m_size * sizeof(InT));
                    m_head = 0u;
                }
                return;
            }

            if (!wrapped)
            {
                if (m_head > 0u)
                {
                    gp::memory::moveMemory(m_data, m_data + m_head, m_size * sizeof(InT));
                    m_head = 0u;
                }
                shrinkRelocatableStorage(oldCap, targetCapacity, newBytes);
                return;
            }

            if (m_size <= (oldCap / 2u))
            {
                linearizeWrappedWithLargeGap(firstCount, secondCount);
                shrinkRelocatableStorage(oldCap, targetCapacity, newBytes);
                return;
            }
        }

        if constexpr (gp::concepts::IsRelocatable<InT> || std::is_move_constructible_v<InT>)
        {
            const Pointer newData = static_cast<Pointer>(m_allocator.allocate(newBytes, kAllocAlignment));
            GP_ASSERT(newData != nullptr && "Deque allocation failed.");
            if (newData == nullptr) [[unlikely]]
            {
                return;
            }

            const SizeType tailSpace = m_capacity - m_head;
            const SizeType firstCount = (m_size <= tailSpace) ? m_size : tailSpace;
            const SizeType secondCount = m_size - firstCount;

            gp::memory::relocateConstructItems<InT, InT>(newData, m_data + m_head, firstCount);
            if (secondCount > 0u)
            {
                gp::memory::relocateConstructItems<InT, InT>(newData + firstCount, m_data, secondCount);
            }

            m_allocator.deallocate(m_data, m_capacity * sizeof(InT), kAllocAlignment);
            m_data = newData;
            m_capacity = targetCapacity;
            m_head = 0u;
        }
        else
        {
            GP_ASSERT(false && "Deque cannot relocate non-movable element type.");
        }
    }

    /// @brief Linearizes a wrapped relocatable ring whose free gap `[secondCount, m_head)` holds at least `m_size`
    /// slots.
    void linearizeWrappedWithLargeGap(const SizeType firstCount, const SizeType secondCount) noexcept
    {
        gp::memory::moveMemory(m_data + secondCount, m_data + m_head, firstCount * sizeof(InT));
        gp::memory::moveMemory(m_data + secondCount + firstCount, m_data, secondCount * sizeof(InT));
        gp::memory::moveMemory(m_data, m_data + secondCount, m_size * sizeof(InT));
        m_head = 0u;
    }

    /// @brief Shrinks an already linearized relocatable ring buffer in place when `targetCapacity < oldCap`.
    void
        shrinkRelocatableStorage(const SizeType oldCap, const SizeType targetCapacity, const SizeType newBytes) noexcept
    {
        if (targetCapacity == oldCap)
        {
            return;
        }
        const Pointer shrunk =
            static_cast<Pointer>(m_allocator.reallocate(m_data, oldCap * sizeof(InT), newBytes, kAllocAlignment));
        GP_ASSERT(shrunk != nullptr && "Deque shrink reallocation failed.");
        if (shrunk == nullptr) [[unlikely]]
        {
            return;
        }
        m_data = shrunk;
        m_capacity = targetCapacity;
    }

    /// @brief Rotates `count` contiguous relocatable elements left by `pivot` using 64-byte stack chunks.
    static void rotateRelocatableLeft(Pointer data, const SizeType count, const SizeType pivot) noexcept
    {
        if (pivot == 0u || pivot >= count)
        {
            return;
        }

        reverseRelocatableRange(data, pivot);
        reverseRelocatableRange(data + pivot, count - pivot);
        reverseRelocatableRange(data, count);
    }

    /// @brief Reverses `count` contiguous elements by swapping aligned pairs across endpoints.
    static void reverseRelocatableRange(Pointer data, const SizeType count) noexcept
    {
        SizeType left = 0u;
        SizeType right = count;
        while ((left + 1u) < right)
        {
            --right;
            swapRelocatableElements(data + left, data + right);
            ++left;
        }
    }

    /// @brief Swaps two disjoint `InT` elements bitwise through a 64-byte stack scratch buffer.
    static void swapRelocatableElements(Pointer lhs, Pointer rhs) noexcept
    {
        alignas(64) gp::Byte scratch[64];
        if constexpr (sizeof(InT) <= sizeof(scratch))
        {
            gp::memory::copyMemory(scratch, lhs, sizeof(InT));
            gp::memory::copyMemory(lhs, rhs, sizeof(InT));
            gp::memory::copyMemory(rhs, scratch, sizeof(InT));
        }
        else
        {
            gp::UInt8* bytesA = reinterpret_cast<gp::UInt8*>(lhs);
            gp::UInt8* bytesB = reinterpret_cast<gp::UInt8*>(rhs);
            SizeType remaining = sizeof(InT);
            while (remaining > 0u)
            {
                const SizeType chunk = (remaining < sizeof(scratch)) ? remaining : sizeof(scratch);
                gp::memory::copyMemory(scratch, bytesA, chunk);
                gp::memory::copyMemory(bytesA, bytesB, chunk);
                gp::memory::copyMemory(bytesB, scratch, chunk);
                bytesA += chunk;
                bytesB += chunk;
                remaining -= chunk;
            }
        }
    }

    /// @brief Copies all elements from another deque in logical front-to-back order.
    template <typename InOtherDeque>
    void copyElementsFrom(const InOtherDeque& other) noexcept
    {
        if (other.m_size == 0u)
        {
            return;
        }
        reserve(other.m_size);
        if (m_data == nullptr) [[unlikely]]
        {
            return;
        }
        const SizeType otherMask = other.mask();
        for (SizeType i = 0u; i < other.m_size; ++i)
        {
            ::new (static_cast<void*>(m_data + i)) InT(other.m_data[(other.m_head + i) & otherMask]);
        }
        m_size = other.m_size;
    }

    /// @brief Moves all elements from another deque into `*this` in logical order.
    void moveElementsFrom(Deque& other) noexcept
    {
        if (other.m_size == 0u)
        {
            return;
        }
        if constexpr (std::is_move_constructible_v<InT>)
        {
            reserve(other.m_size);
            if (m_data == nullptr) [[unlikely]]
            {
                return;
            }
            const SizeType otherMask = other.mask();
            for (SizeType i = 0u; i < other.m_size; ++i)
            {
                ::new (static_cast<void*>(m_data + i)) InT(std::move(other.m_data[(other.m_head + i) & otherMask]));
            }
            m_size = other.m_size;
        }
        else
        {
            GP_ASSERT(false && "Deque cannot move non-movable elements across unequal allocators.");
        }
    }

    /// @brief Steals internal buffer pointers from `other` in O(1) time.
    void stealFrom(Deque& other) noexcept
    {
        m_data = other.m_data;
        m_head = other.m_head;
        m_size = other.m_size;
        m_capacity = other.m_capacity;

        other.m_data = nullptr;
        other.m_head = 0u;
        other.m_size = 0u;
        other.m_capacity = 0u;
    }

private:
    Pointer m_data{ nullptr };
    SizeType m_head{ 0u };
    SizeType m_size{ 0u };
    SizeType m_capacity{ 0u };
    GP_NO_UNIQUE_ADDRESS InAllocator m_allocator{};
};

/// @brief Swaps two `Deque` instances via `lhs.swap(rhs)`.
/// @tparam InT Element type.
/// @tparam InAllocator Allocator type.
/// @param[in,out] lhs First deque.
/// @param[in,out] rhs Second deque.
template <typename InT, gp::concepts::IsAllocator InAllocator>
inline void swap(Deque<InT, InAllocator>& lhs, Deque<InT, InAllocator>& rhs) noexcept
{
    lhs.swap(rhs);
}

static_assert(
    sizeof(Deque<gp::Int32>) == 32u, "gp::Deque with stateless DefaultAllocator must occupy exactly 32 bytes."
);
static_assert(
    std::random_access_iterator<Deque<gp::Int32>::Iterator>,
    "gp::Deque::Iterator must satisfy std::random_access_iterator."
);
static_assert(
    std::random_access_iterator<Deque<gp::Int32>::ConstIterator>,
    "gp::Deque::ConstIterator must satisfy std::random_access_iterator."
);

}   // namespace gp

namespace gp::concepts
{

namespace detail
{

template <typename InType>
constexpr bool IsDequeV = false;

template <typename InT, typename InAllocator>
constexpr bool IsDequeV<gp::Deque<InT, InAllocator>> = true;

}   // namespace detail

/// @brief Concept satisfied by any instantiation of `gp::Deque`.
/// @tparam InType Candidate type to test.
template <typename InType>
concept IsDeque = detail::IsDequeV<std::remove_cvref_t<InType>>;

/// @brief Specialization enabling bitwise relocation of `gp::Deque` when its allocator is bitwise relocatable.
/// @tparam InT Element type.
/// @tparam InAllocator Allocator type.
template <typename InT, typename InAllocator>
struct IsRelocatableTrait<gp::Deque<InT, InAllocator>>
    : std::bool_constant<std::is_move_constructible_v<InAllocator> && IsRelocatable<InAllocator>>
{};

}   // namespace gp::concepts
