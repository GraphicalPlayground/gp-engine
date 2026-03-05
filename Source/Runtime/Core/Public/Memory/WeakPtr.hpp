// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include "Memory/ControlBlock.hpp"
#include "Memory/DefaultDeleter.hpp"
#include "Memory/SharedPtr.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"
#include "Templates/Core/Utility.hpp"
#include <compare>

namespace GP
{

/// @brief Non-owning observer of a TSharedPtr-managed object.
///
///        - Does not extend the lifetime of the managed object.
///        - Call Lock() to obtain a TSharedPtr; the result is empty if the object
///          has already been destroyed.
///        - Keeps the control block alive until the last TWeakPtr is destroyed.
///
/// @tparam T The observed type.
template <typename T>
class TWeakPtr
{
private:
    T* m_ptr{ nullptr };
    FControlBlockBase* m_block{ nullptr };

private:
    template <typename U>
    friend class TWeakPtr;
    template <typename U>
    friend class TSharedPtr;

public:
    constexpr TWeakPtr() noexcept = default;

    TWeakPtr(const TSharedPtr<T>& shared) noexcept
        : m_ptr(shared.m_ptr)
        , m_block(shared.m_block)
    {
        if (m_block) { m_block->AddWeakRef(); }
    }

    template <typename U>
    requires CDerivedFrom<U, T> TWeakPtr(const TSharedPtr<U>& shared) noexcept
        : m_ptr(shared.m_ptr)
        , m_block(shared.m_block)
    {
        if (m_block) { m_block->AddWeakRef(); }
    }

    TWeakPtr(const TWeakPtr& other) noexcept
        : m_ptr(other.m_ptr)
        , m_block(other.m_block)
    {
        if (m_block) { m_block->AddWeakRef(); }
    }

    template <typename U>
    requires CDerivedFrom<U, T> TWeakPtr(const TWeakPtr<U>& other) noexcept
        : m_ptr(other.m_ptr)
        , m_block(other.m_block)
    {
        if (m_block) { m_block->AddWeakRef(); }
    }

    TWeakPtr(TWeakPtr&& other) noexcept
        : m_ptr(other.m_ptr)
        , m_block(other.m_block)
    {
        other.m_ptr = nullptr;
        other.m_block = nullptr;
    }

    template <typename U>
    requires CDerivedFrom<U, T> TWeakPtr(TWeakPtr<U>&& other) noexcept
        : m_ptr(other.m_ptr)
        , m_block(other.m_block)
    {
        other.m_ptr = nullptr;
        other.m_block = nullptr;
    }

    ~TWeakPtr()
    {
        if (m_block) { m_block->ReleaseWeakRef(); }
    }

public:
    TWeakPtr& operator=(const TWeakPtr& other) noexcept
    {
        TWeakPtr(other).Swap(*this);
        return *this;
    }

    TWeakPtr& operator=(TWeakPtr&& other) noexcept
    {
        TWeakPtr(Move(other)).Swap(*this);
        return *this;
    }

    TWeakPtr& operator=(const TSharedPtr<T>& shared) noexcept
    {
        TWeakPtr(shared).Swap(*this);
        return *this;
    }

public:
    /// @brief Attempts to promote to a TSharedPtr.
    ///        Returns an empty TSharedPtr if the managed object has been destroyed.
    GP_NODISCARD TSharedPtr<T> Lock() const noexcept
    {
        if (m_block && m_block->TryAddStrongRef()) { return TSharedPtr<T>(m_ptr, m_block); }
        return TSharedPtr<T>{};
    }

    /// @brief True if the managed object has been destroyed (strong count is 0).
    GP_NODISCARD bool IsExpired() const noexcept { return !m_block || m_block->GetStrongCount() == 0; }

    GP_NODISCARD int32_t UseCount() const noexcept { return m_block ? m_block->GetStrongCount() : 0; }

    void Reset() noexcept
    {
        if (m_block) { m_block->ReleaseWeakRef(); }
        m_ptr = nullptr;
        m_block = nullptr;
    }

    void Swap(TWeakPtr& Other) noexcept
    {
        T* tempPtr = m_ptr;
        m_ptr = Other.m_ptr;
        Other.m_ptr = tempPtr;
        FControlBlockBase* tempBlock = m_block;
        m_block = Other.m_block;
        Other.m_block = tempBlock;
    }
};

template <typename T>
void Swap(TWeakPtr<T>& a, TWeakPtr<T>& b) noexcept
{
    a.Swap(b);
}

}   // namespace GP
