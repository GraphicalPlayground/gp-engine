// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Relationship.hpp"
#include "CoreMinimal.hpp"
#include "memory/MemoryOrder.hpp"
#include "memory/pointers/PointerEnums.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/Fences.hpp"

namespace gp
{

/// @brief Tag type indicating that `RefCountedPtr` should adopt ownership of a raw pointer without incrementing its
///        reference count.
/// @details Used when wrapping a pointer whose reference count was already incremented by a factory or C/COM API
///          (e.g., `RefCountedPtr<T>(rawPtr, kAdoptRef)` or `adoptRef(rawPtr)`).
struct AdoptRefTag
{
    /// @brief Default constructor.
    explicit constexpr AdoptRefTag() noexcept = default;
};

/// @brief Tag constant for adopting an existing reference without calling `addRef()`.
inline constexpr AdoptRefTag kAdoptRef{};

/// @brief Tag type indicating that `RefCountedPtr` should increment the reference count when wrapping a raw pointer.
/// @details This is the default behavior for raw-pointer construction, made explicit via `kRetainRef` or `retainRef()`.
struct RetainRefTag
{
    /// @brief Default constructor.
    explicit constexpr RetainRefTag() noexcept = default;
};

/// @brief Tag constant for retaining a new reference by calling `addRef()`.
inline constexpr RetainRefTag kRetainRef{};

/// @brief Lock-free 32-bit bit-packed reference counter and RHI resource lifecycle state machine.
/// @details Packs a 30-bit unsigned reference count (`[0..29]`, up to 1,073,741,823 references) alongside two upper
///          lifecycle state bits (`RefStateFlags::MarkedForDelete` at bit 30 and `RefStateFlags::Deleting` at bit 31)
///          inside a single `gp::thread::Atomic<gp::UInt32>`.
///
///          This design—modeled after Unreal Engine 5 (`FRHIResource::FAtomicFlags`) and Frostbite/Decima deferred
///          deletion queues—solves multi-threaded ABA resurrection and double-enqueue bugs when RHI resources reach
///          zero CPU references and are placed into a frame-fenced GPU retirement queue:
///          1. When `release()` drops the count to `0`, calling `tryMarkForDelete()` atomically latches bit 30 and
///             returns `true` only for the first transition, preventing duplicate deletion-queue entries if a cache
///             briefly increments and decrements the resource (`0 -> 1 -> 0`) while queued.
///          2. When the GPU fence retires, the deletion queue calls `tryBeginDeleting()`. If the reference count was
///             resurrected (`> 0`) while queued, `tryBeginDeleting()` atomically clears `MarkedForDelete` and aborts
///             destruction; if the reference count is still `0`, it atomically latches `Deleting` (bit 31) in a single
///             compare-and-swap operation.
class AtomicRefStateCounter final
{
public:
    /// @brief Number of bits dedicated to the unsigned reference count.
    static constexpr gp::UInt32 kRefCountBits = 30u;

    /// @brief Bitmask isolating the 30-bit reference count (`0x3FFFFFFFu`).
    static constexpr gp::UInt32 kRefCountMask = (1u << kRefCountBits) - 1u;

    /// @brief Maximum simultaneous reference count representable in 30 bits (`1,073,741,823`).
    static constexpr gp::UInt32 kMaxRefCount = kRefCountMask;

    /// @brief Bitmask isolating the upper 2 lifecycle state bits (`0xC0000000u`).
    static constexpr gp::UInt32 kFlagsMask = ~kRefCountMask;

private:
    gp::thread::Atomic<gp::UInt32> m_packedState{ 0u };

public:
    /// @brief Constructs a counter initialized to zero references and `RefStateFlags::None`.
    [[nodiscard]] constexpr AtomicRefStateCounter() noexcept = default;

    /// @brief Constructs a counter with an explicit initial reference count and lifecycle flags.
    /// @param[in] initialRefCount Initial reference count (must be `<= kMaxRefCount`).
    /// @param[in] initialFlags Initial lifecycle state flags.
    [[nodiscard]] constexpr explicit AtomicRefStateCounter(
        const gp::UInt32 initialRefCount, const RefStateFlags initialFlags = RefStateFlags::None
    ) noexcept
        : m_packedState((initialRefCount & kRefCountMask) | static_cast<gp::UInt32>(initialFlags))
    {
        GP_ASSERT(initialRefCount <= kMaxRefCount, "Initial reference count exceeds 30-bit maximum.");
    }

    /// @brief Deleted copy and move operations; atomic reference state belongs to a fixed memory address.
    AtomicRefStateCounter(const AtomicRefStateCounter&) = delete;
    AtomicRefStateCounter& operator=(const AtomicRefStateCounter&) = delete;
    AtomicRefStateCounter(AtomicRefStateCounter&&) = delete;
    AtomicRefStateCounter& operator=(AtomicRefStateCounter&&) = delete;

    /// @brief Destructor.
    constexpr ~AtomicRefStateCounter() noexcept = default;

public:
    /// @brief Atomically increments the reference count using `MemoryOrder::Relaxed`.
    /// @details `Relaxed` ordering is sufficient because acquiring a new alias requires the caller to already hold
    ///          a valid reference to the object; no new memory payload is published to other threads on increment.
    /// @note Triggers an assertion if `RefStateFlags::Deleting` is set or if the 30-bit count overflows.
    /// @return The new reference count after incrementing (`>= 1`).
    GP_FORCEINLINE_HINT constexpr gp::UInt32 addRef() noexcept
    {
        const gp::UInt32 prevPacked = m_packedState.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
        const gp::UInt32 prevCount = prevPacked & kRefCountMask;

        GP_ASSERT(
            (prevPacked & static_cast<gp::UInt32>(RefStateFlags::Deleting)) == 0u,
            "Cannot increment reference count on an object with RefStateFlags::Deleting set."
        );
        GP_ASSERT(prevCount < kMaxRefCount, "AtomicRefStateCounter 30-bit reference count overflow.");

        return prevCount + 1u;
    }

    /// @brief Atomically increments the reference count only if the object is not in the terminal `Deleting` state.
    /// @details Used by RHI descriptor and PSO caches that hold non-owning raw pointers and need to safely resurrect
    ///          a resource that may be sitting in a deferred deletion queue (`refCount == 0`, `MarkedForDelete` set),
    ///          while failing cleanly if physical destruction (`Deleting`) has already begun.
    /// @return True if the reference count was incremented; false if `RefStateFlags::Deleting` is latched.
    [[nodiscard]] constexpr bool tryAddRefIfNotDeleting() noexcept
    {
        gp::UInt32 expected = m_packedState.load(gp::memory::MemoryOrder::Relaxed);
        for (;;)
        {
            if ((expected & static_cast<gp::UInt32>(RefStateFlags::Deleting)) != 0u)
            {
                return false;
            }

            const gp::UInt32 prevCount = expected & kRefCountMask;
            GP_ASSERT(prevCount < kMaxRefCount, "AtomicRefStateCounter 30-bit reference count overflow.");

            const gp::UInt32 desired = expected + 1u;
            if (m_packedState.compareExchangeWeak(
                    expected, desired, gp::memory::MemoryOrder::Acquire, gp::memory::MemoryOrder::Relaxed
                ))
            {
                return true;
            }
        }
    }

    /// @brief Atomically increments the reference count only if the object currently has at least one live reference
    ///        (`refCount > 0`) and neither `MarkedForDelete` nor `Deleting` is set.
    /// @details Provides strict weak-promotion semantics for observer tables that must never resurrect an object
    ///          once its reference count has reached zero.
    /// @return True if the reference count was incremented from a live state; false otherwise.
    [[nodiscard]] constexpr bool tryAddRefIfLive() noexcept
    {
        gp::UInt32 expected = m_packedState.load(gp::memory::MemoryOrder::Relaxed);
        for (;;)
        {
            const gp::UInt32 currentCount = expected & kRefCountMask;
            const gp::UInt32 currentFlags = expected & kFlagsMask;
            if (currentCount == 0u || currentFlags != 0u)
            {
                return false;
            }

            GP_ASSERT(currentCount < kMaxRefCount, "AtomicRefStateCounter 30-bit reference count overflow.");

            const gp::UInt32 desired = expected + 1u;
            if (m_packedState.compareExchangeWeak(
                    expected, desired, gp::memory::MemoryOrder::Acquire, gp::memory::MemoryOrder::Relaxed
                ))
            {
                return true;
            }
        }
    }

    /// @brief Atomically decrements the reference count using `MemoryOrder::Release`, issuing an `Acquire` thread
    ///        fence only when the reference count drops to zero.
    /// @details Non-final decrements (`N -> N - 1` where `N > 1`) only require `MemoryOrder::Release` so prior writes
    ///          made through the released alias are published. Only the final decrement (`1 -> 0`) requires an
    ///          `Acquire` barrier before running the destructor or `onZeroReferences()`. Avoiding `AcqRel` on every
    ///          decrement eliminates unnecessary acquire barriers on weakly-ordered architectures (ARM64).
    /// @note Triggers an assertion if the reference count prior to decrementing was already zero.
    /// @return The remaining reference count after decrementing (`0` when the last reference is released).
    GP_FORCEINLINE_HINT constexpr gp::UInt32 release() noexcept
    {
        const gp::UInt32 prevPacked = m_packedState.fetchSub(1u, gp::memory::MemoryOrder::Release);
        const gp::UInt32 prevCount = prevPacked & kRefCountMask;

        GP_ASSERT(prevCount > 0u, "AtomicRefStateCounter reference count underflow (release called at count 0).");

        if (prevCount == 1u)
        {
            gp::thread::atomicThreadFence(gp::memory::MemoryOrder::Acquire);
        }

        return prevCount - 1u;
    }

    /// @brief Atomically reads the current 30-bit reference count, masking out lifecycle state flags.
    /// @param[in] order Memory ordering constraint (defaults to `MemoryOrder::Relaxed`).
    /// @return Current reference count in `[0, kMaxRefCount]`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32
        getRefCount(const gp::memory::MemoryOrder order = gp::memory::MemoryOrder::Relaxed) const noexcept
    {
        return m_packedState.load(order) & kRefCountMask;
    }

    /// @brief Atomically reads the current upper 2-bit lifecycle state flags.
    /// @param[in] order Memory ordering constraint (defaults to `MemoryOrder::Acquire`).
    /// @return Active `RefStateFlags` bitmask.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr RefStateFlags
        getFlags(const gp::memory::MemoryOrder order = gp::memory::MemoryOrder::Acquire) const noexcept
    {
        return static_cast<RefStateFlags>(m_packedState.load(order) & kFlagsMask);
    }

    /// @brief Atomically reads the raw 32-bit packed word containing both reference count and flags.
    /// @param[in] order Memory ordering constraint (defaults to `MemoryOrder::Acquire`).
    /// @return Raw 32-bit packed value.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32
        getPackedValue(const gp::memory::MemoryOrder order = gp::memory::MemoryOrder::Acquire) const noexcept
    {
        return m_packedState.load(order);
    }

    /// @brief Checks whether `RefStateFlags::MarkedForDelete` is currently latched.
    /// @param[in] order Memory ordering constraint (defaults to `MemoryOrder::Acquire`).
    /// @return True if the object is currently marked for deferred deletion; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        isMarkedForDelete(const gp::memory::MemoryOrder order = gp::memory::MemoryOrder::Acquire) const noexcept
    {
        return (getFlags(order) & RefStateFlags::MarkedForDelete) != RefStateFlags::None;
    }

    /// @brief Checks whether `RefStateFlags::Deleting` is currently latched.
    /// @param[in] order Memory ordering constraint (defaults to `MemoryOrder::Acquire`).
    /// @return True if the object is actively undergoing final destruction; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        isDeleting(const gp::memory::MemoryOrder order = gp::memory::MemoryOrder::Acquire) const noexcept
    {
        return (getFlags(order) & RefStateFlags::Deleting) != RefStateFlags::None;
    }

    /// @brief Atomically latches `RefStateFlags::MarkedForDelete` to claim deferred-deletion queue insertion.
    /// @details Executes a wait-free `fetchOr` with `MemoryOrder::AcqRel`. Only the thread that transitions the flag
    ///          from unset to set receives `true`, guaranteeing that an RHI resource is never pushed into a deferred
    ///          deletion queue twice even if transient references (`0 -> 1 -> 0`) occur while queued.
    /// @return True if this call newly latched `MarkedForDelete` (caller must enqueue for deletion); false if the
    ///         object was already marked for deletion or is already deleting.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool tryMarkForDelete() noexcept
    {
        const gp::UInt32 flagBit = static_cast<gp::UInt32>(RefStateFlags::MarkedForDelete);
        const gp::UInt32 prevPacked = m_packedState.fetchOr(flagBit, gp::memory::MemoryOrder::AcqRel);
        const gp::UInt32 blockingFlags =
            static_cast<gp::UInt32>(RefStateFlags::MarkedForDelete | RefStateFlags::Deleting);

        return (prevPacked & blockingFlags) == 0u;
    }

    /// @brief Atomically clears `RefStateFlags::MarkedForDelete`.
    /// @details Used when an object is manually removed from a deferred deletion queue prior to destruction.
    GP_FORCEINLINE_HINT constexpr void clearMarkedForDelete() noexcept
    {
        const gp::UInt32 clearMask = ~static_cast<gp::UInt32>(RefStateFlags::MarkedForDelete);
        m_packedState.fetchAnd(clearMask, gp::memory::MemoryOrder::Release);
    }

    /// @brief Atomically transitions a queued object into the terminal `RefStateFlags::Deleting` state if and only if
    ///        its reference count is still zero; otherwise clears `MarkedForDelete` and aborts deletion.
    /// @details Invoked by the RHI deferred deletion queue when the protecting GPU fence completes:
    ///          - If `refCount == 0`, sets `RefStateFlags::Deleting` and returns `true` so the caller can safely invoke
    ///            `delete` or return the block to a pool allocator.
    ///          - If `refCount > 0` (the resource was resurrected while waiting in the queue), clears
    ///            `RefStateFlags::MarkedForDelete` and returns `false` so the resource remains live.
    /// @return True if `Deleting` was latched and physical destruction must proceed; false if resurrected.
    [[nodiscard]] constexpr bool tryBeginDeleting() noexcept
    {
        gp::UInt32 expected = m_packedState.load(gp::memory::MemoryOrder::Acquire);
        for (;;)
        {
            const gp::UInt32 currentCount = expected & kRefCountMask;
            if (currentCount > 0u)
            {
                const gp::UInt32 desired = expected & ~static_cast<gp::UInt32>(RefStateFlags::MarkedForDelete);
                if (m_packedState.compareExchangeWeak(
                        expected, desired, gp::memory::MemoryOrder::AcqRel, gp::memory::MemoryOrder::Acquire
                    ))
                {
                    return false;
                }
            }
            else
            {
                const gp::UInt32 desired = expected | static_cast<gp::UInt32>(RefStateFlags::Deleting);
                if (m_packedState.compareExchangeWeak(
                        expected, desired, gp::memory::MemoryOrder::AcqRel, gp::memory::MemoryOrder::Acquire
                    ))
                {
                    return true;
                }
            }
        }
    }
};

/// @brief Default intrusive reference-counting policy forwarding operations to `T::addRef()`, `T::release()`, and
///        `T::getRefCount()`.
/// @details Each static method is concept-constrained on `InElementType` so `gp::concepts::IsRefCounted<T>` accurately
///          rejects non-intrusive types at compile time. May be specialized or replaced per-type to wrap external
///          intrusive objects such as Direct3D 12 / DXGI COM interfaces (`IUnknown::AddRef()` / `IUnknown::Release()`).
/// @tparam InElementType Intrusive object type managed by `RefCountedPtr`.
template <typename InElementType>
struct DefaultRefCountPolicy
{
    /// @brief Increments the intrusive reference count on `ptr`.
    /// @param[in] ptr Non-null pointer to the managed object.
    static GP_FORCEINLINE_HINT constexpr void addRef(InElementType* ptr) noexcept requires requires(InElementType* p) {
        {
            p->addRef()
        } noexcept;
    }
    {
        ptr->addRef();
    }

    /// @brief Decrements the intrusive reference count on `ptr`, triggering destruction or deferred deletion at zero.
    /// @param[in] ptr Non-null pointer to the managed object.
    static GP_FORCEINLINE_HINT constexpr void release(InElementType* ptr) noexcept requires requires(InElementType* p) {
        {
            p->release()
        } noexcept;
    }
    {
        ptr->release();
    }

    /// @brief Queries the current reference count of `ptr`.
    /// @param[in] ptr Non-null pointer to the managed object.
    /// @return Current reference count.
    [[nodiscard]] static GP_FORCEINLINE_HINT constexpr gp::UInt32 getRefCount(const InElementType* ptr) noexcept
        requires requires(const InElementType* p) {
            {
                p->getRefCount()
            } noexcept -> concepts::IsConvertibleTo<gp::UInt32>;
        }
    {
        return static_cast<gp::UInt32>(ptr->getRefCount());
    }
};

}   // namespace gp
