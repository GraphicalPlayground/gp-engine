// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/pointers/PointerConcepts.hpp"
#include "memory/pointers/PointerEnums.hpp"
#include "memory/pointers/PointerTypes.hpp"

namespace gp
{

namespace detail
{

/// @brief Non-atomic counterpart to `AtomicRefStateCounter` for single-threaded intrusive objects.
/// @details Uses the identical 30-bit reference count + 2-bit `RefStateFlags` layout (`sizeof == 4` bytes) without
///          atomic memory barrier instructions.
class SingleThreadedRefStateCounter final
{
public:
    static constexpr gp::UInt32 kRefCountBits = AtomicRefStateCounter::kRefCountBits;
    static constexpr gp::UInt32 kRefCountMask = AtomicRefStateCounter::kRefCountMask;
    static constexpr gp::UInt32 kMaxRefCount = AtomicRefStateCounter::kMaxRefCount;
    static constexpr gp::UInt32 kFlagsMask = AtomicRefStateCounter::kFlagsMask;

private:
    gp::UInt32 m_packedState{ 0u };

public:
    /// @brief Constructs a counter initialized to zero references and `RefStateFlags::None`.
    [[nodiscard]] constexpr SingleThreadedRefStateCounter() noexcept = default;

    /// @brief Constructs a counter with an explicit initial reference count and lifecycle flags.
    /// @param[in] initialRefCount Initial reference count.
    /// @param[in] initialFlags Initial lifecycle state flags.
    [[nodiscard]] constexpr explicit SingleThreadedRefStateCounter(
        const gp::UInt32 initialRefCount, const RefStateFlags initialFlags = RefStateFlags::None
    ) noexcept
        : m_packedState((initialRefCount & kRefCountMask) | static_cast<gp::UInt32>(initialFlags))
    {
        GP_ASSERT(initialRefCount <= kMaxRefCount, "Initial reference count exceeds 30-bit maximum.");
    }

    SingleThreadedRefStateCounter(const SingleThreadedRefStateCounter&) = delete;
    SingleThreadedRefStateCounter& operator=(const SingleThreadedRefStateCounter&) = delete;
    SingleThreadedRefStateCounter(SingleThreadedRefStateCounter&&) = delete;
    SingleThreadedRefStateCounter& operator=(SingleThreadedRefStateCounter&&) = delete;

    /// @brief Destructor.
    constexpr ~SingleThreadedRefStateCounter() noexcept = default;

public:
    /// @brief Increments the reference count non-atomically.
    /// @return The new reference count after incrementing.
    GP_FORCEINLINE_HINT constexpr gp::UInt32 addRef() noexcept
    {
        const gp::UInt32 prevCount = m_packedState & kRefCountMask;
        GP_ASSERT(
            (m_packedState & static_cast<gp::UInt32>(RefStateFlags::Deleting)) == 0u,
            "Cannot increment reference count on an object with RefStateFlags::Deleting set."
        );
        GP_ASSERT(prevCount < kMaxRefCount, "SingleThreadedRefStateCounter 30-bit reference count overflow.");

        ++m_packedState;
        return prevCount + 1u;
    }

    /// @brief Increments the reference count only if `RefStateFlags::Deleting` is not set.
    /// @return True if incremented; false if `Deleting` is set.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool tryAddRefIfNotDeleting() noexcept
    {
        if ((m_packedState & static_cast<gp::UInt32>(RefStateFlags::Deleting)) != 0u)
        {
            return false;
        }

        const gp::UInt32 prevCount = m_packedState & kRefCountMask;
        GP_ASSERT(prevCount < kMaxRefCount, "SingleThreadedRefStateCounter 30-bit reference count overflow.");

        ++m_packedState;
        return true;
    }

    /// @brief Increments the reference count only if `refCount > 0` and no lifecycle flags are set.
    /// @return True if incremented from a live state; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool tryAddRefIfLive() noexcept
    {
        const gp::UInt32 currentCount = m_packedState & kRefCountMask;
        const gp::UInt32 currentFlags = m_packedState & kFlagsMask;
        if (currentCount == 0u || currentFlags != 0u)
        {
            return false;
        }

        GP_ASSERT(currentCount < kMaxRefCount, "SingleThreadedRefStateCounter 30-bit reference count overflow.");
        ++m_packedState;
        return true;
    }

    /// @brief Decrements the reference count non-atomically.
    /// @return The remaining reference count after decrementing.
    GP_FORCEINLINE_HINT constexpr gp::UInt32 release() noexcept
    {
        const gp::UInt32 prevCount = m_packedState & kRefCountMask;
        GP_ASSERT(
            prevCount > 0u, "SingleThreadedRefStateCounter reference count underflow (release called at count 0)."
        );

        --m_packedState;
        return prevCount - 1u;
    }

    /// @brief Reads the current 30-bit reference count.
    /// @return Current reference count.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 getRefCount() const noexcept
    {
        return m_packedState & kRefCountMask;
    }

    /// @brief Reads the current upper 2-bit lifecycle state flags.
    /// @return Active `RefStateFlags` bitmask.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr RefStateFlags getFlags() const noexcept
    {
        return static_cast<RefStateFlags>(m_packedState & kFlagsMask);
    }

    /// @brief Reads the raw 32-bit packed value.
    /// @return Raw 32-bit packed value.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 getPackedValue() const noexcept
    {
        return m_packedState;
    }

    /// @brief Checks whether `RefStateFlags::MarkedForDelete` is currently latched.
    /// @return True if marked for deletion; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isMarkedForDelete() const noexcept
    {
        return (getFlags() & RefStateFlags::MarkedForDelete) != RefStateFlags::None;
    }

    /// @brief Checks whether `RefStateFlags::Deleting` is currently latched.
    /// @return True if actively deleting; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isDeleting() const noexcept
    {
        return (getFlags() & RefStateFlags::Deleting) != RefStateFlags::None;
    }

    /// @brief Latches `RefStateFlags::MarkedForDelete` if neither `MarkedForDelete` nor `Deleting` is set.
    /// @return True if newly marked for deletion; false if already marked or deleting.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool tryMarkForDelete() noexcept
    {
        const gp::UInt32 blockingFlags =
            static_cast<gp::UInt32>(RefStateFlags::MarkedForDelete | RefStateFlags::Deleting);
        if ((m_packedState & blockingFlags) != 0u)
        {
            return false;
        }

        m_packedState |= static_cast<gp::UInt32>(RefStateFlags::MarkedForDelete);
        return true;
    }

    /// @brief Clears `RefStateFlags::MarkedForDelete`.
    GP_FORCEINLINE_HINT constexpr void clearMarkedForDelete() noexcept
    {
        m_packedState &= ~static_cast<gp::UInt32>(RefStateFlags::MarkedForDelete);
    }

    /// @brief Transitions to `RefStateFlags::Deleting` if `refCount == 0`; otherwise clears `MarkedForDelete`.
    /// @return True if `Deleting` was latched; false if resurrected (`refCount > 0`).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool tryBeginDeleting() noexcept
    {
        if ((m_packedState & kRefCountMask) > 0u)
        {
            m_packedState &= ~static_cast<gp::UInt32>(RefStateFlags::MarkedForDelete);
            return false;
        }

        m_packedState |= static_cast<gp::UInt32>(RefStateFlags::Deleting);
        return true;
    }
};

/// @brief Selects either `AtomicRefStateCounter` or `SingleThreadedRefStateCounter` based on `InThreadSafety`.
/// @tparam InThreadSafety Thread-safety policy enum.
template <RefCountThreadSafety InThreadSafety>
struct RefCounterSelector
{
    using Type = AtomicRefStateCounter;
};

/// @brief Specialization of `RefCounterSelector` for single-threaded reference counting.
template <>
struct RefCounterSelector<RefCountThreadSafety::SingleThreaded>
{
    using Type = SingleThreadedRefStateCounter;
};

}   // namespace detail

/// @brief Curiously Recurring Template Pattern (CRTP) mixin providing an embedded 32-bit reference and lifecycle
///        state counter.
/// @details Designed as the foundational base class for RHI resources (`RHIBuffer`, `RHITexture`, `RHIShader`),
///          pipeline states, and shared engine assets.
///
///          **Zero-Overhead Custom Deletion Interception (`onZeroReferences()`):**
///          When `release()` decrements the reference count from `1` to `0`:
///          - If `InDerivedType` defines `void onZeroReferences() noexcept`, `IntrusiveRefCounted` invokes it directly
///            via static dispatch. This allows `RHIResource` to call `tryMarkForDelete()` and push `this` onto a
///            frame-fenced GPU deferred deletion queue or return memory to a `PoolAllocator` without virtual call
///            overhead.
///          - Otherwise, `IntrusiveRefCounted` latches `RefStateFlags::Deleting` and invokes
///            `delete static_cast<const InDerivedType*>(this)`.
///
/// @note Copying or moving a derived object never copies or mutates the embedded reference count; a newly constructed
///       copy always begins with a reference count of `0`, and assignment preserves the target object's existing count.
/// @tparam InDerivedType Concrete derived class inheriting from `IntrusiveRefCounted`.
/// @tparam InThreadSafety Selects lock-free atomic (`ThreadSafe`, default) or non-atomic (`SingleThreaded`) counting.
template <typename InDerivedType, RefCountThreadSafety InThreadSafety = RefCountThreadSafety::ThreadSafe>
class IntrusiveRefCounted
{
public:
    /// @brief Underlying counter type (`AtomicRefStateCounter` or `SingleThreadedRefStateCounter`).
    using CounterType = typename detail::RefCounterSelector<InThreadSafety>::Type;

    /// @brief Compile-time flag indicating whether reference operations are thread-safe.
    static constexpr bool kIsThreadSafe = (InThreadSafety == RefCountThreadSafety::ThreadSafe);

private:
    mutable CounterType m_counter{};

public:
    /// @brief Default constructor; initializes the reference count to `0` and flags to `RefStateFlags::None`.
    constexpr IntrusiveRefCounted() noexcept = default;

    /// @brief Copy constructor; initializes the new object's reference count to `0` rather than copying `other`'s
    /// count.
    /// @param[in] other Source object being copied (its reference count is ignored).
    constexpr IntrusiveRefCounted([[maybe_unused]] const IntrusiveRefCounted& other) noexcept
        : m_counter()
    {}

    /// @brief Move constructor; initializes the new object's reference count to `0`.
    /// @param[in] other Source object being moved (its reference count is untouched).
    constexpr IntrusiveRefCounted([[maybe_unused]] IntrusiveRefCounted&& other) noexcept
        : m_counter()
    {}

    /// @brief Copy assignment operator; preserves `this` object's live reference count.
    /// @param[in] other Source object being copied.
    /// @return Reference to `*this`.
    constexpr IntrusiveRefCounted& operator=([[maybe_unused]] const IntrusiveRefCounted& other) noexcept
    {
        return *this;
    }

    /// @brief Move assignment operator; preserves `this` object's live reference count.
    /// @param[in] other Source object being moved.
    /// @return Reference to `*this`.
    constexpr IntrusiveRefCounted& operator=([[maybe_unused]] IntrusiveRefCounted&& other) noexcept
    {
        return *this;
    }

protected:
    /// @brief Protected non-virtual destructor preventing accidental `delete` through a raw `IntrusiveRefCounted*`
    ///        pointer. Destruction is routed through `InDerivedType*`.
    constexpr ~IntrusiveRefCounted() noexcept = default;

public:
    /// @brief Increments the intrusive reference count.
    /// @note Marked `const` so `RefCountedPtr<const InDerivedType>` can retain and release read-only resource handles.
    /// @return The new reference count after incrementing (`>= 1`).
    GP_FORCEINLINE_HINT constexpr gp::UInt32 addRef() const noexcept
    {
        return m_counter.addRef();
    }

    /// @brief Atomically increments the reference count only if `RefStateFlags::Deleting` is not set.
    /// @return True if the reference count was incremented; false if `Deleting` is latched.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool tryAddRefIfNotDeleting() const noexcept
    {
        return m_counter.tryAddRefIfNotDeleting();
    }

    /// @brief Atomically increments the reference count only if `getRefCount() > 0` and no deletion flags are set.
    /// @return True if the reference count was incremented from a live state; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool tryAddRefIfLive() const noexcept
    {
        return m_counter.tryAddRefIfLive();
    }

    /// @brief Decrements the intrusive reference count and triggers destruction or `onZeroReferences()` at zero.
    /// @return The remaining reference count after decrementing (`0` if destroyed or queued for deletion).
    GP_FORCEINLINE_HINT constexpr gp::UInt32 release() const noexcept
    {
        const gp::UInt32 remaining = m_counter.release();
        if (remaining == 0u)
        {
            if constexpr (gp::concepts::HasZeroReferencesHook<InDerivedType>)
            {
                const_cast<InDerivedType*>(static_cast<const InDerivedType*>(this))->onZeroReferences();
            }
            else
            {
                [[maybe_unused]] const bool canDelete = m_counter.tryBeginDeleting();
                GP_ASSERT(canDelete, "IntrusiveRefCounted reached zero references but failed to latch Deleting state.");
                delete static_cast<const InDerivedType*>(this);
            }
        }
        return remaining;
    }

    /// @brief Queries the current 30-bit reference count.
    /// @return Number of active `RefCountedPtr` handles referencing this object.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 getRefCount() const noexcept
    {
        return m_counter.getRefCount();
    }

    /// @brief Queries the active `RefStateFlags` (`MarkedForDelete`, `Deleting`) on the embedded counter.
    /// @return Current `RefStateFlags` bitmask.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr RefStateFlags getRefStateFlags() const noexcept
    {
        return m_counter.getFlags();
    }

    /// @brief Checks whether this object is currently marked for deferred deletion (`RefStateFlags::MarkedForDelete`).
    /// @return True if marked for deletion; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isMarkedForDelete() const noexcept
    {
        return m_counter.isMarkedForDelete();
    }

    /// @brief Checks whether this object is actively undergoing final destruction (`RefStateFlags::Deleting`).
    /// @return True if deleting; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isDeleting() const noexcept
    {
        return m_counter.isDeleting();
    }

    /// @brief Atomically latches `RefStateFlags::MarkedForDelete` to claim deferred deletion queue ownership.
    /// @return True if this call newly marked the object for deletion; false if already marked or deleting.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool tryMarkForDelete() const noexcept
    {
        return m_counter.tryMarkForDelete();
    }

    /// @brief Clears `RefStateFlags::MarkedForDelete` on the embedded counter.
    GP_FORCEINLINE_HINT constexpr void clearMarkedForDelete() const noexcept
    {
        m_counter.clearMarkedForDelete();
    }

    /// @brief Atomically latches `RefStateFlags::Deleting` if `getRefCount() == 0`; otherwise clears
    ///        `MarkedForDelete` and aborts destruction.
    /// @return True if `Deleting` was latched and physical destruction should proceed; false if resurrected.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool tryBeginDeleting() const noexcept
    {
        return m_counter.tryBeginDeleting();
    }
};

/// @brief Polymorphic abstract base class for reference-counted hierarchies requiring virtual destruction through base
///        interface pointers.
class IRefCounted : public IntrusiveRefCounted<IRefCounted, RefCountThreadSafety::ThreadSafe>
{
public:
    /// @brief Default constructor.
    constexpr IRefCounted() noexcept = default;

    /// @brief Virtual destructor enabling safe polymorphic deletion through `IRefCounted*`.
    virtual constexpr ~IRefCounted() noexcept = default;
};

}   // namespace gp
