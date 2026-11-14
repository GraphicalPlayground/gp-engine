// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Memory.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "memory/Memory.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/queues/JobQueueDesc.hpp"      // IWYU pragma: export
#include "threading/queues/JobQueueEnums.hpp"     // IWYU pragma: export
#include "threading/queues/JobQueueForward.hpp"   // IWYU pragma: export
#include "threading/queues/JobQueueTypes.hpp"     // IWYU pragma: export

namespace gp::thread
{

/// @brief Bounded, lock-free Multi-Producer Multi-Consumer (MPMC) per-slot sequence-stamped ring queue ([ADR-0051]).
/// @details Implements Dmitry Vyukov's turn-ticket bounded MPMC ring buffer with 3-zone 64-byte MESI cache-line
///          isolation ([ADR-0025]) and cache-line-aligned ring slots (`alignas(64) Slot`):
///          - **Per-Slot Sequence Tickets (`Slot::sequence`):** Each slot `i` initializes `sequence = i`. A producer
///            observing `sequence == pos` claims `pos` via `m_enqueuePos.compareExchangeWeak`, writes `payload`, and
///            publishes `sequence = pos + 1` (`Release`). A consumer observing `sequence == pos + 1` claims `pos` via
///            `m_dequeuePos.compareExchangeWeak`, reads `payload`, and advances `sequence = pos + mask + 1`
///            (`Release`) to free the slot for the next producer lap.
///          - **Slot-Level Cache-Line Isolation:** Aligning each `Slot` to `kJobQueueCacheLineSize` (64 bytes) ensures
///            adjacent slots (`Slot[i]` and `Slot[i + 1]`) never share or straddle a 64-byte hardware cache line,
///            eliminating false sharing when multiple producer and consumer cores access neighboring indices.
///          - **Primary Role in Fiber Job System:** Serves global priority bands (`High`, `Normal`, `Low`), external OS
///            thread job submissions, and cross-core ready-fiber wake routing from `JobCounter::decrement()`
///            ([ADR-0050]).
/// @tparam InElementType Trivially copyable, non-const payload type satisfying `gp::thread::JobQueueElement`.
template <JobQueueElement InElementType>
class alignas(kJobQueueCacheLineSize) MPMCRingQueue final
{
public:
    /// @brief Element type stored in the MPMC queue.
    using ElementType = InElementType;

    /// @brief Pointer to an element.
    using PointerType = InElementType*;

    /// @brief Pointer to a const element.
    using ConstPointerType = const InElementType*;

    /// @brief Unsigned integer type used for capacities, masks, and sequence tickets.
    using SizeType = gp::USize;

    /// @brief Signed 64-bit integer type used for wrap-safe sequence ticket comparisons.
    using SignedDiffType = gp::Int64;

    /// @brief Maximum supported power-of-two capacity (`2^30` slots) preventing signed sequence diff overflow.
    static constexpr SizeType kMaxCapacity = (static_cast<SizeType>(1u) << 30u);

    /// @brief 64-byte cache-line-isolated ring buffer slot pairing an atomic turn ticket with the element payload.
    struct alignas(kJobQueueCacheLineSize) Slot
    {
        /// @brief Monotonic sequence ticket synchronizing producer writes (`pos + 1`) and consumer releases
        ///        (`pos + mask + 1`).
        Atomic<SizeType> sequence{ 0u };

        /// @brief Stored element payload.
        ElementType payload{};
    };

    /// @brief Alias for the internal sequence-stamped slot structure (used with `initializeWithExternalBuffer`).
    using SlotType = Slot;

    /// @brief Required byte alignment for the allocated slot array (at least one 64-byte cache line).
    static constexpr gp::UInt32 kBufferAlignment = (alignof(SlotType) > kJobQueueCacheLineSize)
                                                       ? static_cast<gp::UInt32>(alignof(SlotType))
                                                       : static_cast<gp::UInt32>(kJobQueueCacheLineSize);

public:
    /// @brief Constructs an uninitialized MPMC ring queue (`isValid() == false`, zero allocations).
    [[nodiscard]] constexpr MPMCRingQueue() noexcept = default;

    /// @brief Constructs and allocates an MPMC ring queue with the specified minimum capacity.
    /// @param[in] minCapacity Minimum element capacity (automatically rounded up to a power of two).
    /// @param[in] allocator Optional custom memory allocator (`nullptr` uses `gp::memory::getGlobalMalloc()`).
    explicit MPMCRingQueue(const SizeType minCapacity, gp::memory::Malloc* const allocator = nullptr) noexcept
    {
        (void)initialize(minCapacity, allocator);
    }

    /// @brief Constructs an MPMC ring queue backed by caller-provided external `SlotType` storage.
    /// @param[in] externalSlots Pre-allocated `SlotType` array aligned to at least `alignof(SlotType)`.
    /// @param[in] capacityInElements Exact capacity in slots (must be a power of two >= `kMinJobQueueCapacity`).
    constexpr MPMCRingQueue(SlotType* const externalSlots, const SizeType capacityInElements) noexcept
    {
        (void)initializeWithExternalBuffer(externalSlots, capacityInElements);
    }

    MPMCRingQueue(const MPMCRingQueue&) = delete;
    MPMCRingQueue& operator=(const MPMCRingQueue&) = delete;

    /// @brief Move-constructs an MPMC ring queue by transferring slot storage ownership and cursor state.
    /// @param[in,out] other Source queue to move from (left in an uninitialized state).
    /// @note Moving a queue while threads are concurrently pushing or popping is undefined behavior.
    [[nodiscard]] constexpr MPMCRingQueue(MPMCRingQueue&& other) noexcept
        : m_slots(other.m_slots)
        , m_capacity(other.m_capacity)
        , m_mask(other.m_mask)
        , m_allocator(other.m_allocator)
        , m_ownsMemory(other.m_ownsMemory)
        , m_enqueuePos(other.m_enqueuePos.load(gp::MemoryOrder::Relaxed))
        , m_dequeuePos(other.m_dequeuePos.load(gp::MemoryOrder::Relaxed))
    {
        other.m_slots = nullptr;
        other.m_capacity = 0u;
        other.m_mask = 0u;
        other.m_allocator = nullptr;
        other.m_ownsMemory = false;
        other.m_enqueuePos.store(0u, gp::MemoryOrder::Relaxed);
        other.m_dequeuePos.store(0u, gp::MemoryOrder::Relaxed);
    }

    /// @brief Move-assigns an MPMC ring queue, releasing any currently owned slot buffer first.
    /// @param[in,out] other Source queue to move from (left in an uninitialized state).
    /// @return Reference to `*this`.
    /// @note Moving a queue while threads are concurrently pushing or popping is undefined behavior.
    constexpr MPMCRingQueue& operator=(MPMCRingQueue&& other) noexcept
    {
        if (this != &other)
        {
            shutdown();

            m_slots = other.m_slots;
            m_capacity = other.m_capacity;
            m_mask = other.m_mask;
            m_allocator = other.m_allocator;
            m_ownsMemory = other.m_ownsMemory;
            m_enqueuePos.store(other.m_enqueuePos.load(gp::MemoryOrder::Relaxed), gp::MemoryOrder::Relaxed);
            m_dequeuePos.store(other.m_dequeuePos.load(gp::MemoryOrder::Relaxed), gp::MemoryOrder::Relaxed);

            other.m_slots = nullptr;
            other.m_capacity = 0u;
            other.m_mask = 0u;
            other.m_allocator = nullptr;
            other.m_ownsMemory = false;
            other.m_enqueuePos.store(0u, gp::MemoryOrder::Relaxed);
            other.m_dequeuePos.store(0u, gp::MemoryOrder::Relaxed);
        }
        return *this;
    }

    /// @brief Destroys the queue and releases any owned backing slot buffer.
    constexpr ~MPMCRingQueue() noexcept
    {
        shutdown();
    }

public:
    /// @section Lifecycle & Storage Management

    /// @brief Allocates backing `SlotType` storage using the specified or global engine allocator.
    /// @param[in] minCapacity Minimum element capacity (rounded up to a power of two >= `kMinJobQueueCapacity`).
    /// @param[in] allocator Optional custom `gp::memory::Malloc` backend (`nullptr` defaults to `getGlobalMalloc()`).
    /// @return `JobQueueResult::Success`, `JobQueueResult::InvalidArguments`, or `JobQueueResult::OutOfMemory`.
    [[nodiscard]] JobQueueResult initialize(
        const SizeType minCapacity = kDefaultMPMCQueueCapacity, gp::memory::Malloc* const allocator = nullptr
    ) noexcept
    {
        shutdown();

        if (minCapacity == 0u || minCapacity > kMaxCapacity) [[unlikely]]
        {
            return JobQueueResult::InvalidArguments;
        }

        const SizeType clampedMin = (minCapacity < kMinJobQueueCapacity) ? kMinJobQueueCapacity : minCapacity;
        const SizeType adjustedCapacity = gp::memory::roundUpToPowerOfTwo(clampedMin);
        if (adjustedCapacity == 0u || adjustedCapacity > kMaxCapacity ||
            adjustedCapacity > (~static_cast<SizeType>(0u) / sizeof(SlotType))) [[unlikely]]
        {
            return JobQueueResult::InvalidArguments;
        }

        gp::memory::Malloc* const targetAllocator = (allocator != nullptr) ? allocator : gp::memory::getGlobalMalloc();
        if (targetAllocator == nullptr) [[unlikely]]
        {
            return JobQueueResult::OutOfMemory;
        }

        const SizeType totalBytes = adjustedCapacity * sizeof(SlotType);
        void* const rawMemory = targetAllocator->tryAllocate(totalBytes, kBufferAlignment);
        if (rawMemory == nullptr) [[unlikely]]
        {
            return JobQueueResult::OutOfMemory;
        }

        m_slots = static_cast<SlotType*>(rawMemory);
        m_capacity = adjustedCapacity;
        m_mask = adjustedCapacity - 1u;
        m_allocator = targetAllocator;
        m_ownsMemory = true;

        resetSequencesAndCursors();
        return JobQueueResult::Success;
    }

    /// @brief Attaches the queue to a caller-provided pre-allocated `SlotType` buffer without heap allocation.
    /// @param[in] externalSlots Pointer to an array of at least `capacityInElements` `SlotType` entries.
    /// @param[in] capacityInElements Exact slot capacity (must be a power of two >= `kMinJobQueueCapacity`).
    /// @return `JobQueueResult::Success`, or `JobQueueResult::InvalidArguments` if null, misaligned, or not a power of
    ///         two.
    [[nodiscard]] constexpr JobQueueResult
        initializeWithExternalBuffer(SlotType* const externalSlots, const SizeType capacityInElements) noexcept
    {
        shutdown();

        if (externalSlots == nullptr || capacityInElements < kMinJobQueueCapacity || capacityInElements > kMaxCapacity)
            [[unlikely]]
        {
            return JobQueueResult::InvalidArguments;
        }

        if ((capacityInElements & (capacityInElements - 1u)) != 0u) [[unlikely]]
        {
            return JobQueueResult::InvalidArguments;
        }

        if (!__builtin_is_constant_evaluated())
        {
            if (!gp::memory::isAligned(externalSlots, alignof(SlotType))) [[unlikely]]
            {
                return JobQueueResult::InvalidArguments;
            }
        }

        m_slots = externalSlots;
        m_capacity = capacityInElements;
        m_mask = capacityInElements - 1u;
        m_allocator = nullptr;
        m_ownsMemory = false;

        resetSequencesAndCursors();
        return JobQueueResult::Success;
    }

    /// @brief Releases any owned slot buffer and resets the queue to an uninitialized state.
    /// @note Must only be called when no threads are concurrently pushing or popping.
    constexpr void shutdown() noexcept
    {
        if (m_slots != nullptr)
        {
            if (!__builtin_is_constant_evaluated())
            {
                if (m_ownsMemory && m_allocator != nullptr)
                {
                    m_allocator->deallocate(m_slots);
                }
            }

            m_slots = nullptr;
            m_capacity = 0u;
            m_mask = 0u;
            m_allocator = nullptr;
            m_ownsMemory = false;
            m_enqueuePos.store(0u, gp::MemoryOrder::Relaxed);
            m_dequeuePos.store(0u, gp::MemoryOrder::Relaxed);
        }
    }

    /// @brief Re-initializes all slot sequence numbers (`m_slots[i].sequence = i`) and resets cursors to `0`.
    /// @note Must only be called when no threads are concurrently pushing or popping.
    constexpr void clear() noexcept
    {
        if (m_slots != nullptr)
        {
            resetSequencesAndCursors();
        }
    }

public:
    /// @section Multi-Producer & Multi-Consumer Operations

    /// @brief Enqueues an item at the tail of the ring buffer in FIFO order.
    /// @details Safe to call concurrently from any number of producer threads:
    ///          1. Loads `pos = m_enqueuePos.load(Relaxed)` and `seq = m_slots[pos & m_mask].sequence.load(Acquire)`.
    ///          2. Computes signed `diff = SignedDiffType(seq) - SignedDiffType(pos)`:
    ///             - `diff == 0`: Slot is available for this producer lap; attempts `m_enqueuePos.compareExchangeWeak`.
    ///             - `diff < 0`: Consumer has not yet freed this slot from the prior lap; returns
    ///               `JobQueueResult::Full`.
    ///             - `diff > 0`: Another producer already claimed `pos`; reloads `m_enqueuePos` and retries.
    ///          3. Writes `slot.payload = item` and commits `slot.sequence.store(pos + 1, Release)`.
    /// @param[in] item Element to copy into the claimed slot.
    /// @return `JobQueueResult::Success`, `JobQueueResult::Full`, or `JobQueueResult::NotInitialized`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr JobQueueResult tryPush(const ElementType& item) noexcept
    {
        if (m_slots == nullptr) [[unlikely]]
        {
            return JobQueueResult::NotInitialized;
        }

        SlotType* targetSlot = nullptr;
        SizeType pos = m_enqueuePos.load(gp::MemoryOrder::Relaxed);

        for (;;)
        {
            targetSlot = &m_slots[pos & m_mask];
            const SizeType seq = targetSlot->sequence.load(gp::MemoryOrder::Acquire);
            const SignedDiffType diff = static_cast<SignedDiffType>(seq) - static_cast<SignedDiffType>(pos);

            if (diff == 0) [[likely]]
            {
                if (m_enqueuePos.compareExchangeWeak(pos, pos + 1u, gp::MemoryOrder::Relaxed, gp::MemoryOrder::Relaxed))
                    [[likely]]
                {
                    break;
                }
            }
            else if (diff < 0)
            {
                return JobQueueResult::Full;
            }
            else
            {
                pos = m_enqueuePos.load(gp::MemoryOrder::Relaxed);
            }
        }

        targetSlot->payload = item;
        targetSlot->sequence.store(pos + 1u, gp::MemoryOrder::Release);
        return JobQueueResult::Success;
    }

    /// @brief Enqueues up to `count` items into the MPMC ring queue, stopping early if the ring becomes full.
    /// @param[in] items Pointer to the source array of `count` elements.
    /// @param[in] count Number of elements requested to enqueue.
    /// @param[out] outPushedCount Receives the actual number of elements enqueued (`0` to `count`).
    /// @return `JobQueueResult::Success` if all `count` items were pushed, `JobQueueResult::Full` if stopped early,
    ///         `JobQueueResult::InvalidArguments`, or `JobQueueResult::NotInitialized`.
    [[nodiscard]] constexpr JobQueueResult
        tryPushBatch(const ConstPointerType items, const SizeType count, SizeType& outPushedCount) noexcept
    {
        outPushedCount = 0u;

        if (m_slots == nullptr) [[unlikely]]
        {
            return JobQueueResult::NotInitialized;
        }

        if (count == 0u) [[unlikely]]
        {
            return JobQueueResult::Success;
        }

        if (items == nullptr) [[unlikely]]
        {
            return JobQueueResult::InvalidArguments;
        }

        for (SizeType i = 0u; i < count; ++i)
        {
            const JobQueueResult res = tryPush(items[i]);
            if (res != JobQueueResult::Success)
            {
                return res;
            }
            ++outPushedCount;
        }

        return JobQueueResult::Success;
    }

    /// @brief Dequeues the oldest item from the head of the ring buffer in FIFO order.
    /// @details Safe to call concurrently from any number of consumer threads:
    ///          1. Loads `pos = m_dequeuePos.load(Relaxed)` and `seq = m_slots[pos & m_mask].sequence.load(Acquire)`.
    ///          2. Computes signed `diff = SignedDiffType(seq) - SignedDiffType(pos + 1)`:
    ///             - `diff == 0`: Producer has committed this slot; attempts `m_dequeuePos.compareExchangeWeak`.
    ///             - `diff < 0`: Slot has not yet been written for this lap; returns `JobQueueResult::Empty`.
    ///             - `diff > 0`: Another consumer already claimed `pos`; reloads `m_dequeuePos` and retries.
    ///          3. Copies `outItem = slot.payload` and releases the slot for the next producer lap via
    ///             `slot.sequence.store(pos + m_mask + 1, Release)`.
    /// @param[out] outItem Destination receiving the dequeued element on `JobQueueResult::Success`.
    /// @return `JobQueueResult::Success`, `JobQueueResult::Empty`, or `JobQueueResult::NotInitialized`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr JobQueueResult tryPop(ElementType& outItem) noexcept
    {
        if (m_slots == nullptr) [[unlikely]]
        {
            return JobQueueResult::NotInitialized;
        }

        SlotType* targetSlot = nullptr;
        SizeType pos = m_dequeuePos.load(gp::MemoryOrder::Relaxed);

        for (;;)
        {
            targetSlot = &m_slots[pos & m_mask];
            const SizeType seq = targetSlot->sequence.load(gp::MemoryOrder::Acquire);
            const SignedDiffType diff = static_cast<SignedDiffType>(seq) - static_cast<SignedDiffType>(pos + 1u);

            if (diff == 0) [[likely]]
            {
                if (m_dequeuePos.compareExchangeWeak(pos, pos + 1u, gp::MemoryOrder::Relaxed, gp::MemoryOrder::Relaxed))
                    [[likely]]
                {
                    break;
                }
            }
            else if (diff < 0)
            {
                return JobQueueResult::Empty;
            }
            else
            {
                pos = m_dequeuePos.load(gp::MemoryOrder::Relaxed);
            }
        }

        outItem = targetSlot->payload;
        targetSlot->sequence.store(pos + m_mask + 1u, gp::MemoryOrder::Release);
        return JobQueueResult::Success;
    }

    /// @brief Dequeues up to `maxCount` items from the MPMC ring queue in FIFO order.
    /// @param[out] outItems Destination array capable of holding at least `maxCount` elements.
    /// @param[in] maxCount Maximum number of elements to dequeue.
    /// @param[out] outPoppedCount Receives the actual number of elements dequeued (`0` to `maxCount`).
    /// @return `JobQueueResult::Success` if at least 1 item was popped, `JobQueueResult::Empty`,
    ///         `JobQueueResult::InvalidArguments`, or `JobQueueResult::NotInitialized`.
    [[nodiscard]] constexpr JobQueueResult
        tryPopBatch(const PointerType outItems, const SizeType maxCount, SizeType& outPoppedCount) noexcept
    {
        outPoppedCount = 0u;

        if (m_slots == nullptr) [[unlikely]]
        {
            return JobQueueResult::NotInitialized;
        }

        if (outItems == nullptr || maxCount == 0u) [[unlikely]]
        {
            return JobQueueResult::InvalidArguments;
        }

        for (SizeType i = 0u; i < maxCount; ++i)
        {
            const JobQueueResult res = tryPop(outItems[outPoppedCount]);
            if (res != JobQueueResult::Success)
            {
                break;
            }
            ++outPoppedCount;
        }

        return (outPoppedCount > 0u) ? JobQueueResult::Success : JobQueueResult::Empty;
    }

public:
    /// @section State Queries

    /// @brief Returns the total slot capacity of the ring queue.
    /// @return Power-of-two capacity in elements, or `0` if uninitialized.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType capacity() const noexcept
    {
        return m_capacity;
    }

    /// @brief Checks whether the queue has an active slot buffer attached.
    /// @return True if initialized, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isValid() const noexcept
    {
        return m_slots != nullptr;
    }

    /// @brief Queries the approximate number of committed items currently in the queue.
    /// @return Approximate queued element count (`0` to `capacity()`).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType sizeApprox() const noexcept
    {
        const SizeType enqueuePos = m_enqueuePos.load(gp::MemoryOrder::Acquire);
        const SizeType dequeuePos = m_dequeuePos.load(gp::MemoryOrder::Acquire);
        const SignedDiffType diff = static_cast<SignedDiffType>(enqueuePos) - static_cast<SignedDiffType>(dequeuePos);
        if (diff <= 0)
        {
            return 0u;
        }
        const SizeType count = static_cast<SizeType>(diff);
        return (count > m_capacity) ? m_capacity : count;
    }

    /// @brief Queries whether the queue appears empty at the instant of evaluation.
    /// @return True if `sizeApprox() == 0`, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isEmptyApprox() const noexcept
    {
        return sizeApprox() == 0u;
    }

    /// @brief Queries whether the queue appears full at the instant of evaluation.
    /// @return True if initialized and `sizeApprox() >= capacity()`, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isFullApprox() const noexcept
    {
        return (m_capacity > 0u) && (sizeApprox() >= m_capacity);
    }

    /// @brief Queries the current monotonic `m_enqueuePos` cursor value for diagnostics and testing.
    /// @param[in] order Memory ordering constraint (defaults to `MemoryOrder::Acquire`).
    /// @return Unsigned 64-bit producer cursor.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType
        getEnqueueCursor(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return m_enqueuePos.load(order);
    }

    /// @brief Queries the current monotonic `m_dequeuePos` cursor value for diagnostics and testing.
    /// @param[in] order Memory ordering constraint (defaults to `MemoryOrder::Acquire`).
    /// @return Unsigned 64-bit consumer cursor.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType
        getDequeueCursor(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return m_dequeuePos.load(order);
    }

private:
    /// @brief Initializes each slot's sequence ticket to its index and zeroes `m_enqueuePos` / `m_dequeuePos`.
    GP_FORCEINLINE_HINT constexpr void resetSequencesAndCursors() noexcept
    {
        for (SizeType i = 0u; i < m_capacity; ++i)
        {
            m_slots[i].sequence.store(i, gp::MemoryOrder::Relaxed);
        }
        m_enqueuePos.store(0u, gp::MemoryOrder::Relaxed);
        m_dequeuePos.store(0u, gp::MemoryOrder::Relaxed);
    }

private:
    // Zone 1: Read-Only metadata (MESI State: Shared [S] across all Producer and Consumer cores)
    alignas(kJobQueueCacheLineSize) SlotType* m_slots{ nullptr };
    SizeType m_capacity{ 0u };
    SizeType m_mask{ 0u };
    gp::memory::Malloc* m_allocator{ nullptr };
    bool m_ownsMemory{ false };

    // Zone 2: Multi-Producer cursor zone (Mutated by Producer threads via compareExchangeWeak)
    alignas(kJobQueueCacheLineSize) Atomic<SizeType> m_enqueuePos{ 0u };

    // Zone 3: Multi-Consumer cursor zone (Mutated by Consumer threads via compareExchangeWeak)
    alignas(kJobQueueCacheLineSize) Atomic<SizeType> m_dequeuePos{ 0u };
};

static_assert(
    sizeof(MPMCRingQueue<JobQueueItem>) == 3u * kJobQueueCacheLineSize,
    "MPMCRingQueue must occupy 3 hardware cache lines (192 bytes: Read-Only, Enqueue Cursor, Dequeue Cursor)."
);
static_assert(
    alignof(MPMCRingQueue<JobQueueItem>) == kJobQueueCacheLineSize,
    "MPMCRingQueue must be aligned to a 64-byte hardware cache line."
);
static_assert(
    sizeof(MPMCRingQueue<JobQueueItem>::SlotType) == kJobQueueCacheLineSize,
    "MPMCRingQueue<JobQueueItem>::SlotType must occupy 1 hardware cache line (64 bytes) to prevent false sharing."
);
static_assert(
    alignof(MPMCRingQueue<JobQueueItem>::SlotType) == kJobQueueCacheLineSize,
    "MPMCRingQueue<JobQueueItem>::SlotType must be 64-byte cache-line aligned."
);

}   // namespace gp::thread
