// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include "Memory/ControlBlock.hpp"
#include "Memory/DefaultDeleter.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"
#include "Templates/Core/Utility.hpp"
#include <compare>

namespace GP
{

template <typename T>
class TWeakPtr;

/// @brief Shared-ownership smart pointer with automatic, thread-safe reference counting.
///
///        - Any number of TSharedPtr instances may share ownership of a single object.
///        - The object is destroyed when the last owning TSharedPtr is destroyed or reset.
///        - TWeakPtr instances can observe the object without extending its lifetime.
///        - The deleter is type-erased inside the control block — safe to convert
///          TSharedPtr<Derived> → TSharedPtr<Base> without a virtual destructor.
///
///        Thread safety:
///          Reference count operations are atomic.  Concurrent reads of the same
///          TSharedPtr are safe.  Concurrent write access to the same TSharedPtr
///          instance requires external synchronisation.
///
/// @tparam T The managed type.
template <typename T>
class TSharedPtr
{
public:
    using ElementType = T;

private:
    // clang-format off
    template <typename U> friend class TSharedPtr;
    template <typename U> friend class TWeakPtr;
    // clang-format on

private:
    T* m_ptr{ nullptr };
    FControlBlockBase* m_block{ nullptr };

public:
    constexpr TSharedPtr() noexcept = default;

    constexpr TSharedPtr(decltype(nullptr)) noexcept {}

    /// @brief Takes ownership of inPtr using the default deleter.
    ///        Accepts T* or Derived* (where Derived : T).
    ///        The control block stores the original typed pointer, so the correct
    ///        destructor is called even if T has no virtual destructor.
    template <typename U>
    requires CSameAs<U, T> || CDerivedFrom<U, T> explicit TSharedPtr(U* inPtr)
    {
        if (inPtr)
        {
            m_ptr = inPtr;
            m_block = new TControlBlockWithPtr<U, TDefaultDeleter<U>>(inPtr, TDefaultDeleter<U>{});
            InternalInitSharedFromThis();
        }
    }

    /// @brief Takes ownership of inPtr with a custom deleter.
    template <typename U, typename TDeleter>
    requires CSameAs<U, T> || CDerivedFrom<U, T> TSharedPtr(U* inPtr, TDeleter inDeleter)
    {
        if (inPtr)
        {
            m_ptr = inPtr;
            m_block = new TControlBlockWithPtr<U, TDeleter>(inPtr, GP::Move(inDeleter));
            InternalInitSharedFromThis();
        }
    }

    TSharedPtr(const TSharedPtr& other) noexcept
        : m_ptr(other.m_ptr)
        , m_block(other.m_block)
    {
        if (m_block) { m_block->AddStrongRef(); }
    }

    /// @brief Converting copy: TSharedPtr<Derived> → TSharedPtr<Base>.
    template <typename U>
    requires CDerivedFrom<U, T> TSharedPtr(const TSharedPtr<U>& other) noexcept
        : m_ptr(other.m_ptr)
        , m_block(other.m_block)
    {
        if (m_block) { m_block->AddStrongRef(); }
    }

    TSharedPtr(TSharedPtr&& other) noexcept
        : m_ptr(other.m_ptr)
        , m_block(other.m_block)
    {
        other.m_ptr = nullptr;
        other.m_block = nullptr;
    }

    /// @brief Converting move: TSharedPtr<Derived> → TSharedPtr<Base>.
    template <typename U>
    requires CDerivedFrom<U, T> TSharedPtr(TSharedPtr<U>&& other) noexcept
        : m_ptr(other.m_ptr)
        , m_block(other.m_block)
    {
        other.m_ptr = nullptr;
        other.m_block = nullptr;
    }

    /// @brief Aliasing constructor.
    ///        Creates a TSharedPtr that shares ownership with other but stores Ptr.
    ///        Useful for pointing to a sub-object or member while keeping the parent alive.
    template <typename U>
    TSharedPtr(const TSharedPtr<U>& other, T* ptr) noexcept
        : m_ptr(ptr)
        , m_block(other.m_block)
    {
        if (m_block) { m_block->AddStrongRef(); }
    }

    /// @brief Aliasing move constructor.
    template <typename U>
    TSharedPtr(TSharedPtr<U>&& other, T* ptr) noexcept
        : m_ptr(ptr)
        , m_block(other.m_block)
    {
        other.m_ptr = nullptr;
        other.m_block = nullptr;
    }

    ~TSharedPtr()
    {
        if (m_block) { m_block->ReleaseStrongRef(); }
    }

private:
    /// @brief Private constructor used by TWeakPtr::Lock() and MakeShared.
    ///        Does NOT increment the reference count - the caller must ensure
    ///        the count was already incremented (e.g. via TryAddStrongRef).
    TSharedPtr(T* inPtr, FControlBlockBase* inBlock) noexcept
        : m_ptr(inPtr)
        , m_block(inBlock)
    {}

public:
    TSharedPtr& operator=(const TSharedPtr& other) noexcept
    {
        TSharedPtr(other).Swap(*this);
        return *this;
    }

    template <typename U>
    requires CDerivedFrom<U, T> TSharedPtr& operator=(const TSharedPtr<U>& other) noexcept
    {
        TSharedPtr(other).Swap(*this);
        return *this;
    }

    TSharedPtr& operator=(TSharedPtr&& other) noexcept
    {
        TSharedPtr(GP::Move(other)).Swap(*this);
        return *this;
    }

    template <typename U>
    requires CDerivedFrom<U, T> TSharedPtr& operator=(TSharedPtr<U>&& other) noexcept
    {
        TSharedPtr(GP::Move(other)).Swap(*this);
        return *this;
    }

    TSharedPtr& operator=(decltype(nullptr)) noexcept
    {
        Reset();
        return *this;
    }

    GP_NODISCARD T& operator*() const noexcept
    {
        GP_ASSUME(m_ptr);
        return *m_ptr;
    }

    GP_NODISCARD T* operator->() const noexcept
    {
        GP_ASSUME(m_ptr);
        return m_ptr;
    }

    GP_NODISCARD explicit operator bool() const noexcept { return m_ptr != nullptr; }

public:
    GP_NODISCARD T* Get() const noexcept { return m_ptr; }

    /// @brief Number of TSharedPtr instances sharing ownership of this object.
    GP_NODISCARD Int32 UseCount() const noexcept { return m_block ? m_block->GetStrongCount() : 0; }

    /// @brief True when this is the sole owner.
    GP_NODISCARD bool IsUnique() const noexcept { return UseCount() == 1; }

    void Reset() noexcept { TSharedPtr().Swap(*this); }

    template <typename U>
    requires CSameAs<U, T> || CDerivedFrom<U, T> void Reset(U* inPtr)
    {
        TSharedPtr(inPtr).Swap(*this);
    }

    void Swap(TSharedPtr& other) noexcept
    {
        T* tempPtr = m_ptr;
        m_ptr = other.m_ptr;
        other.m_ptr = tempPtr;
        FControlBlockBase* tempBlock = m_block;
        m_block = other.m_block;
        other.m_block = tempBlock;
    }

private:
    void InternalInitSharedFromThis() noexcept
    {
        if constexpr (requires { m_ptr->InternalInitWeakThis(*this); })
        {
            if (m_ptr) { m_ptr->InternalInitWeakThis(*this); }
        }
    }

    template <typename U, typename... TArgs>
    friend TSharedPtr<U> MakeShared(TArgs&&...);
};

template <typename T, typename U>
GP_NODISCARD bool operator==(const TSharedPtr<T>& lhs, const TSharedPtr<U>& rhs) noexcept
{
    return lhs.Get() == rhs.Get();
}

template <typename T, typename U>
GP_NODISCARD auto operator<=>(const TSharedPtr<T>& lhs, const TSharedPtr<U>& rhs) noexcept
{
    return lhs.Get() <=> rhs.Get();
}

template <typename T>
GP_NODISCARD bool operator==(const TSharedPtr<T>& lhs, decltype(nullptr)) noexcept
{
    return !lhs;
}

template <typename T>
GP_NODISCARD auto operator<=>(const TSharedPtr<T>& lhs, decltype(nullptr)) noexcept
{
    return lhs.Get() <=> static_cast<T*>(nullptr);
}

template <typename T>
void Swap(TSharedPtr<T>& a, TSharedPtr<T>& b) noexcept
{
    a.Swap(b);
}

/// @brief Constructs T with Args in a single allocation shared between the
///        object and its control block.
///
///        Advantages over TSharedPtr<T>(new T(...)):
///          - One allocation instead of two.
///          - Better cache locality: control block and object are adjacent.
///
/// @tparam T The type to construct.
/// @tparam TArgs Constructor argument types.
template <typename T, typename... TArgs>
GP_NODISCARD TSharedPtr<T> MakeShared(TArgs&&... args)
{
    auto* block = new TControlBlockInline<T>(Forward<TArgs>(args)...);
    TSharedPtr<T> result(block->GetPtr(), static_cast<FControlBlockBase*>(block));
    result.InternalInitSharedFromThis();
    return result;
}

/// @brief static_cast between TSharedPtr types. Shares the control block - no extra allocation.
template <typename TTo, typename TFrom>
GP_NODISCARD TSharedPtr<TTo> StaticPointerCast(const TSharedPtr<TFrom>& ptr) noexcept
{
    return TSharedPtr<TTo>(ptr, static_cast<TTo*>(ptr.Get()));
}

template <typename TTo, typename TFrom>
GP_NODISCARD TSharedPtr<TTo> StaticPointerCast(TSharedPtr<TFrom>&& ptr) noexcept
{
    return TSharedPtr<TTo>(GP::Move(ptr), static_cast<TTo*>(ptr.Get()));
}

/// @brief const_cast between TSharedPtr types.
template <typename TTo, typename TFrom>
GP_NODISCARD TSharedPtr<TTo> ConstPointerCast(const TSharedPtr<TFrom>& ptr) noexcept
{
    return TSharedPtr<TTo>(ptr, const_cast<TTo*>(ptr.Get()));
}

template <typename TTo, typename TFrom>
GP_NODISCARD TSharedPtr<TTo> ConstPointerCast(TSharedPtr<TFrom>&& ptr) noexcept
{
    return TSharedPtr<TTo>(GP::Move(ptr), const_cast<TTo*>(ptr.Get()));
}

/// @brief reinterpret_cast between TSharedPtr types. Use with caution - bypasses the type system.
template <typename TTo, typename TFrom>
GP_NODISCARD TSharedPtr<TTo> ReinterpretPointerCast(const TSharedPtr<TFrom>& ptr) noexcept
{
    return TSharedPtr<TTo>(ptr, reinterpret_cast<TTo*>(ptr.Get()));
}

template <typename TTo, typename TFrom>
GP_NODISCARD TSharedPtr<TTo> ReinterpretPointerCast(TSharedPtr<TFrom>&& ptr) noexcept
{
    return TSharedPtr<TTo>(GP::Move(ptr), reinterpret_cast<TTo*>(ptr.Get()));
}

}   // namespace GP
