// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/ContainerFwd.hpp"
#include "Container/Sequential/Array.hpp"
#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include "Math/Scalar/Max.hpp"
#include "Memory/AtomicOps.hpp"
#include "Memory/MemoryUtils.hpp"

namespace GP::Detail
{

/// @brief Byte alignment applied to every event slot in the flat buffer.
///        Must be a power of two and >= alignof(void*).
///        64 bytes == one x86/ARM cache-line, giving optimal prefetch behaviour when
///        iterating the dispatch loop.
static constexpr SizeT kEventRecordAlignment = 64u;

/// @brief Default per-buffer byte reservation.
///        Sized for ~60 window events at ~64 bytes each without reallocation.
static constexpr SizeT kEventQueueDefaultCapacity = 4096u;

/// @brief Intrusive header stored at the start of every event slot in the flat byte buffer.
///
/// Each slot has the layout:
/// @code
///   ┌────────────────────────────────────── kEventRecordAlignment ──────────────────────┐
///   │  FEventRecord (header)  │  [alignment pad]  │  TEventType data  │  [tail pad]     │
///   └───────────────────────────────────────────────────────────────────────────────────┘
/// @endcode
///
/// @c dataOffset is the byte distance from the first byte of this header to the first byte
/// of the event data.  @c totalSize covers the entire slot including tail padding, so the
/// walker can jump to the next slot with a single addition.
///
/// The struct is aligned to @c kEventRecordAlignment so that every slot is cache-line
/// aligned when the backing buffer's base allocation is sufficiently aligned.
struct alignas(kEventRecordAlignment) FEventRecord
{
public:
    ///
    /// @section Type aliases
    ///

    /// @brief Broadcasts the event data through its associated TEvent<>.
    ///        @p data    is a pointer to the TEventType stored in the slot.
    ///        @p context is the void*-erased TEvent<>* supplied at Emit() time.
    using FDispatchFn = void (*)(const void* data, void* context) noexcept;

    /// @brief Merges @p incoming into @p existing in-place.
    ///        Called when a second event of the same (type, target) pair is emitted
    ///        during the same frame.  nullptr when the type is not coalesceable.
    using FCoalesceFn = void (*)(void* existing, const void* incoming) noexcept;

    /// @brief Runs the in-place destructor for non-trivially-destructible event types.
    ///        nullptr for trivially destructible event types (skips the call).
    using FDestroyFn = void (*)(void* data) noexcept;

public:
    ///
    /// @section Members
    ///

    FDispatchFn dispatchFn{ nullptr };   //<! Non-null; invokes the event through its TEvent<> target.
    FCoalesceFn coalesceFn{ nullptr };   //<! Null when the event type does not opt in to coalescing.
    FDestroyFn destroyFn{ nullptr };     //<! Null for trivially destructible event types.
    void* context{ nullptr };            //<! Opaque pointer to the owning TEvent<> instance.
    const void* typeKey{ nullptr };      //<! Unique compile-time sentinel address; one per TEventType.
    UInt32 dataOffset{ 0u };             //<! Bytes from [this] to the start of the event data.
    UInt32 totalSize{ 0u };              //<! Byte size of this full slot (header + data + tail padding).
};

// Five pointer-width members (dispatch, coalesce, destroy, context, typeKey) plus two UInt32s
// total 48 logical bytes on LP64.  alignas(kEventRecordAlignment = 64) pads this to exactly
// one cache line (64 bytes), so sizeof(FEventRecord) == kEventRecordAlignment on all
// supported 64-bit ABIs.  kDataOffset therefore equals kEventRecordAlignment, placing event
// data at the start of the second cache line - optimal for hardware prefetch during dispatch.
static_assert(
    sizeof(FEventRecord) <= kEventRecordAlignment,
    "FEventRecord exceeds kEventRecordAlignment; increase kEventRecordAlignment or slim the header."
);

/// @brief Lightweight entry in the per-buffer coalesce index.
///
/// Maps a unique (typeKey, context) pair to the byte offset of the most-recently
/// pushed matching record so that FEventBuffer::Push() can locate and update it
/// in O(n_unique_coalesceable_types).  In practice this table holds < 16 entries.
struct FCoalesceEntry
{
    const void* typeKey{ nullptr };   //<! Static compile-time sentinel; unique per TEventType.
    void* context{ nullptr };         //<! TEvent<>* cast to void*; disambiguates identical types on different targets.
    SizeT offset{ 0u };               //<! Byte offset of the matching FEventRecord inside the backing buffer.
};

/// @brief Lightweight test-and-set spinlock.
///
/// Protects very short critical sections: a single buffer pointer read plus a
/// TArray<Byte>::Resize and two placement-news.  Not suitable for contended or
/// long-running critical sections.
///
/// @par Platform hints
/// The inner spin loop should use a CPU-pause instruction to reduce power
/// consumption and improve branch-prediction accuracy on x86 (PAUSE) and ARM (YIELD).
class FSpinLock
{
private:
    TAtomic<bool> m_flag{ false };

public:
    ///
    /// @section Constructors / Destructor
    ///

    FSpinLock() noexcept = default;

    FSpinLock(const FSpinLock&) = delete;
    FSpinLock& operator=(const FSpinLock&) = delete;

public:
    ///
    /// @section Methods
    ///

    /// @brief Spins until the lock is acquired.  Acquire semantics.
    GP_FORCEINLINE void Lock() noexcept
    {
        while (m_flag.exchange(true, std::memory_order_acquire)) { GP_CPU_PAUSE(); }
    }

    /// @brief Releases the lock.  Release semantics.
    GP_FORCEINLINE void Unlock() noexcept { m_flag.store(false, std::memory_order_release); }

    /// @brief Attempts to acquire the lock without spinning.
    /// @return true if the lock was acquired; false if it was already held.
    GP_NODISCARD GP_FORCEINLINE bool TryLock() noexcept { return !m_flag.exchange(true, std::memory_order_acquire); }
};

/// @brief RAII scope guard for FSpinLock.
class FScopedSpinLock
{
private:
    FSpinLock& m_lock;

public:
    explicit FScopedSpinLock(FSpinLock& lock) noexcept
        : m_lock(lock)
    {
        m_lock.Lock();
    }

    ~FScopedSpinLock() noexcept { m_lock.Unlock(); }

    FScopedSpinLock(const FScopedSpinLock&) = delete;
    FScopedSpinLock& operator=(const FScopedSpinLock&) = delete;
};

/// @brief One half of the double buffer used by TEventQueue.
///
/// Stores type-erased event blobs in a single flat, contiguous byte array.
/// Each blob is prefixed by an FEventRecord header that carries the dispatch,
/// coalesce, and destroy trampolines generated at Push() time.
///
/// A parallel @c m_coalesceIndex allows O(n_unique_types) lookup of existing
/// records during Push() so that coalesceable events can be merged in-place
/// rather than appended.
///
/// @par Memory layout per slot
/// @code
///   [FEventRecord | alignment-pad | TEventType-data | tail-pad-to-kEventRecordAlignment]
/// @endcode
///
/// All slot starts are @c kEventRecordAlignment aligned, so the dispatch loop
/// can walk the buffer without per-record alignment computation.
///
/// @warning Not thread-safe.  External synchronization (FSpinLock inside
///          TEventQueue) must be held for concurrent Push() or Clear() calls.
class FEventBuffer
{
public:
    ///
    /// @section Constants
    ///

    /// @brief Small-stack reservation for the coalesce index.  Avoids a heap
    ///        allocation for the common case of <= kCoalesceIndexInlineCapacity
    ///        distinct coalesceable event types per frame.
    static constexpr SizeT kCoalesceIndexInlineCapacity = 16u;

private:
    ///
    /// @section Members
    ///

    TArray<Byte> m_storage;                   //<! Flat byte backing store; grows as needed.
    TArray<FCoalesceEntry> m_coalesceIndex;   //<! Per-slot coalesce lookup table.
    SizeT m_count{ 0u };                      //<! Number of active (non-coalesced-away) records.

public:
    ///
    /// @section Constructors / Destructor
    ///

    FEventBuffer() noexcept = default;

    FEventBuffer(const FEventBuffer&) = delete;
    FEventBuffer& operator=(const FEventBuffer&) = delete;

    FEventBuffer(FEventBuffer&&) noexcept = default;
    FEventBuffer& operator=(FEventBuffer&&) noexcept = default;

    ~FEventBuffer() noexcept { Clear(); }

public:
    ///
    /// @section Methods - queries
    ///

    /// @brief Returns true when no event records are present.
    GP_NODISCARD GP_FORCEINLINE bool IsEmpty() const noexcept { return m_count == 0u; }

    /// @brief Returns the number of active event records.
    GP_NODISCARD GP_FORCEINLINE SizeT GetCount() const noexcept { return m_count; }

    /// @brief Returns the raw backing-store byte size (includes headers and padding).
    GP_NODISCARD GP_FORCEINLINE SizeT GetByteSize() const noexcept { return m_storage.Size(); }

    ///
    /// @section Methods - mutation
    ///

    /// @brief Pre-allocates backing-store capacity to reduce per-frame reallocation.
    /// @param bytes  Minimum byte capacity to reserve in the backing store.
    void Reserve(SizeT bytes) { m_storage.Reserve(bytes); }

    /// @brief Appends an event record to the buffer, or coalesces it into an existing
    ///        record of the same (type, context) pair when @p coalesceFn is non-null.
    ///
    /// @par Coalescing path (O(n_unique_coalesceable_types))
    ///   If an FCoalesceEntry with a matching (typeKey, context) pair already exists,
    ///   @p coalesceFn is called with a mutable pointer to the stored event data and
    ///   a const pointer to @p event, then the function returns without appending.
    ///
    /// @par Append path
    ///   A new slot is carved from the backing store with the layout described in the
    ///   class documentation.  An FCoalesceEntry is registered when @p coalesceFn is
    ///   non-null so future calls to Push() with the same type can coalesce.
    ///
    /// @tparam TEventType  Concrete event struct.  Must be copy-constructible.
    /// @param event Event data to copy into the buffer.
    /// @param dispatchFn Non-null trampoline that broadcasts the event.
    /// @param context Pointer to the owning TEvent<> instance.
    /// @param coalesceFn Merge function; nullptr disables coalescing for this push.
    /// @param destroyFn In-place destructor; nullptr for trivially destructible types.
    template <typename TEventType>
    void Push(
        const TEventType& event,
        FEventRecord::FDispatchFn dispatchFn,
        void* context,
        FEventRecord::FCoalesceFn coalesceFn,
        FEventRecord::FDestroyFn destroyFn
    )
    {
        GP_ASSERT(dispatchFn != nullptr, "FEventBuffer::Push - dispatchFn must not be null.");

        // One static token per TEventType template instantiation gives a unique address
        // that acts as a compile-time type identity key.  No RTTI required.
        static const UInt8 s_typeToken{ 0u };
        const void* const typeKey = &s_typeToken;

        // Coalesce: merge into the most-recent record of this type.
        if (coalesceFn != nullptr)
        {
            for (FCoalesceEntry& entry: m_coalesceIndex)
            {
                if (entry.typeKey == typeKey && entry.context == context)
                {
                    auto* header = reinterpret_cast<FEventRecord*>(m_storage.Data() + entry.offset);
                    void* existing = m_storage.Data() + entry.offset + header->dataOffset;
                    coalesceFn(existing, &event);
                    return;   // Merged; no new slot needed.
                }
            }
        }

        // Compute slot layout. All computation is constexpr; zero runtime overhead.

        // If the event requires stricter alignment than the record header, the slot
        // must be aligned to that requirement so that (slotStart + dataOffset) is
        // correctly aligned for TEventType placement-new.
        constexpr SizeT kDataAlign = alignof(TEventType);
        constexpr SizeT kSlotAlign = GP::Max(kDataAlign, kEventRecordAlignment);
        constexpr SizeT kHeaderSize = sizeof(FEventRecord);
        constexpr SizeT kDataOffset = AlignUp(kHeaderSize, kDataAlign);
        constexpr SizeT kDataSize = sizeof(TEventType);
        constexpr SizeT kSlotSize = AlignUp(kDataOffset + kDataSize, kSlotAlign);

        // Grow backing store and locate the new slot.
        const SizeT slotStart = AlignUp(m_storage.Size(), kSlotAlign);
        m_storage.Resize(slotStart + kSlotSize);

        // IMPORTANT: slotPtr is derived AFTER Resize() so it reflects any reallocation.
        Byte* const slotPtr = m_storage.Data() + slotStart;

        // Placement-new the header.
        auto* header = new (slotPtr) FEventRecord{};
        header->dispatchFn = dispatchFn;
        header->coalesceFn = coalesceFn;
        header->destroyFn = destroyFn;
        header->context = context;
        header->typeKey = typeKey;
        header->dataOffset = static_cast<UInt32>(kDataOffset);
        header->totalSize = static_cast<UInt32>(kSlotSize);

        // Placement-new the event data.
        new (slotPtr + kDataOffset) TEventType(event);

        // Register in the coalesce index (coalesceable types only).
        if (coalesceFn != nullptr)
        {
            FCoalesceEntry& ce = m_coalesceIndex.EmplaceBack();
            ce.typeKey = typeKey;
            ce.context = context;
            ce.offset = slotStart;
        }

        ++m_count;
    }

    /// @brief Invokes the dispatch function of every record in push order.
    ///
    /// The dispatch function calls @c TEvent::Broadcast() on the stored event data,
    /// which fans out to all registered listeners through the normal TEvent priority system.
    ///
    /// @note Does not clear the buffer.  Clearing is deferred to the next Swap() call
    ///       so that callers can inspect the read buffer after dispatch if required.
    void DispatchAll() const
    {
        SizeT offset = 0u;
        const SizeT total = m_storage.Size();

        while (offset < total)
        {
            offset = AlignUp(offset, kEventRecordAlignment);
            if (offset >= total) { break; }

            const auto* header = reinterpret_cast<const FEventRecord*>(m_storage.Data() + offset);
            const void* data = m_storage.Data() + offset + header->dataOffset;

            header->dispatchFn(data, header->context);

            offset += header->totalSize;
        }
    }

    /// @brief Runs in-place destructors on non-trivial event data and resets the buffer
    ///        to an empty state without releasing the backing allocation.
    ///
    /// Retaining the backing allocation means the next frame's pushes will not
    /// reallocate as long as the per-frame event volume stays within the high-water mark.
    void Clear() noexcept
    {
        SizeT offset = 0u;
        const SizeT total = m_storage.Size();

        while (offset < total)
        {
            offset = AlignUp(offset, kEventRecordAlignment);
            if (offset >= total) { break; }

            const auto* header = reinterpret_cast<const FEventRecord*>(m_storage.Data() + offset);

            if (header->destroyFn != nullptr)
            {
                void* data = m_storage.Data() + offset + header->dataOffset;
                header->destroyFn(data);
            }

            offset += header->totalSize;
        }

        // TArray::Clear() resets size to 0 but keeps the allocated capacity intact.
        m_storage.Clear();
        m_coalesceIndex.Clear();
        m_count = 0u;
    }
};

}   // namespace GP::Detail
