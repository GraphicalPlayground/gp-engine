// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Construction.hpp"
#include "concepts/Fundamental.hpp"
#include "concepts/Memory.hpp"
#include "concepts/Relationship.hpp"
#include "CoreMinimal.hpp"
#include "hashing/HashMix.hpp"
#include "memory/allocators/AllocatorConcepts.hpp"
#include "memory/allocators/HeapAllocator.hpp"
#include "memory/MemoryOrder.hpp"
#include "memory/pointers/PointerEnums.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/Fences.hpp"
#include <compare>
#include <new>   // IWYU pragma: keep
#include <type_traits>
#include <utility>

namespace gp
{

template <typename InElementType, RefCountThreadSafety InMode = RefCountThreadSafety::ThreadSafe>
class SharedRef;

template <typename InElementType, RefCountThreadSafety InMode = RefCountThreadSafety::ThreadSafe>
class SharedPtr;

template <typename InElementType, RefCountThreadSafety InMode = RefCountThreadSafety::ThreadSafe>
class WeakPtr;

template <typename InElementType, RefCountThreadSafety InMode = RefCountThreadSafety::ThreadSafe>
class SharedFromThis;

namespace detail
{

/// @brief Base control block managing strong and weak reference counts for `SharedRef`, `SharedPtr`, and `WeakPtr`.
/// @details Modeled after Unreal Engine 5 (`TReferenceControllerBase<Mode>`) and Boost (`sp_counted_base`).
///          Occupies 16 bytes on 64-bit targets (`8B vptr + 4B m_sharedRefs + 4B m_weakRefs`).
///
///          **The "+1 Weak Reference" Invariant:**
///          `m_weakRefs` is initialized to `1`, representing the collective existence of all live `m_sharedRefs`.
///          Strong reference increments and non-final decrements never touch `m_weakRefs`. When `releaseSharedRef()`
///          drops `m_sharedRefs` from `1` to `0`, it invokes `destroyObject()` and then calls `releaseWeakRef()`,
///          which drops the collective weak reference (`1 -> 0` if no `WeakPtr` observers exist) and invokes
///          `destroyController()`.
/// @tparam InMode Selects lock-free atomic (`ThreadSafe`) or plain integer (`SingleThreaded`) reference counting.
template <RefCountThreadSafety InMode>
class ReferenceControllerBase
{
public:
    /// @brief Underlying counter storage type (`Atomic<UInt32>` or `UInt32`).
    using CounterStorageType =
        std::conditional_t<InMode == RefCountThreadSafety::ThreadSafe, gp::thread::Atomic<gp::UInt32>, gp::UInt32>;

private:
    CounterStorageType m_sharedRefs{ 1u };
    CounterStorageType m_weakRefs{ 1u };

public:
    /// @brief Constructs a control block with `1` strong reference and `1` collective weak reference.
    constexpr ReferenceControllerBase() noexcept = default;

    ReferenceControllerBase(const ReferenceControllerBase&) = delete;
    ReferenceControllerBase& operator=(const ReferenceControllerBase&) = delete;
    ReferenceControllerBase(ReferenceControllerBase&&) = delete;
    ReferenceControllerBase& operator=(ReferenceControllerBase&&) = delete;

    /// @brief Virtual destructor.
    virtual constexpr ~ReferenceControllerBase() noexcept = default;

public:
    /// @brief Destroys the managed object payload when `m_sharedRefs` transitions from `1` to `0`.
    virtual constexpr void destroyObject() noexcept = 0;

    /// @brief Destroys and deallocates this control block when `m_weakRefs` transitions from `1` to `0`.
    virtual constexpr void destroyController() noexcept = 0;

    /// @brief Queries the live strong reference count.
    /// @return Number of active `SharedRef` and `SharedPtr` owners.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 getSharedReferenceCount() const noexcept
    {
        if constexpr (InMode == RefCountThreadSafety::ThreadSafe)
        {
            return m_sharedRefs.load(gp::memory::MemoryOrder::Relaxed);
        }
        else
        {
            return m_sharedRefs;
        }
    }

    /// @brief Queries the raw internal weak reference count (including `+1` if `getSharedReferenceCount() > 0`).
    /// @return Raw internal weak counter value.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 getWeakReferenceCount() const noexcept
    {
        if constexpr (InMode == RefCountThreadSafety::ThreadSafe)
        {
            return m_weakRefs.load(gp::memory::MemoryOrder::Relaxed);
        }
        else
        {
            return m_weakRefs;
        }
    }

    /// @brief Increments the strong reference count (`m_sharedRefs > 0` assumed).
    GP_FORCEINLINE_HINT constexpr void addSharedRef() noexcept
    {
        if constexpr (InMode == RefCountThreadSafety::ThreadSafe)
        {
            const gp::UInt32 prev = m_sharedRefs.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
            GP_ASSERT(prev > 0u, "Cannot addSharedRef() on a control block whose strong count already reached zero.");
        }
        else
        {
            GP_ASSERT(
                m_sharedRefs > 0u, "Cannot addSharedRef() on a control block whose strong count already reached zero."
            );
            ++m_sharedRefs;
        }
    }

    /// @brief Conditionally increments the strong reference count only if `m_sharedRefs > 0` (used by `WeakPtr::pin`).
    /// @return `true` if a new strong reference was acquired; `false` if the object has already been destroyed.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool tryAddSharedRef() noexcept
    {
        if constexpr (InMode == RefCountThreadSafety::ThreadSafe)
        {
            gp::UInt32 expected = m_sharedRefs.load(gp::memory::MemoryOrder::Relaxed);
            while (expected != 0u)
            {
                if (m_sharedRefs.compareExchangeWeak(
                        expected, expected + 1u, gp::memory::MemoryOrder::Acquire, gp::memory::MemoryOrder::Relaxed
                    ))
                {
                    return true;
                }
            }
            return false;
        }
        else
        {
            if (m_sharedRefs == 0u)
            {
                return false;
            }
            ++m_sharedRefs;
            return true;
        }
    }

    /// @brief Decrements the strong reference count, destroying the managed object and releasing the collective weak
    ///        reference when `m_sharedRefs` drops from `1` to `0`.
    GP_FORCEINLINE_HINT constexpr void releaseSharedRef() noexcept
    {
        if constexpr (InMode == RefCountThreadSafety::ThreadSafe)
        {
            const gp::UInt32 prev = m_sharedRefs.fetchSub(1u, gp::memory::MemoryOrder::Release);
            GP_ASSERT(prev > 0u, "ReferenceControllerBase strong reference count underflow.");
            if (prev == 1u)
            {
                gp::thread::atomicThreadFence(gp::memory::MemoryOrder::Acquire);
                destroyObject();
                releaseWeakRef();
            }
        }
        else
        {
            GP_ASSERT(m_sharedRefs > 0u, "ReferenceControllerBase strong reference count underflow.");
            --m_sharedRefs;
            if (m_sharedRefs == 0u)
            {
                destroyObject();
                releaseWeakRef();
            }
        }
    }

    /// @brief Increments the weak reference count.
    GP_FORCEINLINE_HINT constexpr void addWeakRef() noexcept
    {
        if constexpr (InMode == RefCountThreadSafety::ThreadSafe)
        {
            const gp::UInt32 prev = m_weakRefs.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
            GP_ASSERT(prev > 0u, "Cannot addWeakRef() on a destroyed control block.");
        }
        else
        {
            GP_ASSERT(m_weakRefs > 0u, "Cannot addWeakRef() on a destroyed control block.");
            ++m_weakRefs;
        }
    }

    /// @brief Decrements the weak reference count, destroying the control block when `m_weakRefs` drops from `1` to
    /// `0`.
    GP_FORCEINLINE_HINT constexpr void releaseWeakRef() noexcept
    {
        if constexpr (InMode == RefCountThreadSafety::ThreadSafe)
        {
            const gp::UInt32 prev = m_weakRefs.fetchSub(1u, gp::memory::MemoryOrder::Release);
            GP_ASSERT(prev > 0u, "ReferenceControllerBase weak reference count underflow.");
            if (prev == 1u)
            {
                gp::thread::atomicThreadFence(gp::memory::MemoryOrder::Acquire);
                destroyController();
            }
        }
        else
        {
            GP_ASSERT(m_weakRefs > 0u, "ReferenceControllerBase weak reference count underflow.");
            --m_weakRefs;
            if (m_weakRefs == 0u)
            {
                destroyController();
            }
        }
    }
};

/// @brief Control block managing an externally allocated raw pointer with a customizable deleter and engine allocator.
/// @tparam InObjectType Concrete pointer type originally passed to the control block.
/// @tparam InDeleterType Callable invoked on `m_ptr` when `destroyObject()` executes.
/// @tparam InMode Thread-safety policy (`ThreadSafe` or `SingleThreaded`).
/// @tparam InAllocator Engine allocator type used to allocate and free the control block itself.
template <
    typename InObjectType,
    typename InDeleterType,
    RefCountThreadSafety InMode,
    concepts::IsAllocator InAllocator = gp::memory::HeapAllocator>
class DefaultReferenceController final : public ReferenceControllerBase<InMode>
{
private:
    InObjectType* m_ptr{ nullptr };
    GP_NO_UNIQUE_ADDRESS InDeleterType m_deleter;
    GP_NO_UNIQUE_ADDRESS InAllocator m_allocator;

public:
    /// @brief Constructs a control block wrapping @p inPtr, @p inDeleter, and @p inAllocator.
    /// @tparam InDeleterArg Forwarded deleter type.
    /// @tparam InAllocatorArg Forwarded allocator type.
    /// @param[in] inPtr Raw pointer to manage.
    /// @param[in] inDeleter Deleter callable invoked when strong references reach zero.
    /// @param[in] inAllocator Allocator that owns this control block's memory.
    template <typename InDeleterArg, typename InAllocatorArg = InAllocator>
    constexpr DefaultReferenceController(
        InObjectType* inPtr, InDeleterArg&& inDeleter, InAllocatorArg&& inAllocator = InAllocator{}
    ) noexcept
        : ReferenceControllerBase<InMode>()
        , m_ptr(inPtr)
        , m_deleter(std::forward<InDeleterArg>(inDeleter))
        , m_allocator(std::forward<InAllocatorArg>(inAllocator))
    {}

    /// @copydoc ReferenceControllerBase::destroyObject()
    virtual constexpr void destroyObject() noexcept override
    {
        m_deleter(m_ptr);
        m_ptr = nullptr;
    }

    /// @copydoc ReferenceControllerBase::destroyController()
    virtual constexpr void destroyController() noexcept override
    {
        if consteval
        {
            delete this;
        }
        else
        {
            InAllocator localAlloc = std::move(m_allocator);
            this->~DefaultReferenceController();
            localAlloc.deallocate(
                this, sizeof(DefaultReferenceController), static_cast<gp::UInt32>(alignof(DefaultReferenceController))
            );
        }
    }
};

/// @brief Single-allocation control block embedding `InObjectType` storage in-place for `makeShared` and
///        `allocateShared`.
/// @details Separates `~InObjectType()` execution in `destroyObject()` from physical block deallocation in
///          `destroyController()` so lingering `WeakPtr` handles can safely inspect `m_sharedRefs == 0` after the
///          object is destroyed.
/// @tparam InObjectType Concrete type constructed in-place.
/// @tparam InMode Thread-safety policy (`ThreadSafe` or `SingleThreaded`).
/// @tparam InAllocator Engine allocator type used to allocate and free the combined block.
template <
    typename InObjectType,
    RefCountThreadSafety InMode,
    concepts::IsAllocator InAllocator = gp::memory::HeapAllocator>
class IntrusiveReferenceController final : public ReferenceControllerBase<InMode>
{
private:
    GP_NO_UNIQUE_ADDRESS InAllocator m_allocator;

    union
    {
        InObjectType m_value;
    };

public:
    /// @brief Constructs the control block and placement-initializes `m_value` in-place from @p args.
    /// @tparam InAllocatorArg Forwarded allocator type.
    /// @tparam InArgs Constructor argument types for `InObjectType`.
    /// @param[in] inAllocator Engine allocator instance to store for deallocation.
    /// @param[in] args Arguments forwarded to `InObjectType`'s constructor.
    template <typename InAllocatorArg, typename... InArgs>
    constexpr explicit IntrusiveReferenceController(InAllocatorArg&& inAllocator, InArgs&&... args) noexcept
        : ReferenceControllerBase<InMode>()
        , m_allocator(std::forward<InAllocatorArg>(inAllocator))
    {
        std::construct_at(&m_value, std::forward<InArgs>(args)...);
    }

    /// @brief Empty union destructor; `m_value` lifetime is explicitly ended inside `destroyObject()`.
    virtual constexpr ~IntrusiveReferenceController() noexcept override
    {}

    /// @brief Returns a pointer to the in-place constructed `InObjectType`.
    /// @return Pointer to `m_value`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr InObjectType* getObjectPtr() noexcept
    {
        return &m_value;
    }

    /// @copydoc ReferenceControllerBase::destroyObject()
    virtual constexpr void destroyObject() noexcept override
    {
        std::destroy_at(&m_value);
    }

    /// @copydoc ReferenceControllerBase::destroyController()
    virtual constexpr void destroyController() noexcept override
    {
        if consteval
        {
            delete this;
        }
        else
        {
            InAllocator localAlloc = std::move(m_allocator);
            this->~IntrusiveReferenceController();
            localAlloc.deallocate(
                this,
                sizeof(IntrusiveReferenceController),
                static_cast<gp::UInt32>(alignof(IntrusiveReferenceController))
            );
        }
    }
};

/// @brief Allocates and constructs a `DefaultReferenceController` using @p allocator (or `new` in `consteval`),
///        invoking @p deleter on @p ptr if control-block allocation returns `nullptr`.
/// @tparam InMode Thread-safety policy.
/// @tparam InObjectType Concrete pointee type.
/// @tparam InDeleterType Deleter callable type.
/// @tparam InAllocator Engine allocator type.
/// @param[in] ptr Raw pointer to manage.
/// @param[in] deleter Deleter callable.
/// @param[in,out] allocator Engine allocator used to allocate the control block.
/// @return Pointer to the newly constructed control block, or `nullptr` on out-of-memory.
template <
    RefCountThreadSafety InMode,
    typename InObjectType,
    typename InDeleterType = DefaultDelete<InObjectType>,
    concepts::IsAllocator InAllocator = gp::memory::HeapAllocator>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr ReferenceControllerBase<InMode>* createDefaultReferenceController(
    InObjectType* ptr, InDeleterType&& deleter = InDeleterType{}, InAllocator allocator = InAllocator{}
) noexcept
{
    using ControllerType = DefaultReferenceController<InObjectType, std::decay_t<InDeleterType>, InMode, InAllocator>;

    if consteval
    {
        return new ControllerType(ptr, std::forward<InDeleterType>(deleter), std::move(allocator));
    }
    else
    {
        void* rawBlock = allocator.allocate(sizeof(ControllerType), static_cast<gp::UInt32>(alignof(ControllerType)));
        if (rawBlock == nullptr) [[unlikely]]
        {
            deleter(ptr);
            return nullptr;
        }
        return ::new (rawBlock) ControllerType(ptr, std::forward<InDeleterType>(deleter), std::move(allocator));
    }
}

/// @brief Allocates and constructs an in-place `IntrusiveReferenceController` using @p allocator (or `new` in
///        `consteval`).
/// @tparam InObjectType Concrete object type to construct in-place.
/// @tparam InMode Thread-safety policy.
/// @tparam InAllocator Engine allocator type.
/// @tparam InArgs Constructor argument types for `InObjectType`.
/// @param[in,out] allocator Engine allocator used to allocate the combined block.
/// @param[in] args Arguments forwarded to `InObjectType`.
/// @return Pointer to the newly constructed in-place control block, or `nullptr` if @p allocator returned `nullptr`.
template <
    typename InObjectType,
    RefCountThreadSafety InMode,
    concepts::IsAllocator InAllocator = gp::memory::HeapAllocator,
    typename... InArgs>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr IntrusiveReferenceController<InObjectType, InMode, InAllocator>*
    createIntrusiveReferenceController(InAllocator allocator, InArgs&&... args) noexcept
{
    using ControllerType = IntrusiveReferenceController<InObjectType, InMode, InAllocator>;

    if consteval
    {
        return new ControllerType(std::move(allocator), std::forward<InArgs>(args)...);
    }
    else
    {
        void* rawBlock = allocator.allocate(sizeof(ControllerType), static_cast<gp::UInt32>(alignof(ControllerType)));
        if (rawBlock == nullptr) [[unlikely]]
        {
            return nullptr;
        }
        return ::new (rawBlock) ControllerType(std::move(allocator), std::forward<InArgs>(args)...);
    }
}

/// @brief Proxy object returned by `gp::makeShareable()` to defer `ReferenceControllerBase<InMode>` instantiation
///        until the target `SharedPtr<T, InMode>` or `SharedRef<T, InMode>` thread-safety mode is known.
/// @tparam InObjectType Raw pointer element type.
/// @tparam InDeleterType Deleter callable type.
/// @tparam InAllocator Engine allocator type.
template <
    typename InObjectType,
    typename InDeleterType = DefaultDelete<InObjectType>,
    concepts::IsAllocator InAllocator = gp::memory::HeapAllocator>
struct RawPtrProxy
{
public:
    InObjectType* object{ nullptr };
    GP_NO_UNIQUE_ADDRESS InDeleterType deleter;
    GP_NO_UNIQUE_ADDRESS InAllocator allocator;

public:
    /// @brief Constructs a proxy wrapping @p inObject, @p inDeleter, and @p inAllocator.
    /// @tparam InDeleterArg Forwarded deleter type.
    /// @tparam InAllocatorArg Forwarded allocator type.
    /// @param[in] inObject Raw pointer to wrap.
    /// @param[in] inDeleter Custom deleter.
    /// @param[in] inAllocator Engine allocator for the control block.
    template <typename InDeleterArg = InDeleterType, typename InAllocatorArg = InAllocator>
    constexpr explicit RawPtrProxy(
        InObjectType* inObject, InDeleterArg&& inDeleter = InDeleterType{}, InAllocatorArg&& inAllocator = InAllocator{}
    ) noexcept
        : object(inObject)
        , deleter(std::forward<InDeleterArg>(inDeleter))
        , allocator(std::forward<InAllocatorArg>(inAllocator))
    {}
};

/// @brief Internal tag type used for unchecked construction between `SharedRef`, `SharedPtr`, and `WeakPtr`.
struct SharedPointerInternalsTag
{
    explicit constexpr SharedPointerInternalsTag() noexcept = default;
};

/// @brief Constant tag instance for internal smart pointer construction.
inline constexpr SharedPointerInternalsTag kSharedInternalsTag{};

/// @brief Helper hook that binds `SharedFromThis::m_weakThis` if @p inObject publicly and unambiguously inherits from
///        `SharedFromThis<InBaseType, InMode>`.
/// @tparam InMode Thread-safety policy.
/// @tparam InOwnerType Owning pointer element type.
/// @tparam InObjectType Concrete pointee type being registered.
/// @tparam InBaseType Base type passed to `SharedFromThis<InBaseType, InMode>`.
/// @param[in] owner Newly constructed `SharedPtr` or `SharedRef` controller owner.
/// @param[in] inObject Raw pointer to the newly managed object.
/// @param[in] inShareable Base subobject pointer to `SharedFromThis<InBaseType, InMode>`.
template <RefCountThreadSafety InMode, typename InOwnerType, typename InObjectType, typename InBaseType>
requires concepts::IsPointerConvertibleTo<InObjectType, InBaseType> constexpr void enableSharedFromThisHelper(
    const SharedPtr<InOwnerType, InMode>* owner,
    const InObjectType* inObject,
    const SharedFromThis<InBaseType, InMode>* inShareable
) noexcept;

/// @brief Fallback overload when @p inObject does not inherit from `SharedFromThis`.
template <RefCountThreadSafety InMode, typename InOwnerType>
constexpr void enableSharedFromThisHelper(const SharedPtr<InOwnerType, InMode>*, ...) noexcept
{}

/// @brief Invokes `enableSharedFromThisHelper` if @p inObject is non-null and a class type.
/// @tparam InMode Thread-safety policy.
/// @tparam InOwnerType Element type of `owner`.
/// @tparam InObjectType Concrete pointee type.
/// @param[in] owner Newly constructed `SharedPtr`.
/// @param[in] inObject Raw pointer to the managed object.
template <RefCountThreadSafety InMode, typename InOwnerType, typename InObjectType>
constexpr void updateSharedFromThis(const SharedPtr<InOwnerType, InMode>* owner, InObjectType* inObject) noexcept
{
    if constexpr (std::is_class_v<std::remove_cv_t<InObjectType>>)
    {
        if (inObject != nullptr)
        {
            enableSharedFromThisHelper<InMode>(owner, inObject, inObject);
        }
    }
}

/// @brief Overload of `updateSharedFromThis` accepting a `SharedRef<InOwnerType, InMode>`.
/// @tparam InMode Thread-safety policy.
/// @tparam InOwnerType Element type of `owner`.
/// @tparam InObjectType Concrete pointee type.
/// @param[in] owner Newly constructed `SharedRef`.
/// @param[in] inObject Non-null pointer to the managed object.
template <RefCountThreadSafety InMode, typename InOwnerType, typename InObjectType>
constexpr void updateSharedFromThis(const SharedRef<InOwnerType, InMode>* owner, InObjectType* inObject) noexcept;

}   // namespace detail

/// @brief Non-nullable 16-byte shared smart reference guaranteeing a valid, live heap-allocated object.
/// @details Modeled after Unreal Engine's `TSharedRef<ObjectType, Mode>` ([ADR-0059]).
///          Unlike `SharedPtr<T, Mode>`, `SharedRef<T, Mode>` has no default constructor, cannot be constructed from
///          or assigned `nullptr`, and exposes no `reset()` method. APIs accepting or returning `SharedRef<T, Mode>`
///          statically guarantee non-null ownership without defensive null checks.
///
///          `SharedRef<T, Mode>` converts implicitly to `SharedPtr<T, Mode>`, and `gp::makeShared<T, Mode>()` returns
///          `SharedRef<T, Mode>` so callers can initialize either `SharedRef` or `SharedPtr` seamlessly.
/// @tparam InElementType Managed object type.
/// @tparam InMode Thread-safety policy (`ThreadSafe` by default, or `SingleThreaded`).
/// @see SharedPtr, WeakPtr, SharedFromThis, makeShared, allocateShared, makeShareable
template <typename InElementType, RefCountThreadSafety InMode>
class SharedRef
{
private:
    template <typename InOtherType, RefCountThreadSafety InOtherMode>
    friend class SharedRef;

    template <typename InOtherType, RefCountThreadSafety InOtherMode>
    friend class SharedPtr;

    template <typename InOtherType, RefCountThreadSafety InOtherMode>
    friend class WeakPtr;

    template <typename InOtherType, RefCountThreadSafety InOtherMode>
    friend class SharedFromThis;

public:
    /// @brief Managed object type.
    using ElementType = InElementType;

    /// @brief Raw pointer type to `InElementType`.
    using PointerType = InElementType*;

    /// @brief Compile-time thread-safety policy mode.
    static constexpr RefCountThreadSafety kThreadSafetyMode = InMode;

    /// @brief Compile-time flag indicating whether reference counting is atomic.
    static constexpr bool kIsThreadSafe = (InMode == RefCountThreadSafety::ThreadSafe);

private:
    PointerType m_object{ nullptr };
    detail::ReferenceControllerBase<InMode>* m_controller{ nullptr };

public:
    SharedRef() = delete;
    SharedRef(gp::NullPtrT) = delete;
    SharedRef& operator=(gp::NullPtrT) = delete;

    /// @brief Takes shared ownership of a non-null raw pointer @p ptr using `DefaultDelete<InOtherType>`.
    /// @tparam InOtherType Pointee type convertible to `InElementType*`.
    /// @param[in] ptr Non-null raw pointer to manage.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    explicit constexpr SharedRef(InOtherType* ptr) noexcept
        : m_object(ptr)
        , m_controller(nullptr)
    {
        GP_ASSERT(ptr != nullptr, "SharedRef cannot be constructed from a null pointer.");
        m_controller = detail::createDefaultReferenceController<InMode>(ptr);
        detail::updateSharedFromThis(this, ptr);
    }

    /// @brief Takes shared ownership of a non-null raw pointer @p ptr with a custom @p deleter and optional
    ///        @p allocator.
    /// @tparam InOtherType Pointee type convertible to `InElementType*`.
    /// @tparam InDeleter Custom deleter callable type.
    /// @tparam InAllocator Engine allocator type for the control block.
    /// @param[in] ptr Non-null raw pointer to manage.
    /// @param[in] deleter Custom deleter invoked when strong references reach zero.
    /// @param[in] allocator Engine allocator used to allocate the control block.
    template <
        concepts::IsPointerConvertibleTo<InElementType> InOtherType,
        typename InDeleter,
        concepts::IsAllocator InAllocator = gp::memory::HeapAllocator>
    constexpr SharedRef(InOtherType* ptr, InDeleter&& deleter, InAllocator allocator = InAllocator{}) noexcept
        : m_object(ptr)
        , m_controller(nullptr)
    {
        GP_ASSERT(ptr != nullptr, "SharedRef cannot be constructed from a null pointer.");
        m_controller = detail::createDefaultReferenceController<InMode>(
            ptr, std::forward<InDeleter>(deleter), std::move(allocator)
        );
        detail::updateSharedFromThis(this, ptr);
    }

    /// @brief Constructs a `SharedRef` from a `gp::makeShareable()` proxy.
    /// @tparam InOtherType Pointee type convertible to `InElementType*`.
    /// @tparam InDeleter Deleter type stored in the proxy.
    /// @tparam InAllocator Allocator type stored in the proxy.
    /// @param[in,out] proxy Temporary proxy returned by `makeShareable()`.
    template <
        concepts::IsPointerConvertibleTo<InElementType> InOtherType,
        typename InDeleter,
        concepts::IsAllocator InAllocator>
    constexpr explicit(false) SharedRef(detail::RawPtrProxy<InOtherType, InDeleter, InAllocator>&& proxy) noexcept
        : m_object(proxy.object)
        , m_controller(nullptr)
    {
        GP_ASSERT(proxy.object != nullptr, "SharedRef cannot be constructed from a null makeShareable() pointer.");
        m_controller = detail::createDefaultReferenceController<InMode>(
            proxy.object, std::move(proxy.deleter), std::move(proxy.allocator)
        );
        detail::updateSharedFromThis(this, proxy.object);
    }

    /// @brief Aliasing constructor: shares reference ownership with @p other while pointing to a non-null @p aliasedPtr
    ///        (such as a struct member or base subobject).
    /// @tparam InOtherType Element type of the owning `SharedRef`.
    /// @param[in] other Existing `SharedRef` whose control block will be shared.
    /// @param[in] aliasedPtr Non-null pointer to expose via `get()`, `operator*()`, and `operator->()`.
    template <typename InOtherType>
    constexpr SharedRef(const SharedRef<InOtherType, InMode>& other, PointerType aliasedPtr) noexcept
        : m_object(aliasedPtr)
        , m_controller(other.m_controller)
    {
        GP_ASSERT(aliasedPtr != nullptr, "SharedRef aliasing constructor requires a non-null aliasedPtr.");
        GP_ASSERT(m_controller != nullptr, "SharedRef aliasing constructor requires a valid control block.");
        m_controller->addSharedRef();
    }

    /// @brief Aliasing constructor from a non-null `SharedPtr`: shares reference ownership with @p other while pointing
    ///        to a non-null @p aliasedPtr.
    /// @tparam InOtherType Element type of the owning `SharedPtr`.
    /// @param[in] other Existing non-null `SharedPtr` whose control block will be shared.
    /// @param[in] aliasedPtr Non-null pointer to expose via `get()`, `operator*()`, and `operator->()`.
    template <typename InOtherType>
    constexpr SharedRef(const SharedPtr<InOtherType, InMode>& other, PointerType aliasedPtr) noexcept
        : m_object(aliasedPtr)
        , m_controller(other.m_controller)
    {
        GP_ASSERT(aliasedPtr != nullptr, "SharedRef aliasing constructor requires a non-null aliasedPtr.");
        GP_ASSERT(m_controller != nullptr, "SharedRef aliasing constructor requires a valid non-null SharedPtr.");
        m_controller->addSharedRef();
    }

    /// @brief Aliasing move constructor from a non-null rvalue `SharedPtr`: transfers ownership with zero atomic
    ///        operations while pointing to a non-null @p aliasedPtr.
    /// @tparam InOtherType Element type of the owning `SharedPtr`.
    /// @param[in,out] other Existing non-null `SharedPtr` to move from; reset to `nullptr`.
    /// @param[in] aliasedPtr Non-null pointer to expose via `get()`, `operator*()`, and `operator->()`.
    template <typename InOtherType>
    constexpr SharedRef(SharedPtr<InOtherType, InMode>&& other, PointerType aliasedPtr) noexcept
        : m_object(aliasedPtr)
        , m_controller(other.m_controller)
    {
        GP_ASSERT(aliasedPtr != nullptr, "SharedRef aliasing constructor requires a non-null aliasedPtr.");
        GP_ASSERT(m_controller != nullptr, "SharedRef aliasing constructor requires a valid non-null SharedPtr.");
        other.m_object = nullptr;
        other.m_controller = nullptr;
    }

    /// @brief Copy constructor; increments the shared reference count.
    /// @param[in] other Source `SharedRef` to copy.
    constexpr SharedRef(const SharedRef& other) noexcept
        : m_object(other.m_object)
        , m_controller(other.m_controller)
    {
        GP_ASSERT(m_controller != nullptr, "SharedRef source control block cannot be null.");
        m_controller->addSharedRef();
    }

    /// @brief Converting copy constructor from a compatible `SharedRef<InOtherType, InMode>`.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @param[in] other Source `SharedRef` to copy.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr explicit(false) SharedRef(const SharedRef<InOtherType, InMode>& other) noexcept
        : m_object(other.m_object)
        , m_controller(other.m_controller)
    {
        GP_ASSERT(m_controller != nullptr, "SharedRef source control block cannot be null.");
        m_controller->addSharedRef();
    }

    /// @brief Move constructor; retains a valid non-null state on @p other by copying the reference (mirroring Unreal
    ///        Engine `TSharedRef`, which can never leave a moved-from `SharedRef` null).
    /// @note Moving a `SharedRef` into a nullable `SharedPtr` transfers ownership with zero atomic operations.
    /// @param[in] other Source `SharedRef`.
    constexpr SharedRef(SharedRef&& other) noexcept
        : m_object(other.m_object)
        , m_controller(other.m_controller)
    {
        GP_ASSERT(m_controller != nullptr, "SharedRef source control block cannot be null.");
        m_controller->addSharedRef();
    }

    /// @brief Converting move constructor from a compatible `SharedRef<InOtherType, InMode>`.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @param[in] other Source `SharedRef`.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr explicit(false) SharedRef(SharedRef<InOtherType, InMode>&& other) noexcept
        : m_object(other.m_object)
        , m_controller(other.m_controller)
    {
        GP_ASSERT(m_controller != nullptr, "SharedRef source control block cannot be null.");
        m_controller->addSharedRef();
    }

    /// @brief Internal constructor adopting an already-incremented `(inObject, inController)` pair.
    /// @param[in] inObject Non-null object pointer.
    /// @param[in] inController Non-null reference controller with an active strong reference.
    /// @param[in] tag Internal construction tag.
    constexpr SharedRef(
        PointerType inObject,
        detail::ReferenceControllerBase<InMode>* inController,
        [[maybe_unused]] const detail::SharedPointerInternalsTag tag
    ) noexcept
        : m_object(inObject)
        , m_controller(inController)
    {
        GP_ASSERT(m_object != nullptr && m_controller != nullptr, "SharedRef internal construction must be non-null.");
    }

    /// @brief Destructor; decrements the strong reference count if not moved into a `SharedPtr`.
    GP_FORCEINLINE_HINT constexpr ~SharedRef() noexcept
    {
        detail::ReferenceControllerBase<InMode>* oldController = m_controller;
        m_object = nullptr;
        m_controller = nullptr;
        if (oldController != nullptr)
        {
            oldController->releaseSharedRef();
        }
    }

public:
    /// @brief Copy assignment operator; retains @p rhs before releasing the previously referenced object.
    /// @param[in] rhs Source `SharedRef` to copy.
    /// @return Reference to `*this`.
    constexpr SharedRef& operator=(const SharedRef& rhs) noexcept
    {
        GP_ASSERT(rhs.m_object != nullptr && rhs.m_controller != nullptr, "SharedRef rhs must be valid.");
        rhs.m_controller->addSharedRef();
        detail::ReferenceControllerBase<InMode>* oldController = m_controller;
        m_object = rhs.m_object;
        m_controller = rhs.m_controller;
        if (oldController != nullptr)
        {
            oldController->releaseSharedRef();
        }
        return *this;
    }

    /// @brief Converting copy assignment operator from a compatible `SharedRef<InOtherType, InMode>`.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @param[in] rhs Source `SharedRef` to copy.
    /// @return Reference to `*this`.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr SharedRef& operator=(const SharedRef<InOtherType, InMode>& rhs) noexcept
    {
        GP_ASSERT(rhs.m_object != nullptr && rhs.m_controller != nullptr, "SharedRef rhs must be valid.");
        rhs.m_controller->addSharedRef();
        detail::ReferenceControllerBase<InMode>* oldController = m_controller;
        m_object = rhs.m_object;
        m_controller = rhs.m_controller;
        if (oldController != nullptr)
        {
            oldController->releaseSharedRef();
        }
        return *this;
    }

    /// @brief Move assignment operator; preserves non-nullability on both `*this` and @p rhs.
    /// @param[in,out] rhs Source `SharedRef`.
    /// @return Reference to `*this`.
    constexpr SharedRef& operator=(SharedRef&& rhs) noexcept
    {
        return *this = static_cast<const SharedRef&>(rhs);
    }

    /// @brief Converting move assignment operator from a compatible `SharedRef<InOtherType, InMode>`.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @param[in,out] rhs Source `SharedRef`.
    /// @return Reference to `*this`.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr SharedRef& operator=(SharedRef<InOtherType, InMode>&& rhs) noexcept
    {
        return (*this = static_cast<const SharedRef<InOtherType, InMode>&>(rhs));
    }

public:
    /// @brief Returns the non-null raw pointer to the referenced object.
    /// @return Non-null pointer to `InElementType`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr PointerType get() const noexcept
    {
        GP_ASSERT(m_object != nullptr, "Dereferencing a moved-from or corrupted SharedRef.");
        return m_object;
    }

    /// @brief Member access operator to the referenced object.
    /// @return Non-null pointer to `InElementType`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr PointerType operator->() const noexcept
    {
        GP_ASSERT(m_object != nullptr, "Dereferencing a moved-from or corrupted SharedRef via operator->().");
        return m_object;
    }

    /// @brief Dereferences the managed object.
    /// @return Reference to `InElementType`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr std::add_lvalue_reference_t<InElementType> operator*() const noexcept
        requires(!concepts::IsVoid<InElementType>)
    {
        GP_ASSERT(m_object != nullptr, "Dereferencing a moved-from or corrupted SharedRef via operator*().");
        return *m_object;
    }

    /// @brief Always returns `true` for a valid `SharedRef`.
    /// @return `true`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isValid() const noexcept
    {
        return m_object != nullptr;
    }

    /// @brief Explicit boolean conversion (`true` for any live `SharedRef`).
    /// @return `true`.
    [[nodiscard]] GP_FORCEINLINE_HINT explicit constexpr operator bool() const noexcept
    {
        return m_object != nullptr;
    }

    /// @brief Queries the number of active strong references (`SharedRef` + `SharedPtr`) sharing this object.
    /// @return Live strong reference count (`>= 1`).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 getSharedReferenceCount() const noexcept
    {
        return m_controller != nullptr ? m_controller->getSharedReferenceCount() : 0u;
    }

    /// @brief Alias for `getSharedReferenceCount()` matching `RefCountedPtr::getRefCount()`.
    /// @return Live strong reference count (`>= 1`).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 getRefCount() const noexcept
    {
        return getSharedReferenceCount();
    }

    /// @brief Checks whether this `SharedRef` holds the sole strong reference (`getSharedReferenceCount() == 1`).
    /// @return `true` if `getSharedReferenceCount() == 1u`; `false` otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isUnique() const noexcept
    {
        return getSharedReferenceCount() == 1u;
    }

    /// @brief Converts this `SharedRef` into a nullable `SharedPtr` retaining a shared reference.
    /// @return `SharedPtr<InElementType, InMode>` pointing to the same object.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedPtr<InElementType, InMode> toSharedPtr() const& noexcept;

    /// @brief Converts an rvalue `SharedRef` into a nullable `SharedPtr` with zero atomic reference-count operations.
    /// @return `SharedPtr<InElementType, InMode>` adopting the strong reference from `*this`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedPtr<InElementType, InMode> toSharedPtr() && noexcept;

    /// @brief Creates a `WeakPtr` observing this `SharedRef`.
    /// @return `WeakPtr<InElementType, InMode>` bound to this object.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr WeakPtr<InElementType, InMode> toWeakPtr() const noexcept;

    /// @brief Exchanges the referenced objects and control blocks of `*this` and @p other with zero atomic operations.
    /// @param[in,out] other Other `SharedRef` to swap with.
    constexpr void swap(SharedRef& other) noexcept
    {
        using std::swap;
        swap(m_object, other.m_object);
        swap(m_controller, other.m_controller);
    }

    /// @brief Checks whether `*this` and @p other share the exact same underlying control block (even if aliased to
    ///        different subobject pointers).
    /// @tparam InOtherType Element type of @p other.
    /// @param[in] other Other `SharedRef` to compare ownership against.
    /// @return `true` if both share the same control block; `false` otherwise.
    template <typename InOtherType>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        hasSameOwner(const SharedRef<InOtherType, InMode>& other) const noexcept
    {
        return m_controller == other.m_controller;
    }

    /// @brief Checks whether `*this` and @p other `SharedPtr` share the exact same underlying control block.
    /// @tparam InOtherType Element type of @p other.
    /// @param[in] other `SharedPtr` to compare ownership against.
    /// @return `true` if both share the same control block; `false` otherwise.
    template <typename InOtherType>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        hasSameOwner(const SharedPtr<InOtherType, InMode>& other) const noexcept;

    /// @brief Checks whether `*this` and @p other `WeakPtr` share the exact same underlying control block.
    /// @tparam InOtherType Element type of @p other.
    /// @param[in] other `WeakPtr` to compare ownership against.
    /// @return `true` if both share the same control block; `false` otherwise.
    template <typename InOtherType>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        hasSameOwner(const WeakPtr<InOtherType, InMode>& other) const noexcept;

    /// @brief Strict weak ordering based on control-block address rather than pointee address.
    /// @tparam InOtherType Element type of @p other.
    /// @param[in] other Other `SharedRef` to order against.
    /// @return `true` if this control block address precedes `other`'s control block address.
    template <typename InOtherType>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        ownerBefore(const SharedRef<InOtherType, InMode>& other) const noexcept
    {
        return std::compare_three_way{}(m_controller, other.m_controller) < 0;
    }

    /// @brief Strict weak ordering based on control-block address against a `SharedPtr`.
    /// @tparam InOtherType Element type of @p other.
    /// @param[in] other `SharedPtr` to order against.
    /// @return `true` if this control block address precedes `other`'s control block address.
    template <typename InOtherType>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        ownerBefore(const SharedPtr<InOtherType, InMode>& other) const noexcept;

    /// @brief Strict weak ordering based on control-block address against a `WeakPtr`.
    /// @tparam InOtherType Element type of @p other.
    /// @param[in] other `WeakPtr` to order against.
    /// @return `true` if this control block address precedes `other`'s control block address.
    template <typename InOtherType>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        ownerBefore(const WeakPtr<InOtherType, InMode>& other) const noexcept;

    /// @brief Computes a deterministic pointer hash of `m_object` for hash maps and sets.
    /// @return 64-bit pointer hash (`1` during `consteval`).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::USize getHash() const noexcept
    {
        if (m_object == nullptr)
        {
            return 0u;
        }
        if consteval
        {
            return 1u;
        }
        else
        {
            return static_cast<gp::USize>(gp::hashing::hashPointer(m_object));
        }
    }
};

/// @brief Nullable 16-byte non-intrusive shared smart pointer backed by an external or in-place reference controller.
/// @details Modeled after Unreal Engine's `TSharedPtr<ObjectType, Mode>` and `eastl::shared_ptr` ([ADR-0059]).
///          Supports configurable `RefCountThreadSafety` (`ThreadSafe` lock-free atomics or `SingleThreaded` plain
///          integers), subobject aliasing, custom deleters, `gp::concepts::IsAllocator` control-block allocation,
///          `WeakPtr` weak observation, and seamless conversion from `SharedRef`.
/// @tparam InElementType Managed object type.
/// @tparam InMode Thread-safety policy (`ThreadSafe` by default, or `SingleThreaded`).
/// @see SharedRef, WeakPtr, SharedFromThis, makeShared, allocateShared, makeShareable
template <typename InElementType, RefCountThreadSafety InMode>
class SharedPtr
{
private:
    template <typename InOtherType, RefCountThreadSafety InOtherMode>
    friend class SharedRef;

    template <typename InOtherType, RefCountThreadSafety InOtherMode>
    friend class SharedPtr;

    template <typename InOtherType, RefCountThreadSafety InOtherMode>
    friend class WeakPtr;

    template <typename InOtherType, RefCountThreadSafety InOtherMode>
    friend class SharedFromThis;

public:
    /// @brief Managed object type.
    using ElementType = InElementType;

    /// @brief Raw pointer type to `InElementType`.
    using PointerType = InElementType*;

    /// @brief Compile-time thread-safety policy mode.
    static constexpr RefCountThreadSafety kThreadSafetyMode = InMode;

    /// @brief Compile-time flag indicating whether reference counting is atomic.
    static constexpr bool kIsThreadSafe = (InMode == RefCountThreadSafety::ThreadSafe);

private:
    PointerType m_object{ nullptr };
    detail::ReferenceControllerBase<InMode>* m_controller{ nullptr };

public:
    /// @brief Default constructor; initializes a null `SharedPtr` with zero allocations or atomic operations.
    constexpr SharedPtr() noexcept = default;

    /// @brief Constructs a null `SharedPtr` from `nullptr`.
    /// @param[in] nullValue Null pointer literal.
    constexpr explicit(false) SharedPtr([[maybe_unused]] gp::NullPtrT nullValue) noexcept
        : m_object(nullptr)
        , m_controller(nullptr)
    {}

    /// @brief Takes shared ownership of @p ptr using `DefaultDelete<InOtherType>`.
    /// @details If `ptr == nullptr` or control-block allocation fails, leaves the `SharedPtr` empty.
    /// @tparam InOtherType Pointee type convertible to `InElementType*`.
    /// @param[in] ptr Raw pointer to manage (may be `nullptr`).
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    explicit constexpr SharedPtr(InOtherType* ptr) noexcept
        : m_object(nullptr)
        , m_controller(nullptr)
    {
        if (ptr != nullptr)
        {
            m_controller = detail::createDefaultReferenceController<InMode>(ptr);
            if (m_controller != nullptr) [[likely]]
            {
                m_object = ptr;
                detail::updateSharedFromThis(this, ptr);
            }
        }
    }

    /// @brief Takes shared ownership of @p ptr with a custom @p deleter and optional @p allocator.
    /// @tparam InOtherType Pointee type convertible to `InElementType*`.
    /// @tparam InDeleter Custom deleter callable type.
    /// @tparam InAllocator Engine allocator type for the control block.
    /// @param[in] ptr Raw pointer to manage (may be `nullptr`).
    /// @param[in] deleter Custom deleter invoked when strong references reach zero.
    /// @param[in] allocator Engine allocator used to allocate the control block.
    template <
        concepts::IsPointerConvertibleTo<InElementType> InOtherType,
        typename InDeleter,
        concepts::IsAllocator InAllocator = gp::memory::HeapAllocator>
    constexpr SharedPtr(InOtherType* ptr, InDeleter&& deleter, InAllocator allocator = InAllocator{}) noexcept
        : m_object(nullptr)
        , m_controller(nullptr)
    {
        if (ptr != nullptr)
        {
            m_controller = detail::createDefaultReferenceController<InMode>(
                ptr, std::forward<InDeleter>(deleter), std::move(allocator)
            );
            if (m_controller != nullptr) [[likely]]
            {
                m_object = ptr;
                detail::updateSharedFromThis(this, ptr);
            }
        }
    }

    /// @brief Constructs a `SharedPtr` from a `gp::makeShareable()` proxy.
    /// @tparam InOtherType Pointee type convertible to `InElementType*`.
    /// @tparam InDeleter Deleter type stored in the proxy.
    /// @tparam InAllocator Allocator type stored in the proxy.
    /// @param[in,out] proxy Temporary proxy returned by `makeShareable()`.
    template <
        concepts::IsPointerConvertibleTo<InElementType> InOtherType,
        typename InDeleter,
        concepts::IsAllocator InAllocator>
    constexpr explicit(false) SharedPtr(detail::RawPtrProxy<InOtherType, InDeleter, InAllocator>&& proxy) noexcept
        : m_object(nullptr)
        , m_controller(nullptr)
    {
        if (proxy.object != nullptr)
        {
            m_controller = detail::createDefaultReferenceController<InMode>(
                proxy.object, std::move(proxy.deleter), std::move(proxy.allocator)
            );
            if (m_controller != nullptr) [[likely]]
            {
                m_object = proxy.object;
                detail::updateSharedFromThis(this, proxy.object);
            }
        }
    }

    /// @brief Aliasing copy constructor: shares ownership with @p other while exposing @p aliasedPtr.
    /// @tparam InOtherType Element type of the owning `SharedPtr`.
    /// @param[in] other Source `SharedPtr` whose control block will be shared.
    /// @param[in] aliasedPtr Pointer to expose via `get()`, `operator*()`, and `operator->()`.
    template <typename InOtherType>
    constexpr SharedPtr(const SharedPtr<InOtherType, InMode>& other, PointerType aliasedPtr) noexcept
        : m_object(other.m_controller != nullptr ? aliasedPtr : nullptr)
        , m_controller(other.m_controller)
    {
        if (m_controller != nullptr)
        {
            m_controller->addSharedRef();
        }
    }

    /// @brief Aliasing move constructor: transfers ownership from @p other while exposing @p aliasedPtr.
    /// @tparam InOtherType Element type of the owning `SharedPtr`.
    /// @param[in,out] other Source `SharedPtr` to move from; reset to `nullptr`.
    /// @param[in] aliasedPtr Pointer to expose via `get()`, `operator*()`, and `operator->()`.
    template <typename InOtherType>
    constexpr SharedPtr(SharedPtr<InOtherType, InMode>&& other, PointerType aliasedPtr) noexcept
        : m_object(other.m_controller != nullptr ? aliasedPtr : nullptr)
        , m_controller(other.m_controller)
    {
        other.m_object = nullptr;
        other.m_controller = nullptr;
    }

    /// @brief Aliasing constructor from a `SharedRef`: shares ownership with @p other while exposing @p aliasedPtr.
    /// @tparam InOtherType Element type of the owning `SharedRef`.
    /// @param[in] other Source `SharedRef` whose control block will be shared.
    /// @param[in] aliasedPtr Pointer to expose via `get()`, `operator*()`, and `operator->()`.
    template <typename InOtherType>
    constexpr SharedPtr(const SharedRef<InOtherType, InMode>& other, PointerType aliasedPtr) noexcept
        : m_object(aliasedPtr)
        , m_controller(other.m_controller)
    {
        if (m_controller != nullptr)
        {
            m_controller->addSharedRef();
        }
    }

    /// @brief Aliasing move constructor from a `SharedRef`: transfers ownership from @p other with zero atomic
    ///        operations while exposing @p aliasedPtr.
    /// @tparam InOtherType Element type of the owning `SharedRef`.
    /// @param[in,out] other Source rvalue `SharedRef` to move from.
    /// @param[in] aliasedPtr Pointer to expose via `get()`, `operator*()`, and `operator->()`.
    template <typename InOtherType>
    constexpr SharedPtr(SharedRef<InOtherType, InMode>&& other, PointerType aliasedPtr) noexcept
        : m_object(other.m_controller != nullptr ? aliasedPtr : nullptr)
        , m_controller(other.m_controller)
    {
        other.m_object = nullptr;
        other.m_controller = nullptr;
    }

    /// @brief Implicitly constructs a `SharedPtr` by copying a `SharedRef<InOtherType, InMode>`.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @param[in] sharedRef Source `SharedRef` to retain.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr explicit(false) SharedPtr(const SharedRef<InOtherType, InMode>& sharedRef) noexcept
        : m_object(sharedRef.m_object)
        , m_controller(sharedRef.m_controller)
    {
        if (m_controller != nullptr)
        {
            m_controller->addSharedRef();
        }
    }

    /// @brief Implicitly constructs a `SharedPtr` from an rvalue `SharedRef<InOtherType, InMode>` with zero atomic
    ///        operations (transfers the strong reference directly from the temporary `SharedRef`).
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @param[in,out] sharedRef Rvalue `SharedRef` (such as the return value of `makeShared`).
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr explicit(false) SharedPtr(SharedRef<InOtherType, InMode>&& sharedRef) noexcept
        : m_object(sharedRef.m_object)
        , m_controller(sharedRef.m_controller)
    {
        sharedRef.m_object = nullptr;
        sharedRef.m_controller = nullptr;
    }

    /// @brief Copy constructor; retains a new strong reference if @p other is non-null.
    /// @param[in] other Source `SharedPtr` to copy.
    constexpr SharedPtr(const SharedPtr& other) noexcept
        : m_object(other.m_object)
        , m_controller(other.m_controller)
    {
        if (m_controller != nullptr)
        {
            m_controller->addSharedRef();
        }
    }

    /// @brief Converting copy constructor from a compatible `SharedPtr<InOtherType, InMode>`.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @param[in] other Source `SharedPtr` to copy.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr explicit(false) SharedPtr(const SharedPtr<InOtherType, InMode>& other) noexcept
        : m_object(other.m_object)
        , m_controller(other.m_controller)
    {
        if (m_controller != nullptr)
        {
            m_controller->addSharedRef();
        }
    }

    /// @brief Move constructor; transfers ownership from @p other with zero reference-count operations.
    /// @param[in,out] other Source `SharedPtr` to move from; reset to `nullptr`.
    constexpr SharedPtr(SharedPtr&& other) noexcept
        : m_object(other.m_object)
        , m_controller(other.m_controller)
    {
        other.m_object = nullptr;
        other.m_controller = nullptr;
    }

    /// @brief Converting move constructor from a compatible `SharedPtr<InOtherType, InMode>` with zero reference-count
    ///        operations.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @param[in,out] other Source `SharedPtr` to move from; reset to `nullptr`.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr explicit(false) SharedPtr(SharedPtr<InOtherType, InMode>&& other) noexcept
        : m_object(other.m_object)
        , m_controller(other.m_controller)
    {
        other.m_object = nullptr;
        other.m_controller = nullptr;
    }

    /// @brief Internal constructor adopting an already-incremented `(inObject, inController)` pair.
    /// @param[in] inObject Raw object pointer (or `nullptr`).
    /// @param[in] inController Reference controller with an active strong reference (or `nullptr`).
    /// @param[in] tag Internal construction tag.
    constexpr SharedPtr(
        PointerType inObject,
        detail::ReferenceControllerBase<InMode>* inController,
        [[maybe_unused]] const detail::SharedPointerInternalsTag tag
    ) noexcept
        : m_object(inObject)
        , m_controller(inController)
    {}

    /// @brief Destructor; decrements the strong reference count if non-null.
    /// @details Clears `m_object` and `m_controller` prior to invoking `releaseSharedRef()` to guarantee reentrancy
    ///          safety if a destructor inspects `*this`.
    GP_FORCEINLINE_HINT constexpr ~SharedPtr() noexcept
    {
        detail::ReferenceControllerBase<InMode>* oldController = m_controller;
        m_object = nullptr;
        m_controller = nullptr;
        if (oldController != nullptr)
        {
            oldController->releaseSharedRef();
        }
    }

public:
    /// @brief Copy assignment operator; increments @p rhs before releasing the previously managed object.
    /// @details Safe under both direct self-assignment (`ptr = ptr`) and hierarchical aliasing (`node = node->child`).
    /// @param[in] rhs Source `SharedPtr` to copy.
    /// @return Reference to `*this`.
    constexpr SharedPtr& operator=(const SharedPtr& rhs) noexcept
    {
        detail::ReferenceControllerBase<InMode>* newController = rhs.m_controller;
        PointerType newObject = rhs.m_object;
        if (newController != nullptr)
        {
            newController->addSharedRef();
        }

        detail::ReferenceControllerBase<InMode>* oldController = m_controller;
        m_object = newObject;
        m_controller = newController;
        if (oldController != nullptr)
        {
            oldController->releaseSharedRef();
        }

        return *this;
    }

    /// @brief Converting copy assignment operator from a compatible `SharedPtr<InOtherType, InMode>`.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @param[in] rhs Source `SharedPtr` to copy.
    /// @return Reference to `*this`.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr SharedPtr& operator=(const SharedPtr<InOtherType, InMode>& rhs) noexcept
    {
        detail::ReferenceControllerBase<InMode>* newController = rhs.m_controller;
        PointerType newObject = rhs.m_object;
        if (newController != nullptr)
        {
            newController->addSharedRef();
        }

        detail::ReferenceControllerBase<InMode>* oldController = m_controller;
        m_object = newObject;
        m_controller = newController;
        if (oldController != nullptr)
        {
            oldController->releaseSharedRef();
        }

        return *this;
    }

    /// @brief Move assignment operator; transfers ownership from @p rhs and releases the previous object.
    /// @param[in,out] rhs Source `SharedPtr` to move from; reset to `nullptr`.
    /// @return Reference to `*this`.
    constexpr SharedPtr& operator=(SharedPtr&& rhs) noexcept
    {
        if (this != &rhs)
        {
            detail::ReferenceControllerBase<InMode>* oldController = m_controller;
            m_object = rhs.m_object;
            m_controller = rhs.m_controller;
            rhs.m_object = nullptr;
            rhs.m_controller = nullptr;
            if (oldController != nullptr)
            {
                oldController->releaseSharedRef();
            }
        }
        return *this;
    }

    /// @brief Converting move assignment operator from a compatible `SharedPtr<InOtherType, InMode>`.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @param[in,out] rhs Source `SharedPtr` to move from; reset to `nullptr`.
    /// @return Reference to `*this`.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr SharedPtr& operator=(SharedPtr<InOtherType, InMode>&& rhs) noexcept
    {
        if (static_cast<const void*>(this) != static_cast<const void*>(&rhs))
        {
            detail::ReferenceControllerBase<InMode>* oldController = m_controller;
            m_object = rhs.m_object;
            m_controller = rhs.m_controller;
            rhs.m_object = nullptr;
            rhs.m_controller = nullptr;
            if (oldController != nullptr)
            {
                oldController->releaseSharedRef();
            }
        }
        return *this;
    }

    /// @brief Copy assignment operator from a `SharedRef<InOtherType, InMode>`.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @param[in] rhs Source `SharedRef` to copy.
    /// @return Reference to `*this`.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr SharedPtr& operator=(const SharedRef<InOtherType, InMode>& rhs) noexcept
    {
        detail::ReferenceControllerBase<InMode>* newController = rhs.m_controller;
        PointerType newObject = rhs.m_object;
        if (newController != nullptr)
        {
            newController->addSharedRef();
        }

        detail::ReferenceControllerBase<InMode>* oldController = m_controller;
        m_object = newObject;
        m_controller = newController;
        if (oldController != nullptr)
        {
            oldController->releaseSharedRef();
        }

        return *this;
    }

    /// @brief Move assignment operator from an rvalue `SharedRef<InOtherType, InMode>`.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @param[in,out] rhs Rvalue `SharedRef` to move from.
    /// @return Reference to `*this`.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr SharedPtr& operator=(SharedRef<InOtherType, InMode>&& rhs) noexcept
    {
        detail::ReferenceControllerBase<InMode>* oldController = m_controller;
        m_object = rhs.m_object;
        m_controller = rhs.m_controller;
        rhs.m_object = nullptr;
        rhs.m_controller = nullptr;
        if (oldController != nullptr)
        {
            oldController->releaseSharedRef();
        }
        return *this;
    }

    /// @brief Assigns from a `gp::makeShareable()` proxy.
    /// @tparam InOtherType Pointee type convertible to `InElementType*`.
    /// @tparam InDeleter Deleter type stored in the proxy.
    /// @tparam InAllocator Allocator type stored in the proxy.
    /// @param[in,out] proxy Temporary proxy returned by `makeShareable()`.
    /// @return Reference to `*this`.
    template <
        concepts::IsPointerConvertibleTo<InElementType> InOtherType,
        typename InDeleter,
        concepts::IsAllocator InAllocator>
    constexpr SharedPtr& operator=(detail::RawPtrProxy<InOtherType, InDeleter, InAllocator>&& proxy) noexcept
    {
        SharedPtr temp(std::move(proxy));
        swap(temp);
        return *this;
    }

    /// @brief Resets this `SharedPtr` to `nullptr`, releasing any currently managed object.
    /// @param[in] nullValue Null pointer literal.
    /// @return Reference to `*this`.
    constexpr SharedPtr& operator=([[maybe_unused]] gp::NullPtrT nullValue) noexcept
    {
        reset();
        return *this;
    }

public:
    /// @brief Releases ownership of the managed object and resets this `SharedPtr` to `nullptr`.
    constexpr void reset() noexcept
    {
        detail::ReferenceControllerBase<InMode>* oldController = m_controller;
        m_object = nullptr;
        m_controller = nullptr;
        if (oldController != nullptr)
        {
            oldController->releaseSharedRef();
        }
    }

    /// @brief Replaces the managed object with @p ptr using `DefaultDelete<InOtherType>`.
    /// @tparam InOtherType Pointee type convertible to `InElementType*`.
    /// @param[in] ptr New raw pointer to manage (may be `nullptr`).
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr void reset(InOtherType* ptr) noexcept
    {
        SharedPtr temp(ptr);
        swap(temp);
    }

    /// @brief Replaces the managed object with @p ptr using a custom @p deleter and optional @p allocator.
    /// @tparam InOtherType Pointee type convertible to `InElementType*`.
    /// @tparam InDeleter Custom deleter callable type.
    /// @tparam InAllocator Engine allocator type for the control block.
    /// @param[in] ptr New raw pointer to manage (may be `nullptr`).
    /// @param[in] deleter Custom deleter callable.
    /// @param[in] allocator Engine allocator for the control block.
    template <
        concepts::IsPointerConvertibleTo<InElementType> InOtherType,
        typename InDeleter,
        concepts::IsAllocator InAllocator = gp::memory::HeapAllocator>
    constexpr void reset(InOtherType* ptr, InDeleter&& deleter, InAllocator allocator = InAllocator{}) noexcept
    {
        SharedPtr temp(ptr, std::forward<InDeleter>(deleter), std::move(allocator));
        swap(temp);
    }

    /// @brief Exchanges the managed pointers and control blocks of `*this` and @p other with zero atomic operations.
    /// @param[in,out] other Other `SharedPtr` to swap with.
    constexpr void swap(SharedPtr& other) noexcept
    {
        using std::swap;
        swap(m_object, other.m_object);
        swap(m_controller, other.m_controller);
    }

    /// @brief Returns the raw pointer to the managed object without altering its reference count.
    /// @return Managed raw pointer, or `nullptr` if empty.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr PointerType get() const noexcept
    {
        return m_object;
    }

    /// @brief Member access operator to the managed object.
    /// @note Triggers `GP_ASSERT` in checked builds if the `SharedPtr` is null.
    /// @return Raw pointer to the managed object.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr PointerType operator->() const noexcept
    {
        GP_ASSERT(m_object != nullptr, "Attempting to dereference a null SharedPtr via operator->().");
        return m_object;
    }

    /// @brief Dereferences the managed object.
    /// @note Triggers `GP_ASSERT` in checked builds if the `SharedPtr` is null.
    /// @return Reference to the managed object.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr std::add_lvalue_reference_t<InElementType> operator*() const noexcept
        requires(!concepts::IsVoid<InElementType>)
    {
        GP_ASSERT(m_object != nullptr, "Attempting to dereference a null SharedPtr via operator*().");
        return *m_object;
    }

    /// @brief Checks whether this `SharedPtr` points to a non-null object (`m_object != nullptr`).
    /// @return `true` if `m_object != nullptr`; `false` otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isValid() const noexcept
    {
        return m_object != nullptr;
    }

    /// @brief Explicit boolean conversion operator (`isValid()`).
    /// @return `true` if `m_object != nullptr`; `false` otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT explicit constexpr operator bool() const noexcept
    {
        return isValid();
    }

    /// @brief Queries the number of active strong references (`SharedRef` + `SharedPtr`) sharing this control block.
    /// @return Live strong reference count, or `0` if empty.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 getSharedReferenceCount() const noexcept
    {
        return m_controller != nullptr ? m_controller->getSharedReferenceCount() : 0u;
    }

    /// @brief Alias for `getSharedReferenceCount()` matching `RefCountedPtr::getRefCount()`.
    /// @return Live strong reference count, or `0` if empty.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 getRefCount() const noexcept
    {
        return getSharedReferenceCount();
    }

    /// @brief Checks whether this `SharedPtr` holds the sole strong reference (`getSharedReferenceCount() == 1`).
    /// @return `true` if non-empty and `getSharedReferenceCount() == 1u`; `false` otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isUnique() const noexcept
    {
        return getSharedReferenceCount() == 1u;
    }

    /// @brief Promotes this `SharedPtr` to a non-nullable `SharedRef`, asserting that `isValid()` is `true`.
    /// @return `SharedRef<InElementType, InMode>` retaining a strong reference to the managed object.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedRef<InElementType, InMode> toSharedRef() const& noexcept
    {
        GP_ASSERT(
            m_object != nullptr && m_controller != nullptr, "Cannot convert a null SharedPtr to a non-null SharedRef."
        );
        m_controller->addSharedRef();
        return SharedRef<InElementType, InMode>(m_object, m_controller, detail::kSharedInternalsTag);
    }

    /// @brief Promotes an rvalue `SharedPtr` to a non-nullable `SharedRef` with zero atomic operations.
    /// @return `SharedRef<InElementType, InMode>` adopting the strong reference from `*this`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedRef<InElementType, InMode> toSharedRef() && noexcept
    {
        GP_ASSERT(
            m_object != nullptr && m_controller != nullptr, "Cannot convert a null SharedPtr to a non-null SharedRef."
        );
        PointerType obj = m_object;
        detail::ReferenceControllerBase<InMode>* ctrl = m_controller;
        m_object = nullptr;
        m_controller = nullptr;
        return SharedRef<InElementType, InMode>(obj, ctrl, detail::kSharedInternalsTag);
    }

    /// @brief Creates a `WeakPtr` observing this `SharedPtr`.
    /// @return `WeakPtr<InElementType, InMode>` bound to this object (or empty if null).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr WeakPtr<InElementType, InMode> toWeakPtr() const noexcept;

    /// @brief Checks whether `*this` and @p other share the exact same underlying control block.
    /// @tparam InOtherType Element type of @p other.
    /// @param[in] other Other `SharedPtr` to compare ownership against.
    /// @return `true` if both share the same control block; `false` otherwise.
    template <typename InOtherType>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        hasSameOwner(const SharedPtr<InOtherType, InMode>& other) const noexcept
    {
        return m_controller == other.m_controller;
    }

    /// @brief Checks whether `*this` and @p other `SharedRef` share the exact same underlying control block.
    /// @tparam InOtherType Element type of @p other.
    /// @param[in] other `SharedRef` to compare ownership against.
    /// @return `true` if both share the same control block; `false` otherwise.
    template <typename InOtherType>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        hasSameOwner(const SharedRef<InOtherType, InMode>& other) const noexcept
    {
        return m_controller == other.m_controller;
    }

    /// @brief Checks whether `*this` and @p other `WeakPtr` share the exact same underlying control block.
    /// @tparam InOtherType Element type of @p other.
    /// @param[in] other `WeakPtr` to compare ownership against.
    /// @return `true` if both share the same control block; `false` otherwise.
    template <typename InOtherType>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        hasSameOwner(const WeakPtr<InOtherType, InMode>& other) const noexcept;

    /// @brief Strict weak ordering based on control-block address rather than pointee address.
    /// @tparam InOtherType Element type of @p other.
    /// @param[in] other Other `SharedPtr` to order against.
    /// @return `true` if this control block address precedes `other`'s control block address.
    template <typename InOtherType>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        ownerBefore(const SharedPtr<InOtherType, InMode>& other) const noexcept
    {
        return std::compare_three_way{}(m_controller, other.m_controller) < 0;
    }

    /// @brief Strict weak ordering based on control-block address against a `SharedRef`.
    /// @tparam InOtherType Element type of @p other.
    /// @param[in] other `SharedRef` to order against.
    /// @return `true` if this control block address precedes `other`'s control block address.
    template <typename InOtherType>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        ownerBefore(const SharedRef<InOtherType, InMode>& other) const noexcept
    {
        return std::compare_three_way{}(m_controller, other.m_controller) < 0;
    }

    /// @brief Strict weak ordering based on control-block address against a `WeakPtr`.
    /// @tparam InOtherType Element type of @p other.
    /// @param[in] other `WeakPtr` to order against.
    /// @return `true` if this control block address precedes `other`'s control block address.
    template <typename InOtherType>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        ownerBefore(const WeakPtr<InOtherType, InMode>& other) const noexcept;

    /// @brief Computes a deterministic pointer hash of `m_object` for hash maps and sets.
    /// @return 64-bit pointer hash (`0` if `m_object == nullptr`, `1` during `consteval`).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::USize getHash() const noexcept
    {
        if (m_object == nullptr)
        {
            return 0u;
        }
        if consteval
        {
            return 1u;
        }
        else
        {
            return static_cast<gp::USize>(gp::hashing::hashPointer(m_object));
        }
    }
};

template <typename InElementType, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedPtr<InElementType, InMode>
    SharedRef<InElementType, InMode>::toSharedPtr() const& noexcept
{
    return SharedPtr<InElementType, InMode>(*this);
}

template <typename InElementType, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedPtr<InElementType, InMode>
    SharedRef<InElementType, InMode>::toSharedPtr() && noexcept
{
    return SharedPtr<InElementType, InMode>(std::move(*this));
}

template <typename InElementType, RefCountThreadSafety InMode>
template <typename InOtherType>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
    SharedRef<InElementType, InMode>::hasSameOwner(const SharedPtr<InOtherType, InMode>& other) const noexcept
{
    return m_controller == other.m_controller;
}

template <typename InElementType, RefCountThreadSafety InMode>
template <typename InOtherType>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
    SharedRef<InElementType, InMode>::ownerBefore(const SharedPtr<InOtherType, InMode>& other) const noexcept
{
    return std::compare_three_way{}(m_controller, other.m_controller) < 0;
}

}   // namespace gp

#include "memory/pointers/WeakPtr.hpp"   // IWYU pragma: export

namespace gp
{

template <typename InElementType, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr WeakPtr<InElementType, InMode>
    SharedRef<InElementType, InMode>::toWeakPtr() const noexcept
{
    return WeakPtr<InElementType, InMode>(*this);
}

template <typename InElementType, RefCountThreadSafety InMode>
template <typename InOtherType>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
    SharedRef<InElementType, InMode>::hasSameOwner(const WeakPtr<InOtherType, InMode>& other) const noexcept
{
    return m_controller == other.m_controller;
}

template <typename InElementType, RefCountThreadSafety InMode>
template <typename InOtherType>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
    SharedRef<InElementType, InMode>::ownerBefore(const WeakPtr<InOtherType, InMode>& other) const noexcept
{
    return std::compare_three_way{}(m_controller, other.m_controller) < 0;
}

template <typename InElementType, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr WeakPtr<InElementType, InMode>
    SharedPtr<InElementType, InMode>::toWeakPtr() const noexcept
{
    return WeakPtr<InElementType, InMode>(*this);
}

template <typename InElementType, RefCountThreadSafety InMode>
template <typename InOtherType>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
    SharedPtr<InElementType, InMode>::hasSameOwner(const WeakPtr<InOtherType, InMode>& other) const noexcept
{
    return m_controller == other.m_controller;
}

template <typename InElementType, RefCountThreadSafety InMode>
template <typename InOtherType>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
    SharedPtr<InElementType, InMode>::ownerBefore(const WeakPtr<InOtherType, InMode>& other) const noexcept
{
    return std::compare_three_way{}(m_controller, other.m_controller) < 0;
}

/// @brief CRTP mixin enabling an object managed by `SharedRef` or `SharedPtr` to safely generate additional
///        `SharedRef`, `SharedPtr`, or `WeakPtr` instances from `this`.
/// @details Modeled after Unreal Engine's `TSharedFromThis<ObjectType, Mode>` and `std::enable_shared_from_this`.
///          Embeds a mutable `WeakPtr<InElementType, InMode> m_weakThis` that is automatically bound whenever a
///          `SharedRef` or `SharedPtr` takes ownership of the derived object.
/// @note Never call `asShared()` or `asWeak()` inside `InElementType`'s constructor before the outer control block has
///       finished initializing `m_weakThis`.
/// @tparam InElementType Derived class type inheriting from `SharedFromThis`.
/// @tparam InMode Thread-safety policy (`ThreadSafe` by default, or `SingleThreaded`).
template <typename InElementType, RefCountThreadSafety InMode>
class SharedFromThis
{
private:
    template <typename InOtherType, RefCountThreadSafety InOtherMode>
    friend class SharedRef;

    template <typename InOtherType, RefCountThreadSafety InOtherMode>
    friend class SharedPtr;

    template <RefCountThreadSafety InHelperMode, typename InOwnerType, typename InObjectType, typename InBaseType>
    requires concepts::IsPointerConvertibleTo<InObjectType, InBaseType>
    friend constexpr void detail::enableSharedFromThisHelper(
        const SharedPtr<InOwnerType, InHelperMode>* owner,
        const InObjectType* inObject,
        const SharedFromThis<InBaseType, InHelperMode>* inShareable
    ) noexcept;

    template <RefCountThreadSafety InHelperMode, typename InOwnerType, typename InObjectType, typename InBaseType>
    requires concepts::IsPointerConvertibleTo<InObjectType, InBaseType>
    friend constexpr void detail::enableSharedFromThisHelper(
        const SharedRef<InOwnerType, InHelperMode>* owner,
        const InObjectType* inObject,
        const SharedFromThis<InBaseType, InHelperMode>* inShareable
    ) noexcept;

private:
    mutable WeakPtr<InElementType, InMode> m_weakThis{};

protected:
    /// @brief Default constructor; leaves `m_weakThis` empty until a `SharedRef` or `SharedPtr` binds ownership.
    constexpr SharedFromThis() noexcept = default;

    /// @brief Copy constructor; does not copy `other.m_weakThis` because the new copy has its own independent lifetime.
    /// @param[in] other Source object being copied.
    constexpr SharedFromThis([[maybe_unused]] const SharedFromThis& other) noexcept
        : m_weakThis()
    {}

    /// @brief Move constructor; does not steal `other.m_weakThis`.
    /// @param[in] other Source object being moved.
    constexpr SharedFromThis([[maybe_unused]] SharedFromThis&& other) noexcept
        : m_weakThis()
    {}

    /// @brief Copy assignment operator; preserves `this->m_weakThis`.
    /// @param[in] other Source object being copied.
    /// @return Reference to `*this`.
    constexpr SharedFromThis& operator=([[maybe_unused]] const SharedFromThis& other) noexcept
    {
        return *this;
    }

    /// @brief Move assignment operator; preserves `this->m_weakThis`.
    /// @param[in] other Source object being moved.
    /// @return Reference to `*this`.
    constexpr SharedFromThis& operator=([[maybe_unused]] SharedFromThis&& other) noexcept
    {
        return *this;
    }

    /// @brief Protected destructor preventing deletion through a raw `SharedFromThis*` base pointer.
    constexpr ~SharedFromThis() noexcept = default;

public:
    /// @brief Returns a non-nullable `SharedRef<InElementType, InMode>` sharing ownership of `*this`.
    /// @note Asserts via `GP_ASSERT` if `*this` is not currently owned by a live `SharedRef` or `SharedPtr`.
    /// @return `SharedRef<InElementType, InMode>` referencing `*this`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedRef<InElementType, InMode> asShared() noexcept
    {
        SharedPtr<InElementType, InMode> pinned = m_weakThis.pin();
        GP_ASSERT(
            pinned.isValid(),
            "SharedFromThis::asShared() called on an object not currently owned by a SharedRef or SharedPtr."
        );
        return std::move(pinned).toSharedRef();
    }

    /// @brief Returns a non-nullable `SharedRef<const InElementType, InMode>` sharing ownership of `*this`.
    /// @note Asserts via `GP_ASSERT` if `*this` is not currently owned by a live `SharedRef` or `SharedPtr`.
    /// @return `SharedRef<const InElementType, InMode>` referencing `*this`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedRef<const InElementType, InMode> asShared() const noexcept
    {
        SharedPtr<const InElementType, InMode> pinned = m_weakThis.pin();
        GP_ASSERT(
            pinned.isValid(),
            "SharedFromThis::asShared() const called on an object not currently owned by a SharedRef or SharedPtr."
        );
        return std::move(pinned).toSharedRef();
    }

    /// @brief Returns a `SharedPtr<InElementType, InMode>` sharing ownership of `*this` (STL-compatible alias).
    /// @note Asserts via `GP_ASSERT` if `*this` is not currently owned by a live `SharedRef` or `SharedPtr`.
    /// @return `SharedPtr<InElementType, InMode>` referencing `*this`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedPtr<InElementType, InMode> sharedFromThis() noexcept
    {
        SharedPtr<InElementType, InMode> pinned = m_weakThis.pin();
        GP_ASSERT(
            pinned.isValid(),
            "SharedFromThis::sharedFromThis() called on an object not currently owned by a SharedRef or SharedPtr."
        );
        return pinned;
    }

    /// @brief Returns a `SharedPtr<const InElementType, InMode>` sharing ownership of `*this` (STL-compatible alias).
    /// @note Asserts via `GP_ASSERT` if `*this` is not currently owned by a live `SharedRef` or `SharedPtr`.
    /// @return `SharedPtr<const InElementType, InMode>` referencing `*this`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedPtr<const InElementType, InMode> sharedFromThis() const noexcept
    {
        SharedPtr<InElementType, InMode> pinned = m_weakThis.pin();
        GP_ASSERT(
            pinned.isValid(),
            "SharedFromThis::sharedFromThis() const called on an object not currently owned by a SharedRef or SharedPtr."
        );
        return SharedPtr<const InElementType, InMode>(std::move(pinned));
    }

    /// @brief Returns a `WeakPtr<InElementType, InMode>` observing `*this`.
    /// @return `WeakPtr<InElementType, InMode>` bound to `*this`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr WeakPtr<InElementType, InMode> asWeak() noexcept
    {
        return m_weakThis;
    }

    /// @brief Returns a `WeakPtr<const InElementType, InMode>` observing `*this`.
    /// @return `WeakPtr<const InElementType, InMode>` bound to `*this`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr WeakPtr<const InElementType, InMode> asWeak() const noexcept
    {
        return WeakPtr<const InElementType, InMode>(m_weakThis);
    }

    /// @brief Returns a `WeakPtr<InElementType, InMode>` observing `*this` (STL-compatible alias for `asWeak()`).
    /// @return `WeakPtr<InElementType, InMode>` bound to `*this`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr WeakPtr<InElementType, InMode> weakFromThis() noexcept
    {
        return m_weakThis;
    }

    /// @brief Returns a `WeakPtr<const InElementType, InMode>` observing `*this` (STL-compatible alias for `asWeak()`).
    /// @return `WeakPtr<const InElementType, InMode>` bound to `*this`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr WeakPtr<const InElementType, InMode> weakFromThis() const noexcept
    {
        return WeakPtr<const InElementType, InMode>(m_weakThis);
    }

    /// @brief Checks whether `*this` has been bound to a live `SharedRef` or `SharedPtr` control block.
    /// @return `true` if `m_weakThis.isValid()`; `false` otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool doesSharedInstanceExist() const noexcept
    {
        return m_weakThis.isValid();
    }

private:
    /// @brief Updates `m_weakThis` when the first `SharedRef` or `SharedPtr` takes ownership of @p inObject.
    /// @tparam InOwnerType Element type of `owner`.
    /// @tparam InObjectType Concrete derived type convertible to `InElementType*`.
    /// @param[in] owner Newly constructed `SharedPtr`.
    /// @param[in] inObject Raw pointer to the newly managed object.
    template <typename InOwnerType, typename InObjectType>
    constexpr void
        updateWeakThisInternal(const SharedPtr<InOwnerType, InMode>* owner, const InObjectType* inObject) const noexcept
    {
        if (!m_weakThis.isValid())
        {
            m_weakThis = WeakPtr<InElementType, InMode>(
                *owner, const_cast<InElementType*>(static_cast<const InElementType*>(inObject))
            );
        }
    }

    /// @brief Updates `m_weakThis` directly from a `SharedRef` without constructing a temporary `SharedPtr`.
    /// @tparam InOwnerType Element type of `owner`.
    /// @tparam InObjectType Concrete derived type convertible to `InElementType*`.
    /// @param[in] owner Newly constructed `SharedRef`.
    /// @param[in] inObject Raw pointer to the newly managed object.
    template <typename InOwnerType, typename InObjectType>
    constexpr void
        updateWeakThisInternal(const SharedRef<InOwnerType, InMode>* owner, const InObjectType* inObject) const noexcept
    {
        if (!m_weakThis.isValid())
        {
            m_weakThis = WeakPtr<InElementType, InMode>(
                *owner, const_cast<InElementType*>(static_cast<const InElementType*>(inObject))
            );
        }
    }
};

namespace detail
{

template <RefCountThreadSafety InMode, typename InOwnerType, typename InObjectType, typename InBaseType>
requires concepts::IsPointerConvertibleTo<InObjectType, InBaseType> constexpr void enableSharedFromThisHelper(
    const SharedPtr<InOwnerType, InMode>* owner,
    const InObjectType* inObject,
    const SharedFromThis<InBaseType, InMode>* inShareable
) noexcept
{
    if (inShareable != nullptr)
    {
        inShareable->updateWeakThisInternal(owner, inObject);
    }
}

template <RefCountThreadSafety InMode, typename InOwnerType, typename InObjectType, typename InBaseType>
requires concepts::IsPointerConvertibleTo<InObjectType, InBaseType> constexpr void enableSharedFromThisHelper(
    const SharedRef<InOwnerType, InMode>* owner,
    const InObjectType* inObject,
    const SharedFromThis<InBaseType, InMode>* inShareable
) noexcept
{
    if (inShareable != nullptr)
    {
        inShareable->updateWeakThisInternal(owner, inObject);
    }
}

template <RefCountThreadSafety InMode, typename InOwnerType>
constexpr void enableSharedFromThisHelper(const SharedRef<InOwnerType, InMode>*, ...) noexcept
{}

template <RefCountThreadSafety InMode, typename InOwnerType, typename InObjectType>
constexpr void updateSharedFromThis(const SharedRef<InOwnerType, InMode>* owner, InObjectType* inObject) noexcept
{
    if constexpr (std::is_class_v<std::remove_cv_t<InObjectType>>)
    {
        if (inObject != nullptr)
        {
            enableSharedFromThisHelper<InMode>(owner, inObject, inObject);
        }
    }
}

}   // namespace detail

/// @section Comparison & Swap Operators (`SharedRef` and `SharedPtr`)

/// @brief ADL-friendly swap exchanging two `SharedRef` instances with zero atomic operations.
/// @tparam InElementType Managed type.
/// @tparam InMode Thread-safety policy.
/// @param[in,out] lhs First `SharedRef`.
/// @param[in,out] rhs Second `SharedRef`.
template <typename InElementType, RefCountThreadSafety InMode>
GP_FORCEINLINE_HINT constexpr void
    swap(SharedRef<InElementType, InMode>& lhs, SharedRef<InElementType, InMode>& rhs) noexcept
{
    lhs.swap(rhs);
}

/// @brief ADL-friendly swap exchanging two `SharedPtr` instances with zero atomic operations.
/// @tparam InElementType Managed type.
/// @tparam InMode Thread-safety policy.
/// @param[in,out] lhs First `SharedPtr`.
/// @param[in,out] rhs Second `SharedPtr`.
template <typename InElementType, RefCountThreadSafety InMode>
GP_FORCEINLINE_HINT constexpr void
    swap(SharedPtr<InElementType, InMode>& lhs, SharedPtr<InElementType, InMode>& rhs) noexcept
{
    lhs.swap(rhs);
}

/// @brief Equality comparison between two `SharedRef` instances.
template <typename InElementTypeA, typename InElementTypeB, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
    operator==(const SharedRef<InElementTypeA, InMode>& lhs, const SharedRef<InElementTypeB, InMode>& rhs) noexcept
{
    return lhs.get() == rhs.get();
}

/// @brief Three-way comparison between two `SharedRef` instances by pointee address.
template <typename InElementTypeA, typename InElementTypeB, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr std::strong_ordering
    operator<=>(const SharedRef<InElementTypeA, InMode>& lhs, const SharedRef<InElementTypeB, InMode>& rhs) noexcept
{
    return std::compare_three_way{}(lhs.get(), rhs.get());
}

/// @brief Equality comparison between two `SharedPtr` instances.
template <typename InElementTypeA, typename InElementTypeB, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
    operator==(const SharedPtr<InElementTypeA, InMode>& lhs, const SharedPtr<InElementTypeB, InMode>& rhs) noexcept
{
    return lhs.get() == rhs.get();
}

/// @brief Three-way comparison between two `SharedPtr` instances by pointee address.
template <typename InElementTypeA, typename InElementTypeB, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr std::strong_ordering
    operator<=>(const SharedPtr<InElementTypeA, InMode>& lhs, const SharedPtr<InElementTypeB, InMode>& rhs) noexcept
{
    return std::compare_three_way{}(lhs.get(), rhs.get());
}

/// @brief Equality comparison between a `SharedRef` and a `SharedPtr`.
template <typename InElementTypeA, typename InElementTypeB, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
    operator==(const SharedRef<InElementTypeA, InMode>& lhs, const SharedPtr<InElementTypeB, InMode>& rhs) noexcept
{
    return lhs.get() == rhs.get();
}

/// @brief Three-way comparison between a `SharedRef` and a `SharedPtr`.
template <typename InElementTypeA, typename InElementTypeB, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr std::strong_ordering
    operator<=>(const SharedRef<InElementTypeA, InMode>& lhs, const SharedPtr<InElementTypeB, InMode>& rhs) noexcept
{
    return std::compare_three_way{}(lhs.get(), rhs.get());
}

/// @brief Equality comparison between a `SharedPtr` and `nullptr`.
template <typename InElementType, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
    operator==(const SharedPtr<InElementType, InMode>& lhs, [[maybe_unused]] gp::NullPtrT nullValue) noexcept
{
    return !lhs.isValid();
}

/// @brief Three-way comparison between a `SharedPtr` and `nullptr`.
template <typename InElementType, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr std::strong_ordering
    operator<=>(const SharedPtr<InElementType, InMode>& lhs, [[maybe_unused]] gp::NullPtrT nullValue) noexcept
{
    return std::compare_three_way{}(
        lhs.get(), static_cast<typename SharedPtr<InElementType, InMode>::PointerType>(nullptr)
    );
}

/// @brief Equality comparison between a `SharedPtr` and a raw pointer.
template <typename InElementTypeA, typename InElementTypeB, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
    operator==(const SharedPtr<InElementTypeA, InMode>& lhs, const InElementTypeB* rhs) noexcept
{
    return lhs.get() == rhs;
}

/// @brief Equality comparison between a `SharedRef` and a raw pointer.
template <typename InElementTypeA, typename InElementTypeB, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
    operator==(const SharedRef<InElementTypeA, InMode>& lhs, const InElementTypeB* rhs) noexcept
{
    return lhs.get() == rhs;
}

/// @section Factory Functions (`makeShared`, `allocateShared`, `makeShareable`)

/// @brief Allocates and constructs an object of type `InElementType` and its reference control block in a single
///        contiguous heap allocation, returning a non-nullable `SharedRef<InElementType, InMode>`.
/// @details Modeled after Unreal Engine's `MakeShared<T, Mode>(args...)`. The returned `SharedRef` converts implicitly
///          to `SharedPtr<InElementType, InMode>` with zero atomic operations.
/// @tparam InElementType Non-array, non-const object type to construct.
/// @tparam InMode Thread-safety policy (`ThreadSafe` by default, or `SingleThreaded`).
/// @tparam InArgs Constructor argument types forwarded to `InElementType`.
/// @param[in] args Arguments forwarded to `InElementType`'s constructor.
/// @return `SharedRef<InElementType, InMode>` owning the newly constructed object.
template <typename InElementType, RefCountThreadSafety InMode = RefCountThreadSafety::ThreadSafe, typename... InArgs>
requires(
    !concepts::IsArray<InElementType> && concepts::IsConstructibleWith<std::remove_cv_t<InElementType>, InArgs && ...>
)
[[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedRef<InElementType, InMode> makeShared(InArgs&&... args) noexcept
{
    using CleanType = std::remove_cv_t<InElementType>;
    detail::IntrusiveReferenceController<CleanType, InMode, gp::memory::HeapAllocator>* controller =
        detail::createIntrusiveReferenceController<CleanType, InMode, gp::memory::HeapAllocator>(
            gp::memory::HeapAllocator{}, std::forward<InArgs>(args)...
        );
    CleanType* objPtr = controller->getObjectPtr();
    SharedRef<InElementType, InMode> result(objPtr, controller, detail::kSharedInternalsTag);
    detail::updateSharedFromThis(&result, objPtr);
    return result;
}

/// @brief Allocates and constructs an object of type `InElementType` and its reference control block in a single
///        contiguous allocation from @p allocator (`gp::concepts::IsAllocator`), returning a `SharedRef`.
/// @tparam InElementType Non-array object type to construct.
/// @tparam InMode Thread-safety policy (`ThreadSafe` by default, or `SingleThreaded`).
/// @tparam InAllocator Engine allocator type satisfying `gp::concepts::IsAllocator`.
/// @tparam InArgs Constructor argument types forwarded to `InElementType`.
/// @param[in,out] allocator Engine allocator instance used to allocate and later free the combined block.
/// @param[in] args Arguments forwarded to `InElementType`'s constructor.
/// @return `SharedRef<InElementType, InMode>` owning the newly constructed object.
template <
    typename InElementType,
    RefCountThreadSafety InMode = RefCountThreadSafety::ThreadSafe,
    concepts::IsAllocator InAllocator,
    typename... InArgs>
requires(
    !concepts::IsArray<InElementType> && concepts::IsConstructibleWith<std::remove_cv_t<InElementType>, InArgs && ...>
) [[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedRef<InElementType, InMode>
    allocateShared(InAllocator allocator, InArgs&&... args) noexcept
{
    using CleanType = std::remove_cv_t<InElementType>;
    detail::IntrusiveReferenceController<CleanType, InMode, InAllocator>* controller =
        detail::createIntrusiveReferenceController<CleanType, InMode, InAllocator>(
            std::move(allocator), std::forward<InArgs>(args)...
        );
    CleanType* objPtr = controller->getObjectPtr();
    SharedRef<InElementType, InMode> result(objPtr, controller, detail::kSharedInternalsTag);
    detail::updateSharedFromThis(&result, objPtr);
    return result;
}

/// @brief Allocates and constructs an object of type `InElementType` and its reference control block in a single
///        contiguous allocation from @p allocator (`gp::concepts::IsAllocator`), returning a nullable `SharedPtr`
///        (`nullptr` if @p allocator is exhausted).
/// @details Unlike `allocateShared` (which returns a non-nullable `SharedRef` and asserts that the allocator
/// succeeded),
///          `tryAllocateShared` returns an empty `SharedPtr<InElementType, InMode>` when bounded arenas or fixed
///          `PoolAllocator` slabs run out of capacity.
/// @tparam InElementType Non-array object type to construct.
/// @tparam InMode Thread-safety policy (`ThreadSafe` by default, or `SingleThreaded`).
/// @tparam InAllocator Engine allocator type satisfying `gp::concepts::IsAllocator`.
/// @tparam InArgs Constructor argument types forwarded to `InElementType`.
/// @param[in,out] allocator Engine allocator instance used to allocate and later free the combined block.
/// @param[in] args Arguments forwarded to `InElementType`'s constructor.
/// @return Valid `SharedPtr<InElementType, InMode>` on success, or `nullptr` if allocation failed.
template <
    typename InElementType,
    RefCountThreadSafety InMode = RefCountThreadSafety::ThreadSafe,
    concepts::IsAllocator InAllocator,
    typename... InArgs>
requires(
    !concepts::IsArray<InElementType> && concepts::IsConstructibleWith<std::remove_cv_t<InElementType>, InArgs && ...>
) [[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedPtr<InElementType, InMode>
    tryAllocateShared(InAllocator allocator, InArgs&&... args) noexcept
{
    using CleanType = std::remove_cv_t<InElementType>;
    detail::IntrusiveReferenceController<CleanType, InMode, InAllocator>* controller =
        detail::createIntrusiveReferenceController<CleanType, InMode, InAllocator>(
            std::move(allocator), std::forward<InArgs>(args)...
        );
    if (controller == nullptr) [[unlikely]]
    {
        return SharedPtr<InElementType, InMode>(nullptr);
    }
    CleanType* objPtr = controller->getObjectPtr();
    SharedPtr<InElementType, InMode> result(objPtr, controller, detail::kSharedInternalsTag);
    detail::updateSharedFromThis(&result, objPtr);
    return result;
}

/// @brief Wraps a raw pointer @p ptr (and optional custom @p deleter and @p allocator) so it can implicitly initialize
///        a `SharedPtr<T, Mode>` or `SharedRef<T, Mode>` of either thread-safety mode.
/// @details Modeled after Unreal Engine's `MakeShareable(ptr, deleter)`. Useful when wrapping objects with private
///          constructors, third-party factory outputs, or custom deletion hooks.
/// @tparam InObjectType Pointee type of @p ptr.
/// @tparam InDeleter Custom deleter callable type (defaults to `DefaultDelete<InObjectType>`).
/// @tparam InAllocator Engine allocator type for the control block (defaults to `gp::memory::HeapAllocator`).
/// @param[in] ptr Raw pointer whose ownership is transferred.
/// @param[in] deleter Custom deleter callable.
/// @param[in] allocator Engine allocator used to allocate the control block.
/// @return Temporary `detail::RawPtrProxy` convertible to `SharedPtr` or `SharedRef`.
template <
    typename InObjectType,
    typename InDeleter = DefaultDelete<InObjectType>,
    concepts::IsAllocator InAllocator = gp::memory::HeapAllocator>
requires(!concepts::IsArray<InObjectType>)
[[nodiscard]] GP_FORCEINLINE_HINT constexpr detail::RawPtrProxy<InObjectType, std::decay_t<InDeleter>, InAllocator>
    makeShareable(InObjectType* ptr, InDeleter&& deleter = InDeleter{}, InAllocator allocator = InAllocator{}) noexcept
{
    return detail::RawPtrProxy<InObjectType, std::decay_t<InDeleter>, InAllocator>(
        ptr, std::forward<InDeleter>(deleter), std::move(allocator)
    );
}

/// @section Casting Helpers (`staticCast`, `constCast`, `reinterpretCast`)

/// @brief Performs a `static_cast<InTargetType*>` on a `SharedPtr`, sharing ownership of the control block.
template <typename InTargetType, typename InSourceType, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedPtr<InTargetType, InMode>
    staticCastSharedPtr(const SharedPtr<InSourceType, InMode>& src) noexcept
{
    return SharedPtr<InTargetType, InMode>(src, static_cast<InTargetType*>(src.get()));
}

/// @brief Performs a `static_cast<InTargetType*>` on an rvalue `SharedPtr` with zero atomic operations.
template <typename InTargetType, typename InSourceType, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedPtr<InTargetType, InMode>
    staticCastSharedPtr(SharedPtr<InSourceType, InMode>&& src) noexcept
{
    InTargetType* castPtr = static_cast<InTargetType*>(src.get());
    return SharedPtr<InTargetType, InMode>(std::move(src), castPtr);
}

/// @brief Performs a `static_cast<InTargetType*>` on a `SharedRef`, sharing ownership of the control block.
template <typename InTargetType, typename InSourceType, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedRef<InTargetType, InMode>
    staticCastSharedRef(const SharedRef<InSourceType, InMode>& src) noexcept
{
    return SharedRef<InTargetType, InMode>(src, static_cast<InTargetType*>(src.get()));
}

/// @brief Performs a `static_cast<InTargetType*>` on an rvalue `SharedRef` with zero atomic operations.
template <typename InTargetType, typename InSourceType, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedRef<InTargetType, InMode>
    staticCastSharedRef(SharedRef<InSourceType, InMode>&& src) noexcept
{
    InTargetType* castPtr = static_cast<InTargetType*>(src.get());
    SharedPtr<InSourceType, InMode> movedPtr(std::move(src));
    return SharedRef<InTargetType, InMode>(std::move(movedPtr), castPtr);
}

/// @brief Performs a `const_cast<InTargetType*>` on a `SharedPtr`, sharing ownership of the control block.
template <typename InTargetType, typename InSourceType, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedPtr<InTargetType, InMode>
    constCastSharedPtr(const SharedPtr<InSourceType, InMode>& src) noexcept
{
    return SharedPtr<InTargetType, InMode>(src, const_cast<InTargetType*>(src.get()));
}

/// @brief Performs a `const_cast<InTargetType*>` on an rvalue `SharedPtr` with zero atomic operations.
template <typename InTargetType, typename InSourceType, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedPtr<InTargetType, InMode>
    constCastSharedPtr(SharedPtr<InSourceType, InMode>&& src) noexcept
{
    InTargetType* castPtr = const_cast<InTargetType*>(src.get());
    return SharedPtr<InTargetType, InMode>(std::move(src), castPtr);
}

/// @brief Performs a `const_cast<InTargetType*>` on a `SharedRef`, sharing ownership of the control block.
template <typename InTargetType, typename InSourceType, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedRef<InTargetType, InMode>
    constCastSharedRef(const SharedRef<InSourceType, InMode>& src) noexcept
{
    return SharedRef<InTargetType, InMode>(src, const_cast<InTargetType*>(src.get()));
}

/// @brief Performs a `const_cast<InTargetType*>` on an rvalue `SharedRef` with zero atomic operations.
template <typename InTargetType, typename InSourceType, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedRef<InTargetType, InMode>
    constCastSharedRef(SharedRef<InSourceType, InMode>&& src) noexcept
{
    InTargetType* castPtr = const_cast<InTargetType*>(src.get());
    SharedPtr<InSourceType, InMode> movedPtr(std::move(src));
    return SharedRef<InTargetType, InMode>(std::move(movedPtr), castPtr);
}

/// @brief Performs a `reinterpret_cast<InTargetType*>` on a `SharedPtr`, sharing ownership of the control block.
template <typename InTargetType, typename InSourceType, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT SharedPtr<InTargetType, InMode>
    reinterpretCastSharedPtr(const SharedPtr<InSourceType, InMode>& src) noexcept
{
    return SharedPtr<InTargetType, InMode>(src, reinterpret_cast<InTargetType*>(src.get()));
}

/// @brief Performs a `reinterpret_cast<InTargetType*>` on an rvalue `SharedPtr` with zero atomic operations.
template <typename InTargetType, typename InSourceType, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT SharedPtr<InTargetType, InMode>
    reinterpretCastSharedPtr(SharedPtr<InSourceType, InMode>&& src) noexcept
{
    InTargetType* castPtr = reinterpret_cast<InTargetType*>(src.get());
    return SharedPtr<InTargetType, InMode>(std::move(src), castPtr);
}

/// @brief Performs a `reinterpret_cast<InTargetType*>` on a `SharedRef`, sharing ownership of the control block.
template <typename InTargetType, typename InSourceType, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT SharedRef<InTargetType, InMode>
    reinterpretCastSharedRef(const SharedRef<InSourceType, InMode>& src) noexcept
{
    return SharedRef<InTargetType, InMode>(src, reinterpret_cast<InTargetType*>(src.get()));
}

/// @brief Performs a `reinterpret_cast<InTargetType*>` on an rvalue `SharedRef` with zero atomic operations.
template <typename InTargetType, typename InSourceType, RefCountThreadSafety InMode>
[[nodiscard]] GP_FORCEINLINE_HINT SharedRef<InTargetType, InMode>
    reinterpretCastSharedRef(SharedRef<InSourceType, InMode>&& src) noexcept
{
    InTargetType* castPtr = reinterpret_cast<InTargetType*>(src.get());
    SharedPtr<InSourceType, InMode> movedPtr(std::move(src));
    return SharedRef<InTargetType, InMode>(std::move(movedPtr), castPtr);
}

namespace concepts
{

/// @brief Marks `SharedRef<T, Mode>` as bitwise relocatable for `gp::Vector` `memmove` optimization.
template <typename InElementType, RefCountThreadSafety InMode>
struct IsRelocatableTrait<gp::SharedRef<InElementType, InMode>> : std::true_type
{};

/// @brief Marks `SharedPtr<T, Mode>` as bitwise relocatable for `gp::Vector` `memmove` optimization.
template <typename InElementType, RefCountThreadSafety InMode>
struct IsRelocatableTrait<gp::SharedPtr<InElementType, InMode>> : std::true_type
{};

}   // namespace concepts

}   // namespace gp
