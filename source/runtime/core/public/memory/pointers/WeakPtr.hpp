// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Memory.hpp"
#include "concepts/Relationship.hpp"
#include "CoreMinimal.hpp"
#include "memory/pointers/PointerEnums.hpp"
#include "memory/pointers/SharedPtr.hpp"
#include <compare>
#include <utility>

namespace gp
{

/// @brief Non-owning 16-byte weak observer for objects managed by `SharedRef<T, Mode>` or `SharedPtr<T, Mode>`.
/// @details Modeled after Unreal Engine's `TWeakPtr<ObjectType, Mode>` and `eastl::weak_ptr` ([ADR-0059]).
///          `WeakPtr<T, Mode>` holds a pointer to the shared `ReferenceControllerBase<Mode>` and increments its weak
///          reference count (`m_weakRefs`) without preventing the managed object from being destroyed when its strong
///          reference count (`m_sharedRefs`) reaches zero.
///
///          To safely access the observed object, callers invoke `pin()` (or `lock()`), which atomically promotes the
///          weak reference into a strong `SharedPtr<T, Mode>` if and only if `m_sharedRefs > 0`.
/// @tparam InElementType Observed object type.
/// @tparam InMode Thread-safety policy (`ThreadSafe` by default, or `SingleThreaded`).
/// @see SharedRef, SharedPtr, SharedFromThis
template <typename InElementType, RefCountThreadSafety InMode>
class WeakPtr
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
    /// @brief Observed object type.
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
    /// @brief Default constructor; initializes an empty `WeakPtr` with zero atomic operations.
    constexpr WeakPtr() noexcept = default;

    /// @brief Constructs an empty `WeakPtr` from `nullptr`.
    /// @param[in] nullValue Null pointer literal.
    constexpr explicit(false) WeakPtr([[maybe_unused]] gp::NullPtrT nullValue) noexcept
        : m_object(nullptr)
        , m_controller(nullptr)
    {}

    /// @brief Constructs a `WeakPtr` observing a `SharedRef<InOtherType, InMode>`.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @param[in] sharedRef Source `SharedRef` to observe.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr explicit(false) WeakPtr(const SharedRef<InOtherType, InMode>& sharedRef) noexcept
        : m_object(sharedRef.m_object)
        , m_controller(sharedRef.m_controller)
    {
        if (m_controller != nullptr)
        {
            m_controller->addWeakRef();
        }
    }

    /// @brief Constructs a `WeakPtr` observing a `SharedPtr<InOtherType, InMode>`.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @param[in] sharedPtr Source `SharedPtr` to observe.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr explicit(false) WeakPtr(const SharedPtr<InOtherType, InMode>& sharedPtr) noexcept
        : m_object(sharedPtr.m_object)
        , m_controller(sharedPtr.m_controller)
    {
        if (m_controller != nullptr)
        {
            m_controller->addWeakRef();
        }
    }

    /// @brief Aliasing constructor used by `SharedFromThis` to bind a typed subobject pointer to an owner's control
    ///        block.
    /// @tparam InOtherType Element type of the owning `SharedPtr`.
    /// @param[in] owner Owning `SharedPtr` providing the control block.
    /// @param[in] aliasedPtr Subobject pointer to store inside this `WeakPtr`.
    template <typename InOtherType>
    constexpr WeakPtr(const SharedPtr<InOtherType, InMode>& owner, PointerType aliasedPtr) noexcept
        : m_object(owner.m_controller != nullptr ? aliasedPtr : nullptr)
        , m_controller(owner.m_controller)
    {
        if (m_controller != nullptr)
        {
            m_controller->addWeakRef();
        }
    }

    /// @brief Aliasing constructor used by `SharedFromThis` to bind a typed subobject pointer directly from a
    ///        `SharedRef` owner without intermediate `SharedPtr` construction.
    /// @tparam InOtherType Element type of the owning `SharedRef`.
    /// @param[in] owner Owning `SharedRef` providing the control block.
    /// @param[in] aliasedPtr Subobject pointer to store inside this `WeakPtr`.
    template <typename InOtherType>
    constexpr WeakPtr(const SharedRef<InOtherType, InMode>& owner, PointerType aliasedPtr) noexcept
        : m_object(owner.m_controller != nullptr ? aliasedPtr : nullptr)
        , m_controller(owner.m_controller)
    {
        if (m_controller != nullptr)
        {
            m_controller->addWeakRef();
        }
    }

    /// @brief Copy constructor; increments the weak reference count if non-empty.
    /// @param[in] other Source `WeakPtr` to copy.
    constexpr WeakPtr(const WeakPtr& other) noexcept
        : m_object(other.m_object)
        , m_controller(other.m_controller)
    {
        if (m_controller != nullptr)
        {
            m_controller->addWeakRef();
        }
    }

    /// @brief Converting copy constructor from a compatible `WeakPtr<InOtherType, InMode>`.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @param[in] other Source `WeakPtr` to copy.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr explicit(false) WeakPtr(const WeakPtr<InOtherType, InMode>& other) noexcept
        : m_object(other.m_object)
        , m_controller(other.m_controller)
    {
        if (m_controller != nullptr)
        {
            m_controller->addWeakRef();
        }
    }

    /// @brief Move constructor; transfers the weak reference from @p other with zero atomic operations.
    /// @param[in,out] other Source `WeakPtr` to move from; reset to empty.
    constexpr WeakPtr(WeakPtr&& other) noexcept
        : m_object(other.m_object)
        , m_controller(other.m_controller)
    {
        other.m_object = nullptr;
        other.m_controller = nullptr;
    }

    /// @brief Converting move constructor from a compatible `WeakPtr<InOtherType, InMode>` with zero atomic
    ///        operations.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @param[in,out] other Source `WeakPtr` to move from; reset to empty.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr explicit(false) WeakPtr(WeakPtr<InOtherType, InMode>&& other) noexcept
        : m_object(other.m_object)
        , m_controller(other.m_controller)
    {
        other.m_object = nullptr;
        other.m_controller = nullptr;
    }

    /// @brief Destructor; decrements the weak reference count if bound to a control block.
    GP_FORCEINLINE_HINT constexpr ~WeakPtr() noexcept
    {
        detail::ReferenceControllerBase<InMode>* oldController = m_controller;
        m_object = nullptr;
        m_controller = nullptr;
        if (oldController != nullptr)
        {
            oldController->releaseWeakRef();
        }
    }

public:
    /// @brief Copy assignment operator; increments @p rhs's weak count before releasing the previous control block.
    /// @param[in] rhs Source `WeakPtr` to copy.
    /// @return Reference to `*this`.
    constexpr WeakPtr& operator=(const WeakPtr& rhs) noexcept
    {
        detail::ReferenceControllerBase<InMode>* newController = rhs.m_controller;
        PointerType newObject = rhs.m_object;
        if (newController != nullptr)
        {
            newController->addWeakRef();
        }

        detail::ReferenceControllerBase<InMode>* oldController = m_controller;
        m_object = newObject;
        m_controller = newController;
        if (oldController != nullptr)
        {
            oldController->releaseWeakRef();
        }

        return *this;
    }

    /// @brief Converting copy assignment operator from a compatible `WeakPtr<InOtherType, InMode>`.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @param[in] rhs Source `WeakPtr` to copy.
    /// @return Reference to `*this`.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr WeakPtr& operator=(const WeakPtr<InOtherType, InMode>& rhs) noexcept
    {
        detail::ReferenceControllerBase<InMode>* newController = rhs.m_controller;
        PointerType newObject = rhs.m_object;
        if (newController != nullptr)
        {
            newController->addWeakRef();
        }

        detail::ReferenceControllerBase<InMode>* oldController = m_controller;
        m_object = newObject;
        m_controller = newController;
        if (oldController != nullptr)
        {
            oldController->releaseWeakRef();
        }

        return *this;
    }

    /// @brief Move assignment operator; transfers the weak reference from @p rhs and releases the previous control
    ///        block.
    /// @param[in,out] rhs Source `WeakPtr` to move from; reset to empty.
    /// @return Reference to `*this`.
    constexpr WeakPtr& operator=(WeakPtr&& rhs) noexcept
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
                oldController->releaseWeakRef();
            }
        }
        return *this;
    }

    /// @brief Converting move assignment operator from a compatible `WeakPtr<InOtherType, InMode>`.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @param[in,out] rhs Source `WeakPtr` to move from; reset to empty.
    /// @return Reference to `*this`.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr WeakPtr& operator=(WeakPtr<InOtherType, InMode>&& rhs) noexcept
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
                oldController->releaseWeakRef();
            }
        }
        return *this;
    }

    /// @brief Assigns from a `SharedRef<InOtherType, InMode>`.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @param[in] rhs Source `SharedRef` to observe.
    /// @return Reference to `*this`.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr WeakPtr& operator=(const SharedRef<InOtherType, InMode>& rhs) noexcept
    {
        detail::ReferenceControllerBase<InMode>* newController = rhs.m_controller;
        PointerType newObject = rhs.m_object;
        if (newController != nullptr)
        {
            newController->addWeakRef();
        }

        detail::ReferenceControllerBase<InMode>* oldController = m_controller;
        m_object = newObject;
        m_controller = newController;
        if (oldController != nullptr)
        {
            oldController->releaseWeakRef();
        }

        return *this;
    }

    /// @brief Assigns from a `SharedPtr<InOtherType, InMode>`.
    /// @tparam InOtherType Derived or cv-convertible element type.
    /// @param[in] rhs Source `SharedPtr` to observe.
    /// @return Reference to `*this`.
    template <concepts::IsPointerConvertibleTo<InElementType> InOtherType>
    constexpr WeakPtr& operator=(const SharedPtr<InOtherType, InMode>& rhs) noexcept
    {
        detail::ReferenceControllerBase<InMode>* newController = rhs.m_controller;
        PointerType newObject = rhs.m_object;
        if (newController != nullptr)
        {
            newController->addWeakRef();
        }

        detail::ReferenceControllerBase<InMode>* oldController = m_controller;
        m_object = newObject;
        m_controller = newController;
        if (oldController != nullptr)
        {
            oldController->releaseWeakRef();
        }

        return *this;
    }

    /// @brief Resets this `WeakPtr` to empty.
    /// @param[in] nullValue Null pointer literal.
    /// @return Reference to `*this`.
    constexpr WeakPtr& operator=([[maybe_unused]] gp::NullPtrT nullValue) noexcept
    {
        reset();
        return *this;
    }

public:
    /// @brief Atomically promotes this weak reference into a strong `SharedPtr<InElementType, InMode>` if the observed
    ///        object is still alive (`getSharedReferenceCount() > 0`).
    /// @details Modeled after Unreal Engine's `TWeakPtr::Pin()`. In `ThreadSafe` mode, executes a lock-free
    ///          compare-and-swap loop (`tryAddSharedRef()`) to prevent races against concurrent destruction on another
    ///          thread.
    /// @return Valid `SharedPtr<InElementType, InMode>` if the object is still live; null `SharedPtr` if expired.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedPtr<InElementType, InMode> pin() const noexcept
    {
        if (m_controller != nullptr && m_controller->tryAddSharedRef())
        {
            return SharedPtr<InElementType, InMode>(m_object, m_controller, detail::kSharedInternalsTag);
        }
        return SharedPtr<InElementType, InMode>(nullptr);
    }

    /// @brief STL-compatible alias for `pin()`, promoting this `WeakPtr` to a strong `SharedPtr` if not expired.
    /// @return Valid `SharedPtr<InElementType, InMode>` if the object is still live; null `SharedPtr` if expired.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SharedPtr<InElementType, InMode> lock() const noexcept
    {
        return pin();
    }

    /// @brief Releases this weak reference and resets the observer to empty.
    constexpr void reset() noexcept
    {
        detail::ReferenceControllerBase<InMode>* oldController = m_controller;
        m_object = nullptr;
        m_controller = nullptr;
        if (oldController != nullptr)
        {
            oldController->releaseWeakRef();
        }
    }

    /// @brief Exchanges the observed pointers and control blocks of `*this` and @p other with zero atomic operations.
    /// @param[in,out] other Other `WeakPtr` to swap with.
    constexpr void swap(WeakPtr& other) noexcept
    {
        using std::swap;
        swap(m_object, other.m_object);
        swap(m_controller, other.m_controller);
    }

    /// @brief Queries the current strong reference count of the observed control block.
    /// @return Number of active `SharedRef` / `SharedPtr` owners, or `0` if empty or expired.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 getSharedReferenceCount() const noexcept
    {
        return m_controller != nullptr ? m_controller->getSharedReferenceCount() : 0u;
    }

    /// @brief Checks whether the observed object has already been destroyed or this `WeakPtr` is empty.
    /// @return `true` if `getSharedReferenceCount() == 0u`; `false` if at least one strong reference remains.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isExpired() const noexcept
    {
        return getSharedReferenceCount() == 0u;
    }

    /// @brief Checks whether the observed object is currently alive (`!isExpired()`).
    /// @note In `ThreadSafe` mode, use `pin()` rather than branching on `isValid()` to avoid TOCTOU races.
    /// @return `true` if `m_object != nullptr && !isExpired()`; `false` otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isValid() const noexcept
    {
        return m_object != nullptr && !isExpired();
    }

    /// @brief Checks whether `*this` and @p other reference the exact same underlying control block (even if expired).
    /// @tparam InOtherType Element type of @p other.
    /// @param[in] other Other `WeakPtr` to compare against.
    /// @return `true` if both share the same control block pointer; `false` otherwise.
    template <typename InOtherType>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        hasSameOwner(const WeakPtr<InOtherType, InMode>& other) const noexcept
    {
        return m_controller == other.m_controller;
    }

    /// @brief Checks whether `*this` and @p other `SharedPtr` reference the exact same underlying control block.
    /// @tparam InOtherType Element type of @p other.
    /// @param[in] other `SharedPtr` to compare against.
    /// @return `true` if both share the same control block pointer; `false` otherwise.
    template <typename InOtherType>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        hasSameOwner(const SharedPtr<InOtherType, InMode>& other) const noexcept
    {
        return m_controller == other.m_controller;
    }

    /// @brief Checks whether `*this` and @p other `SharedRef` reference the exact same underlying control block.
    /// @tparam InOtherType Element type of @p other.
    /// @param[in] other `SharedRef` to compare against.
    /// @return `true` if both share the same control block pointer; `false` otherwise.
    template <typename InOtherType>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        hasSameOwner(const SharedRef<InOtherType, InMode>& other) const noexcept
    {
        return m_controller == other.m_controller;
    }

    /// @brief Strict weak ordering based on control-block address against another `WeakPtr`.
    /// @tparam InOtherType Element type of @p other.
    /// @param[in] other Other `WeakPtr` to order against.
    /// @return `true` if this control block address precedes `other`'s control block address.
    template <typename InOtherType>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        ownerBefore(const WeakPtr<InOtherType, InMode>& other) const noexcept
    {
        return std::compare_three_way{}(m_controller, other.m_controller) < 0;
    }

    /// @brief Strict weak ordering based on control-block address against a `SharedPtr`.
    /// @tparam InOtherType Element type of @p other.
    /// @param[in] other `SharedPtr` to order against.
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
};

/// @brief ADL-friendly swap exchanging two `WeakPtr` instances with zero atomic operations.
/// @tparam InElementType Observed object type.
/// @tparam InMode Thread-safety policy.
/// @param[in,out] lhs First `WeakPtr`.
/// @param[in,out] rhs Second `WeakPtr`.
template <typename InElementType, RefCountThreadSafety InMode>
GP_FORCEINLINE_HINT constexpr void
    swap(WeakPtr<InElementType, InMode>& lhs, WeakPtr<InElementType, InMode>& rhs) noexcept
{
    lhs.swap(rhs);
}

namespace concepts
{

/// @brief Marks `WeakPtr<T, Mode>` as bitwise relocatable for `gp::Vector` `memmove` optimization.
template <typename InElementType, RefCountThreadSafety InMode>
struct IsRelocatableTrait<gp::WeakPtr<InElementType, InMode>> : std::true_type
{};

}   // namespace concepts

}   // namespace gp
