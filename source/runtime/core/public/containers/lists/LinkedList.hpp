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

/// @brief Generational handle-addressed singly-linked list backed by a contiguous slot pool.
/// @details Omits per-node backward (`prev`) links compared to `DoubleLinkedList`, cutting each slot's link metadata
///          from 12 bytes to 8 bytes (for example, packing four 8-byte handles into one 64-byte cache line) while
///          preserving O(1) `pushFront`, `popFront`, `pushBack`, `insertAfter`, `removeAfter`, and generational
///          `NodeHandle` safety across slot-pool growth.
/// @note Like `DoubleLinkedList`, slot-pool growth may relocate elements in memory, invalidating raw `T*` pointers
///       while keeping `NodeHandle` values and `Iterator` positions valid.
/// @tparam InT Element type stored in the list (cannot be const or a reference).
/// @tparam InAllocator Engine memory allocator satisfying `gp::concepts::IsAllocator`.
template <typename InT, typename InAllocator>
class LinkedList
{
    static_assert(!gp::concepts::IsConst<InT>, "LinkedList element type cannot be const.");
    static_assert(!gp::concepts::IsReference<InT>, "LinkedList element type cannot be a reference.");
    static_assert(gp::concepts::IsAllocator<InAllocator>, "LinkedList requires InAllocator to satisfy IsAllocator.");

private:
    struct Slot
    {
        alignas(InT) gp::Byte storage[sizeof(InT)];
        gp::UInt32 next{ kInvalidListNodeIndex };
        gp::UInt32 generation{ 0u };
    };

    using StorageType = container::detail::SlotPoolStorage<InT, Slot, InAllocator>;

    template <typename InOtherT, typename InOtherAllocator>
    friend class LinkedList;

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
    using NodeHandle = LinkedListNodeHandle<InT>;
    using Iterator = container::detail::SlotListIterator<LinkedList, ValueType, NodeHandle, false>;
    using ConstIterator = container::detail::SlotListIterator<const LinkedList, const ValueType, NodeHandle, false>;

    /// @brief Byte alignment requested from `InAllocator` for the internal slot array.
    static constexpr gp::UInt32 kAllocAlignment = StorageType::kAllocAlignment;

public:
    /// @brief Constructs an empty singly-linked list with zero allocations.
    constexpr LinkedList() noexcept = default;

    /// @brief Constructs an empty list bound to a copy of `allocator`.
    /// @param[in] allocator Allocator instance to copy.
    constexpr explicit LinkedList(const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_storage(allocator)
    {}

    /// @brief Constructs an empty list by moving `allocator`.
    /// @param[in,out] allocator Allocator instance to move from.
    constexpr explicit LinkedList(InAllocator&& allocator) noexcept requires std::is_move_constructible_v<InAllocator>
        : m_storage(std::move(allocator))
    {}

    /// @brief Constructs a list containing `count` default-constructed elements.
    /// @param[in] count Initial number of elements.
    explicit LinkedList(const SizeType count) noexcept
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
    LinkedList(const SizeType count, const InT& value) noexcept
    {
        assign(count, value);
    }

    /// @brief Constructs a list containing `count` copies of `value` using `allocator`.
    /// @param[in] count Number of elements to insert.
    /// @param[in] value Fill value copied into each node.
    /// @param[in] allocator Allocator instance to copy.
    LinkedList(const SizeType count, const InT& value, const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_storage(allocator)
    {
        assign(count, value);
    }

    /// @brief Constructs a list from an iterator range `[first, last)`.
    /// @tparam InInputIt Input iterator yielding elements convertible to `InT`.
    /// @param[in] first Start of source range.
    /// @param[in] last End of source range.
    template <typename InInputIt>
    requires(!gp::concepts::IsIntegral<InInputIt>) LinkedList(InInputIt first, InInputIt last) noexcept
    {
        assign(first, last);
    }

    /// @brief Constructs a list from an iterator range `[first, last)` using `allocator`.
    /// @tparam InInputIt Input iterator yielding elements convertible to `InT`.
    /// @param[in] first Start of source range.
    /// @param[in] last End of source range.
    /// @param[in] allocator Allocator instance to copy.
    template <typename InInputIt>
    requires(!gp::concepts::IsIntegral<InInputIt> && std::is_copy_constructible_v<InAllocator>)
    LinkedList(InInputIt first, InInputIt last, const InAllocator& allocator) noexcept
        : m_storage(allocator)
    {
        assign(first, last);
    }

    /// @brief Constructs a list from an initializer list.
    /// @param[in] init Initializer list of elements appended in order.
    LinkedList(const std::initializer_list<InT> init) noexcept
    {
        assign(init);
    }

    /// @brief Constructs a list from an initializer list using `allocator`.
    /// @param[in] init Initializer list of elements appended in order.
    /// @param[in] allocator Allocator instance to copy.
    LinkedList(const std::initializer_list<InT> init, const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_storage(allocator)
    {
        assign(init);
    }

    /// @brief Copy constructor when `InAllocator` is copy-constructible.
    /// @param[in] other Source list to copy from.
    LinkedList(const LinkedList& other) noexcept requires std::is_copy_constructible_v<InAllocator>
        : m_storage(other.m_storage.m_allocator)
    {
        m_storage.copyChainFrom(other.m_storage);
    }

    /// @brief Copy constructor when `InAllocator` is non-copyable (such as `InlineAllocator`).
    /// @param[in] other Source list to copy into a default-constructed allocator.
    LinkedList(const LinkedList& other) noexcept
        requires(!std::is_copy_constructible_v<InAllocator> && std::is_default_constructible_v<InAllocator>)
    {
        m_storage.copyChainFrom(other.m_storage);
    }

    /// @brief Cross-allocator copy constructor from any `LinkedList` sharing element type `InT`.
    /// @tparam InOtherAllocator Source list's allocator type.
    /// @param[in] other Source list to copy from.
    template <gp::concepts::IsAllocator InOtherAllocator>
    requires(!gp::concepts::IsSameAs<InAllocator, InOtherAllocator>)
    explicit LinkedList(const LinkedList<InT, InOtherAllocator>& other) noexcept
    {
        m_storage.copyChainFrom(other.m_storage);
    }

    /// @brief Move constructor when `InAllocator` is move-constructible.
    /// @param[in,out] other Source list left empty after the move.
    LinkedList(LinkedList&& other) noexcept requires std::is_move_constructible_v<InAllocator>
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
    LinkedList(LinkedList&& other) noexcept
        requires(!std::is_move_constructible_v<InAllocator> && std::is_default_constructible_v<InAllocator>)
    {
        m_storage.moveChainFrom(other.m_storage);
        other.reset();
    }

    /// @brief Destroys all live elements and releases the slot buffer to `m_allocator`.
    ~LinkedList() noexcept
    {
        reset();
    }

public:
    /// @brief Copy assignment operator.
    /// @param[in] other Source list to copy from.
    /// @return Reference to `*this`.
    LinkedList& operator=(const LinkedList& other) noexcept
    {
        if (this != &other)
        {
            assign(other.begin(), other.end());
        }
        return *this;
    }

    /// @brief Move assignment operator.
    /// @param[in,out] other Source list left empty after the move.
    /// @return Reference to `*this`.
    LinkedList& operator=(LinkedList&& other) noexcept
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
    LinkedList& operator=(const std::initializer_list<InT> init) noexcept
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
        GP_ASSERT(isValidHandle(handle) && "LinkedList::get() called with an invalid or stale handle.");
        return *m_storage.slotValuePtr(handle.index);
    }

    /// @brief Accesses the element referenced by `handle` (`const` overload).
    /// @param[in] handle Live node handle.
    /// @return Const reference to the node's payload.
    [[nodiscard]] GP_FORCEINLINE_HINT ConstReference get(const NodeHandle handle) const noexcept
    {
        GP_ASSERT(isValidHandle(handle) && "LinkedList::get() called with an invalid or stale handle.");
        return *m_storage.slotValuePtr(handle.index);
    }

    /// @brief Returns a pointer to the payload of `handle`, or `nullptr` if invalid or stale.
    /// @param[in] handle Candidate node handle.
    /// @return Mutable pointer if live, otherwise `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT Pointer tryGet(const NodeHandle handle) noexcept
    {
        return isValidHandle(handle) ? m_storage.slotValuePtr(handle.index) : nullptr;
    }

    /// @brief Returns a const pointer to the payload of `handle`, or `nullptr` if invalid or stale.
    /// @param[in] handle Candidate node handle.
    /// @return Const pointer if live, otherwise `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT ConstPointer tryGet(const NodeHandle handle) const noexcept
    {
        return isValidHandle(handle) ? m_storage.slotValuePtr(handle.index) : nullptr;
    }

    /// @brief Checks whether `handle` currently addresses a live node in this list.
    /// @param[in] handle Candidate handle to inspect.
    /// @return True if live and generation-matched, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isValidHandle(const NodeHandle handle) const noexcept
    {
        return m_storage.isValidHandle(handle);
    }

    /// @brief Accesses the first element in the list.
    /// @return Mutable reference to the head payload.
    [[nodiscard]] GP_FORCEINLINE_HINT Reference front() noexcept
    {
        GP_ASSERT(m_storage.m_head != kInvalidListNodeIndex && "LinkedList::front() called on an empty list.");
        return *m_storage.slotValuePtr(m_storage.m_head);
    }

    /// @brief Accesses the first element in the list (`const` overload).
    /// @return Const reference to the head payload.
    [[nodiscard]] GP_FORCEINLINE_HINT ConstReference front() const noexcept
    {
        GP_ASSERT(m_storage.m_head != kInvalidListNodeIndex && "LinkedList::front() called on an empty list.");
        return *m_storage.slotValuePtr(m_storage.m_head);
    }

    /// @brief Accesses the last element in the list in O(1) time via the maintained tail index.
    /// @return Mutable reference to the tail payload.
    [[nodiscard]] GP_FORCEINLINE_HINT Reference back() noexcept
    {
        GP_ASSERT(m_storage.m_tail != kInvalidListNodeIndex && "LinkedList::back() called on an empty list.");
        return *m_storage.slotValuePtr(m_storage.m_tail);
    }

    /// @brief Accesses the last element in the list (`const` overload).
    /// @return Const reference to the tail payload.
    [[nodiscard]] GP_FORCEINLINE_HINT ConstReference back() const noexcept
    {
        GP_ASSERT(m_storage.m_tail != kInvalidListNodeIndex && "LinkedList::back() called on an empty list.");
        return *m_storage.slotValuePtr(m_storage.m_tail);
    }

    /// @brief Returns a pointer to the head element, or `nullptr` when empty.
    /// @return Pointer to the front element or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT Pointer tryFront() noexcept
    {
        return (m_storage.m_head != kInvalidListNodeIndex) ? m_storage.slotValuePtr(m_storage.m_head) : nullptr;
    }

    /// @brief Returns a const pointer to the head element, or `nullptr` when empty.
    /// @return Const pointer to the front element or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT ConstPointer tryFront() const noexcept
    {
        return (m_storage.m_head != kInvalidListNodeIndex) ? m_storage.slotValuePtr(m_storage.m_head) : nullptr;
    }

    /// @brief Returns a pointer to the tail element, or `nullptr` when empty.
    /// @return Pointer to the back element or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT Pointer tryBack() noexcept
    {
        return (m_storage.m_tail != kInvalidListNodeIndex) ? m_storage.slotValuePtr(m_storage.m_tail) : nullptr;
    }

    /// @brief Returns a const pointer to the tail element, or `nullptr` when empty.
    /// @return Const pointer to the back element or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT ConstPointer tryBack() const noexcept
    {
        return (m_storage.m_tail != kInvalidListNodeIndex) ? m_storage.slotValuePtr(m_storage.m_tail) : nullptr;
    }

    /// @brief Returns the `NodeHandle` of the first element, or `NodeHandle::invalid()` when empty.
    /// @return Head node handle.
    [[nodiscard]] GP_FORCEINLINE_HINT NodeHandle headHandle() const noexcept
    {
        return m_storage.template makeHandleForSlot<NodeHandle>(m_storage.m_head);
    }

    /// @brief Returns the `NodeHandle` of the last element, or `NodeHandle::invalid()` when empty.
    /// @return Tail node handle.
    [[nodiscard]] GP_FORCEINLINE_HINT NodeHandle tailHandle() const noexcept
    {
        return m_storage.template makeHandleForSlot<NodeHandle>(m_storage.m_tail);
    }

    /// @brief Returns the handle of the node following `handle`, or `NodeHandle::invalid()` if invalid or at tail.
    /// @param[in] handle Current live node handle.
    /// @return Successor handle or invalid handle.
    [[nodiscard]] GP_FORCEINLINE_HINT NodeHandle nextHandle(const NodeHandle handle) const noexcept
    {
        if (!isValidHandle(handle))
        {
            return NodeHandle::invalid();
        }
        return m_storage.template makeHandleForSlot<NodeHandle>(m_storage.m_slots[handle.index].next);
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

    /// @brief Constructs a new node immediately after `pos` in O(1) time.
    /// @tparam InArgs Constructor argument types forwarded to `InT`.
    /// @param[in] pos Valid node handle after which the new node is linked.
    /// @param[in,out] args Constructor arguments.
    /// @return Handle to the inserted node, or `NodeHandle::invalid()` if `pos` is invalid or stale.
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

    /// @brief Inserts a new node immediately after iterator `pos`.
    /// @tparam InArgs Constructor argument types forwarded to `InT`.
    /// @param[in] pos Valid element iterator in `[begin(), end())`.
    /// @param[in,out] args Constructor arguments.
    /// @return Iterator pointing to the inserted node.
    template <typename... InArgs>
    requires std::is_constructible_v<InT, InArgs...>
    Iterator emplaceAfter(const ConstIterator pos, InArgs&&... args) noexcept
    {
        GP_ASSERT(pos.owner() == this && "LinkedList::emplaceAfter() iterator belongs to a different list.");
        const NodeHandle target = pos.handle();
        GP_ASSERT(isValidHandle(target) && "LinkedList::emplaceAfter() called with invalid or end iterator.");
        const NodeHandle inserted = emplaceAfter(target, std::forward<InArgs>(args)...);
        return Iterator(this, inserted.index);
    }

    /// @brief Inserts a copy of `value` immediately after iterator `pos`.
    /// @param[in] pos Valid element iterator in `[begin(), end())`.
    /// @param[in] value Element value to copy.
    /// @return Iterator pointing to the inserted node.
    Iterator insertAfter(const ConstIterator pos, const InT& value) noexcept
    {
        return emplaceAfter(pos, value);
    }

    /// @brief Inserts `value` immediately after iterator `pos` by move.
    /// @param[in] pos Valid element iterator in `[begin(), end())`.
    /// @param[in,out] value Element value to move from.
    /// @return Iterator pointing to the inserted node.
    Iterator insertAfter(const ConstIterator pos, InT&& value) noexcept
    {
        return emplaceAfter(pos, std::move(value));
    }

    /// @brief Removes and destroys the head node in O(1) time.
    void popFront() noexcept
    {
        GP_ASSERT(m_storage.m_head != kInvalidListNodeIndex && "LinkedList::popFront() called on empty list.");
        const gp::UInt32 oldHead = m_storage.m_head;
        m_storage.m_head = m_storage.m_slots[oldHead].next;
        if (m_storage.m_head == kInvalidListNodeIndex)
        {
            m_storage.m_tail = kInvalidListNodeIndex;
        }
        m_storage.releaseSlot(oldHead);
    }

    /// @brief Moves the head element into `outValue` and removes the head node in O(1) time.
    /// @param[out] outValue Destination receiving the removed front value.
    /// @return True if an element was popped, false if empty.
    [[nodiscard]] bool tryPopFront(InT& outValue) noexcept
    {
        if (m_storage.m_head == kInvalidListNodeIndex)
        {
            return false;
        }
        outValue = std::move(*m_storage.slotValuePtr(m_storage.m_head));
        popFront();
        return true;
    }

    /// @brief Removes the node immediately following `prevHandle` in O(1) time.
    /// @param[in] prevHandle Valid handle to the predecessor node.
    /// @return True if a successor node existed and was removed, false otherwise.
    bool removeAfter(const NodeHandle prevHandle) noexcept
    {
        if (!isValidHandle(prevHandle))
        {
            return false;
        }
        const gp::UInt32 targetIdx = m_storage.m_slots[prevHandle.index].next;
        if (targetIdx == kInvalidListNodeIndex)
        {
            return false;
        }

        m_storage.m_slots[prevHandle.index].next = m_storage.m_slots[targetIdx].next;
        if (m_storage.m_tail == targetIdx)
        {
            m_storage.m_tail = prevHandle.index;
        }
        m_storage.releaseSlot(targetIdx);
        return true;
    }

    /// @brief Removes the node immediately following iterator `pos` in O(1) time.
    /// @param[in] pos Valid iterator whose successor should be removed.
    /// @return Iterator to the new successor after `pos`, or `end()`.
    Iterator eraseAfter(const ConstIterator pos) noexcept
    {
        GP_ASSERT(pos.owner() == this && "LinkedList::eraseAfter() iterator belongs to a different list.");
        const NodeHandle prevHandle = pos.handle();
        GP_ASSERT(isValidHandle(prevHandle) && "LinkedList::eraseAfter() called with invalid or end iterator.");
        (void)removeAfter(prevHandle);
        return Iterator(this, m_storage.m_slots[prevHandle.index].next);
    }

    /// @brief Removes the node referenced by `handle` (O(1) for the head, O(N) predecessor scan otherwise).
    /// @param[in] handle Handle of the node to remove.
    /// @return True if `handle` was live and removed, false if invalid or stale.
    bool remove(const NodeHandle handle) noexcept
    {
        if (!isValidHandle(handle))
        {
            return false;
        }
        const gp::UInt32 idx = handle.index;
        if (idx == m_storage.m_head)
        {
            popFront();
            return true;
        }

        gp::UInt32 prev = m_storage.m_head;
        while (prev != kInvalidListNodeIndex && m_storage.m_slots[prev].next != idx)
        {
            prev = m_storage.m_slots[prev].next;
        }
        if (prev == kInvalidListNodeIndex)
        {
            return false;
        }

        m_storage.m_slots[prev].next = m_storage.m_slots[idx].next;
        if (m_storage.m_tail == idx)
        {
            m_storage.m_tail = prev;
        }
        m_storage.releaseSlot(idx);
        return true;
    }

    /// @brief Removes every node whose value compares equal to `value`.
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
        gp::UInt32 prev = kInvalidListNodeIndex;
        gp::UInt32 cur = m_storage.m_head;

        while (cur != kInvalidListNodeIndex)
        {
            const gp::UInt32 nextIdx = m_storage.m_slots[cur].next;
            const InT* const curPtr = m_storage.slotValuePtr(cur);
            if (*curPtr == value)
            {
                if (prev == kInvalidListNodeIndex)
                {
                    m_storage.m_head = nextIdx;
                }
                else
                {
                    m_storage.m_slots[prev].next = nextIdx;
                }
                if (m_storage.m_tail == cur)
                {
                    m_storage.m_tail = prev;
                }
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
            else
            {
                prev = cur;
            }
            cur = nextIdx;
        }

        if (deferredAliasedSlot != kInvalidListNodeIndex)
        {
            m_storage.releaseSlot(deferredAliasedSlot);
        }
        return removedCount;
    }

    /// @brief Removes every node satisfying unary predicate `predicate`.
    /// @tparam InPredicate Predicate type satisfying `gp::concepts::IsPredicate<InPredicate, const InT&>`.
    /// @param[in,out] predicate Callable returning `true` for nodes to remove.
    /// @return Number of nodes removed.
    template <typename InPredicate>
    requires gp::concepts::IsPredicate<InPredicate, const InT&> SizeType removeIf(InPredicate&& predicate) noexcept
    {
        SizeType removedCount = 0u;
        gp::UInt32 prev = kInvalidListNodeIndex;
        gp::UInt32 cur = m_storage.m_head;

        while (cur != kInvalidListNodeIndex)
        {
            const gp::UInt32 nextIdx = m_storage.m_slots[cur].next;
            if (predicate(*m_storage.slotValuePtr(cur)))
            {
                if (prev == kInvalidListNodeIndex)
                {
                    m_storage.m_head = nextIdx;
                }
                else
                {
                    m_storage.m_slots[prev].next = nextIdx;
                }
                if (m_storage.m_tail == cur)
                {
                    m_storage.m_tail = prev;
                }
                m_storage.releaseSlot(cur);
                ++removedCount;
            }
            else
            {
                prev = cur;
            }
            cur = nextIdx;
        }

        return removedCount;
    }

    /// @brief Reverses node order in place without moving `InT` values or invalidating `NodeHandle`s.
    void reverse() noexcept
    {
        m_storage.reverseNextChain();
    }

    /// @brief Sorts elements in ascending `<` order via stable O(N log N) merge sort while preserving all handles.
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
        m_storage.sortNextChain(comparator);
    }

public:
    /// @brief Returns the number of live elements in the list.
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
    /// @brief Returns a mutable forward iterator to the head element.
    /// @return Start iterator.
    [[nodiscard]] constexpr Iterator begin() noexcept
    {
        return Iterator(this, m_storage.m_head);
    }

    /// @brief Returns a const forward iterator to the head element.
    /// @return Const start iterator.
    [[nodiscard]] constexpr ConstIterator begin() const noexcept
    {
        return ConstIterator(this, m_storage.m_head);
    }

    /// @brief Returns a mutable forward end sentinel iterator.
    /// @return End iterator.
    [[nodiscard]] constexpr Iterator end() noexcept
    {
        return Iterator(this, kInvalidListNodeIndex);
    }

    /// @brief Returns a const forward end sentinel iterator.
    /// @return Const end iterator.
    [[nodiscard]] constexpr ConstIterator end() const noexcept
    {
        return ConstIterator(this, kInvalidListNodeIndex);
    }

    /// @brief Returns a const forward iterator to the head element.
    /// @return Const start iterator.
    [[nodiscard]] constexpr ConstIterator cbegin() const noexcept
    {
        return begin();
    }

    /// @brief Returns a const forward end sentinel iterator.
    /// @return Const end iterator.
    [[nodiscard]] constexpr ConstIterator cend() const noexcept
    {
        return end();
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
    void swap(LinkedList& other) noexcept
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

        LinkedList temp(std::move(*this));
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
    [[nodiscard]] bool operator==(const LinkedList<InT, InOtherAllocator>& other) const noexcept
    {
        return m_storage.equalChain(other.m_storage);
    }

    /// @brief Performs lexicographical three-way comparison against another list.
    /// @tparam InOtherAllocator Right-hand list's allocator type.
    /// @param[in] other List to compare against.
    /// @return Three-way comparison ordering result matching `InT`.
    template <gp::concepts::IsAllocator InOtherAllocator, typename InElemT = InT>
    requires gp::concepts::IsThreeWayComparable<InElemT> [[nodiscard]] std::compare_three_way_result_t<InElemT>
        operator<=>(const LinkedList<InT, InOtherAllocator>& other) const noexcept
    {
        return m_storage.compareChain(other.m_storage);
    }

private:
    /// @brief Links `slotIndex` at the head of the active chain.
    GP_FORCEINLINE_HINT void linkAtFront(const gp::UInt32 slotIndex) noexcept
    {
        m_storage.m_slots[slotIndex].next = m_storage.m_head;
        m_storage.m_head = slotIndex;
        if (m_storage.m_tail == kInvalidListNodeIndex)
        {
            m_storage.m_tail = slotIndex;
        }
    }

    /// @brief Links `slotIndex` at the tail of the active chain.
    GP_FORCEINLINE_HINT void linkAtBack(const gp::UInt32 slotIndex) noexcept
    {
        m_storage.m_slots[slotIndex].next = kInvalidListNodeIndex;
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

    /// @brief Links `newSlotIndex` immediately after occupied `targetSlotIndex`.
    GP_FORCEINLINE_HINT void linkAfter(const gp::UInt32 targetSlotIndex, const gp::UInt32 newSlotIndex) noexcept
    {
        m_storage.m_slots[newSlotIndex].next = m_storage.m_slots[targetSlotIndex].next;
        m_storage.m_slots[targetSlotIndex].next = newSlotIndex;
        if (m_storage.m_tail == targetSlotIndex)
        {
            m_storage.m_tail = newSlotIndex;
        }
    }

private:
    StorageType m_storage{};
};

/// @brief Swaps two `LinkedList` instances via `lhs.swap(rhs)`.
/// @tparam InT Element type.
/// @tparam InAllocator Allocator type.
/// @param[in,out] lhs First list.
/// @param[in,out] rhs Second list.
template <typename InT, gp::concepts::IsAllocator InAllocator>
inline void swap(LinkedList<InT, InAllocator>& lhs, LinkedList<InT, InAllocator>& rhs) noexcept
{
    lhs.swap(rhs);
}

static_assert(
    sizeof(LinkedList<gp::Int32>) == 32u, "gp::LinkedList with stateless DefaultAllocator must occupy exactly 32 bytes."
);
static_assert(
    std::forward_iterator<LinkedList<gp::Int32>::Iterator>,
    "gp::LinkedList::Iterator must satisfy std::forward_iterator."
);
static_assert(
    std::forward_iterator<LinkedList<gp::Int32>::ConstIterator>,
    "gp::LinkedList::ConstIterator must satisfy std::forward_iterator."
);

}   // namespace gp

namespace gp::concepts
{

namespace detail
{

template <typename InType>
constexpr bool IsLinkedListV = false;

template <typename InT, typename InAllocator>
constexpr bool IsLinkedListV<gp::LinkedList<InT, InAllocator>> = true;

}   // namespace detail

/// @brief Concept satisfied by any instantiation of `gp::LinkedList`.
/// @tparam InType Candidate type to test.
template <typename InType>
concept IsLinkedList = detail::IsLinkedListV<std::remove_cvref_t<InType>>;

/// @brief Specialization enabling bitwise relocation of `gp::LinkedList` when its allocator is bitwise relocatable.
/// @tparam InT Element type.
/// @tparam InAllocator Allocator type.
template <typename InT, typename InAllocator>
struct IsRelocatableTrait<gp::LinkedList<InT, InAllocator>>
    : std::bool_constant<std::is_move_constructible_v<InAllocator> && IsRelocatable<InAllocator>>
{};

}   // namespace gp::concepts
