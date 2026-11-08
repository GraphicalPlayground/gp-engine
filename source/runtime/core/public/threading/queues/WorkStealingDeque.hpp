// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Memory.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "memory/Memory.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/Fences.hpp"
#include "threading/queues/JobQueueDesc.hpp"      // IWYU pragma: export
#include "threading/queues/JobQueueEnums.hpp"     // IWYU pragma: export
#include "threading/queues/JobQueueForward.hpp"   // IWYU pragma: export
#include "threading/queues/JobQueueTypes.hpp"     // IWYU pragma: export

namespace gp::thread
{

/// @brief Bounded, lock-free Single-Producer Multi-Consumer (SPMC) Chase-Lev work-stealing deque ([ADR-0051]).
/// @details Implements the Chase-Lev work-stealing algorithm under the Lê et al. (PPoPP 2013) weak memory model over a
///          fixed power-of-two circular buffer with 3-zone 64-byte MESI cache-line isolation ([ADR-0025]):
///          - **Owner Thread (`tryPushBottom` / `tryPopBottom` — LIFO):** Only the single owning worker thread may
///            invoke `tryPushBottom()`, `tryPushBottomRange()`, and `tryPopBottom()`. Local pushes execute a single
///            `MemoryOrder::Release` store with zero atomic Read-Modify-Write (RMW) instructions. Local pops execute
///            zero CAS instructions whenever more than one item is present (`top < bottom`), falling back to a single
///            `compareExchangeStrong` on `m_top` only when racing a thief for the final remaining item
///            (`top == bottom`).
///          - **Thief Threads (`tryStealTop` / `tryStealTopBatch` — FIFO):** Arbitrary idle worker threads may
///            concurrently invoke `tryStealTop()` to steal the oldest item at the top of the deque in FIFO order.
///            Returns `JobQueueResult::Contended` when a non-empty slot is lost to a competing CAS so thief schedulers
///            can distinguish an empty victim (`JobQueueResult::Empty`) from an active victim under contention.
/// @tparam InElementType Trivially copyable, non-const payload type satisfying `gp::thread::JobQueueElement`.
template <JobQueueElement InElementType>
class alignas(kJobQueueCacheLineSize) WorkStealingDeque final
{
public:
    /// @brief Element type stored in the work-stealing deque.
    using ElementType = InElementType;

    /// @brief Pointer to an element in the backing ring buffer.
    using PointerType = InElementType*;

    /// @brief Pointer to a const element in the backing ring buffer.
    using ConstPointerType = const InElementType*;

    /// @brief Unsigned integer type used for capacities and bitmasks.
    using SizeType = gp::USize;

    /// @brief Signed 64-bit monotonic cursor type used for `m_bottom` and `m_top`.
    /// @details Monotonic 64-bit signed indices never wrap in practice (>29 years at 10 billion ops/sec) and allow
    ///          signed `top > bottom` empty detection when the owner decrements `m_bottom` below `m_top`.
    using CursorType = gp::Int64;

    /// @brief Maximum supported power-of-two capacity (`2^30` elements) preventing signed cursor or byte overflow.
    static constexpr SizeType kMaxCapacity = (static_cast<SizeType>(1u) << 30u);

    /// @brief Required byte alignment for the allocated backing buffer (at least one 64-byte cache line).
    static constexpr gp::UInt32 kBufferAlignment = (alignof(ElementType) > kJobQueueCacheLineSize)
                                                       ? static_cast<gp::UInt32>(alignof(ElementType))
                                                       : static_cast<gp::UInt32>(kJobQueueCacheLineSize);

public:
    /// @brief Constructs an uninitialized work-stealing deque (`isValid() == false`, zero allocations).
    [[nodiscard]] constexpr WorkStealingDeque() noexcept = default;

    /// @brief Constructs and allocates a work-stealing deque with the specified minimum capacity.
    /// @param[in] minCapacity Minimum element capacity (automatically rounded up to a power of two).
    /// @param[in] allocator Optional custom memory allocator (`nullptr` uses `gp::memory::getGlobalMalloc()`).
    explicit WorkStealingDeque(const SizeType minCapacity, gp::memory::Malloc* const allocator = nullptr) noexcept
    {
        (void)initialize(minCapacity, allocator);
    }

    /// @brief Constructs a work-stealing deque backed by caller-provided external storage.
    /// @param[in] externalBuffer Pre-allocated element array aligned to at least `alignof(ElementType)`.
    /// @param[in] capacityInElements Exact capacity in elements (must be a power of two >= `kMinJobQueueCapacity`).
    constexpr WorkStealingDeque(const PointerType externalBuffer, const SizeType capacityInElements) noexcept
    {
        (void)initializeWithExternalBuffer(externalBuffer, capacityInElements);
    }

    WorkStealingDeque(const WorkStealingDeque&) = delete;
    WorkStealingDeque& operator=(const WorkStealingDeque&) = delete;

    /// @brief Move-constructs a work-stealing deque by transferring buffer ownership and cursor state.
    /// @param[in,out] other Source deque to move from (left in an uninitialized state).
    /// @note Moving a deque while threads are concurrently pushing, popping, or stealing is undefined behavior.
    [[nodiscard]] constexpr WorkStealingDeque(WorkStealingDeque&& other) noexcept
        : m_buffer(other.m_buffer)
        , m_capacity(other.m_capacity)
        , m_mask(other.m_mask)
        , m_allocator(other.m_allocator)
        , m_ownsMemory(other.m_ownsMemory)
        , m_bottom(other.m_bottom.load(gp::MemoryOrder::Relaxed))
        , m_top(other.m_top.load(gp::MemoryOrder::Relaxed))
    {
        other.m_buffer = nullptr;
        other.m_capacity = 0u;
        other.m_mask = 0u;
        other.m_allocator = nullptr;
        other.m_ownsMemory = false;
        other.m_bottom.store(0, gp::MemoryOrder::Relaxed);
        other.m_top.store(0, gp::MemoryOrder::Relaxed);
    }

    /// @brief Move-assigns a work-stealing deque, releasing any currently owned buffer first.
    /// @param[in,out] other Source deque to move from (left in an uninitialized state).
    /// @return Reference to `*this`.
    /// @note Moving a deque while threads are concurrently pushing, popping, or stealing is undefined behavior.
    constexpr WorkStealingDeque& operator=(WorkStealingDeque&& other) noexcept
    {
        if (this != &other)
        {
            shutdown();

            m_buffer = other.m_buffer;
            m_capacity = other.m_capacity;
            m_mask = other.m_mask;
            m_allocator = other.m_allocator;
            m_ownsMemory = other.m_ownsMemory;
            m_bottom.store(other.m_bottom.load(gp::MemoryOrder::Relaxed), gp::MemoryOrder::Relaxed);
            m_top.store(other.m_top.load(gp::MemoryOrder::Relaxed), gp::MemoryOrder::Relaxed);

            other.m_buffer = nullptr;
            other.m_capacity = 0u;
            other.m_mask = 0u;
            other.m_allocator = nullptr;
            other.m_ownsMemory = false;
            other.m_bottom.store(0, gp::MemoryOrder::Relaxed);
            other.m_top.store(0, gp::MemoryOrder::Relaxed);
        }
        return *this;
    }

    /// @brief Destroys the deque and releases any owned backing buffer.
    constexpr ~WorkStealingDeque() noexcept
    {
        shutdown();
    }

public:
    /// @section Lifecycle & Storage Management

    /// @brief Allocates backing storage for the deque using the specified or global engine allocator.
    /// @param[in] minCapacity Minimum element capacity (rounded up to a power of two >= `kMinJobQueueCapacity`).
    /// @param[in] allocator Optional custom `gp::memory::Malloc` backend (`nullptr` defaults to `getGlobalMalloc()`).
    /// @return `JobQueueResult::Success`, `JobQueueResult::InvalidArguments`, or `JobQueueResult::OutOfMemory`.
    [[nodiscard]] JobQueueResult initialize(
        const SizeType minCapacity = kDefaultWorkStealingCapacity, gp::memory::Malloc* const allocator = nullptr
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
            adjustedCapacity > (~static_cast<SizeType>(0u) / sizeof(ElementType))) [[unlikely]]
        {
            return JobQueueResult::InvalidArguments;
        }

        gp::memory::Malloc* const targetAllocator = (allocator != nullptr) ? allocator : gp::memory::getGlobalMalloc();
        if (targetAllocator == nullptr) [[unlikely]]
        {
            return JobQueueResult::OutOfMemory;
        }

        const SizeType totalBytes = adjustedCapacity * sizeof(ElementType);
        void* const rawMemory = targetAllocator->tryAllocate(totalBytes, kBufferAlignment);
        if (rawMemory == nullptr) [[unlikely]]
        {
            return JobQueueResult::OutOfMemory;
        }

        m_buffer = static_cast<PointerType>(rawMemory);
        m_capacity = adjustedCapacity;
        m_mask = adjustedCapacity - 1u;
        m_allocator = targetAllocator;
        m_ownsMemory = true;

        resetCursors();
        return JobQueueResult::Success;
    }

    /// @brief Attaches the deque to a caller-provided pre-allocated element buffer without heap allocation.
    /// @param[in] externalBuffer Pointer to an array of at least `capacityInElements` elements.
    /// @param[in] capacityInElements Exact element capacity (must be a power of two >= `kMinJobQueueCapacity`).
    /// @return `JobQueueResult::Success`, or `JobQueueResult::InvalidArguments` if null, misaligned, or not a power of
    ///         two.
    [[nodiscard]] constexpr JobQueueResult
        initializeWithExternalBuffer(const PointerType externalBuffer, const SizeType capacityInElements) noexcept
    {
        shutdown();

        if (externalBuffer == nullptr || capacityInElements < kMinJobQueueCapacity || capacityInElements > kMaxCapacity)
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
            if (!gp::memory::isAligned(externalBuffer, alignof(ElementType))) [[unlikely]]
            {
                return JobQueueResult::InvalidArguments;
            }
        }

        m_buffer = externalBuffer;
        m_capacity = capacityInElements;
        m_mask = capacityInElements - 1u;
        m_allocator = nullptr;
        m_ownsMemory = false;

        resetCursors();
        return JobQueueResult::Success;
    }

    /// @brief Releases any owned memory and resets the deque to an uninitialized state.
    /// @note Must only be called when no threads are concurrently accessing the deque.
    constexpr void shutdown() noexcept
    {
        if (m_buffer != nullptr)
        {
            if (!__builtin_is_constant_evaluated())
            {
                if (m_ownsMemory && m_allocator != nullptr)
                {
                    m_allocator->deallocate(m_buffer);
                }
            }

            m_buffer = nullptr;
            m_capacity = 0u;
            m_mask = 0u;
            m_allocator = nullptr;
            m_ownsMemory = false;
            resetCursors();
        }
    }

    /// @brief Discards all queued elements by synchronizing `m_top` and `m_bottom` to `0`.
    /// @note Must only be called on the Owner thread when no thieves are concurrently stealing.
    constexpr void clear() noexcept
    {
        resetCursors();
    }

public:
    /// @section Owner Operations (Single-Producer / Local LIFO Consumer)

    /// @brief Pushes an element onto the bottom of the deque in LIFO order.
    /// @details Executes zero atomic Read-Modify-Write (RMW) instructions: reads `m_bottom` (`Relaxed`) and `m_top`
    ///          (`Acquire`), writes the item into `m_buffer[bottom & m_mask]`, and publishes `m_bottom = bottom + 1`
    ///          with `MemoryOrder::Release`.
    /// @note Strictly restricted to the single owning worker thread.
    /// @param[in] item Element to copy into the bottom slot.
    /// @return `JobQueueResult::Success`, `JobQueueResult::Full` if `(bottom - top) >= capacity`, or
    ///         `JobQueueResult::NotInitialized`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr JobQueueResult tryPushBottom(const ElementType& item) noexcept
    {
        if (m_buffer == nullptr) [[unlikely]]
        {
            return JobQueueResult::NotInitialized;
        }

        const CursorType bottom = m_bottom.load(gp::MemoryOrder::Relaxed);
        const CursorType top = m_top.load(gp::MemoryOrder::Acquire);

        if ((bottom - top) >= static_cast<CursorType>(m_capacity)) [[unlikely]]
        {
            return JobQueueResult::Full;
        }

        m_buffer[static_cast<SizeType>(bottom) & m_mask] = item;
        m_bottom.store(bottom + 1, gp::MemoryOrder::Release);
        return JobQueueResult::Success;
    }

    /// @brief Pushes a contiguous batch of elements onto the bottom of the deque with a single atomic `Release` commit.
    /// @details Performs one `m_bottom` / `m_top` capacity check for all `count` items, writes the items into the
    ///          circular buffer, and issues a single `m_bottom.store(bottom + count, MemoryOrder::Release)` to publish
    ///          the entire batch to thieves at once.
    /// @note Strictly restricted to the single owning worker thread. All-or-nothing: if fewer than `count` free slots
    ///       remain, returns `JobQueueResult::Full` without modifying the deque.
    /// @param[in] items Pointer to the source array of `count` elements.
    /// @param[in] count Number of elements to push.
    /// @return `JobQueueResult::Success`, `JobQueueResult::Full`, `JobQueueResult::InvalidArguments`, or
    ///         `JobQueueResult::NotInitialized`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr JobQueueResult
        tryPushBottomRange(const ConstPointerType items, const SizeType count) noexcept
    {
        if (m_buffer == nullptr) [[unlikely]]
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

        if (count > m_capacity) [[unlikely]]
        {
            return JobQueueResult::Full;
        }

        const CursorType bottom = m_bottom.load(gp::MemoryOrder::Relaxed);
        const CursorType top = m_top.load(gp::MemoryOrder::Acquire);
        const CursorType available = static_cast<CursorType>(m_capacity) - (bottom - top);

        if (static_cast<CursorType>(count) > available) [[unlikely]]
        {
            return JobQueueResult::Full;
        }

        for (SizeType i = 0u; i < count; ++i)
        {
            m_buffer[static_cast<SizeType>(bottom + static_cast<CursorType>(i)) & m_mask] = items[i];
        }

        m_bottom.store(bottom + static_cast<CursorType>(count), gp::MemoryOrder::Release);
        return JobQueueResult::Success;
    }

    /// @brief Pops the most recently pushed element from the bottom of the deque in LIFO order.
    /// @details Implements the Lê et al. Chase-Lev owner pop protocol:
    ///          1. Decrements `b = m_bottom - 1`, stores `m_bottom = b` (`Relaxed`), and issues a `MemoryOrder::SeqCst`
    ///             StoreLoad thread fence so `m_bottom` is globally visible before loading `m_top`.
    ///          2. When `top < b` (at least 2 items were present), extracts `m_buffer[b & m_mask]` with **zero atomic
    ///             CAS instructions**.
    ///          3. When `top == b` (exactly 1 item remains), races potential thieves via a single
    ///             `m_top.compareExchangeStrong(top, top + 1, SeqCst, Relaxed)` and restores `m_bottom = b + 1`.
    /// @note Strictly restricted to the single owning worker thread.
    /// @param[out] outItem Destination receiving the popped element on `JobQueueResult::Success`.
    /// @return `JobQueueResult::Success`, `JobQueueResult::Empty` (including when the last item is stolen by a racing
    ///         thief), or `JobQueueResult::NotInitialized`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr JobQueueResult tryPopBottom(ElementType& outItem) noexcept
    {
        if (m_buffer == nullptr) [[unlikely]]
        {
            return JobQueueResult::NotInitialized;
        }

        const CursorType bottom = m_bottom.load(gp::MemoryOrder::Relaxed) - 1;
        m_bottom.store(bottom, gp::MemoryOrder::Relaxed);

        gp::thread::atomicThreadFence(gp::MemoryOrder::SeqCst);

        CursorType top = m_top.load(gp::MemoryOrder::Relaxed);
        if (top <= bottom) [[likely]]
        {
            const ElementType candidate = m_buffer[static_cast<SizeType>(bottom) & m_mask];
            if (top < bottom) [[likely]]
            {
                outItem = candidate;
                return JobQueueResult::Success;
            }

            // Exactly one element remained (top == bottom): race concurrent thieves for the final slot.
            const bool wonRace =
                m_top.compareExchangeStrong(top, top + 1, gp::MemoryOrder::SeqCst, gp::MemoryOrder::Relaxed);
            m_bottom.store(bottom + 1, gp::MemoryOrder::Relaxed);

            if (wonRace) [[likely]]
            {
                outItem = candidate;
                return JobQueueResult::Success;
            }

            return JobQueueResult::Empty;
        }

        // Deque was already empty prior to decrementing m_bottom: restore m_bottom to top (== bottom + 1).
        m_bottom.store(bottom + 1, gp::MemoryOrder::Relaxed);
        return JobQueueResult::Empty;
    }

public:
    /// @section Thief Operations (Multi-Consumer FIFO Steal)

    /// @brief Attempts to steal the oldest element from the top of the deque in FIFO order.
    /// @details Safe to call concurrently from any number of thief worker threads:
    ///          1. Loads `t = m_top.load(Acquire)`, emits a `MemoryOrder::SeqCst` fence, and loads
    ///             `b = m_bottom.load(Acquire)`.
    ///          2. If `t >= b`, returns `JobQueueResult::Empty`.
    ///          3. Speculatively copies `m_buffer[t & m_mask]` (safe because `ElementType` is trivially copyable) and
    ///             executes `m_top.compareExchangeStrong(t, t + 1, SeqCst, Relaxed)`.
    ///          4. If the CAS fails due to a competing thief or last-item owner pop, returns
    ///             `JobQueueResult::Contended`.
    /// @param[out] outItem Destination receiving the stolen element on `JobQueueResult::Success`.
    /// @return `JobQueueResult::Success`, `JobQueueResult::Empty`, `JobQueueResult::Contended`, or
    ///         `JobQueueResult::NotInitialized`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr JobQueueResult tryStealTop(ElementType& outItem) noexcept
    {
        if (m_buffer == nullptr) [[unlikely]]
        {
            return JobQueueResult::NotInitialized;
        }

        CursorType top = m_top.load(gp::MemoryOrder::Acquire);
        gp::thread::atomicThreadFence(gp::MemoryOrder::SeqCst);
        const CursorType bottom = m_bottom.load(gp::MemoryOrder::Acquire);

        if (top >= bottom)
        {
            return JobQueueResult::Empty;
        }

        const ElementType candidate = m_buffer[static_cast<SizeType>(top) & m_mask];
        if (!m_top.compareExchangeStrong(top, top + 1, gp::MemoryOrder::SeqCst, gp::MemoryOrder::Relaxed))
        {
            return JobQueueResult::Contended;
        }

        outItem = candidate;
        return JobQueueResult::Success;
    }

    /// @brief Steals up to `maxCount` elements (capped at half of the victim's currently queued items, minimum 1) from
    ///        the top of the deque in FIFO order.
    /// @details Preserves the Chase-Lev single-step `top -> top + 1` CAS invariant on each stolen element so
    ///          `tryPopBottom`'s uncontended fast path (`top < bottom`) remains 100% race-free.
    /// @param[out] outItems Destination array capable of holding at least `maxCount` elements.
    /// @param[in] maxCount Maximum number of elements to steal.
    /// @param[out] outStolenCount Receives the actual number of elements stolen (`0` to `maxCount`).
    /// @return `JobQueueResult::Success` if at least 1 item was stolen, `JobQueueResult::Empty`,
    ///         `JobQueueResult::Contended`, `JobQueueResult::InvalidArguments`, or `JobQueueResult::NotInitialized`.
    [[nodiscard]] constexpr JobQueueResult
        tryStealTopBatch(const PointerType outItems, const SizeType maxCount, SizeType& outStolenCount) noexcept
    {
        outStolenCount = 0u;

        if (m_buffer == nullptr) [[unlikely]]
        {
            return JobQueueResult::NotInitialized;
        }

        if (outItems == nullptr || maxCount == 0u) [[unlikely]]
        {
            return JobQueueResult::InvalidArguments;
        }

        const SizeType approxAvailable = sizeApprox();
        if (approxAvailable == 0u)
        {
            return JobQueueResult::Empty;
        }

        const SizeType halfAvailable = (approxAvailable > 1u) ? (approxAvailable / 2u) : 1u;
        const SizeType targetCount = (halfAvailable < maxCount) ? halfAvailable : maxCount;

        JobQueueResult lastResult = JobQueueResult::Empty;
        for (SizeType i = 0u; i < targetCount; ++i)
        {
            lastResult = tryStealTop(outItems[outStolenCount]);
            if (lastResult != JobQueueResult::Success)
            {
                break;
            }
            ++outStolenCount;
        }

        return (outStolenCount > 0u) ? JobQueueResult::Success : lastResult;
    }

public:
    /// @section State Queries

    /// @brief Returns the total element capacity of the backing ring buffer.
    /// @return Power-of-two capacity in elements, or `0` if uninitialized.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType capacity() const noexcept
    {
        return m_capacity;
    }

    /// @brief Checks whether the deque has an active backing buffer attached.
    /// @return True if initialized, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isValid() const noexcept
    {
        return m_buffer != nullptr;
    }

    /// @brief Queries the approximate number of elements currently in the deque.
    /// @return Approximate queued element count (`0` to `capacity()`).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType sizeApprox() const noexcept
    {
        const CursorType bottom = m_bottom.load(gp::MemoryOrder::Acquire);
        const CursorType top = m_top.load(gp::MemoryOrder::Acquire);
        const CursorType diff = bottom - top;
        if (diff <= 0)
        {
            return 0u;
        }
        const SizeType count = static_cast<SizeType>(diff);
        return (count > m_capacity) ? m_capacity : count;
    }

    /// @brief Queries whether the deque appears empty at the instant of evaluation.
    /// @return True if `sizeApprox() == 0`, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isEmptyApprox() const noexcept
    {
        return sizeApprox() == 0u;
    }

    /// @brief Queries whether the deque appears full at the instant of evaluation.
    /// @return True if initialized and `sizeApprox() >= capacity()`, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isFullApprox() const noexcept
    {
        return (m_capacity > 0u) && (sizeApprox() >= m_capacity);
    }

    /// @brief Queries the current monotonic `m_bottom` cursor value for diagnostics and testing.
    /// @param[in] order Memory ordering constraint (defaults to `MemoryOrder::Acquire`).
    /// @return Signed 64-bit bottom cursor.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr CursorType
        getBottomCursor(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return m_bottom.load(order);
    }

    /// @brief Queries the current monotonic `m_top` cursor value for diagnostics and testing.
    /// @param[in] order Memory ordering constraint (defaults to `MemoryOrder::Acquire`).
    /// @return Signed 64-bit top cursor.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr CursorType
        getTopCursor(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return m_top.load(order);
    }

private:
    /// @brief Resets `m_bottom` and `m_top` cursors to zero.
    GP_FORCEINLINE_HINT constexpr void resetCursors() noexcept
    {
        m_bottom.store(0, gp::MemoryOrder::Relaxed);
        m_top.store(0, gp::MemoryOrder::Relaxed);
    }

private:
    // Zone 1: Read-Only metadata (MESI State: Shared [S] across Owner and Thief cores)
    alignas(kJobQueueCacheLineSize) PointerType m_buffer{ nullptr };
    SizeType m_capacity{ 0u };
    SizeType m_mask{ 0u };
    gp::memory::Malloc* m_allocator{ nullptr };
    bool m_ownsMemory{ false };

    // Zone 2: Owner-Modified state (Mutated exclusively by the single owning worker thread)
    alignas(kJobQueueCacheLineSize) Atomic<CursorType> m_bottom{ 0 };

    // Zone 3: Thief-Modified state (CAS-mutated by Thief threads and last-element Owner pops)
    alignas(kJobQueueCacheLineSize) Atomic<CursorType> m_top{ 0 };
};

static_assert(
    sizeof(WorkStealingDeque<JobQueueItem>) == 3u * kJobQueueCacheLineSize,
    "WorkStealingDeque must occupy 3 hardware cache lines (192 bytes: Read-Only, Owner Bottom, Thief Top)."
);
static_assert(
    alignof(WorkStealingDeque<JobQueueItem>) == kJobQueueCacheLineSize,
    "WorkStealingDeque must be aligned to a 64-byte hardware cache line."
);

}   // namespace gp::thread
