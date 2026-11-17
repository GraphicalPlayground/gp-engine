// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "containers/lists/ListTypes.hpp"
#include "CoreMinimal.hpp"
#include "memory/allocators/AllocatorConcepts.hpp"
#include "memory/MemoryBase.hpp"
#include "memory/utils/AllocatorUtilities.hpp"
#include "memory/utils/MemoryOps.hpp"
#include <compare>
#include <cstdint>
#include <iterator>
#include <new>
#include <type_traits>
#include <utility>

namespace gp::container::detail
{

/// @brief Shared forward or bidirectional iterator traversing live nodes inside a slot-pooled list container.
/// @details Stores only the owning list pointer and the current slot index (`16 bytes` on 64-bit), so iterators stay
///          valid across slot-pool growth as long as the referenced node itself is not removed.
/// @tparam InOwner Owning list class (`DoubleLinkedList` or `LinkedList`, optionally const-qualified).
/// @tparam InElementType Yielded element type (`T` or `const T`).
/// @tparam InHandleType Strongly typed `ListNodeHandle` returned by `handle()`.
/// @tparam InIsBidirectional True for `DoubleLinkedList` (`--` enabled), false for `LinkedList` (forward-only).
template <typename InOwner, typename InElementType, typename InHandleType, bool InIsBidirectional>
class SlotListIterator
{
public:
    using iterator_category =
        std::conditional_t<InIsBidirectional, std::bidirectional_iterator_tag, std::forward_iterator_tag>;
    using iterator_concept = iterator_category;
    using value_type = std::remove_const_t<InElementType>;
    using difference_type = gp::ISize;
    using pointer = InElementType*;
    using reference = InElementType&;
    using HandleType = InHandleType;

public:
    /// @brief Constructs an unassociated end-like iterator.
    constexpr SlotListIterator() noexcept = default;

    /// @brief Constructs an iterator pointing to slot `slotIndex` inside `owner`.
    /// @param[in] owner Owning list container pointer.
    /// @param[in] slotIndex Slot index, or `kInvalidListNodeIndex` for `end()`.
    constexpr SlotListIterator(InOwner* owner, const gp::UInt32 slotIndex) noexcept
        : m_owner(owner)
        , m_slotIndex(slotIndex)
    {}

    /// @brief Implicit conversion from mutable iterator to const iterator.
    /// @tparam InOtherOwner Mutable owner type matching `std::remove_const_t<InOwner>`.
    /// @tparam InOtherElement Mutable element type matching `value_type`.
    /// @param[in] other Source mutable iterator.
    template <typename InOtherOwner, typename InOtherElement>
    requires(std::is_const_v<InElementType> && std::is_same_v<std::remove_const_t<InOwner>, InOtherOwner> &&
             std::is_same_v<value_type, InOtherElement>)
    constexpr SlotListIterator(
        const SlotListIterator<InOtherOwner, InOtherElement, InHandleType, InIsBidirectional>& other
    ) noexcept
        : m_owner(other.m_owner)
        , m_slotIndex(other.m_slotIndex)
    {}

public:
    /// @brief Dereferences the element at the current node.
    /// @return Reference to the element in the current slot.
    [[nodiscard]] GP_FORCEINLINE_HINT reference operator*() const noexcept
    {
        GP_ASSERT(
            m_owner != nullptr && m_slotIndex != kInvalidListNodeIndex && "Cannot dereference end list iterator."
        );
        return *m_owner->m_storage.slotValuePtr(m_slotIndex);
    }

    /// @brief Returns a pointer to the element at the current node.
    /// @return Pointer to the element in the current slot.
    [[nodiscard]] GP_FORCEINLINE_HINT pointer operator->() const noexcept
    {
        GP_ASSERT(
            m_owner != nullptr && m_slotIndex != kInvalidListNodeIndex && "Cannot dereference end list iterator."
        );
        return m_owner->m_storage.slotValuePtr(m_slotIndex);
    }

    /// @brief Returns the generational `NodeHandle` identifying the current node (`invalid()` at `end()`).
    /// @return Generational handle for the current node.
    [[nodiscard]] GP_FORCEINLINE_HINT HandleType handle() const noexcept
    {
        if (m_owner == nullptr)
        {
            return HandleType::invalid();
        }
        return m_owner->m_storage.template makeHandleForSlot<HandleType>(m_slotIndex);
    }

    /// @brief Checks whether this iterator refers to an end sentinel or unassociated position.
    /// @return True if pointing at `end()` or default-constructed, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isEnd() const noexcept
    {
        return m_slotIndex == kInvalidListNodeIndex;
    }

    /// @brief Returns an opaque pointer to the owning list container for debug ownership checks.
    /// @return Owning container pointer, or `nullptr` if unassociated.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr const void* owner() const noexcept
    {
        return m_owner;
    }

    /// @brief Advances the iterator to the next node in the list.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT SlotListIterator& operator++() noexcept
    {
        GP_ASSERT(m_owner != nullptr && m_slotIndex != kInvalidListNodeIndex && "Cannot increment end list iterator.");
        m_slotIndex = m_owner->m_storage.m_slots[m_slotIndex].next;
        return *this;
    }

    /// @brief Advances the iterator to the next node and returns the previous position.
    /// @return Copy of the iterator prior to advancing.
    GP_FORCEINLINE_HINT SlotListIterator operator++(int) noexcept
    {
        const SlotListIterator copy(*this);
        ++(*this);
        return copy;
    }

    /// @brief Steps the iterator backward to the previous node (or `tail` when starting at `end()`).
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT SlotListIterator& operator--() noexcept requires InIsBidirectional
    {
        GP_ASSERT(m_owner != nullptr && "Cannot decrement unassociated list iterator.");
        if (m_slotIndex == kInvalidListNodeIndex)
        {
            m_slotIndex = m_owner->m_storage.m_tail;
        }
        else
        {
            m_slotIndex = m_owner->m_storage.m_slots[m_slotIndex].prev;
        }
        GP_ASSERT(m_slotIndex != kInvalidListNodeIndex && "Cannot decrement begin list iterator.");
        return *this;
    }

    /// @brief Steps the iterator backward and returns the previous position.
    /// @return Copy of the iterator prior to stepping back.
    GP_FORCEINLINE_HINT SlotListIterator operator--(int) noexcept requires InIsBidirectional
    {
        const SlotListIterator copy(*this);
        --(*this);
        return copy;
    }

    /// @brief Compares two iterators for node identity equality.
    /// @param[in] other Right-hand iterator.
    /// @return True if both belong to the same list and refer to the same slot index.
    [[nodiscard]] constexpr bool operator==(const SlotListIterator& other) const noexcept
    {
        return m_owner == other.m_owner && m_slotIndex == other.m_slotIndex;
    }

private:
    template <typename InOtherOwner, typename InOtherElement, typename InOtherHandle, bool InOtherBidir>
    friend class SlotListIterator;

    InOwner* m_owner{ nullptr };
    gp::UInt32 m_slotIndex{ kInvalidListNodeIndex };
};

/// @brief Concept checking whether a slot struct carries a `prev` link index (doubly-linked) or only `next`.
/// @tparam InSlot Candidate slot struct type.
template <typename InSlot>
concept HasSlotPrevLink = requires(InSlot slot) {
    {
        slot.prev
    } -> gp::concepts::IsSameAs<gp::UInt32&>;
};

/// @brief Shared contiguous slot-pool storage and generational free-list engine backing list containers.
/// @details Stores all list nodes inside one contiguous `InSlot` buffer managed through `InAllocator`. Unallocated
///          slots beyond `m_carvedCount` remain uninitialized until first needed (lazy carving), while freed slots
///          enter an intrusive singly-linked stack through `InSlot::next`. Slot occupancy lives in the least
///          significant bit of `generation` (odd = occupied, even = free), keeping slot metadata compact and catching
///          stale handles in O(1) time.
/// @tparam InT Element payload type stored inside each occupied slot.
/// @tparam InSlot Concrete node slot struct (`storage`, `next`, `generation`, and optional `prev`).
/// @tparam InAllocator Engine allocator satisfying `gp::concepts::IsAllocator`.
template <typename InT, typename InSlot, gp::concepts::IsAllocator InAllocator>
struct SlotPoolStorage
{
public:
    static constexpr gp::UInt32 kAllocAlignment = static_cast<gp::UInt32>(
        alignof(InSlot) > gp::memory::kMinimumAlignment ? alignof(InSlot) : gp::memory::kMinimumAlignment
    );

public:
    /// @brief Pointer to the contiguous slot array, or `nullptr` when unallocated.
    InSlot* m_slots{ nullptr };

    /// @brief Number of currently live elements in the list.
    gp::UInt32 m_size{ 0u };

    /// @brief Total allocated slot capacity in `m_slots`.
    gp::UInt32 m_capacity{ 0u };

    /// @brief High-water count of carved slots in `[0, m_carvedCount)` whose `generation` fields are initialized.
    gp::UInt32 m_carvedCount{ 0u };

    /// @brief Slot index of the first element in the active list, or `kInvalidListNodeIndex` when empty.
    gp::UInt32 m_head{ kInvalidListNodeIndex };

    /// @brief Slot index of the last element in the active list, or `kInvalidListNodeIndex` when empty.
    gp::UInt32 m_tail{ kInvalidListNodeIndex };

    /// @brief Head slot index of the intrusive free-list stack, or `kInvalidListNodeIndex` when empty.
    gp::UInt32 m_freeHead{ kInvalidListNodeIndex };

    /// @brief Bound allocator instance (occupies zero bytes when stateless via `GP_NO_UNIQUE_ADDRESS`).
    GP_NO_UNIQUE_ADDRESS InAllocator m_allocator{};

public:
    /// @brief Default-constructs empty storage with a default-initialized allocator.
    constexpr SlotPoolStorage() noexcept = default;

    /// @brief Constructs empty storage bound to a copy of `allocator`.
    /// @param[in] allocator Allocator instance to copy.
    constexpr explicit SlotPoolStorage(const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_allocator(allocator)
    {}

    /// @brief Constructs empty storage by moving `allocator`.
    /// @param[in,out] allocator Allocator instance to move from.
    constexpr explicit SlotPoolStorage(InAllocator&& allocator) noexcept
        requires std::is_move_constructible_v<InAllocator>
        : m_allocator(std::move(allocator))
    {}

public:
    /// @brief Returns a pointer to the constructed `InT` payload inside slot `slotIndex`.
    /// @param[in] slotIndex Index of an occupied slot.
    /// @return Mutable pointer to the contained element.
    [[nodiscard]] GP_FORCEINLINE_HINT InT* slotValuePtr(const gp::UInt32 slotIndex) noexcept
    {
        return std::launder(reinterpret_cast<InT*>(m_slots[slotIndex].storage));
    }

    /// @brief Returns a const pointer to the constructed `InT` payload inside slot `slotIndex`.
    /// @param[in] slotIndex Index of an occupied slot.
    /// @return Const pointer to the contained element.
    [[nodiscard]] GP_FORCEINLINE_HINT const InT* slotValuePtr(const gp::UInt32 slotIndex) const noexcept
    {
        return std::launder(reinterpret_cast<const InT*>(m_slots[slotIndex].storage));
    }

    /// @brief Checks whether a raw pointer falls anywhere inside the currently carved slot storage range.
    /// @param[in] ptr Arbitrary address to test for self-aliasing against internal slot storage.
    /// @return True if `ptr` lies within `[m_slots, m_slots + m_carvedCount)`, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isPointerInStorage(const void* const ptr) const noexcept
    {
        if (m_slots == nullptr || m_carvedCount == 0u || ptr == nullptr)
        {
            return false;
        }
        const std::uintptr_t targetAddr = reinterpret_cast<std::uintptr_t>(ptr);
        const std::uintptr_t slotsBegin = reinterpret_cast<std::uintptr_t>(m_slots);
        const std::uintptr_t slotsEnd = reinterpret_cast<std::uintptr_t>(m_slots + m_carvedCount);
        return targetAddr >= slotsBegin && targetAddr < slotsEnd;
    }

    /// @brief Checks whether `slotIndex` currently holds a live element (`generation` is odd).
    /// @param[in] slotIndex Carved slot index to inspect.
    /// @return True if occupied, false if free.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isSlotOccupied(const gp::UInt32 slotIndex) const noexcept
    {
        return (m_slots[slotIndex].generation & 1u) != 0u;
    }

    /// @brief Validates whether `handle` points to a currently occupied slot with a matching generation.
    /// @tparam InHandle List node handle specialization.
    /// @param[in] handle Handle to validate.
    /// @return True if `handle` refers to a live node in this container, false otherwise.
    template <typename InHandle>
    [[nodiscard]] GP_FORCEINLINE_HINT bool isValidHandle(const InHandle handle) const noexcept
    {
        return handle.index < m_carvedCount && handle.isValid() &&
               m_slots[handle.index].generation == handle.generation;
    }

    /// @brief Builds a `ListNodeHandle` matching the current generation of occupied slot `slotIndex`.
    /// @tparam InHandle Target handle specialization.
    /// @param[in] slotIndex Slot index (`kInvalidListNodeIndex` yields an invalid handle).
    /// @return Valid `InHandle` for `slotIndex`, or `InHandle::invalid()`.
    template <typename InHandle>
    [[nodiscard]] GP_FORCEINLINE_HINT InHandle makeHandleForSlot(const gp::UInt32 slotIndex) const noexcept
    {
        if (slotIndex == kInvalidListNodeIndex)
        {
            return InHandle::invalid();
        }
        return InHandle{ slotIndex, m_slots[slotIndex].generation };
    }

    /// @brief Acquires a free or freshly carved slot and constructs `InT(args...)` in place.
    /// @details When the slot pool must grow, stages the new `InT` on the stack first (for movable types) so
    ///          arguments aliasing an existing element inside `m_slots` stay valid across buffer reallocation.
    /// @tparam InArgs Constructor argument types forwarded to `InT`.
    /// @param[in,out] args Constructor arguments for the new element.
    /// @return Index of the newly occupied slot, or `kInvalidListNodeIndex` on failure.
    template <typename... InArgs>
    [[nodiscard]] gp::UInt32 acquireSlot(InArgs&&... args) noexcept
    {
        if (m_freeHead != kInvalidListNodeIndex) [[likely]]
        {
            const gp::UInt32 idx = m_freeHead;
            m_freeHead = m_slots[idx].next;
            return constructInSlot(idx, std::forward<InArgs>(args)...);
        }

        if (m_carvedCount < m_capacity) [[likely]]
        {
            const gp::UInt32 idx = m_carvedCount++;
            m_slots[idx].generation = 0u;
            return constructInSlot(idx, std::forward<InArgs>(args)...);
        }

        if constexpr (std::is_move_constructible_v<InT>)
        {
            // Stage on the stack before growing so self-referential arguments (e.g. pushBack(front())) stay safe.
            InT stagedValue(std::forward<InArgs>(args)...);
            if (!growForOneMore()) [[unlikely]]
            {
                return kInvalidListNodeIndex;
            }
            const gp::UInt32 idx = m_carvedCount++;
            m_slots[idx].generation = 0u;
            return constructInSlot(idx, std::move(stagedValue));
        }
        else
        {
            GP_ASSERT(false && "SlotPoolStorage cannot grow storage for non-movable types; call reserve() first.");
            return kInvalidListNodeIndex;
        }
    }

    /// @brief Destructs the payload in `slotIndex`, advances its generation to even, and pushes it onto `m_freeHead`.
    /// @param[in] slotIndex Index of the occupied slot to release.
    GP_FORCEINLINE_HINT void releaseSlot(const gp::UInt32 slotIndex) noexcept
    {
        gp::memory::destructItem(slotValuePtr(slotIndex));
        ++m_slots[slotIndex].generation;
        m_slots[slotIndex].next = m_freeHead;
        if constexpr (HasSlotPrevLink<InSlot>)
        {
            m_slots[slotIndex].prev = kInvalidListNodeIndex;
        }
        m_freeHead = slotIndex;
        --m_size;
    }

    /// @brief Destroys every active node along `m_head -> ... -> m_tail` while advancing slot generations.
    /// @details Walking the live chain advances every freed slot generation so handles created before `clear()` stay
    ///          detectably stale even when subsequent insertions reuse the same slot indices.
    void clearChain() noexcept
    {
        gp::UInt32 cur = m_head;
        while (cur != kInvalidListNodeIndex)
        {
            const gp::UInt32 nextIdx = m_slots[cur].next;
            releaseSlot(cur);
            cur = nextIdx;
        }
        GP_ASSERT(m_size == 0u && "SlotPoolStorage::clearChain() found corrupted live chain.");
        m_head = kInvalidListNodeIndex;
        m_tail = kInvalidListNodeIndex;
    }

    /// @brief Destroys all elements and returns the slot buffer to `m_allocator`.
    void resetStorage() noexcept
    {
        clearChain();
        if (m_slots != nullptr)
        {
            const gp::USize oldBytes = static_cast<gp::USize>(m_capacity) * sizeof(InSlot);
            m_allocator.deallocate(m_slots, oldBytes, kAllocAlignment);
            m_slots = nullptr;
        }
        m_capacity = 0u;
        m_carvedCount = 0u;
        m_freeHead = kInvalidListNodeIndex;
    }

    /// @brief Ensures the slot buffer can hold at least `minCapacity` nodes without reallocating.
    /// @param[in] minCapacity Minimum desired slot capacity.
    void reserveSlots(const gp::USize minCapacity) noexcept
    {
        if (minCapacity <= static_cast<gp::USize>(m_capacity))
        {
            return;
        }

        GP_ASSERT(minCapacity <= static_cast<gp::USize>(kMaxListSlotCapacity) && "List slot capacity exceeded.");
        const gp::UInt32 clampedTarget = static_cast<gp::UInt32>(
            minCapacity <= static_cast<gp::USize>(kMaxListSlotCapacity) ? minCapacity : kMaxListSlotCapacity
        );

        gp::UInt32 targetCapacity = gp::memory::calculateSlackReserve<gp::UInt32>(
            m_allocator, clampedTarget, sizeof(InSlot), true, kAllocAlignment
        );
        if (targetCapacity < clampedTarget)
        {
            targetCapacity = clampedTarget;
        }

        (void)reallocateSlots(targetCapacity);
    }

    /// @brief Shrinks trailing uncarved capacity (or frees the buffer if empty) without invalidating active handles.
    void shrinkStorage() noexcept
    {
        if (m_size == 0u)
        {
            resetStorage();
            return;
        }

        if (m_carvedCount < m_capacity)
        {
            (void)reallocateSlots(m_carvedCount);
        }
    }

    /// @brief Steals raw storage pointers and indices from `other` in O(1) time.
    /// @param[in,out] other Source storage left empty after the transfer.
    void stealFrom(SlotPoolStorage& other) noexcept
    {
        m_slots = other.m_slots;
        m_size = other.m_size;
        m_capacity = other.m_capacity;
        m_carvedCount = other.m_carvedCount;
        m_head = other.m_head;
        m_tail = other.m_tail;
        m_freeHead = other.m_freeHead;

        other.m_slots = nullptr;
        other.m_size = 0u;
        other.m_capacity = 0u;
        other.m_carvedCount = 0u;
        other.m_head = kInvalidListNodeIndex;
        other.m_tail = kInvalidListNodeIndex;
        other.m_freeHead = kInvalidListNodeIndex;
    }

    /// @brief Swaps raw storage pointers and indices with `other` in O(1) time.
    /// @param[in,out] other Other storage instance sharing an equal allocator.
    void swapStorageMembers(SlotPoolStorage& other) noexcept
    {
        std::swap(m_slots, other.m_slots);
        std::swap(m_size, other.m_size);
        std::swap(m_capacity, other.m_capacity);
        std::swap(m_carvedCount, other.m_carvedCount);
        std::swap(m_head, other.m_head);
        std::swap(m_tail, other.m_tail);
        std::swap(m_freeHead, other.m_freeHead);
    }

    /// @brief Compares active elements in head-to-tail order with `other` for equality.
    /// @tparam InOtherSlot Other storage slot type.
    /// @tparam InOtherAllocator Other storage allocator type.
    /// @param[in] other Target storage to compare against.
    /// @return True if sizes and corresponding elements compare equal, false otherwise.
    template <typename InOtherSlot, gp::concepts::IsAllocator InOtherAllocator>
    [[nodiscard]] bool equalChain(const SlotPoolStorage<InT, InOtherSlot, InOtherAllocator>& other) const noexcept
    {
        if (m_size != other.m_size)
        {
            return false;
        }

        gp::UInt32 curA = m_head;
        gp::UInt32 curB = other.m_head;
        while (curA != kInvalidListNodeIndex && curB != kInvalidListNodeIndex)
        {
            if (!(*slotValuePtr(curA) == *other.slotValuePtr(curB)))
            {
                return false;
            }
            curA = m_slots[curA].next;
            curB = other.m_slots[curB].next;
        }
        return true;
    }

    /// @brief Performs lexicographical three-way comparison against `other` in head-to-tail order.
    /// @tparam InOtherSlot Other storage slot type.
    /// @tparam InOtherAllocator Other storage allocator type.
    /// @param[in] other Target storage to compare against.
    /// @return Three-way comparison ordering matching `InT`.
    template <typename InOtherSlot, gp::concepts::IsAllocator InOtherAllocator, typename InElemT = InT>
    requires gp::concepts::IsThreeWayComparable<InElemT> [[nodiscard]] std::compare_three_way_result_t<InElemT>
        compareChain(const SlotPoolStorage<InT, InOtherSlot, InOtherAllocator>& other) const noexcept
    {
        gp::UInt32 curA = m_head;
        gp::UInt32 curB = other.m_head;
        while (curA != kInvalidListNodeIndex && curB != kInvalidListNodeIndex)
        {
            const std::compare_three_way_result_t<InElemT> cmp = (*slotValuePtr(curA) <=> *other.slotValuePtr(curB));
            if (cmp != 0)
            {
                return cmp;
            }
            curA = m_slots[curA].next;
            curB = other.m_slots[curB].next;
        }
        return m_size <=> other.m_size;
    }

    /// @brief Finds the first occupied slot in head-to-tail order matching `value`.
    /// @param[in] value Value to search for.
    /// @return Slot index on match, or `kInvalidListNodeIndex` if absent.
    [[nodiscard]] gp::UInt32 findSlotByValue(const InT& value) const noexcept
    {
        for (gp::UInt32 cur = m_head; cur != kInvalidListNodeIndex; cur = m_slots[cur].next)
        {
            if (*slotValuePtr(cur) == value)
            {
                return cur;
            }
        }
        return kInvalidListNodeIndex;
    }

    /// @brief Finds the first occupied slot in head-to-tail order satisfying `predicate`.
    /// @tparam InPredicate Unary predicate type accepting `const InT&`.
    /// @param[in,out] predicate Predicate tested against each element.
    /// @return Slot index of the first match, or `kInvalidListNodeIndex` if none match.
    template <typename InPredicate>
    [[nodiscard]] gp::UInt32 findSlotByPredicate(InPredicate& predicate) const noexcept
    {
        for (gp::UInt32 cur = m_head; cur != kInvalidListNodeIndex; cur = m_slots[cur].next)
        {
            if (predicate(*slotValuePtr(cur)))
            {
                return cur;
            }
        }
        return kInvalidListNodeIndex;
    }

    /// @brief Counts occupied nodes whose values equal `value`.
    /// @param[in] value Value to match.
    /// @return Number of matching nodes.
    [[nodiscard]] gp::USize countMatching(const InT& value) const noexcept
    {
        gp::USize matches = 0u;
        for (gp::UInt32 cur = m_head; cur != kInvalidListNodeIndex; cur = m_slots[cur].next)
        {
            if (*slotValuePtr(cur) == value)
            {
                ++matches;
            }
        }
        return matches;
    }

    /// @brief Streams all active payloads in head-to-tail order plus trailing element count into `hasher`.
    /// @tparam InHasher Concrete streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppendChain(InHasher& hasher) const noexcept
    {
        for (gp::UInt32 cur = m_head; cur != kInvalidListNodeIndex; cur = m_slots[cur].next)
        {
            hasher.append(*slotValuePtr(cur));
        }
        hasher.append(static_cast<gp::UInt64>(m_size));
    }

    /// @brief Copies all live elements from `other` into freshly linked contiguous slots `[0, other.m_size)`.
    /// @tparam InOtherSlot Other storage slot type.
    /// @tparam InOtherAllocator Other storage allocator type.
    /// @param[in] other Source storage to copy from.
    template <typename InOtherSlot, gp::concepts::IsAllocator InOtherAllocator>
    void copyChainFrom(const SlotPoolStorage<InT, InOtherSlot, InOtherAllocator>& other) noexcept
    {
        if (other.m_size == 0u)
        {
            return;
        }
        reserveSlots(static_cast<gp::USize>(other.m_size));
        for (gp::UInt32 cur = other.m_head; cur != kInvalidListNodeIndex; cur = other.m_slots[cur].next)
        {
            const gp::UInt32 slotIdx = acquireSlot(*other.slotValuePtr(cur));
            appendAcquiredSlot(slotIdx);
        }
    }

    /// @brief Moves all live elements from `other` into freshly linked slots inside `*this`.
    /// @param[in,out] other Source storage to consume elements from.
    void moveChainFrom(SlotPoolStorage& other) noexcept
    {
        if (other.m_size == 0u)
        {
            return;
        }
        if constexpr (std::is_move_constructible_v<InT>)
        {
            reserveSlots(static_cast<gp::USize>(other.m_size));
            for (gp::UInt32 cur = other.m_head; cur != kInvalidListNodeIndex; cur = other.m_slots[cur].next)
            {
                const gp::UInt32 slotIdx = acquireSlot(std::move(*other.slotValuePtr(cur)));
                appendAcquiredSlot(slotIdx);
            }
        }
    }

    /// @brief Repacks live nodes into contiguous slots `[0, m_size)` in logical order and shrinks capacity to `m_size`.
    /// @return True if slots were compacted into `[0, m_size)`, false if empty or relocation failed.
    bool compactStorage() noexcept
    {
        if (m_size == 0u)
        {
            resetStorage();
            return false;
        }

        if constexpr (gp::concepts::IsRelocatable<InT> || std::is_move_constructible_v<InT>)
        {
            const gp::UInt32 targetCount = m_size;
            const gp::USize newBytes = static_cast<gp::USize>(targetCount) * sizeof(InSlot);
            InSlot* const newSlots = static_cast<InSlot*>(m_allocator.allocate(newBytes, kAllocAlignment));
            GP_ASSERT(newSlots != nullptr && "List compact allocation failed.");
            if (newSlots == nullptr) [[unlikely]]
            {
                return false;
            }

            gp::UInt32 writeIdx = 0u;
            gp::UInt32 cur = m_head;
            while (cur != kInvalidListNodeIndex)
            {
                const gp::UInt32 nextCur = m_slots[cur].next;
                gp::memory::relocateConstructItems<InT, InT>(newSlots[writeIdx].storage, slotValuePtr(cur), 1u);

                // Advance past whatever generation previously occupied destination slot `writeIdx` so any stale
                // handle addressing `writeIdx` stays invalid even if a younger node relocated into `writeIdx`.
                const gp::UInt32 prevDestGen = (writeIdx < m_carvedCount) ? m_slots[writeIdx].generation : 0u;
                const gp::UInt32 nextOddGen = ((prevDestGen & 1u) != 0u) ? (prevDestGen + 2u) : (prevDestGen + 1u);
                newSlots[writeIdx].generation = (nextOddGen != 0u) ? nextOddGen : 1u;
                newSlots[writeIdx].next = (writeIdx + 1u < targetCount) ? (writeIdx + 1u) : kInvalidListNodeIndex;
                if constexpr (HasSlotPrevLink<InSlot>)
                {
                    newSlots[writeIdx].prev = (writeIdx > 0u) ? (writeIdx - 1u) : kInvalidListNodeIndex;
                }

                ++writeIdx;
                cur = nextCur;
            }

            const gp::USize oldBytes = static_cast<gp::USize>(m_capacity) * sizeof(InSlot);
            m_allocator.deallocate(m_slots, oldBytes, kAllocAlignment);

            m_slots = newSlots;
            m_capacity = targetCount;
            m_carvedCount = targetCount;
            m_head = 0u;
            m_tail = targetCount - 1u;
            m_freeHead = kInvalidListNodeIndex;
            return true;
        }
        else
        {
            GP_ASSERT(false && "List::compact() requires a movable element type.");
            return false;
        }
    }

    /// @brief Reverses the singly-linked `next` chain from `m_head` to `m_tail` in place.
    void reverseNextChain() noexcept
    {
        gp::UInt32 prevIdx = kInvalidListNodeIndex;
        gp::UInt32 curIdx = m_head;
        m_tail = m_head;

        while (curIdx != kInvalidListNodeIndex)
        {
            const gp::UInt32 nextIdx = m_slots[curIdx].next;
            m_slots[curIdx].next = prevIdx;
            prevIdx = curIdx;
            curIdx = nextIdx;
        }

        m_head = prevIdx;
    }

    /// @brief Sorts the active node chain in O(N log N) time and O(1) stack space via iterative bottom-up merge sort.
    /// @details Relinks `next` indices in place without moving `InT` values, keeping every active `NodeHandle` valid.
    /// @tparam InComparator Strict-weak-ordering predicate `bool(const InT& lhs, const InT& rhs)`.
    /// @param[in,out] comp Comparator instance.
    template <typename InComparator>
    void sortNextChain(InComparator& comp) noexcept
    {
        if (m_size < 2u)
        {
            return;
        }

        gp::UInt32 head = m_head;
        gp::UInt32 step = 1u;
        gp::UInt32 finalTail = m_tail;

        while (step < m_size)
        {
            gp::UInt32 newHead = kInvalidListNodeIndex;
            gp::UInt32 newTail = kInvalidListNodeIndex;
            gp::UInt32 cur = head;

            while (cur != kInvalidListNodeIndex)
            {
                const gp::UInt32 left = cur;
                const gp::UInt32 right = splitChain(left, step);
                cur = splitChain(right, step);

                gp::UInt32 mergedTail = kInvalidListNodeIndex;
                const gp::UInt32 mergedHead = mergeChains(left, right, comp, mergedTail);

                if (newHead == kInvalidListNodeIndex)
                {
                    newHead = mergedHead;
                }
                else
                {
                    m_slots[newTail].next = mergedHead;
                }
                newTail = mergedTail;
            }

            head = newHead;
            finalTail = newTail;
            if (step > (m_size / 2u))
            {
                break;
            }
            step <<= 1u;
        }

        m_head = head;
        m_tail = finalTail;
    }

private:
    /// @brief Appends a freshly acquired slot onto the end of the active `m_head`/`m_tail` chain.
    GP_FORCEINLINE_HINT void appendAcquiredSlot(const gp::UInt32 slotIndex) noexcept
    {
        if (slotIndex == kInvalidListNodeIndex) [[unlikely]]
        {
            return;
        }
        m_slots[slotIndex].next = kInvalidListNodeIndex;
        if constexpr (HasSlotPrevLink<InSlot>)
        {
            m_slots[slotIndex].prev = m_tail;
        }
        if (m_tail != kInvalidListNodeIndex)
        {
            m_slots[m_tail].next = slotIndex;
        }
        else
        {
            m_head = slotIndex;
        }
        m_tail = slotIndex;
    }

    /// @brief Constructs `InT(args...)` inside `m_slots[slotIndex]` and marks its generation odd and non-zero.
    template <typename... InArgs>
    GP_FORCEINLINE_HINT gp::UInt32 constructInSlot(const gp::UInt32 slotIndex, InArgs&&... args) noexcept
    {
        ::new (static_cast<void*>(m_slots[slotIndex].storage)) InT(std::forward<InArgs>(args)...);
        ++m_slots[slotIndex].generation;
        if (m_slots[slotIndex].generation == 0u) [[unlikely]]
        {
            m_slots[slotIndex].generation = 1u;
        }
        m_slots[slotIndex].next = kInvalidListNodeIndex;
        if constexpr (HasSlotPrevLink<InSlot>)
        {
            m_slots[slotIndex].prev = kInvalidListNodeIndex;
        }
        ++m_size;
        return slotIndex;
    }

    /// @brief Expands slot capacity by at least one slot using `calculateSlackGrow`.
    [[nodiscard]] bool growForOneMore() noexcept
    {
        if (m_capacity >= kMaxListSlotCapacity) [[unlikely]]
        {
            GP_ASSERT(false && "List reached maximum 32-bit slot capacity.");
            return false;
        }

        const gp::UInt32 minRequired = m_capacity + 1u;
        gp::UInt32 targetCapacity = gp::memory::calculateSlackGrow<gp::UInt32>(
            m_allocator, minRequired, m_capacity, sizeof(InSlot), true, kAllocAlignment
        );
        if (targetCapacity < minRequired)
        {
            targetCapacity = minRequired;
        }
        if (targetCapacity > kMaxListSlotCapacity)
        {
            targetCapacity = kMaxListSlotCapacity;
        }

        return reallocateSlots(targetCapacity);
    }

    /// @brief Allocates or resizes `m_slots` to `targetCapacity` while preserving live elements and free-list links.
    [[nodiscard]] bool reallocateSlots(const gp::UInt32 targetCapacity) noexcept
    {
        GP_ASSERT(targetCapacity >= m_carvedCount);
        const gp::USize oldBytes = static_cast<gp::USize>(m_capacity) * sizeof(InSlot);
        const gp::USize newBytes = static_cast<gp::USize>(targetCapacity) * sizeof(InSlot);

        if (m_slots == nullptr)
        {
            InSlot* const freshSlots = static_cast<InSlot*>(m_allocator.allocate(newBytes, kAllocAlignment));
            GP_ASSERT(freshSlots != nullptr && "List slot allocation failed.");
            if (freshSlots == nullptr) [[unlikely]]
            {
                return false;
            }
            m_slots = freshSlots;
            m_capacity = targetCapacity;
            return true;
        }

        if constexpr (gp::concepts::IsRelocatable<InT>)
        {
            InSlot* const resizedSlots =
                static_cast<InSlot*>(m_allocator.reallocate(m_slots, oldBytes, newBytes, kAllocAlignment));
            GP_ASSERT(resizedSlots != nullptr && "List slot reallocation failed.");
            if (resizedSlots == nullptr) [[unlikely]]
            {
                return false;
            }
            m_slots = resizedSlots;
            m_capacity = targetCapacity;
            return true;
        }
        else if constexpr (std::is_move_constructible_v<InT>)
        {
            InSlot* const newSlots = static_cast<InSlot*>(m_allocator.allocate(newBytes, kAllocAlignment));
            GP_ASSERT(newSlots != nullptr && "List slot allocation failed.");
            if (newSlots == nullptr) [[unlikely]]
            {
                return false;
            }

            for (gp::UInt32 i = 0u; i < m_carvedCount; ++i)
            {
                newSlots[i].next = m_slots[i].next;
                newSlots[i].generation = m_slots[i].generation;
                if constexpr (HasSlotPrevLink<InSlot>)
                {
                    newSlots[i].prev = m_slots[i].prev;
                }
                if (isSlotOccupied(i))
                {
                    gp::memory::relocateConstructItems<InT, InT>(newSlots[i].storage, slotValuePtr(i), 1u);
                }
            }

            m_allocator.deallocate(m_slots, oldBytes, kAllocAlignment);
            m_slots = newSlots;
            m_capacity = targetCapacity;
            return true;
        }
        else
        {
            GP_ASSERT(false && "Cannot relocate non-movable element type.");
            return false;
        }
    }

    /// @brief Detaches up to `count` nodes starting at `start` and returns the head of the remainder chain.
    [[nodiscard]] gp::UInt32 splitChain(const gp::UInt32 start, const gp::UInt32 count) noexcept
    {
        if (start == kInvalidListNodeIndex)
        {
            return kInvalidListNodeIndex;
        }

        gp::UInt32 cur = start;
        for (gp::UInt32 i = 1u; i < count && m_slots[cur].next != kInvalidListNodeIndex; ++i)
        {
            cur = m_slots[cur].next;
        }

        const gp::UInt32 nextSegment = m_slots[cur].next;
        m_slots[cur].next = kInvalidListNodeIndex;
        return nextSegment;
    }

    /// @brief Merges two sorted `next`-linked chains using `comp`, outputting the new tail index.
    template <typename InComparator>
    [[nodiscard]] gp::UInt32
        mergeChains(gp::UInt32 left, gp::UInt32 right, InComparator& comp, gp::UInt32& outMergedTail) noexcept
    {
        if (left == kInvalidListNodeIndex)
        {
            outMergedTail = findChainTail(right);
            return right;
        }
        if (right == kInvalidListNodeIndex)
        {
            outMergedTail = findChainTail(left);
            return left;
        }

        gp::UInt32 head = kInvalidListNodeIndex;
        if (comp(*slotValuePtr(right), *slotValuePtr(left)))
        {
            head = right;
            right = m_slots[right].next;
        }
        else
        {
            head = left;
            left = m_slots[left].next;
        }

        gp::UInt32 tail = head;
        while (left != kInvalidListNodeIndex && right != kInvalidListNodeIndex)
        {
            if (comp(*slotValuePtr(right), *slotValuePtr(left)))
            {
                m_slots[tail].next = right;
                tail = right;
                right = m_slots[right].next;
            }
            else
            {
                m_slots[tail].next = left;
                tail = left;
                left = m_slots[left].next;
            }
        }

        gp::UInt32 remainder = (left != kInvalidListNodeIndex) ? left : right;
        m_slots[tail].next = remainder;
        while (remainder != kInvalidListNodeIndex)
        {
            tail = remainder;
            remainder = m_slots[remainder].next;
        }

        outMergedTail = tail;
        return head;
    }

    /// @brief Returns the last valid slot index in a non-empty `next` chain.
    [[nodiscard]] gp::UInt32 findChainTail(gp::UInt32 cur) const noexcept
    {
        if (cur == kInvalidListNodeIndex)
        {
            return kInvalidListNodeIndex;
        }
        while (m_slots[cur].next != kInvalidListNodeIndex)
        {
            cur = m_slots[cur].next;
        }
        return cur;
    }
};

}   // namespace gp::container::detail
