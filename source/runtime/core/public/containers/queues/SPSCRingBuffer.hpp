// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/HardwareConstants.hpp"
#include "memory/Memory.hpp"
#include "threading/Atomic.hpp"

namespace gp
{

/// @brief Standard hardware L1 data cache-line size in bytes used to prevent false sharing between CPU cores.
inline constexpr gp::USize kCacheLineSize = gp::memory::HardwareConstants::cacheLineSize;

/// @brief Wait-free Single-Producer Single-Consumer (SPSC) ring buffer with shadow index caching.
/// @details Designed for zero-allocation hot paths, cache-line separation, and custom engine memory backends.
///          Supports both typed element queuing (SPSCRingBuffer<T>) and raw variable-length packet
///          routing (SPSCRingBuffer<gp::UInt8>).
/// @tparam T Element type stored in the ring buffer (cannot be const-qualified).
template <typename T>
class alignas(kCacheLineSize) SPSCRingBuffer final
{
    static_assert(!gp::concepts::IsConst<T>, "SPSCRingBuffer element type T cannot be const.");

public:
    /// @brief Type of elements stored in the queue.
    using ElementType = T;

    /// @brief Pointer to an element.
    using PointerType = T*;

    /// @brief Pointer to a const element.
    using ConstPointerType = const T*;

    /// @brief Unsigned integer size and index type.
    using SizeType = gp::USize;

    /// @brief Required memory alignment for the underlying buffer allocation.
    /// @details Ensures the allocated buffer is aligned to at least a full CPU cache line or the element alignment.
    static constexpr gp::UInt32 kBufferAlignment =
        (alignof(T) > kCacheLineSize) ? static_cast<gp::UInt32>(alignof(T)) : static_cast<gp::UInt32>(kCacheLineSize);

public:
    /// @brief Pointer to the contiguous backing storage buffer.
    alignas(kCacheLineSize) PointerType m_buffer{ nullptr };

    /// @brief Total allocated capacity in elements (always a power of two).
    SizeType m_capacity{ 0u };

    /// @brief Bitmask for wrapping indices (equal to m_capacity - 1).
    SizeType m_mask{ 0u };

    /// @brief Allocator used to allocate and free backing memory, or nullptr if externally provided.
    gp::memory::Malloc* m_allocator{ nullptr };

    /// @brief Indicates whether this instance owns and must free the backing buffer.
    bool m_ownsMemory{ false };

    /// @brief Atomic write cursor owned by the Producer thread.
    alignas(kCacheLineSize) gp::Atomic<SizeType> m_head{ 0u };

    /// @brief Cached shadow copy of m_tail read by the Producer to reduce cross-core cache invalidations.
    SizeType m_cachedTail{ 0u };

    /// @brief Atomic read cursor owned by the Consumer thread.
    alignas(kCacheLineSize) gp::Atomic<SizeType> m_tail{ 0u };

    /// @brief Cached shadow copy of m_head read by the Consumer to reduce cross-core cache invalidations.
    SizeType m_cachedHead{ 0u };

public:
    /// @brief Constructs an uninitialized ring buffer without performing allocations.
    /// @note Call initialize() or initializeWithExternalBuffer() before pushing elements.
    [[nodiscard]] constexpr SPSCRingBuffer() noexcept = default;

    /// @brief Constructs and allocates a ring buffer with the specified minimum capacity.
    /// @param[in] minCapacity Minimum element count (automatically rounded up to the nearest power of two).
    /// @param[in] allocator Optional memory allocator (defaults to the inline engine allocator).
    explicit SPSCRingBuffer(SizeType minCapacity, gp::memory::Malloc* allocator = nullptr) noexcept
    {
        (void)initialize(minCapacity, allocator);
    }

    /// @brief Constructs a ring buffer backed by pre-allocated external memory.
    /// @param[in] externalBuffer Pointer to pre-allocated memory aligned to at least alignof(T).
    /// @param[in] capacityInElements Exact capacity in elements (must be a power of two).
    SPSCRingBuffer(PointerType externalBuffer, SizeType capacityInElements) noexcept
    {
        (void)initializeWithExternalBuffer(externalBuffer, capacityInElements);
    }

    /// @brief Deleted copy constructor enforcing single ownership of lock-free state.
    SPSCRingBuffer(const SPSCRingBuffer&) = delete;

    /// @brief Deleted copy assignment operator enforcing single ownership of lock-free state.
    SPSCRingBuffer& operator=(const SPSCRingBuffer&) = delete;

    /// @brief Moves resources from another ring buffer into this instance.
    /// @param[in,out] other Ring buffer to move from (reset to uninitialized state).
    /// @warning Moving a ring buffer while threads are concurrently pushing or popping is undefined behavior.
    [[nodiscard]] SPSCRingBuffer(SPSCRingBuffer&& other) noexcept
        : m_buffer(other.m_buffer)
        , m_capacity(other.m_capacity)
        , m_mask(other.m_mask)
        , m_allocator(other.m_allocator)
        , m_ownsMemory(other.m_ownsMemory)
        , m_head(other.m_head.load(gp::MemoryOrder::Relaxed))
        , m_cachedTail(other.m_cachedTail)
        , m_tail(other.m_tail.load(gp::MemoryOrder::Relaxed))
        , m_cachedHead(other.m_cachedHead)
    {
        other.m_buffer = nullptr;
        other.m_capacity = 0u;
        other.m_mask = 0u;
        other.m_allocator = nullptr;
        other.m_ownsMemory = false;
        other.m_head.store(0u, gp::MemoryOrder::Relaxed);
        other.m_cachedTail = 0u;
        other.m_tail.store(0u, gp::MemoryOrder::Relaxed);
        other.m_cachedHead = 0u;
    }

    /// @brief Move assignment operator.
    /// @param[in,out] other Ring buffer to move from (reset to uninitialized state).
    /// @return Reference to this instance.
    /// @warning Moving a ring buffer while threads are concurrently pushing or popping is undefined behavior.
    SPSCRingBuffer& operator=(SPSCRingBuffer&& other) noexcept
    {
        if (this != &other)
        {
            shutdown();

            m_buffer = other.m_buffer;
            m_capacity = other.m_capacity;
            m_mask = other.m_mask;
            m_allocator = other.m_allocator;
            m_ownsMemory = other.m_ownsMemory;
            m_head.store(other.m_head.load(gp::MemoryOrder::Relaxed), gp::MemoryOrder::Relaxed);
            m_cachedTail = other.m_cachedTail;
            m_tail.store(other.m_tail.load(gp::MemoryOrder::Relaxed), gp::MemoryOrder::Relaxed);
            m_cachedHead = other.m_cachedHead;

            other.m_buffer = nullptr;
            other.m_capacity = 0u;
            other.m_mask = 0u;
            other.m_allocator = nullptr;
            other.m_ownsMemory = false;
            other.m_head.store(0u, gp::MemoryOrder::Relaxed);
            other.m_cachedTail = 0u;
            other.m_tail.store(0u, gp::MemoryOrder::Relaxed);
            other.m_cachedHead = 0u;
        }
        return *this;
    }

    /// @brief Destructor. Destroys active elements and releases owned memory.
    ~SPSCRingBuffer() noexcept
    {
        shutdown();
    }

public:
    /// @brief Allocates backing storage using the specified or default engine memory allocator.
    /// @param[in] minCapacity Minimum element count (automatically rounded up to a power of two, minimum 2).
    /// @param[in] allocator Custom allocator, or nullptr to use the inline engine allocator.
    /// @return True if allocation succeeded, false otherwise.
    [[nodiscard]] bool initialize(SizeType minCapacity, gp::memory::Malloc* allocator = nullptr) noexcept
    {
        shutdown();

        const SizeType adjustedCapacity = memory::roundUpToPowerOfTwo(minCapacity < 2u ? 2u : minCapacity);
        if (adjustedCapacity == 0u || adjustedCapacity > (~static_cast<SizeType>(0u) / sizeof(T))) [[unlikely]]
        {
            return false;
        }

        gp::memory::Malloc* targetAllocator = (allocator != nullptr) ? allocator : gp::memory::getGlobalMalloc();
        if (targetAllocator == nullptr) [[unlikely]]
        {
            return false;
        }

        const SizeType byteSize = adjustedCapacity * sizeof(T);
        void* rawMemory = targetAllocator->tryAllocate(byteSize, kBufferAlignment);
        if (rawMemory == nullptr) [[unlikely]]
        {
            return false;
        }

        m_buffer = static_cast<PointerType>(rawMemory);
        m_capacity = adjustedCapacity;
        m_mask = adjustedCapacity - 1u;
        m_allocator = targetAllocator;
        m_ownsMemory = true;

        resetIndices();
        return true;
    }

    /// @brief Configures the ring buffer to use a caller-provided memory block.
    /// @param[in] externalBuffer Pointer to pre-allocated memory aligned to at least alignof(T).
    /// @param[in] capacityInElements Exact element capacity (must be a power of two greater than or equal to 2).
    /// @return True if parameters are valid and buffer attached, false otherwise.
    [[nodiscard]] bool initializeWithExternalBuffer(PointerType externalBuffer, SizeType capacityInElements) noexcept
    {
        shutdown();

        if (externalBuffer == nullptr || capacityInElements < 2u) [[unlikely]]
        {
            return false;
        }

        if ((capacityInElements & (capacityInElements - 1u)) != 0u ||
            !gp::memory::isAligned(externalBuffer, alignof(T))) [[unlikely]]
        {
            return false;
        }

        m_buffer = externalBuffer;
        m_capacity = capacityInElements;
        m_mask = capacityInElements - 1u;
        m_allocator = nullptr;
        m_ownsMemory = false;

        resetIndices();
        return true;
    }

    /// @brief Destroys active elements and releases owned memory.
    void shutdown() noexcept
    {
        if (m_buffer != nullptr)
        {
            clear();

            if (m_ownsMemory && m_allocator != nullptr)
            {
                m_allocator->deallocate(m_buffer);
            }

            m_buffer = nullptr;
            m_capacity = 0u;
            m_mask = 0u;
            m_allocator = nullptr;
            m_ownsMemory = false;
        }
    }

    /// @brief Destroys all currently queued elements and resets head and tail indices.
    /// @warning Must only be called on the Consumer thread or when concurrent producer access is paused.
    void clear() noexcept
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            SizeType tail = m_tail.load(gp::MemoryOrder::Relaxed);
            const SizeType head = m_head.load(gp::MemoryOrder::Acquire);

            while (tail != head)
            {
                m_buffer[tail & m_mask].~T();
                ++tail;
            }
            m_tail.store(head, gp::MemoryOrder::Release);
            m_cachedHead = head;
        }
        else
        {
            const SizeType head = m_head.load(gp::MemoryOrder::Acquire);
            m_tail.store(head, gp::MemoryOrder::Release);
            m_cachedHead = head;
        }
    }

public:
    /// @brief Constructs an element directly in place at the head of the buffer.
    /// @tparam Args Types of constructor arguments forwarded to T.
    /// @param[in] args Constructor arguments forwarded to in-place construction.
    /// @return True if constructed and published, false if the buffer is full.
    template <typename... Args>
    requires std::is_constructible_v<T, Args...> [[nodiscard]] GP_FORCEINLINE_HINT bool
        tryEmplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        const SizeType head = m_head.load(gp::MemoryOrder::Relaxed);

        if ((head - m_cachedTail) == m_capacity) [[unlikely]]
        {
            m_cachedTail = m_tail.load(gp::MemoryOrder::Acquire);
            if ((head - m_cachedTail) == m_capacity) [[unlikely]]
            {
                return false;
            }
        }

        ::new (static_cast<void*>(m_buffer + (head & m_mask))) T(std::forward<Args>(args)...);

        m_head.store(head + 1u, gp::MemoryOrder::Release);
        return true;
    }

    /// @brief Copies an element into the ring buffer.
    /// @param[in] value Element to copy into the buffer.
    /// @return True if copied and published, false if the buffer is full.
    [[nodiscard]] GP_FORCEINLINE_HINT bool tryPush(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>)
    {
        return tryEmplace(value);
    }

    /// @brief Moves an element into the ring buffer.
    /// @param[in,out] value Element to move into the buffer.
    /// @return True if moved and published, false if the buffer is full.
    [[nodiscard]] GP_FORCEINLINE_HINT bool tryPush(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        return tryEmplace(std::move(value));
    }

    /// @brief Writes a contiguous sequence of trivially copyable elements into the buffer.
    /// @param[in] source Pointer to the array of elements to copy from.
    /// @param[in] count Number of elements to write.
    /// @return True if all elements were written, false if insufficient space exists.
    [[nodiscard]] GP_FORCEINLINE_HINT bool tryPushRange(ConstPointerType source, SizeType count) noexcept
        requires std::is_trivially_copyable_v<T>
    {
        if (count == 0u) [[unlikely]]
        {
            return true;
        }

        if (source == nullptr || !canWrite(count)) [[unlikely]]
        {
            return false;
        }

        const SizeType head = m_head.load(gp::MemoryOrder::Relaxed);
        writeSplitRing(head, source, count);

        m_head.store(head + count, gp::MemoryOrder::Release);
        return true;
    }

    /// @brief Writes a packet consisting of a fixed header and variable payload into a byte ring.
    /// @details Ensures the Consumer observes both header and payload together without partial visibility.
    /// @tparam HeaderType Trivially copyable struct type representing the packet descriptor.
    /// @param[in] header Header descriptor to write.
    /// @param[in] payload Pointer to payload bytes (can be nullptr if payloadSize is 0).
    /// @param[in] payloadSize Number of payload bytes following the header.
    /// @return True if the entire packet was written, false if insufficient space exists.
    template <typename HeaderType>
    requires(sizeof(T) == 1u && std::is_trivially_copyable_v<HeaderType>) [[nodiscard]] GP_FORCEINLINE_HINT bool
        tryWritePacket(const HeaderType& header, const void* payload, SizeType payloadSize) noexcept
    {
        const SizeType totalBytes = sizeof(HeaderType) + payloadSize;
        if (!canWrite(totalBytes)) [[unlikely]]
        {
            return false;
        }

        const SizeType head = m_head.load(gp::MemoryOrder::Relaxed);

        writeSplitRing(head, reinterpret_cast<ConstPointerType>(&header), sizeof(HeaderType));

        if (payloadSize > 0u && payload != nullptr) [[likely]]
        {
            writeSplitRing(head + sizeof(HeaderType), static_cast<ConstPointerType>(payload), payloadSize);
        }

        m_head.store(head + totalBytes, gp::MemoryOrder::Release);
        return true;
    }

    /// @brief Checks if at least the requested element count can be written by the Producer.
    /// @param[in] requiredCount Number of elements to test for available capacity.
    /// @return True if at least requiredCount slots are available, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool canWrite(SizeType requiredCount = 1u) noexcept
    {
        if (requiredCount > m_capacity) [[unlikely]]
        {
            return false;
        }

        const SizeType head = m_head.load(gp::MemoryOrder::Relaxed);
        SizeType available = m_capacity - (head - m_cachedTail);

        if (available < requiredCount) [[unlikely]]
        {
            m_cachedTail = m_tail.load(gp::MemoryOrder::Acquire);
            available = m_capacity - (head - m_cachedTail);
        }

        return available >= requiredCount;
    }

public:
    /// @brief Moves the front element from the ring buffer into outValue.
    /// @param[out] outValue Destination reference receiving the element.
    /// @return True if an element was extracted, false if the buffer is empty.
    [[nodiscard]] GP_FORCEINLINE_HINT bool
        tryPop(T& outValue) noexcept(std::is_nothrow_move_assignable_v<T> && std::is_nothrow_destructible_v<T>)
    {
        const SizeType tail = m_tail.load(gp::MemoryOrder::Relaxed);

        if (tail == m_cachedHead) [[unlikely]]
        {
            m_cachedHead = m_head.load(gp::MemoryOrder::Acquire);
            if (tail == m_cachedHead) [[unlikely]]
            {
                return false;
            }
        }

        PointerType slot = m_buffer + (tail & m_mask);
        outValue = std::move(*slot);

        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            slot->~T();
        }

        m_tail.store(tail + 1u, gp::MemoryOrder::Release);
        return true;
    }

    /// @brief Provides direct pointer access to the front element without advancing the read index.
    /// @return Pointer to the front element, or nullptr if the buffer is currently empty.
    /// @note The returned pointer remains valid until the Consumer thread calls tryPop() or advanceConsumer().
    [[nodiscard]] GP_FORCEINLINE_HINT PointerType peek() noexcept
    {
        const SizeType tail = m_tail.load(gp::MemoryOrder::Relaxed);

        if (tail == m_cachedHead) [[unlikely]]
        {
            m_cachedHead = m_head.load(gp::MemoryOrder::Acquire);
            if (tail == m_cachedHead) [[unlikely]]
            {
                return nullptr;
            }
        }

        return m_buffer + (tail & m_mask);
    }

    /// @brief Reads and consumes a contiguous sequence of trivially copyable elements.
    /// @param[out] destination Pointer to the output buffer receiving the elements.
    /// @param[in] count Number of elements to extract.
    /// @return True if count elements were read, false if fewer than count elements were available.
    [[nodiscard]] GP_FORCEINLINE_HINT bool tryPopRange(PointerType destination, SizeType count) noexcept
        requires std::is_trivially_copyable_v<T>
    {
        if (count == 0u) [[unlikely]]
        {
            return true;
        }

        if (destination == nullptr || !canRead(count)) [[unlikely]]
        {
            return false;
        }

        const SizeType tail = m_tail.load(gp::MemoryOrder::Relaxed);
        readSplitRing(tail, destination, count);

        m_tail.store(tail + count, gp::MemoryOrder::Release);
        return true;
    }

    /// @brief Copies elements at an offset relative to the read cursor without advancing the index.
    /// @param[in] relativeOffset Element offset from the current tail position.
    /// @param[out] destination Pointer to the output buffer receiving the copied elements.
    /// @param[in] count Number of elements to read.
    /// @return True if enough readable data exists, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool
        tryPeekAt(SizeType relativeOffset, void* destination, SizeType count) noexcept
        requires std::is_trivially_copyable_v<T>
    {
        if (count == 0u) [[unlikely]]
        {
            return true;
        }

        if (destination == nullptr || !canRead(relativeOffset + count)) [[unlikely]]
        {
            return false;
        }

        const SizeType tail = m_tail.load(gp::MemoryOrder::Relaxed);
        readSplitRing(tail + relativeOffset, static_cast<PointerType>(destination), count);
        return true;
    }

    /// @brief Advances the Consumer read cursor by the specified count after inspection via peek() or tryPeekAt().
    /// @param[in] count Number of elements to advance and mark as consumed.
    GP_FORCEINLINE_HINT void advanceConsumer(SizeType count = 1u) noexcept
    {
        const SizeType tail = m_tail.load(gp::MemoryOrder::Relaxed);

        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            for (SizeType i = 0u; i < count; ++i)
            {
                m_buffer[(tail + i) & m_mask].~T();
            }
        }

        m_tail.store(tail + count, gp::MemoryOrder::Release);
    }

    /// @brief Checks if at least the requested element count is ready for reading by the Consumer.
    /// @param[in] requiredCount Number of elements to test for available readable data.
    /// @return True if at least requiredCount elements are ready to read, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool canRead(SizeType requiredCount = 1u) noexcept
    {
        const SizeType tail = m_tail.load(gp::MemoryOrder::Relaxed);
        SizeType available = m_cachedHead - tail;

        if (available < requiredCount) [[unlikely]]
        {
            m_cachedHead = m_head.load(gp::MemoryOrder::Acquire);
            available = m_cachedHead - tail;
        }

        return available >= requiredCount;
    }

public:
    /// @brief Returns the total element capacity of the ring buffer.
    /// @return Capacity in elements.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType capacity() const noexcept
    {
        return m_capacity;
    }

    /// @brief Checks whether backing storage is currently allocated or attached.
    /// @return True if storage is active, false if uninitialized.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isValid() const noexcept
    {
        return m_buffer != nullptr;
    }

    /// @brief Queries an approximate element count currently in the buffer.
    /// @return Approximate number of queued elements.
    /// @note Concurrently active producer or consumer threads may alter this value before the caller inspects it.
    [[nodiscard]] GP_FORCEINLINE_HINT SizeType sizeApprox() const noexcept
    {
        const SizeType head = m_head.load(gp::MemoryOrder::Acquire);
        const SizeType tail = m_tail.load(gp::MemoryOrder::Acquire);
        return head - tail;
    }

    /// @brief Queries whether the buffer appears empty at the instant of evaluation.
    /// @return True if approximately empty, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isEmptyApprox() const noexcept
    {
        return sizeApprox() == 0u;
    }

private:
    /// @brief Resets head, tail, and shadow indices to zero.
    GP_FORCEINLINE_HINT void resetIndices() noexcept
    {
        m_head.store(0u, gp::MemoryOrder::Relaxed);
        m_cachedTail = 0u;
        m_tail.store(0u, gp::MemoryOrder::Relaxed);
        m_cachedHead = 0u;
    }

    /// @brief Writes elements into the circular buffer, splitting across boundary wraps if needed.
    /// @param[in] logicalIndex Starting logical write position.
    /// @param[in] source Pointer to the input data.
    /// @param[in] count Number of elements to write.
    GP_FORCEINLINE_HINT void writeSplitRing(SizeType logicalIndex, ConstPointerType source, SizeType count) noexcept
        requires std::is_trivially_copyable_v<T>
    {
        const SizeType physicalIndex = logicalIndex & m_mask;
        const SizeType untilEnd = m_capacity - physicalIndex;
        const SizeType firstChunk = (count <= untilEnd) ? count : untilEnd;
        const SizeType secondChunk = count - firstChunk;

        gp::memory::copyMemory(m_buffer + physicalIndex, source, firstChunk * sizeof(T));

        if (secondChunk > 0u) [[unlikely]]
        {
            gp::memory::copyMemory(m_buffer, source + firstChunk, secondChunk * sizeof(T));
        }
    }

    /// @brief Reads elements from the circular buffer, splitting across boundary wraps if needed.
    /// @param[in] logicalIndex Starting logical read position.
    /// @param[out] destination Pointer to the output buffer receiving the copied elements.
    /// @param[in] count Number of elements to read.
    GP_FORCEINLINE_HINT void readSplitRing(SizeType logicalIndex, PointerType destination, SizeType count) noexcept
        requires std::is_trivially_copyable_v<T>
    {
        const SizeType physicalIndex = logicalIndex & m_mask;
        const SizeType untilEnd = m_capacity - physicalIndex;
        const SizeType firstChunk = (count <= untilEnd) ? count : untilEnd;
        const SizeType secondChunk = count - firstChunk;

        gp::memory::copyMemory(destination, m_buffer + physicalIndex, firstChunk * sizeof(T));

        if (secondChunk > 0u) [[unlikely]]
        {
            gp::memory::copyMemory(destination + firstChunk, m_buffer, secondChunk * sizeof(T));
        }
    }
};

/// @brief Convenience alias for a raw byte-stream SPSC ring buffer used for packet routing.
using SPSCByteRingBuffer = SPSCRingBuffer<gp::UInt8>;

static_assert(
    sizeof(SPSCByteRingBuffer) == 3u * kCacheLineSize,
    "SPSCRingBuffer must occupy exactly 3 hardware cache lines (Read-Only, Producer, Consumer)."
);

}   // namespace gp
