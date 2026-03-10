// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"
#include "Templates/Concepts/ObjectConcepts.hpp"
#include "Templates/Core/RemoveCVRef.hpp"
#include "Templates/Core/SmallBufferOptimization.hpp"
#include "Templates/Core/Utility.hpp"
#include "Templates/Traits/AlignedStorage.hpp"
#include <new>

namespace GP
{

namespace Detail
{

/// @brief Address-based unique type tag used as a runtime type identity for TAny.
/// @note  Each instantiation TAnyTypeTag<T> embeds a unique static byte whose address
///        is used as a lightweight type identifier without RTTI.
/// @tparam T The type being identified.
template <typename T>
struct TAnyTypeTag
{
    static constexpr Byte kId = 0;
};

}   // namespace Detail

// clang-format off

/// @brief Tag type for in-place construction inside TAny.
/// @tparam T The type to construct in-place.
template <typename T>
struct TInPlaceTypeT { explicit constexpr TInPlaceTypeT() = default; };

/// @brief Constant tag instance for in-place construction inside TAny.
/// @tparam T The type to construct in-place.
template <typename T>
inline constexpr TInPlaceTypeT<T> InPlaceType{};

// clang-format on

/// @brief Type-erased value container that can hold any CopyConstructible type.
///        Uses a small-buffer optimisation (SBO) for types that fit within the inline storage to avoid
///        heap allocation. Falls back to aligned heap allocation for larger or over-aligned types.
/// @note  An empty TAny (no stored value) is represented by a null vtable pointer.
///        Use HasValue() / operator bool before accessing via Cast<T>() or TryCast<T>().
///        The stored type must be CopyConstructible; move-only types are not supported.
class TAny final
{
private:
    /// @brief Virtual dispatch table for type-erased operations on the stored value.
    struct FVTable
    {
        const void* typeId;                              ///< Unique address per type T.
        bool isSmall;                                    ///< True when the value lives in the SBO buffer.
        void (*destroy)(void* buf) noexcept;             ///< Destroy (and free if heap) the stored value.
        void (*copyTo)(const void* src, void* dst);      ///< Copy-construct the value from src into dst.
        void (*moveTo)(void* src, void* dst) noexcept;   ///< Move-construct the value from src into dst.
    };

    /// @brief SBO operations: the value lives directly in the byte buffer.
    /// @tparam T Stored type — must satisfy CCopyConstructible.
    template <typename T>
    struct TSBOOps
    {
        static void Destroy(void* buf) noexcept { reinterpret_cast<T*>(buf)->~T(); }

        static void CopyTo(const void* src, void* dst) { ::new (dst) T(*reinterpret_cast<const T*>(src)); }

        static void MoveTo(void* src, void* dst) noexcept { ::new (dst) T(GP::Move(*reinterpret_cast<T*>(src))); }

        static constexpr FVTable kVTable{ &Detail::TAnyTypeTag<T>::kId, true, &Destroy, &CopyTo, &MoveTo };
    };

    /// @brief Heap operations: the buffer stores a void* to the heap-allocated value.
    ///        Used when a type does not fit in the SBO buffer or exceeds SBO alignment.
    /// @tparam T Stored type — must satisfy CCopyConstructible.
    template <typename T>
    struct THeapOps
    {
        static void Destroy(void* buf) noexcept
        {
            T* p = *reinterpret_cast<T**>(buf);
            if (p)
            {
                p->~T();
                ::operator delete(p, sizeof(T), AlignVal_T{ alignof(T) });
            }
        }

        static void CopyTo(const void* src, void* dst)
        {
            const T* srcObj = *reinterpret_cast<const T* const*>(src);
            void* mem = ::operator new(sizeof(T), AlignVal_T{ alignof(T) });
            ::new (mem) T(*srcObj);
            *reinterpret_cast<void**>(dst) = mem;
        }

        static void MoveTo(void* src, void* dst) noexcept
        {
            // Transfer heap pointer ownership; src pointer becomes null.
            *reinterpret_cast<void**>(dst) = *reinterpret_cast<void**>(src);
            *reinterpret_cast<void**>(src) = nullptr;
        }

        static constexpr FVTable kVTable{ &Detail::TAnyTypeTag<T>::kId, false, &Destroy, &CopyTo, &MoveTo };
    };

private:
    /// @brief True when T fits in the SBO buffer (small enough and not over-aligned).
    template <typename T>
    static constexpr bool kFitsSBO = (sizeof(T) <= kSBOCapacity) && (alignof(T) <= kSBOAlignment);

    // clang-format off
    GP_ALIGN(kSBOAlignment) Byte m_buf[kSBOCapacity] {};
    // clang-format on
    const FVTable* m_vtable{ nullptr };

public:
    /// @brief Constructs an empty TAny (no stored value).
    TAny() noexcept = default;

    /// @brief Constructs a TAny that stores a copy or move of @p value.
    /// @tparam T Value type. Must be CopyConstructible and not be TAny itself.
    /// @param value Value to store, perfectly forwarded.
    template <typename T>
    requires(!CSameAs<TRemoveCVRef_T<T>, TAny> && CCopyConstructible<TRemoveCVRef_T<T>>) TAny(T&& value)
    {
        using U = TRemoveCVRef_T<T>;
        if constexpr (kFitsSBO<U>) { ::new (m_buf) U(GP::Forward<T>(value)); }
        else
        {
            void* mem = ::operator new(sizeof(U), AlignVal_T{ alignof(U) });
            ::new (mem) U(GP::Forward<T>(value));
            *reinterpret_cast<void**>(m_buf) = mem;
        }
        m_vtable = VTableFor<U>();
    }

    /// @brief Constructs a TAny with a value of type T constructed in-place from @p args.
    /// @tparam T Type to construct. Must be CopyConstructible.
    /// @tparam Args Constructor argument types.
    /// @param args Arguments forwarded to T's constructor.
    template <typename T, typename... Args>
    requires(CCopyConstructible<T> && CConstructibleFrom<T, Args && ...>)
    explicit TAny(TInPlaceTypeT<T>, Args&&... args)
    {
        using U = TRemoveCVRef_T<T>;
        if constexpr (kFitsSBO<U>) { ::new (m_buf) U(GP::Forward<Args>(args)...); }
        else
        {
            void* mem = ::operator new(sizeof(U), AlignVal_T{ alignof(U) });
            ::new (mem) U(GP::Forward<Args>(args)...);
            *reinterpret_cast<void**>(m_buf) = mem;
        }
        m_vtable = VTableFor<U>();
    }

    /// @brief Copy-constructs a TAny from @p other.
    /// @param other TAny to copy from. May be empty.
    TAny(const TAny& other)
    {
        if (other.m_vtable)
        {
            other.m_vtable->copyTo(other.m_buf, m_buf);
            m_vtable = other.m_vtable;
        }
    }

    /// @brief Move-constructs a TAny from @p other, leaving @p other empty.
    /// @param other TAny to move from.
    TAny(TAny&& other) noexcept
    {
        if (other.m_vtable)
        {
            other.m_vtable->moveTo(other.m_buf, m_buf);
            m_vtable = other.m_vtable;
            other.m_vtable->destroy(other.m_buf);
            other.m_vtable = nullptr;
        }
    }

    /// @brief Destroys the TAny and the stored value, if any.
    ~TAny() { Reset(); }

public:
    /// @brief Copy-assigns from @p other, replacing any currently stored value.
    /// @param other TAny to copy from.
    /// @return Reference to this.
    TAny& operator=(const TAny& other)
    {
        if (this != &other)
        {
            Reset();
            if (other.m_vtable)
            {
                other.m_vtable->copyTo(other.m_buf, m_buf);
                m_vtable = other.m_vtable;
            }
        }
        return *this;
    }

    /// @brief Move-assigns from @p other, leaving @p other empty.
    /// @param other TAny to move from.
    /// @return Reference to this.
    TAny& operator=(TAny&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            if (other.m_vtable)
            {
                other.m_vtable->moveTo(other.m_buf, m_buf);
                m_vtable = other.m_vtable;
                other.m_vtable->destroy(other.m_buf);
                other.m_vtable = nullptr;
            }
        }
        return *this;
    }

    /// @brief Assigns a new value of type T, replacing any currently stored value.
    /// @tparam T Value type. Must be CopyConstructible and not be TAny itself.
    /// @param value Value to assign.
    /// @return Reference to this.
    template <typename T>
    requires(!CSameAs<TRemoveCVRef_T<T>, TAny> && CCopyConstructible<TRemoveCVRef_T<T>>) TAny& operator=(T&& value)
    {
        TAny(GP::Forward<T>(value)).Swap(*this);
        return *this;
    }

    /// @brief Explicitly converts to bool to check whether a value is stored.
    /// @return True if the TAny contains a value, false otherwise.
    GP_NODISCARD explicit operator bool() const noexcept { return HasValue(); }

public:
    /// @brief Checks whether the TAny contains a value.
    /// @return True if a value is stored, false if empty.
    GP_NODISCARD bool HasValue() const noexcept { return m_vtable != nullptr; }

    /// @brief Checks whether the stored value is of exactly type T (after cv/ref stripping).
    /// @tparam T The type to test against.
    /// @return True if the TAny holds a T, false if it is empty or holds a different type.
    template <typename T>
    GP_NODISCARD bool Has() const noexcept
    {
        return m_vtable && (m_vtable->typeId == &Detail::TAnyTypeTag<TRemoveCVRef_T<T>>::kId);
    }

    /// @brief Returns a mutable reference to the stored value, asserting the type matches.
    /// @tparam T Expected type. Must match the stored type exactly (cv/ref stripped).
    /// @return Mutable reference to the stored T.
    /// @warning Behaviour is undefined—asserted—if Has<T>() is false.
    template <typename T>
    GP_NODISCARD T& Cast() noexcept
    {
        GP_ASSERT(Has<T>(), "TAny: Cast<T> called with wrong type");
        return *reinterpret_cast<T*>(ObjPtr());
    }

    /// @brief Returns a const reference to the stored value, asserting the type matches.
    /// @tparam T Expected type. Must match the stored type exactly (cv/ref stripped).
    /// @return Const reference to the stored T.
    /// @warning Behaviour is undefined—asserted—if Has<T>() is false.
    template <typename T>
    GP_NODISCARD const T& Cast() const noexcept
    {
        GP_ASSERT(Has<T>(), "TAny: Cast<T> called with wrong type");
        return *reinterpret_cast<const T*>(ObjPtr());
    }

    /// @brief Returns a pointer to the stored value if it holds type T, otherwise nullptr.
    /// @tparam T Expected type. Must match the stored type exactly (cv/ref stripped).
    /// @return Pointer to T or nullptr.
    template <typename T>
    GP_NODISCARD T* TryCast() noexcept
    {
        return Has<T>() ? reinterpret_cast<T*>(ObjPtr()) : nullptr;
    }

    /// @brief Returns a const pointer to the stored value if it holds type T, otherwise nullptr.
    /// @tparam T Expected type.
    /// @return Const pointer to T or nullptr.
    template <typename T>
    GP_NODISCARD const T* TryCast() const noexcept
    {
        return Has<T>() ? reinterpret_cast<const T*>(ObjPtr()) : nullptr;
    }

    /// @brief Destroys the currently stored value (if any) and constructs a new T in-place.
    /// @tparam T Type to emplace. Must be CopyConstructible.
    /// @tparam Args Constructor argument types.
    /// @param args Arguments forwarded to T's constructor.
    /// @return Reference to the newly constructed value.
    template <typename T, typename... Args>
    requires(CCopyConstructible<T> && CConstructibleFrom<T, Args && ...>) T& Emplace(Args&&... args)
    {
        Reset();
        using U = TRemoveCVRef_T<T>;
        if constexpr (kFitsSBO<U>) { ::new (m_buf) U(GP::Forward<Args>(args)...); }
        else
        {
            void* mem = ::operator new(sizeof(U), AlignVal_T{ alignof(U) });
            ::new (mem) U(GP::Forward<Args>(args)...);
            *reinterpret_cast<void**>(m_buf) = mem;
        }
        m_vtable = VTableFor<U>();
        return *reinterpret_cast<U*>(ObjPtr());
    }

    /// @brief Destroys the stored value, leaving the TAny empty.
    void Reset() noexcept
    {
        if (m_vtable)
        {
            m_vtable->destroy(m_buf);
            m_vtable = nullptr;
        }
    }

    /// @brief Swaps the stored values of this TAny and @p other.
    /// @param other TAny to swap with.
    void Swap(TAny& other) noexcept
    {
        if (this == &other) { return; }

        // Both store SBO values, both heap, or one of each: the simplest correct
        // approach is a three-move swap using a temporary.
        TAny tmp(GP::Move(*this));
        *this = GP::Move(other);
        other = GP::Move(tmp);
    }

private:
    /// @brief Returns a raw pointer to the stored value (SBO address or dereferenced heap pointer).
    GP_NODISCARD void* ObjPtr() noexcept
    {
        if (!m_vtable) { return nullptr; }
        return m_vtable->isSmall ? static_cast<void*>(m_buf) : *reinterpret_cast<void**>(m_buf);
    }

    /// @brief Returns a const raw pointer to the stored value.
    GP_NODISCARD const void* ObjPtr() const noexcept
    {
        if (!m_vtable) { return nullptr; }
        return m_vtable->isSmall ? static_cast<const void*>(m_buf) : *reinterpret_cast<const void* const*>(m_buf);
    }

    /// @brief Selects the correct vtable for type T based on SBO eligibility.
    template <typename T>
    GP_NODISCARD static const FVTable* VTableFor() noexcept
    {
        if constexpr (kFitsSBO<T>) { return &TSBOOps<T>::kVTable; }
        else { return &THeapOps<T>::kVTable; }
    }
};

/// @brief Returns a pointer to the value stored in @p any if it holds type T, otherwise nullptr.
/// @tparam T Expected type.
/// @param any TAny instance to inspect.
/// @return Pointer to T or nullptr.
template <typename T>
GP_NODISCARD GP_FORCEINLINE T* AnyCast(TAny* any) noexcept
{
    return any ? any->TryCast<T>() : nullptr;
}

/// @brief Returns a const pointer to the value stored in @p any if it holds type T, otherwise nullptr.
/// @tparam T Expected type.
/// @param any TAny instance to inspect.
/// @return Const pointer to T or nullptr.
template <typename T>
GP_NODISCARD GP_FORCEINLINE const T* AnyCast(const TAny* any) noexcept
{
    return any ? any->TryCast<T>() : nullptr;
}

/// @brief Returns a mutable reference to the value stored in @p any, asserting the type matches.
/// @tparam T Expected type.
/// @param any Mutable TAny reference.
/// @return Mutable reference to T.
/// @warning Asserts if @p any does not hold type T.
template <typename T>
GP_NODISCARD GP_FORCEINLINE T& AnyCast(TAny& any) noexcept
{
    return any.Cast<T>();
}

/// @brief Returns a const reference to the value stored in @p any, asserting the type matches.
/// @tparam T Expected type.
/// @param any Const TAny reference.
/// @return Const reference to T.
/// @warning Asserts if @p any does not hold type T.
template <typename T>
GP_NODISCARD GP_FORCEINLINE const T& AnyCast(const TAny& any) noexcept
{
    return any.Cast<T>();
}

/// @brief Moves the stored value out of @p any and returns it by value, asserting the type matches.
/// @tparam T Expected type. Must be move-constructible.
/// @param any Rvalue TAny reference.
/// @return T by value.
/// @warning Asserts if @p any does not hold type T.
template <typename T>
GP_NODISCARD GP_FORCEINLINE T AnyCast(TAny&& any) noexcept
{
    GP_ASSERT(any.Has<T>(), "TAny: AnyCast<T> called with wrong type");
    return GP::Move(any.Cast<T>());
}

/// @brief Constructs a TAny containing a T built in-place from @p args.
/// @tparam T Type to construct.
/// @tparam Args Constructor argument types.
/// @param args Arguments forwarded to T's constructor.
/// @return A TAny holding the new T value.
template <typename T, typename... Args>
GP_NODISCARD TAny MakeAny(Args&&... args)
{
    return TAny(InPlaceType<T>, GP::Forward<Args>(args)...);
}

}   // namespace GP
