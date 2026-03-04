// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Memory/AtomicOps.hpp"
#include "Templates/Core/Utility.hpp"
#include "Templates/Traits/AlignedStorage.hpp"

namespace GP
{

/// @brief Reference-counting control block shared between TSharedPtr and TWeakPtr.
///
///        Strong count : number of live TSharedPtr instances.
///        Weak count   : number of TWeakPtr instances
///                       + 1 implicit reference held on behalf of the strong group.
///
///        Lifecycle:
///          StrongCount → 0  ⟹  DestroyObject() then release the implicit weak ref.
///          WeakCount   → 0  ⟹  delete this (frees the control block itself).
class FControlBlockBase
{
private:
    TAtomic<Int32> m_strongCount;
    TAtomic<Int32> m_weakCount;

public:
    FControlBlockBase() noexcept
        : m_strongCount(1)
        , m_weakCount(1)   // 1 = the implicit weak ref held by the strong group
    {}

    virtual ~FControlBlockBase() noexcept = default;

public:
    ///
    /// @section Strong reference management
    ///

    GP_FORCEINLINE void AddStrongRef() noexcept { m_strongCount.fetch_add(1, std::memory_order_relaxed); }

    GP_FORCEINLINE void ReleaseStrongRef() noexcept
    {
        if (m_strongCount.fetch_sub(1, std::memory_order_release) == 1)
        {
            DestroyObject();
            ReleaseWeakRef();   // release the implicit weak ref
        }
    }

    ///
    /// @section Weak reference management
    ///

    GP_FORCEINLINE void AddWeakRef() noexcept { m_weakCount.fetch_add(1, std::memory_order_relaxed); }

    GP_FORCEINLINE void ReleaseWeakRef() noexcept
    {
        if (m_weakCount.fetch_sub(1, std::memory_order_release) == 1) { delete this; }
    }

    ///
    /// @section Weak → Strong promotion (lock)
    ///

    /// @brief Lock-free CAS loop: atomically increments the strong count only if it is currently non-zero. Used by
    ///        TWeakPtr::Lock().
    /// @return True if the strong reference was successfully acquired.
    GP_NODISCARD bool TryAddStrongRef() noexcept
    {
        Int32 current = m_strongCount.load(std::memory_order_relaxed);
        while (current != 0)
        {
            if (m_strongCount.compare_exchange_weak(current, current + 1, std::memory_order_acquire)) { return true; }
        }
        return false;
    }

    ///
    /// @section Accessors
    ///

    GP_NODISCARD Int32 GetStrongCount() const noexcept { return m_strongCount.load(std::memory_order_relaxed); }

    GP_NODISCARD Int32 GetWeakCount() const noexcept { return m_weakCount.load(std::memory_order_relaxed); }

protected:
    /// @brief Destroys the managed resource. Called when StrongCount reaches zero.
    virtual void DestroyObject() noexcept = 0;
};

/// @brief Control block owning a separately allocated object + deleter.
///        Created by the TSharedPtr(T*) and TSharedPtr(T*, Deleter) constructors.
/// @tparam T The managed type.
/// @tparam TDeleter The deleter type.
template <typename T, typename TDeleter>
struct TControlBlockWithPtr final : FControlBlockBase
{
private:
    T* m_ptr;
    GP_NO_UNIQUE_ADDRESS TDeleter m_deleter;

public:
    TControlBlockWithPtr(T* inPtr, TDeleter inDeleter) noexcept
        : m_ptr(inPtr)
        , m_deleter(GP::Move(inDeleter))
    {}

protected:
    void DestroyObject() noexcept override
    {
        m_deleter(m_ptr);
        m_ptr = nullptr;
    }
};

/// @brief Control block with the managed object stored inline.
///        A single allocation holds both the block and the object — more
///        cache-friendly and requires one fewer heap allocation than
///        TControlBlockWithPtr.  Used exclusively by MakeShared.
/// @tparam T   The managed type.
template <typename T>
struct TControlBlockInline final : FControlBlockBase
{
private:
    TAlignedStorageFor<T> m_storage;

public:
    template <typename... TArgs>
    explicit TControlBlockInline(TArgs&&... Args)
    {
        // Construct T inside the aligned byte buffer via placement new.
        new (m_storage.GetPtr()) T(GP::Forward<TArgs>(Args)...);
    }

public:
    // clang-format off

    GP_NODISCARD T*       GetPtr()       noexcept { return m_storage.GetPtr(); }
    GP_NODISCARD const T* GetPtr() const noexcept { return m_storage.GetPtr(); }

    // clang-format on

protected:
    void DestroyObject() noexcept override
    {
        // Manually invoke ~T().  The storage itself is raw bytes and will be
        // freed when the control block is deleted — no double-destroy risk.
        m_storage.GetPtr()->~T();
    }
};

}   // namespace GP
