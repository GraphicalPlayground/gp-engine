// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Concepts/CoreConcepts.hpp"
#include "Templates/Concepts/ObjectConcepts.hpp"
#include "Templates/Events/Event.hpp"
#include "Templates/Events/EventQueueInternals.hpp"

namespace GP
{

/// @brief Satisfied when @c TEventType provides a static, noexcept @c Coalesce method
///        that merges an incoming event into an existing one.
///
/// The method must update @p existing in-place to reflect the combined state of both events. The convention is that @p
/// existing retains its position in the queue (FIFO order) while absorbing data from @p incoming.
///
/// @par Contract
/// - Must be @c noexcept (called from a spinlock-protected, latency-critical path).
/// - After the call @p incoming is discarded; @p existing must be a valid, fully-initialised event ready for dispatch.
///
/// @par Example, mouse-move accumulation
/// @code
///   struct FMouseMovedEvent
///   {
///       FIntPoint2D position;   ///< Latest absolute cursor position.
///       FIntPoint2D delta;      ///< Accumulated movement since last dispatch.
///
///       /// @brief Absorb @p incoming: accumulate delta, keep the latest position.
///       static void Coalesce(FMouseMovedEvent& existing, const FMouseMovedEvent& incoming) noexcept
///       {
///           existing.delta    += incoming.delta;    // Sum all micro-movements.
///           existing.position  = incoming.position; // Overwrite with the latest position.
///       }
///   };
/// @endcode
///
/// @par Example, scroll wheel accumulation
/// @code
///   struct FMouseScrolledEvent
///   {
///       Float32 offsetX{ 0.0f };
///       Float32 offsetY{ 0.0f };
///
///       static void Coalesce(FMouseScrolledEvent& existing, const FMouseScrolledEvent& incoming) noexcept
///       {
///           existing.offsetX += incoming.offsetX;
///           existing.offsetY += incoming.offsetY;
///       }
///   };
/// @endcode
template <typename TEventType>
concept CCoalesceable = requires(TEventType& existing, const TEventType& incoming) {
    { TEventType::Coalesce(existing, incoming) } noexcept;
};

/// @brief Double-buffered, SPSC event queue with per-type coalescing.
///
/// Designed for the canonical AAA game-engine event pipeline where a platform
/// thread (producer) generates raw OS events at high frequency and a main thread
/// (consumer) dispatches them once per frame through the GP event system.
///
/// @par Threading model
///
/// @code
///   ┌──────────────────────────────────────────────────────────────────────────────┐
///   │  Platform Thread                     Main Thread                             │
///   │  ─────────────                       ────────────                            │
///   │  while (PollPlatformEvents())         // Frame N+1:                          │
///   │      queue.Emit(event, m_onFoo);      queue.Swap();     // Flip buffers      │
///   │                                       queue.Dispatch(); // Fire events       │
///   └──────────────────────────────────────────────────────────────────────────────┘
/// @endcode
///
///  - @b Emit()    - Thread-safe.  Any number of producer threads may call Emit()
///                   concurrently; a spinlock serialises writes to the current write buffer.
///  - @b Swap()    - Main thread only.  Atomically flips the write and read buffers.
///                   Blocks briefly until any in-progress Emit() finishes.
///  - @b Dispatch() - Main thread only.  Must be called after Swap().  Iterates the
///                   read buffer and broadcasts each event through its TEvent<>.
///
/// @par Double-buffer mechanics
///
/// Two @c FEventBuffer instances alternate roles each frame:
///
/// @code
///   Frame N:  Producer writes to Buffer A.   Consumer reads from Buffer B (Frame N-1 events).
///   Swap():   Buffer A becomes read buffer.   Buffer B is cleared and becomes write buffer.
///   Frame N+1: Producer writes to Buffer B.  Consumer reads from Buffer A (Frame N events).
/// @endcode
///
/// Clearing happens on the buffer that was previously read (not the one that was just
/// written), so dispatch can iterate the read buffer without any concurrent mutation.
///
/// @par Event coalescing
///
/// Event types that satisfy @c CCoalesceable are folded into a single record per frame
/// per dispatch target.  Successive @c Emit() calls for the same (type, TEvent target)
/// pair invoke @c TEventType::Coalesce(existing, incoming) in-place rather than
/// appending a new record.  Types that do not satisfy @c CCoalesceable are always
/// appended in push order.
///
/// @par Dispatch order
///
/// Events are dispatched in the order they were first pushed (FIFO).  Coalesced events
/// retain the position of their first push, not their last.  No intra-queue priority
/// reordering is applied; per-listener priority is handled by the TEvent bucket system.
///
/// @par Integration with IWindow
///
/// @code
///   class FWindowImpl : public IWindow
///   {
///   public:
///       /// Called from the platform thread during the OS event loop.
///       void HandleNativeResize(UInt32 w, UInt32 h)
///       {
///           FWindowResizedEvent ev{ m_cachedSize, FIntExtent2D{ w, h } };
///           m_eventQueue.Emit(ev, m_onResized);
///           m_cachedSize = ev.newSize;
///       }
///
///       /// Called from the main thread at the start of each frame.
///       void PollEvents()
///       {
///           PlatformPollNativeEvents(); // Drives HandleNative*() calls.
///           m_eventQueue.Swap();         // Flip: write buffer becomes read buffer.
///           m_eventQueue.Dispatch();     // Broadcast all buffered events.
///       }
///
///   private:
///       TEventQueue m_eventQueue;
///   };
/// @endcode
///
/// @note The class is non-copyable and non-movable.  Construct in-place and pass
///       references or pointers to collaborators.
class TEventQueue
{
public:
    ///
    /// @section Constants
    ///

    static constexpr SizeT kDefaultCapacity = Detail::kEventQueueDefaultCapacity;

private:
    ///
    /// @section Members
    ///

    Detail::FEventBuffer m_buffers[2];   ///< Ping-pong pair: index 0 and 1 alternate as write / read.
    TAtomic<UInt32> m_writeIdx{ 0u };    ///< Index of the buffer currently accepting Emit() calls.
    Detail::FSpinLock m_emitLock;        ///< Serialises concurrent Emit() calls and Swap() vs. Emit().

public:
    ///
    /// @section Constructors / Destructor
    ///

    /// @brief Constructs a queue and pre-allocates both buffer halves.
    /// @param capacityPerBuffer  Initial byte reservation for each buffer half.
    ///                           Sized to hold a typical frame's worth of events without reallocation.
    explicit TEventQueue(SizeT capacityPerBuffer = kDefaultCapacity) noexcept
    {
        m_buffers[0].Reserve(capacityPerBuffer);
        m_buffers[1].Reserve(capacityPerBuffer);
    }

    TEventQueue(const TEventQueue&) = delete;
    TEventQueue& operator=(const TEventQueue&) = delete;
    TEventQueue(TEventQueue&&) = delete;
    TEventQueue& operator=(TEventQueue&&) = delete;

    ~TEventQueue() noexcept = default;

public:
    ///
    /// @section Operators
    ///

    // No operators defined.  The explicit Emit/Swap/Dispatch API is preferred for clarity.

public:
    ///
    /// @section Methods - emission
    ///

    /// @brief Pushes an event into the write buffer, routing it through @p target on dispatch.
    ///
    /// Thread-safe.  May be called from any thread including concurrently; the internal
    /// spinlock serialises all writes to the same write buffer.
    ///
    /// If @c TEventType satisfies @c CCoalesceable and a record with the same
    /// (type, @p target) pair already exists in the current write buffer, the existing
    /// record is updated via @c TEventType::Coalesce and no new record is appended.
    ///
    /// All trampolines (dispatch, coalesce, destroy) are generated as static captureless
    /// function-pointer literals at compile time; zero heap allocation.
    ///
    /// @tparam TEventType  Concrete event struct.  Must be CopyConstructible.
    ///                     Opt-in to coalescing by adding a static Coalesce() method.
    /// @param  event        Event data to copy into the buffer.
    /// @param  target       The owning TEvent<void(const TEventType&)> that will
    ///                      broadcast the event on Dispatch().
    template <typename TEventType>
    void Emit(const TEventType& event, TEvent<void(const TEventType&)>& target)
    {
        // Compile-time dispatch trampoline.
        // Converts the type-erased void* pointers back to concrete types
        // and calls TEvent::Broadcast.  Generated once per TEventType; the
        // static function pointer is never heap-allocated.
        static constexpr Detail::FEventRecord::FDispatchFn kDispatchFn = [](const void* data, void* ctx) noexcept
        { static_cast<TEvent<void(const TEventType&)>*>(ctx)->Broadcast(*static_cast<const TEventType*>(data)); };

        // Compile-time coalesce trampoline (or nullptr if not applicable).
        Detail::FEventRecord::FCoalesceFn coalesceFn = nullptr;

        if constexpr (CCoalesceable<TEventType>)
        {
            static constexpr Detail::FEventRecord::FCoalesceFn kCoalesceFn =
                [](void* existing, const void* incoming) noexcept
            { TEventType::Coalesce(*static_cast<TEventType*>(existing), *static_cast<const TEventType*>(incoming)); };

            coalesceFn = kCoalesceFn;
        }

        // Compile-time destroy trampoline (or nullptr for trivial types).
        Detail::FEventRecord::FDestroyFn destroyFn = nullptr;

        if constexpr (!std::is_trivially_destructible_v<TEventType>)
        {
            static constexpr Detail::FEventRecord::FDestroyFn kDestroyFn = [](void* data) noexcept
            { static_cast<TEventType*>(data)->~TEventType(); };

            destroyFn = kDestroyFn;
        }

        // Push under spinlock.  Critical section is a single TArray
        // Resize + two placement-news; expected hold time < 100 ns.
        {
            Detail::FScopedSpinLock guard(m_emitLock);
            const UInt32 writeIdx = m_writeIdx.load(std::memory_order_relaxed);
            m_buffers[writeIdx].Push(event, kDispatchFn, &target, coalesceFn, destroyFn);
        }
    }

    /// @brief Pushes an event with a caller-supplied dispatch function.
    ///
    /// Use this overload when dispatching through a mechanism other than TEvent<>,
    /// for example to a custom bus, a scripting VM, or a network replication layer.
    ///
    /// Thread-safe (same spinlock as Emit()).
    ///
    /// @tparam TEventType Concrete event struct. Must be CopyConstructible.
    /// @param event Event data to copy into the buffer.
    /// @param dispatchFn Non-null function called during Dispatch() as @c dispatchFn(eventDataPtr, context).
    /// @param context Opaque pointer forwarded to @p dispatchFn unchanged.
    /// @param coalesceFn Optional merge function.  nullptr disables coalescing.
    template <typename TEventType>
    void EmitRaw(
        const TEventType& event,
        Detail::FEventRecord::FDispatchFn dispatchFn,
        void* context,
        Detail::FEventRecord::FCoalesceFn coalesceFn = nullptr
    )
    {
        GP_ASSERT(dispatchFn != nullptr, "TEventQueue::EmitRaw - dispatchFn must not be null.");

        Detail::FEventRecord::FDestroyFn destroyFn = nullptr;

        if constexpr (!CTriviallyDestructible<TEventType>)
        {
            static constexpr Detail::FEventRecord::FDestroyFn kDestroyFn = [](void* data) noexcept
            { static_cast<TEventType*>(data)->~TEventType(); };

            destroyFn = kDestroyFn;
        }

        Detail::FScopedSpinLock guard(m_emitLock);
        const UInt32 writeIdx = m_writeIdx.load(std::memory_order_relaxed);
        m_buffers[writeIdx].Push(event, dispatchFn, context, coalesceFn, destroyFn);
    }

    ///
    /// @section Methods - frame lifecycle
    ///

    /// @brief Atomically flips the write and read buffers.
    ///
    /// Call once per frame on the main thread, before Dispatch().  Blocks only
    /// long enough for any in-progress Emit() on the producer thread to finish
    /// (typically < 1 µs).
    ///
    /// @par What happens internally
    ///   1. Acquires the spinlock (waits for any in-progress Emit to release it).
    ///   2. Clears the buffer that is about to become the new write buffer.
    ///      This is the @e old read buffer; the consumer has already dispatched it,
    ///      so clearing is safe.
    ///   3. Publishes the new write index with release semantics so the Emit()
    ///      acquire on the producer thread sees the flip immediately.
    ///   4. Releases the spinlock.
    ///
    /// @warning Must not be called concurrently with Dispatch().
    void Swap() noexcept
    {
        Detail::FScopedSpinLock guard(m_emitLock);

        const UInt32 oldWriteIdx = m_writeIdx.load(std::memory_order_relaxed);
        const UInt32 newWriteIdx = 1u - oldWriteIdx;

        // Clear the buffer that is about to become the producer's sandbox.
        // The consumer finished reading it in the previous Dispatch() call.
        m_buffers[newWriteIdx].Clear();

        // Publish.  Release ordering ensures all prior Push() writes to
        // m_buffers[oldWriteIdx] are visible to any thread that reads with acquire.
        m_writeIdx.store(newWriteIdx, std::memory_order_release);
    }

    /// @brief Dispatches all events stored in the read buffer.
    ///
    /// Iterates every event record in push order and calls its dispatch trampoline,
    /// which broadcasts through the associated TEvent<>.  Coalesceable events emit
    /// exactly once per frame, reflecting their merged final state.
    ///
    /// The read buffer is NOT cleared after dispatch; it persists until the next
    /// Swap() call so it can be inspected for diagnostics if needed.
    ///
    /// @warning Must be called on the main thread, after Swap().
    void Dispatch()
    {
        // Acquire load ensures all prior Emit() writes (release via spinlock unlock)
        // are visible before we read the event data.
        const UInt32 readIdx = 1u - m_writeIdx.load(std::memory_order_acquire);
        m_buffers[readIdx].DispatchAll();
    }

    /// @brief Dispatches and immediately clears the write buffer without swapping.
    ///
    /// Intended for single-threaded scenarios (e.g. tools, tests, editor replay)
    /// where no double-buffering is required.  Simulates a Swap() + Dispatch()
    /// cycle on the same buffer in a single atomic step.
    ///
    /// @warning Not safe to call concurrently with Emit() on another thread.
    void FlushImmediate()
    {
        Detail::FScopedSpinLock guard(m_emitLock);
        const UInt32 writeIdx = m_writeIdx.load(std::memory_order_relaxed);
        m_buffers[writeIdx].DispatchAll();
        m_buffers[writeIdx].Clear();
    }

    /// @brief Discards all events currently in the write buffer without dispatching them.
    ///
    /// Use during shutdown or after a context reset when pending events must be
    /// silently dropped.  The read buffer is left intact.
    ///
    /// Thread-safe (acquires the spinlock).
    void ClearWriteBuffer() noexcept
    {
        Detail::FScopedSpinLock guard(m_emitLock);
        const UInt32 writeIdx = m_writeIdx.load(std::memory_order_relaxed);
        m_buffers[writeIdx].Clear();
    }

    /// @brief Discards all events in both buffer halves without dispatching them.
    ///
    /// @warning Must not be called while Dispatch() is executing.
    void ClearAll() noexcept
    {
        Detail::FScopedSpinLock guard(m_emitLock);
        m_buffers[0].Clear();
        m_buffers[1].Clear();
    }

    ///
    /// @section Methods - diagnostics / profiling
    ///

    /// @brief Returns the number of events pending in the read buffer (previous frame).
    GP_NODISCARD SizeT GetPendingCount() const noexcept
    {
        const UInt32 readIdx = 1u - m_writeIdx.load(std::memory_order_acquire);
        return m_buffers[readIdx].GetCount();
    }

    /// @brief Returns the number of events accumulated in the current write buffer.
    GP_NODISCARD SizeT GetWriteCount() const noexcept
    {
        const UInt32 writeIdx = m_writeIdx.load(std::memory_order_acquire);
        return m_buffers[writeIdx].GetCount();
    }

    /// @brief Returns the raw byte size of the read buffer's backing store.
    ///        Useful for tracking per-frame event volume in a profiler.
    GP_NODISCARD SizeT GetPendingByteSize() const noexcept
    {
        const UInt32 readIdx = 1u - m_writeIdx.load(std::memory_order_acquire);
        return m_buffers[readIdx].GetByteSize();
    }

    /// @brief Returns the raw byte size of the write buffer's backing store.
    GP_NODISCARD SizeT GetWriteByteSize() const noexcept
    {
        const UInt32 writeIdx = m_writeIdx.load(std::memory_order_acquire);
        return m_buffers[writeIdx].GetByteSize();
    }

    /// @brief Returns true if the read buffer contains at least one pending event.
    GP_NODISCARD GP_FORCEINLINE bool HasPendingEvents() const noexcept { return GetPendingCount() > 0u; }

    /// @brief Returns true if the write buffer contains no queued events.
    GP_NODISCARD GP_FORCEINLINE bool IsWriteBufferEmpty() const noexcept { return GetWriteCount() == 0u; }
};

}   // namespace GP
