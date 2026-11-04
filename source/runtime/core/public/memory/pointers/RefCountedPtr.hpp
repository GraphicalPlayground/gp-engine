// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Fundamental.hpp"
#include "concepts/Relationship.hpp"
#include "CoreMinimal.hpp"
#include "memory/allocators/AllocatorConcepts.hpp"
#include "memory/pointers/IntrusiveRefCounted.hpp"   // IWYU pragma: keep
#include "memory/pointers/PointerConcepts.hpp"
#include "memory/pointers/PointerEnums.hpp"          // IWYU pragma: keep
#include "memory/pointers/PointerTypes.hpp"
#include <compare>
#include <new>   // IWYU pragma: keep
#include <type_traits>
#include <utility>

namespace gp
{

/// @brief An 8-byte intrusive smart pointer managing objects with an embedded reference count.
/// @details Designed as the primary smart pointer for the Rendering Hardware Interface (`gp::rhi`) and performance-
///          critical engine subsystems (inspired by Unreal Engine `TRefCountPtr`, EA Frostbite `eastl::intrusive_ptr`,
///          and O3DE Atom `AZStd::intrusive_ptr`).
///
///          Unlike `SharedPtr<T>`, which allocates a separate heap control block and occupies 16 bytes (two pointers),
///          `RefCountedPtr<T>` stores only a single raw pointer (`sizeof(RefCountedPtr<T>) == sizeof(T*) == 8` bytes):
///          - **Single-Register ABI:** Passes in a single CPU register across dynamic RHI plugin boundaries.
///          - **Cache Locality:** The reference counter (`AtomicRefStateCounter`) resides directly inside `T`'s header
///            cache line alongside its virtual table and metadata.
///          - **Idempotent Raw-Pointer Round-Tripping:** Raw `T*` pointers passed to low-level C or RHI command
///            contexts can be safely re-wrapped into a `RefCountedPtr<T>` at any time without risking duplicate
///            control blocks.
///          - **Forward-Declaration Friendly:** Class declarations may embed `RefCountedPtr<IncompleteType>` members
///            using forward headers (`RHIForward.hpp`, `MemoryForward.hpp`) as long as copy/destruction definitions
///            see the complete type.
///
/// @tparam InElementType Pointee type managed by this smart pointer.
/// @tparam InPolicy Static policy type providing `addRef`, `release`, and optional `getRefCount` operations.
/// @see IntrusiveRefCounted, AtomicRefStateCounter, UniquePtr, SharedPtr
template <typename InElementType, typename InPolicy = DefaultRefCountPolicy<InElementType>>
class RefCountedPtr
{
private:
    template <typename InOtherType, typename InOtherPolicy>
    friend class RefCountedPtr;

public:
    /// @brief Underlying element type managed by this pointer.
    using ElementType = InElementType;

    /// @brief Raw pointer type to `InElementType`.
    using PointerType = InElementType*;

    /// @brief Reference-counting policy governing `addRef` and `release`.
    using PolicyType = InPolicy;

private:
    PointerType m_ptr{ nullptr };

public:
    /// @brief Default constructor; initializes a null `RefCountedPtr` with zero atomic operations.
    constexpr RefCountedPtr() noexcept = default;

    /// @brief Constructs a null `RefCountedPtr` from `nullptr`.
    /// @param[in] nullValue Null pointer literal.
    constexpr explicit(false) RefCountedPtr([[maybe_unused]] gp::NullPtrT nullValue) noexcept
        : m_ptr(nullptr)
    {}

    /// @brief Constructs a `RefCountedPtr` from a raw pointer and increments its reference count (`kRetainRef`).
    /// @tparam InOtherType Pointer-convertible source type.
    /// @param[in] ptr Raw pointer to retain (may be `nullptr`).
    /// @param[in] tag Explicit retain tag (defaults to `kRetainRef`).
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr explicit(false) RefCountedPtr(
        InOtherType* ptr, [[maybe_unused]] const RetainRefTag tag = kRetainRef
    ) noexcept requires concepts::IsRefCounted<InElementType, InPolicy>
        : m_ptr(ptr)
    {
        if (m_ptr != nullptr)
        {
            InPolicy::addRef(m_ptr);
        }
    }

    /// @brief Constructs a `RefCountedPtr` by adopting an existing reference on `ptr` without calling `addRef()`.
    /// @details Use when `ptr` already has an incremented reference count (e.g., returned by a COM factory or
    ///          detached from another handle).
    /// @tparam InOtherType Pointer-convertible source type.
    /// @param[in] ptr Raw pointer whose reference is adopted (may be `nullptr`).
    /// @param[in] tag Adopt tag (`kAdoptRef`).
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr RefCountedPtr(InOtherType* ptr, [[maybe_unused]] const AdoptRefTag tag) noexcept
        requires concepts::IsRefCounted<InElementType, InPolicy>
        : m_ptr(ptr)
    {}

    /// @brief Copy constructor; retains a new reference to the object managed by `other`.
    /// @param[in] other Source `RefCountedPtr` to copy.
    constexpr RefCountedPtr(const RefCountedPtr& other) noexcept
        : m_ptr(other.m_ptr)
    {
        static_assert(
            concepts::IsRefCounted<InElementType, InPolicy>,
            "RefCountedPtr requires InElementType and InPolicy to satisfy gp::concepts::IsRefCounted."
        );

        if (m_ptr != nullptr)
        {
            InPolicy::addRef(m_ptr);
        }
    }

    /// @brief Converting copy constructor; retains a new reference from a compatible `RefCountedPtr<InOtherType>`.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @tparam InOtherPolicy Policy type of the source smart pointer.
    /// @param[in] other Source `RefCountedPtr` to copy.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType, typename InOtherPolicy>
    constexpr explicit(false) RefCountedPtr(const RefCountedPtr<InOtherType, InOtherPolicy>& other) noexcept
        requires concepts::IsRefCounted<InElementType, InPolicy>
        : m_ptr(other.m_ptr)
    {
        if (m_ptr != nullptr)
        {
            InPolicy::addRef(m_ptr);
        }
    }

    /// @brief Move constructor; transfers ownership from `other` with zero reference-count operations.
    /// @param[in,out] other Source `RefCountedPtr` to move from; reset to `nullptr`.
    constexpr RefCountedPtr(RefCountedPtr&& other) noexcept
        : m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }

    /// @brief Converting move constructor; transfers ownership from a compatible `RefCountedPtr<InOtherType>` with
    ///        zero reference-count operations.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @tparam InOtherPolicy Policy type of the source smart pointer.
    /// @param[in,out] other Source `RefCountedPtr` to move from; reset to `nullptr`.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType, typename InOtherPolicy>
    constexpr explicit(false) RefCountedPtr(RefCountedPtr<InOtherType, InOtherPolicy>&& other) noexcept
        : m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }

    /// @brief Destructor; decrements the reference count of the managed object if non-null.
    /// @details Nullifies `m_ptr` prior to invoking `InPolicy::release(oldPtr)` to guarantee reentrancy safety.
    GP_FORCEINLINE_HINT constexpr ~RefCountedPtr() noexcept
    {
        static_assert(
            concepts::IsRefCounted<InElementType, InPolicy>,
            "RefCountedPtr requires InElementType and InPolicy to satisfy gp::concepts::IsRefCounted."
        );

        PointerType oldPtr = m_ptr;
        m_ptr = nullptr;
        if (oldPtr != nullptr)
        {
            InPolicy::release(oldPtr);
        }
    }

public:
    /// @brief Copy assignment operator; retains `rhs` before releasing the previously managed object.
    /// @details Incrementing `rhs.m_ptr` prior to releasing `m_ptr` guarantees safety under both direct
    ///          self-assignment (`ptr = ptr`) and indirect hierarchical aliasing (`node = node->child`).
    /// @param[in] rhs Source `RefCountedPtr` to copy.
    /// @return Reference to `*this`.
    constexpr RefCountedPtr& operator=(const RefCountedPtr& rhs) noexcept
    {
        static_assert(
            concepts::IsRefCounted<InElementType, InPolicy>,
            "RefCountedPtr requires InElementType and InPolicy to satisfy gp::concepts::IsRefCounted."
        );

        PointerType newPtr = rhs.m_ptr;
        if (newPtr != nullptr)
        {
            InPolicy::addRef(newPtr);
        }

        PointerType oldPtr = m_ptr;
        m_ptr = newPtr;
        if (oldPtr != nullptr)
        {
            InPolicy::release(oldPtr);
        }

        return *this;
    }

    /// @brief Converting copy assignment operator from a compatible `RefCountedPtr<InOtherType>`.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @tparam InOtherPolicy Policy type of the source smart pointer.
    /// @param[in] rhs Source `RefCountedPtr` to copy.
    /// @return Reference to `*this`.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType, typename InOtherPolicy>
    constexpr RefCountedPtr& operator=(const RefCountedPtr<InOtherType, InOtherPolicy>& rhs) noexcept
        requires concepts::IsRefCounted<InElementType, InPolicy>
    {
        PointerType newPtr = rhs.m_ptr;
        if (newPtr != nullptr)
        {
            InPolicy::addRef(newPtr);
        }

        PointerType oldPtr = m_ptr;
        m_ptr = newPtr;
        if (oldPtr != nullptr)
        {
            InPolicy::release(oldPtr);
        }

        return *this;
    }

    /// @brief Move assignment operator; transfers ownership from `rhs` and releases the previously managed object.
    /// @param[in,out] rhs Source `RefCountedPtr` to move from; reset to `nullptr`.
    /// @return Reference to `*this`.
    constexpr RefCountedPtr& operator=(RefCountedPtr&& rhs) noexcept
    {
        static_assert(
            concepts::IsRefCounted<InElementType, InPolicy>,
            "RefCountedPtr requires InElementType and InPolicy to satisfy gp::concepts::IsRefCounted."
        );

        if (this != &rhs)
        {
            PointerType oldPtr = m_ptr;
            m_ptr = rhs.m_ptr;
            rhs.m_ptr = nullptr;
            if (oldPtr != nullptr)
            {
                InPolicy::release(oldPtr);
            }
        }
        return *this;
    }

    /// @brief Converting move assignment operator from a compatible `RefCountedPtr<InOtherType>`.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @tparam InOtherPolicy Policy type of the source smart pointer.
    /// @param[in,out] rhs Source `RefCountedPtr` to move from; reset to `nullptr`.
    /// @return Reference to `*this`.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType, typename InOtherPolicy>
    constexpr RefCountedPtr& operator=(RefCountedPtr<InOtherType, InOtherPolicy>&& rhs) noexcept
        requires concepts::IsRefCounted<InElementType, InPolicy>
    {
        if (static_cast<const void*>(this) != static_cast<const void*>(&rhs))
        {
            PointerType oldPtr = m_ptr;
            m_ptr = rhs.m_ptr;
            rhs.m_ptr = nullptr;
            if (oldPtr != nullptr)
            {
                InPolicy::release(oldPtr);
            }
        }
        return *this;
    }

    /// @brief Raw pointer assignment operator; retains `ptr` and releases the previously managed object.
    /// @tparam InOtherType Pointer-convertible source type.
    /// @param[in] ptr Raw pointer to retain (may be `nullptr`).
    /// @return Reference to `*this`.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr RefCountedPtr& operator=(InOtherType* ptr) noexcept
        requires concepts::IsRefCounted<InElementType, InPolicy>
    {
        reset(ptr);
        return *this;
    }

    /// @brief Null pointer assignment operator; releases the currently managed object and resets to `nullptr`.
    /// @param[in] nullValue Null pointer literal.
    /// @return Reference to `*this`.
    constexpr RefCountedPtr& operator=([[maybe_unused]] gp::NullPtrT nullValue) noexcept
    {
        reset();
        return *this;
    }

public:
    /// @brief Replaces the managed object, retaining `ptr` (if non-null) and releasing the previous object.
    /// @param[in] ptr New raw pointer to retain and manage (defaults to `nullptr`).
    constexpr void reset(PointerType ptr = nullptr) noexcept
    {
        static_assert(
            concepts::IsRefCounted<InElementType, InPolicy>,
            "RefCountedPtr requires InElementType and InPolicy to satisfy gp::concepts::IsRefCounted."
        );

        if (ptr != nullptr)
        {
            InPolicy::addRef(ptr);
        }

        PointerType oldPtr = m_ptr;
        m_ptr = ptr;
        if (oldPtr != nullptr)
        {
            InPolicy::release(oldPtr);
        }
    }

    /// @brief Replaces the managed object by adopting an already-incremented raw pointer without calling `addRef()`.
    /// @param[in] ptr Raw pointer whose existing reference is adopted (may be `nullptr`).
    constexpr void adopt(PointerType ptr) noexcept
    {
        static_assert(
            concepts::IsRefCounted<InElementType, InPolicy>,
            "RefCountedPtr requires InElementType and InPolicy to satisfy gp::concepts::IsRefCounted."
        );

        PointerType oldPtr = m_ptr;
        m_ptr = ptr;
        if (oldPtr != nullptr)
        {
            InPolicy::release(oldPtr);
        }
    }

    /// @brief Relinquishes ownership of the managed pointer and returns it without decrementing its reference count.
    /// @note The caller becomes responsible for eventually calling `release()` on the returned pointer.
    /// @return The previously managed raw pointer, or `nullptr` if empty.
    [[nodiscard]] constexpr PointerType detach() noexcept
    {
        PointerType oldPtr = m_ptr;
        m_ptr = nullptr;
        return oldPtr;
    }

    /// @brief Alias for `detach()` matching `UniquePtr::release()` semantics (relinquishes ownership without
    ///        decrementing the reference count).
    /// @return The previously managed raw pointer, or `nullptr` if empty.
    [[nodiscard]] constexpr PointerType release() noexcept
    {
        return detach();
    }

    /// @brief Exchanges the managed pointers of `*this` and `other` with zero reference-count operations.
    /// @param[in,out] other Other `RefCountedPtr` to swap with.
    constexpr void swap(RefCountedPtr& other) noexcept
    {
        PointerType temp = m_ptr;
        m_ptr = other.m_ptr;
        other.m_ptr = temp;
    }

    /// @brief Releases any currently managed object and returns the address of the internal raw pointer (`&m_ptr`).
    /// @details Modeled after Unreal Engine `TRefCountPtr::GetInitReference()` and COM `ReleaseAndGetAddressOf()`.
    ///          Enables passing `&m_ptr` directly into C or D3D12/DXGI factory out-parameters (such as
    ///          `IID_PPV_ARGS(resource.initReference())`) that write a pre-incremented pointer into `T**`.
    /// @return Address of the internal `m_ptr` member, guaranteed to hold `nullptr` on return.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr PointerType* initReference() noexcept
    {
        reset();
        return &m_ptr;
    }

    /// @brief Returns the address of the internal pointer without releasing the managed object.
    /// @details Useful when passing a 1-element pointer array to graphics APIs (e.g.,
    ///          `SetDescriptorHeaps(1, heap.getAddressOf())`).
    /// @return Const pointer to the internal `m_ptr` member.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr const PointerType* getAddressOf() const noexcept
    {
        return &m_ptr;
    }

    /// @brief Returns the mutable address of the internal pointer without releasing the managed object.
    /// @return Mutable pointer to the internal `m_ptr` member.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr PointerType* getAddressOf() noexcept
    {
        return &m_ptr;
    }

public:
    /// @brief Returns the raw pointer to the managed object without altering its reference count.
    /// @return Raw pointer to the managed object, or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr PointerType get() const noexcept
    {
        return m_ptr;
    }

    /// @brief Member access operator to the managed object.
    /// @note Triggers an assertion if the `RefCountedPtr` is null.
    /// @return Raw pointer to the managed object.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr PointerType operator->() const noexcept
    {
        GP_ASSERT(m_ptr != nullptr, "Attempting to dereference a null RefCountedPtr via operator->().");
        return m_ptr;
    }

    /// @brief Dereference operator to the managed object.
    /// @note Triggers an assertion if the `RefCountedPtr` is null.
    /// @return Reference to the managed object.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr std::add_lvalue_reference_t<InElementType> operator*() const noexcept
        requires(!concepts::IsVoid<InElementType>)
    {
        GP_ASSERT(m_ptr != nullptr, "Attempting to dereference a null RefCountedPtr via operator*().");
        return *m_ptr;
    }

    /// @brief Checks whether this `RefCountedPtr` currently manages a non-null object.
    /// @return True if `m_ptr != nullptr`; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isValid() const noexcept
    {
        return m_ptr != nullptr;
    }

    /// @brief Explicit boolean conversion operator checking whether `m_ptr != nullptr`.
    /// @return True if `isValid()`; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT explicit constexpr operator bool() const noexcept
    {
        return isValid();
    }

    /// @brief Queries the live reference count of the managed object via `InPolicy::getRefCount()`.
    /// @return Reference count of the managed object, or `0` if `m_ptr == nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 getRefCount() const noexcept
    {
        static_assert(
            concepts::HasRefCountQuery<InPolicy, InElementType>,
            "RefCountedPtr::getRefCount requires InPolicy::getRefCount(const InElementType*) to be available."
        );
        return m_ptr != nullptr ? static_cast<gp::UInt32>(InPolicy::getRefCount(m_ptr)) : 0u;
    }

    /// @brief Checks whether this `RefCountedPtr` holds the sole live reference (`getRefCount() == 1`) to the object.
    /// @return True if non-null and `getRefCount() == 1u`; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isUnique() const noexcept
    {
        return getRefCount() == 1u;
    }

    /// @brief Computes a deterministic pointer hash suitable for RHI pipeline state and descriptor map keys.
    /// @details Shifts right by `alignof(void*)` log2 bits (`3` on 64-bit) to discard low-entropy alignment zeros
    ///          before applying a 64-bit SplitMix64 finalizer avalanching mix.
    /// @return Hash value of the underlying pointer (`0` if `m_ptr == nullptr`).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::USize getHash() const noexcept
    {
        if (m_ptr == nullptr)
        {
            return 0u;
        }

        if consteval
        {
            return 1u;
        }
        else
        {
            gp::UInt64 key = static_cast<gp::UInt64>(reinterpret_cast<gp::UIntPtr>(m_ptr));
            key = (key ^ (key >> 30u)) * 0xBF58476D1CE4E5B9ull;
            key = (key ^ (key >> 27u)) * 0x94D049BB133111EBull;
            key = key ^ (key >> 31u);
            return static_cast<gp::USize>(key);
        }
    }

public:
    /// @brief Compares two `RefCountedPtr` instances for pointer equality.
    /// @tparam InOtherType Element type of `rhs`.
    /// @tparam InOtherPolicy Policy type of `rhs`.
    /// @param[in] rhs Other `RefCountedPtr` to compare against.
    /// @return True if both manage the exact same object address; false otherwise.
    template <typename InOtherType, typename InOtherPolicy>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        operator==(const RefCountedPtr<InOtherType, InOtherPolicy>& rhs) const noexcept
    {
        return m_ptr == rhs.m_ptr;
    }

    /// @brief Compares this `RefCountedPtr` against a raw pointer for equality.
    /// @tparam InOtherType Pointee type of `rhs`.
    /// @param[in] rhs Raw pointer to compare against.
    /// @return True if `m_ptr == rhs`; false otherwise.
    template <typename InOtherType>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool operator==(const InOtherType* rhs) const noexcept
    {
        return m_ptr == rhs;
    }

    /// @brief Compares this `RefCountedPtr` against `nullptr`.
    /// @param[in] nullValue Null pointer literal.
    /// @return True if `m_ptr == nullptr`; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool operator==([[maybe_unused]] gp::NullPtrT nullValue) const noexcept
    {
        return m_ptr == nullptr;
    }

    /// @brief Three-way comparison operator ordering two `RefCountedPtr` instances by raw address.
    /// @tparam InOtherType Element type of `rhs`.
    /// @tparam InOtherPolicy Policy type of `rhs`.
    /// @param[in] rhs Other `RefCountedPtr` to compare against.
    /// @return Strong ordering result of the underlying pointers.
    template <typename InOtherType, typename InOtherPolicy>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr std::strong_ordering
        operator<=>(const RefCountedPtr<InOtherType, InOtherPolicy>& rhs) const noexcept
    {
        return std::compare_three_way{}(m_ptr, rhs.m_ptr);
    }

    /// @brief Three-way comparison operator ordering this `RefCountedPtr` against a raw pointer.
    /// @tparam InOtherType Pointee type of `rhs`.
    /// @param[in] rhs Raw pointer to compare against.
    /// @return Strong ordering result.
    template <typename InOtherType>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr std::strong_ordering operator<=>(const InOtherType* rhs) const noexcept
    {
        return std::compare_three_way{}(m_ptr, rhs);
    }

    /// @brief Three-way comparison operator ordering this `RefCountedPtr` against `nullptr`.
    /// @param[in] nullValue Null pointer literal.
    /// @return Strong ordering result against `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr std::strong_ordering
        operator<=>([[maybe_unused]] gp::NullPtrT nullValue) const noexcept
    {
        return std::compare_three_way{}(m_ptr, static_cast<PointerType>(nullptr));
    }
};

/// @brief ADL-friendly free function exchanging two `RefCountedPtr` instances with zero atomic operations.
/// @tparam InElementType Pointee type.
/// @tparam InPolicy Reference-counting policy.
/// @param[in,out] lhs First smart pointer.
/// @param[in,out] rhs Second smart pointer.
template <typename InElementType, typename InPolicy>
GP_FORCEINLINE_HINT constexpr void
    swap(RefCountedPtr<InElementType, InPolicy>& lhs, RefCountedPtr<InElementType, InPolicy>& rhs) noexcept
{
    lhs.swap(rhs);
}

/// @brief Wraps a raw pointer in a `RefCountedPtr` and increments its reference count (`kRetainRef`).
/// @tparam InElementType Intrusive object type.
/// @tparam InPolicy Reference-counting policy (defaults to `DefaultRefCountPolicy<InElementType>`).
/// @param[in] ptr Raw pointer to retain.
/// @return `RefCountedPtr<InElementType, InPolicy>` managing `ptr`.
template <typename InElementType, typename InPolicy = DefaultRefCountPolicy<InElementType>>
requires concepts::IsRefCounted<InElementType, InPolicy>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr RefCountedPtr<InElementType, InPolicy>
    retainRef(InElementType* ptr) noexcept
{
    return RefCountedPtr<InElementType, InPolicy>(ptr, kRetainRef);
}

/// @brief Wraps a raw pointer in a `RefCountedPtr` without incrementing its reference count (`kAdoptRef`).
/// @tparam InElementType Intrusive object type.
/// @tparam InPolicy Reference-counting policy (defaults to `DefaultRefCountPolicy<InElementType>`).
/// @param[in] ptr Raw pointer whose existing reference is adopted.
/// @return `RefCountedPtr<InElementType, InPolicy>` managing `ptr`.
template <typename InElementType, typename InPolicy = DefaultRefCountPolicy<InElementType>>
requires concepts::IsRefCounted<InElementType, InPolicy>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr RefCountedPtr<InElementType, InPolicy> adoptRef(InElementType* ptr) noexcept
{
    return RefCountedPtr<InElementType, InPolicy>(ptr, kAdoptRef);
}

/// @brief Dynamically allocates and constructs an intrusive object of type `InElementType`, returning a
///        `RefCountedPtr<InElementType>` with an initial reference count of `1`.
/// @tparam InElementType Intrusive object type to construct.
/// @tparam InArgs Constructor argument types forwarded to `InElementType`.
/// @param[in] args Arguments forwarded to `InElementType`'s constructor.
/// @return A valid `RefCountedPtr<InElementType>` owning the newly constructed object.
template <typename InElementType, typename... InArgs>
requires(!concepts::IsArray<InElementType> && concepts::IsRefCounted<InElementType>)
[[nodiscard]] GP_FORCEINLINE_HINT constexpr RefCountedPtr<InElementType> makeRefCounted(InArgs&&... args) noexcept
{
    return RefCountedPtr<InElementType>(new InElementType(std::forward<InArgs>(args)...), kRetainRef);
}

/// @brief Allocates and placement-constructs an intrusive object using a first-class engine allocator
///        (`PoolAllocator`, `LinearAllocator`, `HeapAllocator`), returning a `RefCountedPtr<InElementType>`.
/// @details Constrained by `concepts::HasZeroReferencesHook<InElementType>` (or custom policy) to guarantee that
///          when the reference count drops to zero, the object routes deallocation through its custom
///          `onZeroReferences()` hook (e.g., returning the block to its `PoolAllocator`) rather than calling global
///          `operator delete`.
/// @tparam InElementType Intrusive object type to construct.
/// @tparam InAllocator Engine allocator type satisfying `gp::concepts::IsAllocator`.
/// @tparam InArgs Constructor argument types forwarded to `InElementType`.
/// @param[in,out] allocator Engine allocator providing backing storage for `sizeof(InElementType)`.
/// @param[in] args Arguments forwarded to `InElementType`'s constructor.
/// @return `RefCountedPtr<InElementType>` managing the newly constructed object, or `nullptr` if allocation failed.
template <typename InElementType, concepts::IsAllocator InAllocator, typename... InArgs>
requires(
    !concepts::IsArray<InElementType> && concepts::IsRefCounted<InElementType> &&
    concepts::HasZeroReferencesHook<InElementType>
) [[nodiscard]] GP_FORCEINLINE_HINT RefCountedPtr<InElementType>
    allocateRefCounted(InAllocator& allocator, InArgs&&... args) noexcept
{
    void* memoryBlock = allocator.allocate(sizeof(InElementType), static_cast<gp::UInt32>(alignof(InElementType)));
    if (memoryBlock == nullptr) [[unlikely]]
    {
        return RefCountedPtr<InElementType>(nullptr);
    }

    InElementType* instance = ::new (memoryBlock) InElementType(std::forward<InArgs>(args)...);
    return RefCountedPtr<InElementType>(instance, kRetainRef);
}

/// @brief Performs a `static_cast<InTargetType*>` on a `RefCountedPtr`, retaining a new reference.
/// @tparam InTargetType Target pointee type.
/// @tparam InSourceType Source pointee type.
/// @tparam InSourcePolicy Source policy type.
/// @param[in] src Source `RefCountedPtr` to cast.
/// @return `RefCountedPtr<InTargetType>` pointing to the cast object with an incremented reference count.
template <typename InTargetType, typename InSourceType, typename InSourcePolicy>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr RefCountedPtr<InTargetType>
    staticCastRef(const RefCountedPtr<InSourceType, InSourcePolicy>& src) noexcept
{
    return RefCountedPtr<InTargetType>(static_cast<InTargetType*>(src.get()), kRetainRef);
}

/// @brief Performs a `static_cast<InTargetType*>` on an rvalue `RefCountedPtr`, transferring ownership with zero
///        atomic reference-count operations.
/// @tparam InTargetType Target pointee type.
/// @tparam InSourceType Source pointee type.
/// @tparam InSourcePolicy Source policy type.
/// @param[in,out] src Rvalue `RefCountedPtr` to move and cast; reset to `nullptr`.
/// @return `RefCountedPtr<InTargetType>` adopting the transferred reference.
template <typename InTargetType, typename InSourceType, typename InSourcePolicy>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr RefCountedPtr<InTargetType>
    staticCastRef(RefCountedPtr<InSourceType, InSourcePolicy>&& src) noexcept
{
    return RefCountedPtr<InTargetType>(static_cast<InTargetType*>(src.detach()), kAdoptRef);
}

/// @brief Performs a `const_cast<InTargetType*>` on a `RefCountedPtr`, retaining a new reference.
/// @tparam InTargetType Target pointee type.
/// @tparam InSourceType Source pointee type.
/// @tparam InSourcePolicy Source policy type.
/// @param[in] src Source `RefCountedPtr` to cast.
/// @return `RefCountedPtr<InTargetType>` pointing to the cast object with an incremented reference count.
template <typename InTargetType, typename InSourceType, typename InSourcePolicy>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr RefCountedPtr<InTargetType>
    constCastRef(const RefCountedPtr<InSourceType, InSourcePolicy>& src) noexcept
{
    return RefCountedPtr<InTargetType>(const_cast<InTargetType*>(src.get()), kRetainRef);
}

/// @brief Performs a `const_cast<InTargetType*>` on an rvalue `RefCountedPtr`, transferring ownership with zero
///        atomic reference-count operations.
/// @tparam InTargetType Target pointee type.
/// @tparam InSourceType Source pointee type.
/// @tparam InSourcePolicy Source policy type.
/// @param[in,out] src Rvalue `RefCountedPtr` to move and cast; reset to `nullptr`.
/// @return `RefCountedPtr<InTargetType>` adopting the transferred reference.
template <typename InTargetType, typename InSourceType, typename InSourcePolicy>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr RefCountedPtr<InTargetType>
    constCastRef(RefCountedPtr<InSourceType, InSourcePolicy>&& src) noexcept
{
    return RefCountedPtr<InTargetType>(const_cast<InTargetType*>(src.detach()), kAdoptRef);
}

/// @brief Performs a `reinterpret_cast<InTargetType*>` on a `RefCountedPtr`, retaining a new reference.
/// @tparam InTargetType Target pointee type.
/// @tparam InSourceType Source pointee type.
/// @tparam InSourcePolicy Source policy type.
/// @param[in] src Source `RefCountedPtr` to cast.
/// @return `RefCountedPtr<InTargetType>` pointing to the cast object with an incremented reference count.
template <typename InTargetType, typename InSourceType, typename InSourcePolicy>
[[nodiscard]] GP_FORCEINLINE_HINT RefCountedPtr<InTargetType>
    reinterpretCastRef(const RefCountedPtr<InSourceType, InSourcePolicy>& src) noexcept
{
    return RefCountedPtr<InTargetType>(reinterpret_cast<InTargetType*>(src.get()), kRetainRef);
}

/// @brief Performs a `reinterpret_cast<InTargetType*>` on an rvalue `RefCountedPtr`, transferring ownership with zero
///        atomic reference-count operations.
/// @tparam InTargetType Target pointee type.
/// @tparam InSourceType Source pointee type.
/// @tparam InSourcePolicy Source policy type.
/// @param[in,out] src Rvalue `RefCountedPtr` to move and cast; reset to `nullptr`.
/// @return `RefCountedPtr<InTargetType>` adopting the transferred reference.
template <typename InTargetType, typename InSourceType, typename InSourcePolicy>
[[nodiscard]] GP_FORCEINLINE_HINT RefCountedPtr<InTargetType>
    reinterpretCastRef(RefCountedPtr<InSourceType, InSourcePolicy>&& src) noexcept
{
    return RefCountedPtr<InTargetType>(reinterpret_cast<InTargetType*>(src.detach()), kAdoptRef);
}

}   // namespace gp
