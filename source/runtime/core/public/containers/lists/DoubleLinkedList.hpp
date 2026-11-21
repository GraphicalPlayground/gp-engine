// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "containers/ContainerForward.hpp"   // IWYU pragma: keep
#include "containers/details/SlotPoolBase.hpp"
#include "containers/lists/ListTypes.hpp"
#include "CoreMinimal.hpp"
#include "memory/allocators/AllocatorConcepts.hpp"
#include "memory/allocators/HeapAllocator.hpp"   // IWYU pragma: keep
#include <compare>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

namespace gp
{

/// @brief Generational handle-addressed doubly-linked list backed by a contiguous slot pool.
/// @details Storing every node inside one contiguous slot array managed through `InAllocator` avoids per-node heap
///          allocations and pointer chasing. Each node yields an 8-byte generational `NodeHandle` (`index` +
///          `generation`) that stays valid across slot-pool growth and catches stale or double-removed handles in O(1)
///          time. Supports O(1) front/back insertion, O(1) arbitrary handle insertion/removal, and O(1) in-place
///          `moveToFront()` / `moveToBack()` relinking for LRU caches and asset streaming lists.
/// @note Because slot-pool growth relocates the backing buffer, raw `T*` pointers or references obtained from `get()`
///       are invalidated on growth, whereas `NodeHandle` values and `Iterator` positions remain valid across growth.
/// @tparam InT Element type stored in the list (cannot be const or a reference).
/// @tparam InAllocator Engine memory allocator satisfying `gp::concepts::IsAllocator`.
template <typename InT, typename InAllocator>
class DoubleLinkedList
{
    static_assert(!gp::concepts::IsConst<InT>, "DoubleLinkedList element type cannot be const.");
    static_assert(!gp::concepts::IsReference<InT>, "DoubleLinkedList element type cannot be a reference.");
    static_assert(
        gp::concepts::IsAllocator<InAllocator>, "DoubleLinkedList requires InAllocator to satisfy IsAllocator."
    );

private:
    struct Slot
    {
        alignas(InT) gp::Byte storage[sizeof(InT)];
        gp::UInt32 prev{ kInvalidListNodeIndex };
        gp::UInt32 next{ kInvalidListNodeIndex };
        gp::UInt32 generation{ 0u };
    };

    using StorageType = container::detail::SlotPoolStorage<InT, Slot, InAllocator>;

    template <typename InOtherT, typename InOtherAllocator>
    friend class DoubleLinkedList;

    template <typename InOwner, typename InElementType, typename InHandleType, bool InIsBidirectional>
    friend class container::detail::SlotListIterator;

public:
    using ValueType = InT;
    using AllocatorType = InAllocator;
    using SizeType = gp::USize;
    using DifferenceType = gp::ISize;
    using Reference = ValueType&;
    using ConstReference = const ValueType&;
    using Pointer = ValueType*;
    using ConstPointer = const ValueType*;
    using NodeHandle = DoubleLinkedListNodeHandle<InT>;
    using Iterator = container::detail::SlotListIterator<DoubleLinkedList, ValueType, NodeHandle, true>;
    using ConstIterator =
        container::detail::SlotListIterator<const DoubleLinkedList, const ValueType, NodeHandle, true>;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

    /// @brief Byte alignment requested from `InAllocator` for the internal slot array.
    static constexpr gp::UInt32 kAllocAlignment = StorageType::kAllocAlignment;

public:
    /// @brief Constructs an empty list with zero allocations.
    constexpr DoubleLinkedList() noexcept = default;

    /// @brief Constructs an empty list bound to a copy of `allocator`.
    /// @param[in] allocator Allocator instance to copy.
    constexpr explicit DoubleLinkedList(const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_storage(allocator)
    {}

    /// @brief Constructs an empty list by moving `allocator`.
    /// @param[in,out] allocator Allocator instance to move from.
    constexpr explicit DoubleLinkedList(InAllocator&& allocator) noexcept
        requires std::is_move_constructible_v<InAllocator>
        : m_storage(std::move(allocator))
    {}

    /// @brief Constructs a list containing `count` default-constructed elements.
    /// @param[in] count Initial number of elements.
    explicit DoubleLinkedList(const SizeType count) noexcept
    {
        if (count > 0u)
        {
            reserve(count);
            for (SizeType i = 0u; i < count; ++i)
            {
                (void)emplaceBack();
            }
        }
    }

    /// @brief Constructs a list containing `count` copies of `value`.
    /// @param[in] count Number of elements to insert.
    /// @param[in] value Fill value copied into each node.
    DoubleLinkedList(const SizeType count, const InT& value) noexcept
    {
        assign(count, value);
    }

    /// @brief Constructs a list containing `count` copies of `value` using `allocator`.
    /// @param[in] count Number of elements to insert.
    /// @param[in] value Fill value copied into each node.
    /// @param[in] allocator Allocator instance to copy.
    DoubleLinkedList(const SizeType count, const InT& value, const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_storage(allocator)
    {
        assign(count, value);
    }

    /// @brief Constructs a list from an iterator range `[first, last)`.
    /// @tparam InInputIt Input iterator type yielding elements convertible to `InT`.
    /// @param[in] first Start of source range.
    /// @param[in] last End of source range.
    template <typename InInputIt>
    requires(!gp::concepts::IsIntegral<InInputIt>) DoubleLinkedList(InInputIt first, InInputIt last) noexcept
    {
        assign(first, last);
    }

    /// @brief Constructs a list from an iterator range `[first, last)` using `allocator`.
    /// @tparam InInputIt Input iterator type yielding elements convertible to `InT`.
    /// @param[in] first Start of source range.
    /// @param[in] last End of source range.
    /// @param[in] allocator Allocator instance to copy.
    template <typename InInputIt>
    requires(!gp::concepts::IsIntegral<InInputIt> && std::is_copy_constructible_v<InAllocator>)
    DoubleLinkedList(InInputIt first, InInputIt last, const InAllocator& allocator) noexcept
        : m_storage(allocator)
    {
        assign(first, last);
    }

    /// @brief Constructs a list from an initializer list.
    /// @param[in] init Initializer list of elements inserted in sequence.
    DoubleLinkedList(const std::initializer_list<InT> init) noexcept
    {
        assign(init);
    }

    /// @brief Constructs a list from an initializer list using `allocator`.
    /// @param[in] init Initializer list of elements inserted in sequence.
    /// @param[in] allocator Allocator instance to copy.
    DoubleLinkedList(const std::initializer_list<InT> init, const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_storage(allocator)
    {
        assign(init);
    }

    /// @brief Copy constructor when `InAllocator` is copy-constructible.
    /// @details Copies elements densely in logical head-to-tail order into fresh slots.
    /// @param[in] other Source list to copy from.
    DoubleLinkedList(const DoubleLinkedList& other) noexcept requires std::is_copy_constructible_v<InAllocator>
        : m_storage(other.m_storage.m_allocator)
    {
        m_storage.copyChainFrom(other.m_storage);
    }

    /// @brief Copy constructor when `InAllocator` is non-copyable (such as `InlineAllocator`).
    /// @param[in] other Source list to copy into a default-constructed allocator.
    DoubleLinkedList(const DoubleLinkedList& other) noexcept
        requires(!std::is_copy_constructible_v<InAllocator> && std::is_default_constructible_v<InAllocator>)
    {
        m_storage.copyChainFrom(other.m_storage);
    }

    /// @brief Cross-allocator copy constructor from any `DoubleLinkedList` sharing element type `InT`.
    /// @tparam InOtherAllocator Source list's allocator type.
    /// @param[in] other Source list to copy elements from.
    template <gp::concepts::IsAllocator InOtherAllocator>
    requires(!gp::concepts::IsSameAs<InAllocator, InOtherAllocator>)
    explicit DoubleLinkedList(const DoubleLinkedList<InT, InOtherAllocator>& other) noexcept
    {
        m_storage.copyChainFrom(other.m_storage);
    }

    /// @brief Move constructor when `InAllocator` is move-constructible.
    /// @details Transfers the slot buffer in O(1) when allocators compare equal (preserving all existing `NodeHandle`
    ///          values); otherwise moves elements into `m_allocator` and resets `other`.
    /// @param[in,out] other Source list left empty after the move.
    DoubleLinkedList(DoubleLinkedList&& other) noexcept requires std::is_move_constructible_v<InAllocator>
        : m_storage(std::move(other.m_storage.m_allocator))
    {
        if (m_storage.m_allocator == other.m_storage.m_allocator)
        {
            m_storage.stealFrom(other.m_storage);
        }
        else
        {
            m_storage.moveChainFrom(other.m_storage);
            other.reset();
        }
    }

    /// @brief Move constructor when `InAllocator` is non-movable (such as `InlineAllocator`).
    /// @param[in,out] other Source list left empty after the move.
    DoubleLinkedList(DoubleLinkedList&& other) noexcept
        requires(!std::is_move_constructible_v<InAllocator> && std::is_default_constructible_v<InAllocator>)
    {
        m_storage.moveChainFrom(other.m_storage);
        other.reset();
    }

    /// @brief Destroys all active elements and releases the slot buffer to `m_allocator`.
    ~DoubleLinkedList() noexcept
    {
        reset();
    }

public:
    /// @brief Copy assignment operator.
    /// @param[in] other Source list to copy from.
    /// @return Reference to `*this`.
    DoubleLinkedList& operator=(const DoubleLinkedList& other) noexcept
    {
        if (this != &other)
        {
            assign(other.begin(), other.end());
        }
        return *this;
    }

    /// @brief Move assignment operator.
    /// @details Steals the slot buffer in O(1) when allocators compare equal, preserving `other`'s active handles.
    /// @param[in,out] other Source list left empty after the move.
    /// @return Reference to `*this`.
    DoubleLinkedList& operator=(DoubleLinkedList&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        if (m_storage.m_allocator == other.m_storage.m_allocator)
        {
            reset();
            m_storage.stealFrom(other.m_storage);
            return *this;
        }

        clear();
        m_storage.moveChainFrom(other.m_storage);
        other.reset();
        return *this;
    }

    /// @brief Initializer-list assignment operator.
    /// @param[in] init Initializer list replacing current contents.
    /// @return Reference to `*this`.
    DoubleLinkedList& operator=(const std::initializer_list<InT> init) noexcept
    {
        assign(init);
        return *this;
    }

    /// @brief Replaces contents with `count` copies of `value`.
    /// @param[in] count Number of copies to insert.
    /// @param[in] value Fill value copied into each node.
    void assign(const SizeType count, const InT& value) noexcept
    {
        const InT copyValue(value);
        clear();
        reserve(count);
        for (SizeType i = 0u; i < count; ++i)
        {
            (void)emplaceBack(copyValue);
        }
    }

    /// @brief Replaces contents with elements from `[first, last)`.
    /// @tparam InInputIt Input iterator type yielding elements convertible to `InT`.
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
            (void)emplaceBack(*first);
        }
    }

    /// @brief Replaces contents with elements from an initializer list.
    /// @param[in] init Initializer list of elements.
    void assign(const std::initializer_list<InT> init) noexcept
    {
        assign(init.begin(), init.end());
    }

public:
    /// @brief Accesses the element referenced by `handle` with debug validation.
    /// @param[in] handle Live handle obtained from insertion or traversal.
    /// @return Mutable reference to the element.
    [[nodiscard]] GP_FORCEINLINE_HINT Reference operator[](const NodeHandle handle) noexcept
    {
        return get(handle);
    }

    /// @brief Accesses the element referenced by `handle` (`const` overload).
    /// @param[in] handle Live handle obtained from insertion or traversal.
    /// @return Const reference to the element.
    [[nodiscard]] GP_FORCEINLINE_HINT ConstReference operator[](const NodeHandle handle) const noexcept
    {
        return get(handle);
    }

    /// @brief Accesses the element referenced by `handle`, asserting handle liveness in debug builds.
    /// @param[in] handle Live node handle.
    /// @return Mutable reference to the node's payload.
    [[nodiscard]] GP_FORCEINLINE_HINT Reference get(const NodeHandle handle) noexcept
    {
        GP_ASSERT(isValidHandle(handle) && "DoubleLinkedList::get() called with an invalid or stale handle.");
        return *m_storage.slotValuePtr(handle.index);
    }

    /// @brief Accesses the element referenced by `handle` (`const` overload).
    /// @param[in] handle Live node handle.
    /// @return Const reference to the node's payload.
    [[nodiscard]] GP_FORCEINLINE_HINT ConstReference get(const NodeHandle handle) const noexcept
    {
        GP_ASSERT(isValidHandle(handle) && "DoubleLinkedList::get() called with an invalid or stale handle.");
        return *m_storage.slotValuePtr(handle.index);
    }

    /// @brief Returns a pointer to the payload of `handle`, or `nullptr` if invalid or stale.
    /// @param[in] handle Candidate node handle.
    /// @return Mutable pointer to the payload if live, otherwise `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT Pointer tryGet(const NodeHandle handle) noexcept
    {
        return isValidHandle(handle) ? m_storage.slotValuePtr(handle.index) : nullptr;
    }

    /// @brief Returns a const pointer to the payload of `handle`, or `nullptr` if invalid or stale.
    /// @param[in] handle Candidate node handle.
    /// @return Const pointer to the payload if live, otherwise `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT ConstPointer tryGet(const NodeHandle handle) const noexcept
    {
        return isValidHandle(handle) ? m_storage.slotValuePtr(handle.index) : nullptr;
    }

    /// @brief Checks whether `handle` currently addresses a live node in this list.
    /// @param[in] handle Candidate handle to test.
    /// @return True if live and generation-matched, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isValidHandle(const NodeHandle handle) const noexcept
    {
        return m_storage.isValidHandle(handle);
    }

    /// @brief Accesses the first element in the list.
    /// @return Mutable reference to the head payload.
    [[nodiscard]] GP_FORCEINLINE_HINT Reference front() noexcept
    {
        GP_ASSERT(m_storage.m_head != kInvalidListNodeIndex && "DoubleLinkedList::front() called on an empty list.");
        return *m_storage.slotValuePtr(m_storage.m_head);
    }

    /// @brief Accesses the first element in the list (`const` overload).
    /// @return Const reference to the head payload.
    [[nodiscard]] GP_FORCEINLINE_HINT ConstReference front() const noexcept
    {
        GP_ASSERT(m_storage.m_head != kInvalidListNodeIndex && "DoubleLinkedList::front() called on an empty list.");
        return *m_storage.slotValuePtr(m_storage.m_head);
    }

    /// @brief Accesses the last element in the list.
    /// @return Mutable reference to the tail payload.
    [[nodiscard]] GP_FORCEINLINE_HINT Reference back() noexcept
    {
        GP_ASSERT(m_storage.m_tail != kInvalidListNodeIndex && "DoubleLinkedList::back() called on an empty list.");
        return *m_storage.slotValuePtr(m_storage.m_tail);
    }

    /// @brief Accesses the last element in the list (`const` overload).
    /// @return Const reference to the tail payload.
    [[nodiscard]] GP_FORCEINLINE_HINT ConstReference back() const noexcept
    {
        GP_ASSERT(m_storage.m_tail != kInvalidListNodeIndex && "DoubleLinkedList::back() called on an empty list.");
        return *m_storage.slotValuePtr(m_storage.m_tail);
    }

    /// @brief Returns a pointer to the head element, or `nullptr` if empty.
    /// @return Pointer to the front element or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT Pointer tryFront() noexcept
    {
        return (m_storage.m_head != kInvalidListNodeIndex) ? m_storage.slotValuePtr(m_storage.m_head) : nullptr;
    }

    /// @brief Returns a const pointer to the head element, or `nullptr` if empty.
    /// @return Const pointer to the front element or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT ConstPointer tryFront() const noexcept
    {
        return (m_storage.m_head != kInvalidListNodeIndex) ? m_storage.slotValuePtr(m_storage.m_head) : nullptr;
    }

    /// @brief Returns a pointer to the tail element, or `nullptr` if empty.
    /// @return Pointer to the back element or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT Pointer tryBack() noexcept
    {
        return (m_storage.m_tail != kInvalidListNodeIndex) ? m_storage.slotValuePtr(m_storage.m_tail) : nullptr;
    }

    /// @brief Returns a const pointer to the tail element, or `nullptr` if empty.
    /// @return Const pointer to the back element or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT ConstPointer tryBack() const noexcept
    {
        return (m_storage.m_tail != kInvalidListNodeIndex) ? m_storage.slotValuePtr(m_storage.m_tail) : nullptr;
    }

    /// @brief Returns the `NodeHandle` of the first element, or `NodeHandle::invalid()` when empty.
    /// @return Handle to the head node.
    [[nodiscard]] GP_FORCEINLINE_HINT NodeHandle headHandle() const noexcept
    {
        return m_storage.template makeHandleForSlot<NodeHandle>(m_storage.m_head);
    }

    /// @brief Returns the `NodeHandle` of the last element, or `NodeHandle::invalid()` when empty.
    /// @return Handle to the tail node.
    [[nodiscard]] GP_FORCEINLINE_HINT NodeHandle tailHandle() const noexcept
    {
        return m_storage.template makeHandleForSlot<NodeHandle>(m_storage.m_tail);
    }

    /// @brief Returns the handle of the node following `handle`, or `NodeHandle::invalid()` if invalid or at tail.
    /// @param[in] handle Current live node handle.
    /// @return Handle to the successor node, or invalid handle.
    [[nodiscard]] GP_FORCEINLINE_HINT NodeHandle nextHandle(const NodeHandle handle) const noexcept
    {
        if (!isValidHandle(handle))
        {
            return NodeHandle::invalid();
        }
        return m_storage.template makeHandleForSlot<NodeHandle>(m_storage.m_slots[handle.index].next);
    }

    /// @brief Returns the handle of the node preceding `handle`, or `NodeHandle::invalid()` if invalid or at head.
    /// @param[in] handle Current live node handle.
    /// @return Handle to the predecessor node, or invalid handle.
    [[nodiscard]] GP_FORCEINLINE_HINT NodeHandle prevHandle(const NodeHandle handle) const noexcept
    {
        if (!isValidHandle(handle))
        {
            return NodeHandle::invalid();
        }
        return m_storage.template makeHandleForSlot<NodeHandle>(m_storage.m_slots[handle.index].prev);
    }

public:
    /// @brief Constructs a new node in place at the front of the list in O(1) time.
    /// @tparam InArgs Constructor argument types forwarded to `InT`.
    /// @param[in,out] args Constructor arguments.
    /// @return Generational handle to the newly inserted head node.
    template <typename... InArgs>
    requires std::is_constructible_v<InT, InArgs...> NodeHandle emplaceFront(InArgs&&... args) noexcept
    {
        const gp::UInt32 idx = m_storage.acquireSlot(std::forward<InArgs>(args)...);
        if (idx == kInvalidListNodeIndex) [[unlikely]]
        {
            return NodeHandle::invalid();
        }
        linkAtFront(idx);
        return m_storage.template makeHandleForSlot<NodeHandle>(idx);
    }

    /// @brief Copies `value` into a new head node in O(1) time.
    /// @param[in] value Element value to copy.
    /// @return Generational handle to the newly inserted head node.
    NodeHandle pushFront(const InT& value) noexcept
    {
        return emplaceFront(value);
    }

    /// @brief Moves `value` into a new head node in O(1) time.
    /// @param[in,out] value Element value to move from.
    /// @return Generational handle to the newly inserted head node.
    NodeHandle pushFront(InT&& value) noexcept
    {
        return emplaceFront(std::move(value));
    }

    /// @brief Constructs a new node in place at the back of the list in O(1) time.
    /// @tparam InArgs Constructor argument types forwarded to `InT`.
    /// @param[in,out] args Constructor arguments.
    /// @return Generational handle to the newly inserted tail node.
    template <typename... InArgs>
    requires std::is_constructible_v<InT, InArgs...> NodeHandle emplaceBack(InArgs&&... args) noexcept
    {
        const gp::UInt32 idx = m_storage.acquireSlot(std::forward<InArgs>(args)...);
        if (idx == kInvalidListNodeIndex) [[unlikely]]
        {
            return NodeHandle::invalid();
        }
        linkAtBack(idx);
        return m_storage.template makeHandleForSlot<NodeHandle>(idx);
    }

    /// @brief Copies `value` into a new tail node in O(1) time.
    /// @param[in] value Element value to copy.
    /// @return Generational handle to the newly inserted tail node.
    NodeHandle pushBack(const InT& value) noexcept
    {
        return emplaceBack(value);
    }

    /// @brief Moves `value` into a new tail node in O(1) time.
    /// @param[in,out] value Element value to move from.
    /// @return Generational handle to the newly inserted tail node.
    NodeHandle pushBack(InT&& value) noexcept
    {
        return emplaceBack(std::move(value));
    }

    /// @brief Constructs a new node immediately before `pos` in O(1) time.
    /// @tparam InArgs Constructor argument types forwarded to `InT`.
    /// @param[in] pos Valid target node handle before which the new node is linked.
    /// @param[in,out] args Constructor arguments.
    /// @return Handle to the new node, or `NodeHandle::invalid()` if `pos` is invalid or stale.
    template <typename... InArgs>
    requires std::is_constructible_v<InT, InArgs...>
    NodeHandle emplaceBefore(const NodeHandle pos, InArgs&&... args) noexcept
    {
        if (!isValidHandle(pos))
        {
            return NodeHandle::invalid();
        }
        const gp::UInt32 idx = m_storage.acquireSlot(std::forward<InArgs>(args)...);
        if (idx == kInvalidListNodeIndex) [[unlikely]]
        {
            return NodeHandle::invalid();
        }
        linkBefore(pos.index, idx);
        return m_storage.template makeHandleForSlot<NodeHandle>(idx);
    }

    /// @brief Inserts a copy of `value` immediately before `pos` in O(1) time.
    /// @param[in] pos Target live node handle.
    /// @param[in] value Element value to copy.
    /// @return Handle to the inserted node, or `NodeHandle::invalid()` if `pos` is invalid or stale.
    NodeHandle insertBefore(const NodeHandle pos, const InT& value) noexcept
    {
        return emplaceBefore(pos, value);
    }

    /// @brief Inserts `value` immediately before `pos` by move in O(1) time.
    /// @param[in] pos Target live node handle.
    /// @param[in,out] value Element value to move from.
    /// @return Handle to the inserted node, or `NodeHandle::invalid()` if `pos` is invalid or stale.
    NodeHandle insertBefore(const NodeHandle pos, InT&& value) noexcept
    {
        return emplaceBefore(pos, std::move(value));
    }

    /// @brief Constructs a new node immediately after `pos` in O(1) time.
    /// @tparam InArgs Constructor argument types forwarded to `InT`.
    /// @param[in] pos Valid target node handle after which the new node is linked.
    /// @param[in,out] args Constructor arguments.
    /// @return Handle to the new node, or `NodeHandle::invalid()` if `pos` is invalid or stale.
    template <typename... InArgs>
    requires std::is_constructible_v<InT, InArgs...>
    NodeHandle emplaceAfter(const NodeHandle pos, InArgs&&... args) noexcept
    {
        if (!isValidHandle(pos))
        {
            return NodeHandle::invalid();
        }
        const gp::UInt32 idx = m_storage.acquireSlot(std::forward<InArgs>(args)...);
        if (idx == kInvalidListNodeIndex) [[unlikely]]
        {
            return NodeHandle::invalid();
        }
        linkAfter(pos.index, idx);
        return m_storage.template makeHandleForSlot<NodeHandle>(idx);
    }

    /// @brief Inserts a copy of `value` immediately after `pos` in O(1) time.
    /// @param[in] pos Target live node handle.
    /// @param[in] value Element value to copy.
    /// @return Handle to the inserted node, or `NodeHandle::invalid()` if `pos` is invalid or stale.
    NodeHandle insertAfter(const NodeHandle pos, const InT& value) noexcept
    {
        return emplaceAfter(pos, value);
    }

    /// @brief Inserts `value` immediately after `pos` by move in O(1) time.
    /// @param[in] pos Target live node handle.
    /// @param[in,out] value Element value to move from.
    /// @return Handle to the inserted node, or `NodeHandle::invalid()` if `pos` is invalid or stale.
    NodeHandle insertAfter(const NodeHandle pos, InT&& value) noexcept
    {
        return emplaceAfter(pos, std::move(value));
    }

    /// @brief Inserts a new node before iterator `pos` (inserting at back when `pos == end()`).
    /// @tparam InArgs Constructor argument types forwarded to `InT`.
    /// @param[in] pos Iterator position in `[begin(), end()]`.
    /// @param[in,out] args Constructor arguments.
    /// @return Iterator pointing to the inserted node.
    template <typename... InArgs>
    requires std::is_constructible_v<InT, InArgs...>
    Iterator emplace(const ConstIterator pos, InArgs&&... args) noexcept
    {
        GP_ASSERT(pos.owner() == this && "DoubleLinkedList::emplace() iterator belongs to a different list.");
        if (pos.isEnd())
        {
            const NodeHandle inserted = emplaceBack(std::forward<InArgs>(args)...);
            return Iterator(this, inserted.index);
        }
        const NodeHandle target = pos.handle();
        GP_ASSERT(isValidHandle(target) && "DoubleLinkedList::emplace() called with a stale iterator.");
        const NodeHandle inserted = emplaceBefore(target, std::forward<InArgs>(args)...);
        return Iterator(this, inserted.index);
    }

    /// @brief Inserts a copy of `value` before iterator `pos`.
    /// @param[in] pos Iterator position in `[begin(), end()]`.
    /// @param[in] value Element value to copy.
    /// @return Iterator pointing to the inserted node.
    Iterator insert(const ConstIterator pos, const InT& value) noexcept
    {
        return emplace(pos, value);
    }

    /// @brief Inserts `value` before iterator `pos` by move.
    /// @param[in] pos Iterator position in `[begin(), end()]`.
    /// @param[in,out] value Element value to move from.
    /// @return Iterator pointing to the inserted node.
    Iterator insert(const ConstIterator pos, InT&& value) noexcept
    {
        return emplace(pos, std::move(value));
    }

    /// @brief Removes and destroys the head node in O(1) time.
    void popFront() noexcept
    {
        GP_ASSERT(m_storage.m_head != kInvalidListNodeIndex && "DoubleLinkedList::popFront() called on empty list.");
        const gp::UInt32 idx = m_storage.m_head;
        unlinkSlot(idx);
        m_storage.releaseSlot(idx);
    }

    /// @brief Removes and destroys the tail node in O(1) time.
    void popBack() noexcept
    {
        GP_ASSERT(m_storage.m_tail != kInvalidListNodeIndex && "DoubleLinkedList::popBack() called on empty list.");
        const gp::UInt32 idx = m_storage.m_tail;
        unlinkSlot(idx);
        m_storage.releaseSlot(idx);
    }

    /// @brief Moves the head element into `outValue` and removes the head node in O(1) time.
    /// @param[out] outValue Destination receiving the removed front value.
    /// @return True if an element was removed, false if the list was empty.
    [[nodiscard]] bool tryPopFront(InT& outValue) noexcept
    {
        if (m_storage.m_head == kInvalidListNodeIndex)
        {
            return false;
        }
        const gp::UInt32 idx = m_storage.m_head;
        outValue = std::move(*m_storage.slotValuePtr(idx));
        unlinkSlot(idx);
        m_storage.releaseSlot(idx);
        return true;
    }

    /// @brief Moves the tail element into `outValue` and removes the tail node in O(1) time.
    /// @param[out] outValue Destination receiving the removed back value.
    /// @return True if an element was removed, false if the list was empty.
    [[nodiscard]] bool tryPopBack(InT& outValue) noexcept
    {
        if (m_storage.m_tail == kInvalidListNodeIndex)
        {
            return false;
        }
        const gp::UInt32 idx = m_storage.m_tail;
        outValue = std::move(*m_storage.slotValuePtr(idx));
        unlinkSlot(idx);
        m_storage.releaseSlot(idx);
        return true;
    }

    /// @brief Removes the node referenced by `handle` in O(1) time, bumping its slot generation.
    /// @param[in] handle Handle of the node to remove.
    /// @return True if `handle` was valid and removed, false if invalid or stale.
    bool remove(const NodeHandle handle) noexcept
    {
        if (!isValidHandle(handle))
        {
            return false;
        }
        unlinkSlot(handle.index);
        m_storage.releaseSlot(handle.index);
        return true;
    }

    /// @brief Removes the node referenced by `pos` and returns an iterator to its successor.
    /// @param[in] pos Valid iterator in `[begin(), end())`.
    /// @return Iterator to the element that followed `pos`, or `end()`.
    Iterator erase(const ConstIterator pos) noexcept
    {
        GP_ASSERT(pos.owner() == this && "DoubleLinkedList::erase() iterator belongs to a different list.");
        const NodeHandle target = pos.handle();
        GP_ASSERT(isValidHandle(target) && "DoubleLinkedList::erase() called with invalid or end iterator.");
        const gp::UInt32 nextIdx = m_storage.m_slots[target.index].next;
        unlinkSlot(target.index);
        m_storage.releaseSlot(target.index);
        return Iterator(this, nextIdx);
    }

    /// @brief Relinks an existing node to the front of the list in O(1) time without moving `InT` or invalidating
    ///        handles.
    /// @param[in] handle Live handle of the node to promote.
    /// @return True if `handle` was valid, false if invalid or stale.
    bool moveToFront(const NodeHandle handle) noexcept
    {
        if (!isValidHandle(handle))
        {
            return false;
        }
        if (handle.index != m_storage.m_head)
        {
            unlinkSlot(handle.index);
            linkAtFront(handle.index);
        }
        return true;
    }

    /// @brief Relinks an existing node to the back of the list in O(1) time without moving `InT` or invalidating
    /// handles.
    /// @param[in] handle Live handle of the node to move to tail.
    /// @return True if `handle` was valid, false if invalid or stale.
    bool moveToBack(const NodeHandle handle) noexcept
    {
        if (!isValidHandle(handle))
        {
            return false;
        }
        if (handle.index != m_storage.m_tail)
        {
            unlinkSlot(handle.index);
            linkAtBack(handle.index);
        }
        return true;
    }

    /// @brief Removes every node whose value equals `value`.
    /// @param[in] value Value to match for removal.
    /// @return Number of nodes removed.
    SizeType removeAll(const InT& value) noexcept requires gp::concepts::IsEqualityComparable<InT>
    {
        if (m_storage.isPointerInStorage(std::addressof(value)))
        {
            if constexpr (std::is_copy_constructible_v<InT>)
            {
                const InT stagedTarget(value);
                return removeAll(stagedTarget);
            }
        }

        SizeType removedCount = 0u;
        gp::UInt32 deferredAliasedSlot = kInvalidListNodeIndex;
        gp::UInt32 cur = m_storage.m_head;
        while (cur != kInvalidListNodeIndex)
        {
            const gp::UInt32 nextIdx = m_storage.m_slots[cur].next;
            const InT* const curPtr = m_storage.slotValuePtr(cur);
            if (*curPtr == value)
            {
                unlinkSlot(cur);
                if (curPtr == std::addressof(value))
                {
                    deferredAliasedSlot = cur;
                }
                else
                {
                    m_storage.releaseSlot(cur);
                }
                ++removedCount;
            }
            cur = nextIdx;
        }
        if (deferredAliasedSlot != kInvalidListNodeIndex)
        {
            m_storage.releaseSlot(deferredAliasedSlot);
        }
        return removedCount;
    }

    /// @brief Removes every node matching unary predicate `predicate`.
    /// @tparam InPredicate Predicate type satisfying `gp::concepts::IsPredicate<InPredicate, const InT&>`.
    /// @param[in,out] predicate Callable returning `true` for nodes to erase.
    /// @return Number of nodes removed.
    template <typename InPredicate>
    requires gp::concepts::IsPredicate<InPredicate, const InT&> SizeType removeIf(InPredicate&& predicate) noexcept
    {
        SizeType removedCount = 0u;
        gp::UInt32 cur = m_storage.m_head;
        while (cur != kInvalidListNodeIndex)
        {
            const gp::UInt32 nextIdx = m_storage.m_slots[cur].next;
            if (predicate(*m_storage.slotValuePtr(cur)))
            {
                unlinkSlot(cur);
                m_storage.releaseSlot(cur);
                ++removedCount;
            }
            cur = nextIdx;
        }
        return removedCount;
    }

    /// @brief Reverses node order in place without moving `InT` values or invalidating `NodeHandle`s.
    void reverse() noexcept
    {
        gp::UInt32 cur = m_storage.m_head;
        while (cur != kInvalidListNodeIndex)
        {
            const gp::UInt32 oldNext = m_storage.m_slots[cur].next;
            m_storage.m_slots[cur].next = m_storage.m_slots[cur].prev;
            m_storage.m_slots[cur].prev = oldNext;
            cur = oldNext;
        }
        std::swap(m_storage.m_head, m_storage.m_tail);
    }

    /// @brief Sorts elements in ascending `<` order using stable O(N log N) merge sort while preserving all handles.
    void sort() noexcept requires requires(const InT& a, const InT& b) { a < b; }
    {
        const auto lessThan = [](const InT& lhs, const InT& rhs) noexcept -> bool
        {
            return lhs < rhs;
        };
        sort(lessThan);
    }

    /// @brief Sorts elements using `comparator` via stable O(N log N) merge sort while preserving all `NodeHandle`s.
    /// @tparam InComparator Strict-weak-ordering predicate `bool(const InT& lhs, const InT& rhs)`.
    /// @param[in,out] comparator Ordering callable.
    template <typename InComparator>
    requires gp::concepts::IsPredicate<InComparator, const InT&, const InT&>
    void sort(InComparator&& comparator) noexcept
    {
        if (m_storage.m_size < 2u)
        {
            return;
        }
        m_storage.sortNextChain(comparator);
        rebuildPrevLinks();
    }

public:
    /// @brief Returns the number of live elements currently in the list.
    /// @return Active element count.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType size() const noexcept
    {
        return static_cast<SizeType>(m_storage.m_size);
    }

    /// @brief Returns the allocated slot capacity of the internal pool.
    /// @return Total slot capacity.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType capacity() const noexcept
    {
        return static_cast<SizeType>(m_storage.m_capacity);
    }

    /// @brief Returns the maximum slot capacity addressable by 32-bit handle indices.
    /// @return Maximum node count (`kMaxListSlotCapacity`).
    [[nodiscard]] constexpr SizeType maxSize() const noexcept
    {
        return static_cast<SizeType>(kMaxListSlotCapacity);
    }

    /// @brief Checks whether the list contains zero live nodes.
    /// @return True if `size() == 0`, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isEmpty() const noexcept
    {
        return m_storage.m_size == 0u;
    }

    /// @brief Ensures the slot pool can store at least `minCapacity` nodes without reallocating.
    /// @param[in] minCapacity Minimum desired slot capacity.
    void reserve(const SizeType minCapacity) noexcept
    {
        m_storage.reserveSlots(minCapacity);
    }

    /// @brief Trims trailing uncarved capacity (or frees storage when empty) without invalidating active `NodeHandle`s.
    void shrinkToFit() noexcept
    {
        m_storage.shrinkStorage();
    }

    /// @brief Repacks all live nodes into contiguous slots `[0, size())` in logical order and shrinks capacity to
    ///        `size()`.
    /// @warning Invalidates all previously issued `NodeHandle` values and `Iterator`s because live nodes move slots.
    void compact() noexcept
    {
        (void)m_storage.compactStorage();
    }

    /// @brief Destroys all live elements while keeping slot capacity and advancing cleared slot generations.
    void clear() noexcept
    {
        m_storage.clearChain();
    }

    /// @brief Destroys all live elements and frees the slot buffer back to `m_allocator`.
    void reset() noexcept
    {
        m_storage.resetStorage();
    }

public:
    /// @brief Returns a mutable bidirectional iterator to the head element.
    /// @return Start iterator.
    [[nodiscard]] constexpr Iterator begin() noexcept
    {
        return Iterator(this, m_storage.m_head);
    }

    /// @brief Returns a const bidirectional iterator to the head element.
    /// @return Const start iterator.
    [[nodiscard]] constexpr ConstIterator begin() const noexcept
    {
        return ConstIterator(this, m_storage.m_head);
    }

    /// @brief Returns a mutable bidirectional end sentinel iterator.
    /// @return End iterator.
    [[nodiscard]] constexpr Iterator end() noexcept
    {
        return Iterator(this, kInvalidListNodeIndex);
    }

    /// @brief Returns a const bidirectional end sentinel iterator.
    /// @return Const end iterator.
    [[nodiscard]] constexpr ConstIterator end() const noexcept
    {
        return ConstIterator(this, kInvalidListNodeIndex);
    }

    /// @brief Returns a const bidirectional iterator to the head element.
    /// @return Const start iterator.
    [[nodiscard]] constexpr ConstIterator cbegin() const noexcept
    {
        return begin();
    }

    /// @brief Returns a const bidirectional end sentinel iterator.
    /// @return Const end iterator.
    [[nodiscard]] constexpr ConstIterator cend() const noexcept
    {
        return end();
    }

    /// @brief Returns a mutable reverse iterator starting at the tail element.
    /// @return Reverse start iterator.
    [[nodiscard]] constexpr ReverseIterator rbegin() noexcept
    {
        return ReverseIterator(end());
    }

    /// @brief Returns a const reverse iterator starting at the tail element.
    /// @return Const reverse start iterator.
    [[nodiscard]] constexpr ConstReverseIterator rbegin() const noexcept
    {
        return ConstReverseIterator(end());
    }

    /// @brief Returns a mutable reverse iterator one before the head element.
    /// @return Reverse end iterator.
    [[nodiscard]] constexpr ReverseIterator rend() noexcept
    {
        return ReverseIterator(begin());
    }

    /// @brief Returns a const reverse iterator one before the head element.
    /// @return Const reverse end iterator.
    [[nodiscard]] constexpr ConstReverseIterator rend() const noexcept
    {
        return ConstReverseIterator(begin());
    }

    /// @brief Returns a const reverse iterator starting at the tail element.
    /// @return Const reverse start iterator.
    [[nodiscard]] constexpr ConstReverseIterator crbegin() const noexcept
    {
        return ConstReverseIterator(end());
    }

    /// @brief Returns a const reverse iterator one before the head element.
    /// @return Const reverse end iterator.
    [[nodiscard]] constexpr ConstReverseIterator crend() const noexcept
    {
        return ConstReverseIterator(begin());
    }

public:
    /// @brief Finds the handle of the first node whose payload equals `value`.
    /// @param[in] value Target value to match.
    /// @return Live `NodeHandle` on match, or `NodeHandle::invalid()` if absent.
    [[nodiscard]] NodeHandle findHandle(const InT& value) const noexcept
        requires gp::concepts::IsEqualityComparable<InT>
    {
        return m_storage.template makeHandleForSlot<NodeHandle>(m_storage.findSlotByValue(value));
    }

    /// @brief Finds the handle of the first node satisfying `predicate`.
    /// @tparam InPredicate Unary predicate type accepting `const InT&`.
    /// @param[in,out] predicate Predicate tested against each element in head-to-tail order.
    /// @return Live `NodeHandle` of the first match, or `NodeHandle::invalid()` if none match.
    template <typename InPredicate>
    requires gp::concepts::IsPredicate<InPredicate, const InT&>
    [[nodiscard]] NodeHandle findHandleIf(InPredicate&& predicate) const noexcept
    {
        return m_storage.template makeHandleForSlot<NodeHandle>(m_storage.findSlotByPredicate(predicate));
    }

    /// @brief Finds an iterator to the first node whose payload equals `value`.
    /// @param[in] value Target value to match.
    /// @return Mutable iterator to the matching node, or `end()` if absent.
    [[nodiscard]] Iterator find(const InT& value) noexcept requires gp::concepts::IsEqualityComparable<InT>
    {
        return Iterator(this, m_storage.findSlotByValue(value));
    }

    /// @brief Finds a const iterator to the first node whose payload equals `value`.
    /// @param[in] value Target value to match.
    /// @return Const iterator to the matching node, or `end()` if absent.
    [[nodiscard]] ConstIterator find(const InT& value) const noexcept requires gp::concepts::IsEqualityComparable<InT>
    {
        return ConstIterator(this, m_storage.findSlotByValue(value));
    }

    /// @brief Finds a mutable iterator to the first node satisfying `predicate`.
    /// @tparam InPredicate Unary predicate type accepting `const InT&`.
    /// @param[in,out] predicate Predicate tested against each element in head-to-tail order.
    /// @return Mutable iterator to the first matching node, or `end()` if none match.
    template <typename InPredicate>
    requires gp::concepts::IsPredicate<InPredicate, const InT&>
    [[nodiscard]] Iterator findIf(InPredicate&& predicate) noexcept
    {
        return Iterator(this, m_storage.findSlotByPredicate(predicate));
    }

    /// @brief Finds a const iterator to the first node satisfying `predicate`.
    /// @tparam InPredicate Unary predicate type accepting `const InT&`.
    /// @param[in,out] predicate Predicate tested against each element in head-to-tail order.
    /// @return Const iterator to the first matching node, or `end()` if none match.
    template <typename InPredicate>
    requires gp::concepts::IsPredicate<InPredicate, const InT&>
    [[nodiscard]] ConstIterator findIf(InPredicate&& predicate) const noexcept
    {
        return ConstIterator(this, m_storage.findSlotByPredicate(predicate));
    }

    /// @brief Checks whether any node in the list compares equal to `value`.
    /// @param[in] value Target value to match.
    /// @return True if present, false otherwise.
    [[nodiscard]] bool contains(const InT& value) const noexcept requires gp::concepts::IsEqualityComparable<InT>
    {
        return m_storage.findSlotByValue(value) != kInvalidListNodeIndex;
    }

    /// @brief Counts nodes whose payloads compare equal to `value`.
    /// @param[in] value Target value to match.
    /// @return Number of matching nodes.
    [[nodiscard]] SizeType count(const InT& value) const noexcept requires gp::concepts::IsEqualityComparable<InT>
    {
        return m_storage.countMatching(value);
    }

    /// @brief Streams all elements in head-to-tail order plus trailing count framing into `hasher`.
    /// @tparam InHasher Concrete streaming hasher type (`gp::hashing::hash()` / `hashOf()`).
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        m_storage.hashAppendChain(hasher);
    }

    /// @brief Returns a mutable reference to the bound allocator instance.
    /// @return Reference to `m_allocator`.
    [[nodiscard]] constexpr InAllocator& getAllocator() noexcept
    {
        return m_storage.m_allocator;
    }

    /// @brief Returns a const reference to the bound allocator instance.
    /// @return Const reference to `m_allocator`.
    [[nodiscard]] constexpr const InAllocator& getAllocator() const noexcept
    {
        return m_storage.m_allocator;
    }

    /// @brief Exchanges contents with `other` in O(1) when allocators match, preserving handles on both lists.
    /// @param[in,out] other Target list to swap with.
    void swap(DoubleLinkedList& other) noexcept
    {
        if (this == &other)
        {
            return;
        }

        if (m_storage.m_allocator == other.m_storage.m_allocator)
        {
            m_storage.swapStorageMembers(other.m_storage);
            return;
        }

        DoubleLinkedList temp(std::move(*this));
        *this = std::move(other);
        other = std::move(temp);
    }

public:
    /// @brief Compares two lists in logical head-to-tail order for equality regardless of internal slot layout.
    /// @tparam InOtherAllocator Right-hand list's allocator type.
    /// @param[in] other List to compare against.
    /// @return True if sizes and corresponding elements compare equal, false otherwise.
    template <gp::concepts::IsAllocator InOtherAllocator>
    requires gp::concepts::IsEqualityComparable<InT>
    [[nodiscard]] bool operator==(const DoubleLinkedList<InT, InOtherAllocator>& other) const noexcept
    {
        return m_storage.equalChain(other.m_storage);
    }

    /// @brief Performs lexicographical three-way comparison against another list.
    /// @tparam InOtherAllocator Right-hand list's allocator type.
    /// @param[in] other List to compare against.
    /// @return Three-way comparison ordering result matching `InT`.
    template <gp::concepts::IsAllocator InOtherAllocator, typename InElemT = InT>
    requires gp::concepts::IsThreeWayComparable<InElemT> [[nodiscard]] std::compare_three_way_result_t<InElemT>
        operator<=>(const DoubleLinkedList<InT, InOtherAllocator>& other) const noexcept
    {
        return m_storage.compareChain(other.m_storage);
    }

private:
    /// @brief Links `slotIndex` at the head of the active chain.
    GP_FORCEINLINE_HINT void linkAtFront(const gp::UInt32 slotIndex) noexcept
    {
        m_storage.m_slots[slotIndex].prev = kInvalidListNodeIndex;
        m_storage.m_slots[slotIndex].next = m_storage.m_head;
        if (m_storage.m_head != kInvalidListNodeIndex)
        {
            m_storage.m_slots[m_storage.m_head].prev = slotIndex;
        }
        else
        {
            m_storage.m_tail = slotIndex;
        }
        m_storage.m_head = slotIndex;
    }

    /// @brief Links `slotIndex` at the tail of the active chain.
    GP_FORCEINLINE_HINT void linkAtBack(const gp::UInt32 slotIndex) noexcept
    {
        m_storage.m_slots[slotIndex].next = kInvalidListNodeIndex;
        m_storage.m_slots[slotIndex].prev = m_storage.m_tail;
        if (m_storage.m_tail != kInvalidListNodeIndex)
        {
            m_storage.m_slots[m_storage.m_tail].next = slotIndex;
        }
        else
        {
            m_storage.m_head = slotIndex;
        }
        m_storage.m_tail = slotIndex;
    }

    /// @brief Links `newSlotIndex` immediately before occupied `targetSlotIndex`.
    GP_FORCEINLINE_HINT void linkBefore(const gp::UInt32 targetSlotIndex, const gp::UInt32 newSlotIndex) noexcept
    {
        const gp::UInt32 prevIdx = m_storage.m_slots[targetSlotIndex].prev;
        m_storage.m_slots[newSlotIndex].prev = prevIdx;
        m_storage.m_slots[newSlotIndex].next = targetSlotIndex;
        m_storage.m_slots[targetSlotIndex].prev = newSlotIndex;

        if (prevIdx != kInvalidListNodeIndex)
        {
            m_storage.m_slots[prevIdx].next = newSlotIndex;
        }
        else
        {
            m_storage.m_head = newSlotIndex;
        }
    }

    /// @brief Links `newSlotIndex` immediately after occupied `targetSlotIndex`.
    GP_FORCEINLINE_HINT void linkAfter(const gp::UInt32 targetSlotIndex, const gp::UInt32 newSlotIndex) noexcept
    {
        const gp::UInt32 nextIdx = m_storage.m_slots[targetSlotIndex].next;
        m_storage.m_slots[newSlotIndex].next = nextIdx;
        m_storage.m_slots[newSlotIndex].prev = targetSlotIndex;
        m_storage.m_slots[targetSlotIndex].next = newSlotIndex;

        if (nextIdx != kInvalidListNodeIndex)
        {
            m_storage.m_slots[nextIdx].prev = newSlotIndex;
        }
        else
        {
            m_storage.m_tail = newSlotIndex;
        }
    }

    /// @brief Detaches `slotIndex` from its neighbors without destroying its payload or bumping generation.
    GP_FORCEINLINE_HINT void unlinkSlot(const gp::UInt32 slotIndex) noexcept
    {
        const gp::UInt32 prevIdx = m_storage.m_slots[slotIndex].prev;
        const gp::UInt32 nextIdx = m_storage.m_slots[slotIndex].next;

        if (prevIdx != kInvalidListNodeIndex)
        {
            m_storage.m_slots[prevIdx].next = nextIdx;
        }
        else
        {
            m_storage.m_head = nextIdx;
        }

        if (nextIdx != kInvalidListNodeIndex)
        {
            m_storage.m_slots[nextIdx].prev = prevIdx;
        }
        else
        {
            m_storage.m_tail = prevIdx;
        }

        m_storage.m_slots[slotIndex].prev = kInvalidListNodeIndex;
        m_storage.m_slots[slotIndex].next = kInvalidListNodeIndex;
    }

    /// @brief Repairs `prev` links and `m_tail` after `sortNextChain()` reorders `next` indices.
    void rebuildPrevLinks() noexcept
    {
        gp::UInt32 prevIdx = kInvalidListNodeIndex;
        gp::UInt32 curIdx = m_storage.m_head;
        while (curIdx != kInvalidListNodeIndex)
        {
            m_storage.m_slots[curIdx].prev = prevIdx;
            prevIdx = curIdx;
            curIdx = m_storage.m_slots[curIdx].next;
        }
        m_storage.m_tail = prevIdx;
    }

private:
    StorageType m_storage{};
};

/// @brief Swaps two `DoubleLinkedList` instances via `lhs.swap(rhs)`.
/// @tparam InT Element type.
/// @tparam InAllocator Allocator type.
/// @param[in,out] lhs First list.
/// @param[in,out] rhs Second list.
template <typename InT, gp::concepts::IsAllocator InAllocator>
inline void swap(DoubleLinkedList<InT, InAllocator>& lhs, DoubleLinkedList<InT, InAllocator>& rhs) noexcept
{
    lhs.swap(rhs);
}

static_assert(
    sizeof(DoubleLinkedList<gp::Int32>) == 32u,
    "gp::DoubleLinkedList with stateless DefaultAllocator must occupy exactly 32 bytes."
);
static_assert(
    std::bidirectional_iterator<DoubleLinkedList<gp::Int32>::Iterator>,
    "gp::DoubleLinkedList::Iterator must satisfy std::bidirectional_iterator."
);
static_assert(
    std::bidirectional_iterator<DoubleLinkedList<gp::Int32>::ConstIterator>,
    "gp::DoubleLinkedList::ConstIterator must satisfy std::bidirectional_iterator."
);

}   // namespace gp

namespace gp::concepts
{

namespace detail
{

template <typename InType>
constexpr bool IsDoubleLinkedListV = false;

template <typename InT, typename InAllocator>
constexpr bool IsDoubleLinkedListV<gp::DoubleLinkedList<InT, InAllocator>> = true;

}   // namespace detail

/// @brief Concept satisfied by any instantiation of `gp::DoubleLinkedList`.
/// @tparam InType Candidate type to test.
template <typename InType>
concept IsDoubleLinkedList = detail::IsDoubleLinkedListV<std::remove_cvref_t<InType>>;

/// @brief Specialization enabling bitwise relocation of `gp::DoubleLinkedList` when its allocator is bitwise
///        relocatable.
/// @tparam InT Element type.
/// @tparam InAllocator Allocator type.
template <typename InT, typename InAllocator>
struct IsRelocatableTrait<gp::DoubleLinkedList<InT, InAllocator>>
    : std::bool_constant<std::is_move_constructible_v<InAllocator> && IsRelocatable<InAllocator>>
{};

}   // namespace gp::concepts
