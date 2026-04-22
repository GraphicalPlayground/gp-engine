// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "macros/MacroUtilities.hpp"
#include "memory/Forward.hpp"

namespace gp
{

/// @brief Intrusive, concept-gated smart pointer for reference-counted objects.
/// @details
/// The reference count lives inside the managed object itself (intrusive), which means a RefCountPtr is exactly
/// one pointer wide and incurs no separate control block allocation.
/// @tparam T A type satisfying gp::IsRefCounted.
template <gp::IsRefCounted T>
class RefCountPtr
{
public:
    using ValueType = T;
    using Pointer = T*;

public:
    /// @brief Adopts a raw pointer without incrementing the reference count.
    /// @details
    /// Used when the pointer was returned by a factory function that already added a reference on behalf of the
    /// caller (the idiomatic ownership-transfer pattern for COM-style APIs).
    struct AttachTag
    {};

    static constexpr AttachTag attachT{};

private:
    Pointer m_pointer{ nullptr };

private:
    template <gp::IsRefCounted U>
    friend class RefCountPtr;

public:
    /// @brief Constructs an empty RefCountPtr.
    constexpr RefCountPtr() noexcept = default;

    /// @brief Constructs an empty RefCountPtr.
    constexpr RefCountPtr(gp::NullPtr) noexcept {}

    /// @brief Adopts a raw pointer and adds a reference to it.
    /// @param[in] ptr Pointer to adopt; may be nullptr. The refcount is incremented if non-null.
    explicit RefCountPtr(Pointer ptr) noexcept
        : m_pointer(ptr)
    {
        if (m_pointer) { m_pointer->addRef(); }
    }

    /// @brief Adopts a raw pointer without incrementing the reference count.
    /// @param[in] ptr Pointer to adopt; may be nullptr. The refcount is not incremented.
    /// @param[in] tag Tag to disambiguate from the regular constructor.
    RefCountPtr(Pointer ptr, AttachTag) noexcept
        : m_pointer(ptr)
    {}

    /// @brief Constructs a RefCountPtr by copying from another RefCountPtr.
    /// @param[in] other The RefCountPtr to copy from. The refcount of the new pointer is incremented if non-null.
    RefCountPtr(const RefCountPtr& other) noexcept
        : m_pointer(other.m_pointer)
    {
        if (m_pointer) { m_pointer->addRef(); }
    }

    /// @brief Constructs a RefCountPtr by copying from another RefCountPtr of a convertible type.
    /// @tparam U The type of the RefCountPtr to copy from. Must be convertible to T.
    /// @param[in] other The RefCountPtr to copy from. The refcount of the new pointer is incremented if non-null.
    template <gp::IsRefCounted U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    RefCountPtr(const RefCountPtr<U>& other) noexcept
        : m_pointer(other.m_pointer)
    {
        if (m_pointer) { m_pointer->addRef(); }
    }

    /// @brief Constructs a RefCountPtr by moving from another RefCountPtr.
    /// @param[in] other The RefCountPtr to move from. The moved-from RefCountPtr is left empty.
    RefCountPtr(RefCountPtr&& other) noexcept
        : m_pointer(other.m_pointer)
    {
        other.m_pointer = nullptr;
    }

    /// @brief Constructs a RefCountPtr by moving from another RefCountPtr of a convertible type.
    /// @tparam U The type of the RefCountPtr to move from. Must be convertible to T.
    /// @param[in] other The RefCountPtr to move from. The moved-from RefCountPtr is left empty.
    template <gp::IsRefCounted U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    RefCountPtr(RefCountPtr<U>&& other) noexcept
        : m_pointer(other.m_pointer)
    {
        other.m_pointer = nullptr;
    }

    /// @brief Destroys the RefCountPtr, releasing the reference if it manages a non-null pointer.
    ~RefCountPtr() noexcept
    {
        if (m_pointer) { m_pointer->release(); }
    }

    /// @brief Replaces the stored pointer with a new one, adding a reference to it.
    /// @param[in] other The RefCountPtr to copy from. The refcount of the new pointer is incremented if non-null.
    RefCountPtr& operator=(const RefCountPtr& other) noexcept
    {
        RefCountPtr(other).swap(*this);
        return *this;
    }

    /// @brief Replaces the stored pointer with a new one, adding a reference to it.
    /// @tparam U The type of the RefCountPtr to copy from. Must be convertible to T.
    /// @param[in] other The RefCountPtr to copy from. The refcount of the new pointer is incremented if non-null.
    template <gp::IsRefCounted U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    RefCountPtr& operator=(const RefCountPtr<U>& other) noexcept
    {
        RefCountPtr(other).swap(*this);
        return *this;
    }

    /// @brief Replaces the stored pointer with a new one, adding a reference to it.
    /// @param[in] other The RefCountPtr to move from.
    RefCountPtr& operator=(RefCountPtr&& other) noexcept
    {
        RefCountPtr(std::move(other)).swap(*this);
        return *this;
    }

    /// @brief Replaces the stored pointer with a new one, adding a reference to it.
    /// @tparam U The type of the RefCountPtr to move from. Must be convertible to T.
    /// @param[in] other The RefCountPtr to move from.
    template <gp::IsRefCounted U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    RefCountPtr& operator=(RefCountPtr<U>&& other) noexcept
    {
        RefCountPtr(std::move(other)).swap(*this);
        return *this;
    }

    /// @brief Replaces the stored pointer with a new one, adding a reference to it.
    /// @param[in] ptr The new pointer to manage. The refcount is incremented if non-null.
    RefCountPtr& operator=(Pointer ptr) noexcept
    {
        RefCountPtr(ptr).swap(*this);
        return *this;
    }

    /// @brief Resets to empty.
    RefCountPtr& operator=(gp::NullPtr) noexcept
    {
        reset();
        return *this;
    }

public:
    /// @brief Dereferences the stored pointer.
    /// @return A reference to the managed object. The behavior is undefined if this RefCount is empty.
    GP_FORCEINLINE T& operator*() const noexcept
    {
        GP_ASSERT(m_pointer != nullptr, "Dereferencing a null RefCountPtr");
        return *m_pointer;
    }

    /// @brief Accesses members of the stored pointer.
    /// @return The stored pointer. The behavior is undefined if this RefCount is empty.
    GP_FORCEINLINE Pointer operator->() const noexcept
    {
        GP_ASSERT(m_pointer != nullptr, "Accessing a null RefCountPtr");
        return m_pointer;
    }

    /// @brief Checks if the RefCountPtr is non-empty.
    /// @return true if this RefCountPtr manages a non-null pointer, false otherwise.
    GP_FORCEINLINE explicit operator bool() const noexcept { return m_pointer != nullptr; }

public:
    /// @brief Gets the raw pointer managed by this RefCountPtr.
    /// @return The raw pointer, which may be nullptr if this RefCountPtr is empty.
    GP_NODISCARD GP_FORCEINLINE Pointer get() const noexcept { return m_pointer; }

    /// @brief Releases ownership without calling release(). Returns the adopted pointer.
    /// @return The adopted pointer, which is now detached from this RefCountPtr.
    GP_NODISCARD GP_FORCEINLINE Pointer detach() noexcept
    {
        Pointer tmp = m_pointer;
        m_pointer = nullptr;
        return tmp;
    }

    /// @brief Resets to an empty pointer, releasing the current reference if any.
    GP_FORCEINLINE void reset() noexcept
    {
        if (m_pointer)
        {
            m_pointer->release();
            m_pointer = nullptr;
        }
    }

    /// @brief Checks if the RefCountPtr is non-empty.
    /// @return true if this RefCountPtr manages a non-null pointer, false otherwise.
    GP_FORCEINLINE bool isValid() const noexcept { return m_pointer != nullptr; }

    /// @brief Replaces the stored pointer with a new one, adding a reference to it.
    /// @param[in] ptr The new pointer to manage. The refcount is incremented if non-null.
    GP_FORCEINLINE void reset(Pointer ptr) noexcept { RefCountPtr(ptr).swap(*this); }

    /// @brief Adopts a raw pointer without adding a reference (ownership transfer).
    /// @param[in] ptr The new pointer to manage. The refcount is not incremented.
    GP_FORCEINLINE void attach(Pointer ptr) noexcept
    {
        if (m_pointer) { m_pointer->release(); }
        m_pointer = ptr;
    }

    /// @brief Returns the address of the stored pointer after releasing any current reference.
    /// @return The address of the stored pointer, which is now empty.
    GP_NODISCARD GP_FORCEINLINE Pointer* getAddressOf() noexcept
    {
        reset();
        return &m_pointer;
    }

    /// @brief Returns the address of the stored pointer without resetting.
    /// @return The address of the stored pointer, which may still hold a reference.
    GP_NODISCARD GP_FORCEINLINE Pointer* getAddressOfUnsafe() noexcept { return &m_pointer; }

    /// @brief Swaps the managed pointers of this and another RefCountPtr.
    /// @param[in] other The RefCountPtr to swap with.
    GP_FORCEINLINE void swap(RefCountPtr& other) noexcept
    {
        Pointer tmp = m_pointer;
        m_pointer = other.m_pointer;
        other.m_pointer = tmp;
    }
};

}   // namespace gp

/// @brief Equality comparison operators for RefCountPtr.
/// @param[in] lhs The left-hand side RefCountPtr.
/// @param[in] rhs The right-hand side RefCountPtr.
/// @return true if both RefCountPtrs manage the same pointer (including both being empty), false otherwise.
template <gp::IsRefCounted T, gp::IsRefCounted U>
GP_FORCEINLINE bool operator==(const gp::RefCountPtr<T>& lhs, const gp::RefCountPtr<U>& rhs) noexcept
{
    return lhs.get() == rhs.get();
}

/// @brief Inequality comparison operators for RefCountPtr.
/// @param[in] lhs The left-hand side RefCountPtr.
/// @param[in] rhs The right-hand side RefCountPtr.
/// @return true if the RefCountPtrs manage different pointers, false if they manage the same pointer.
template <gp::IsRefCounted T, gp::IsRefCounted U>
GP_FORCEINLINE bool operator!=(const gp::RefCountPtr<T>& lhs, const gp::RefCountPtr<U>& rhs) noexcept
{
    return lhs.get() != rhs.get();
}

/// @brief Equality comparison operators for RefCountPtr and nullptr.
/// @param[in] lhs The RefCountPtr to compare.
/// @param[in] rhs The nullptr to compare.
/// @return true if the RefCountPtr is empty (manages a null pointer), false otherwise.
template <gp::IsRefCounted T>
GP_FORCEINLINE bool operator==(const gp::RefCountPtr<T>& lhs, gp::NullPtr) noexcept
{
    return lhs.get() == nullptr;
}

/// @brief Equality comparison operators for nullptr and RefCountPtr.
/// @param[in] lhs The nullptr to compare.
/// @param[in] rhs The RefCountPtr to compare.
/// @return true if the RefCountPtr is empty (manages a null pointer), false otherwise.
template <gp::IsRefCounted T>
GP_FORCEINLINE bool operator==(gp::NullPtr, const gp::RefCountPtr<T>& rhs) noexcept
{
    return rhs.get() == nullptr;
}

/// @brief Inequality comparison operators for RefCountPtr and nullptr.
/// @param[in] lhs The RefCountPtr to compare.
/// @param[in] rhs The nullptr to compare.
/// @return true if the RefCountPtr is non-empty (manages a non-null pointer), false otherwise.
template <gp::IsRefCounted T>
GP_FORCEINLINE bool operator!=(const gp::RefCountPtr<T>& lhs, gp::NullPtr) noexcept
{
    return lhs.get() != nullptr;
}

/// @brief Inequality comparison operators for nullptr and RefCountPtr.
/// @param[in] lhs The nullptr to compare.
/// @param[in] rhs The RefCountPtr to compare.
/// @return true if the RefCountPtr is non-empty (manages a non-null pointer), false otherwise.
template <gp::IsRefCounted T>
GP_FORCEINLINE bool operator!=(gp::NullPtr, const gp::RefCountPtr<T>& rhs) noexcept
{
    return rhs.get() != nullptr;
}
