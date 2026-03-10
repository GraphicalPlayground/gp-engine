// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include "Templates/Core/SmallBufferOptimization.hpp"
#include "Templates/Core/Utility.hpp"
#include "Templates/Traits/AlignedStorage.hpp"
#include "Templates/Traits/TypeRelationships.hpp"
#include <new>

namespace GP::Detail
{

/// @brief Callable wrapper for a mutable member function.
///
/// Stored by value inside TBinding via SBO. sizeof == sizeof(T*) + sizeof(TMethod),
/// which fits comfortably within kSBOCapacity for all standard ABIs.
///
/// @tparam TInstance Class that owns the member function.
/// @tparam TMethod Pointer-to-member-function type.
/// @tparam TRetVal Return type of the member function.
/// @tparam TArgs Argument types forwarded to the member function.
template <typename TInstance, typename TMethod, typename TRetVal, typename... TArgs>
struct TMemberBinding
{
public:
    TInstance* instance{ nullptr };   //<! Pointer to the object instance on which to invoke the member function.
    TMethod method{};                 //<! Pointer-to-member-function representing the method to invoke.

public:
    GP_FORCEINLINE TRetVal operator()(TArgs&&... args) const
    {
        GP_ASSERT(instance != nullptr, "TMemberBinding: instance pointer is null.");
        return (instance->*method)(GP::Forward<TArgs>(args)...);
    }
};

/// \brief Callable wrapper for a const-qualified member function.
///
/// \tparam TInstance Class that owns the member function.
/// \tparam TMethod Pointer-to-const-member-function type.
/// \tparam TRetVal Return type of the member function.
/// \tparam TArgs Argument types forwarded to the member function.
template <typename TInstance, typename TMethod, typename TRetVal, typename... TArgs>
struct TConstMemberBinding
{
public:
    const TInstance* instance{ nullptr };   //<! Pointer to the object instance on which to invoke the member function.
    TMethod method{};                       //<! Pointer-to-const-member-function representing the method to invoke.

public:
    GP_FORCEINLINE TRetVal operator()(TArgs&&... args) const
    {
        GP_ASSERT(instance != nullptr, "TConstMemberBinding: instance pointer is null.");
        return (instance->*method)(GP::Forward<TArgs>(args)...);
    }
};

/// @brief Type-erased, copyable, movable callable storage with Small Buffer Optimization.
///
/// Stores any callable matching \c TRetVal(TArgs...) without virtual dispatch or
/// std::function overhead. A compile-time-generated VTable of raw function pointers
/// handles per-type operations (invoke, copy, move, destroy). Callables that fit
/// within \c kSBOCapacity bytes are stored inline; larger ones fall back to an
/// aligned heap allocation.
///
/// This class is an internal building block; consumers use TDelegate or
/// TMulticastDelegate.
///
/// @tparam TRetVal Return type of the stored callable.
/// @tparam TArgs Argument pack forwarded on each invocation.
template <typename TRetVal, typename... TArgs>
class TBinding
{
public:
    /// @brief Per-type function pointer table generated at Bind<TCallable>() time.
    struct FVTable
    {
    public:
        using FInvokeFn = TRetVal (*)(void* storage, TArgs&&... args);
        using FCopyFn = void (*)(void* dst, const void* src);
        using FMoveFn = void (*)(void* dst, void* src) noexcept;
        using FDestroyFn = void (*)(void* storage) noexcept;

    public:
        FInvokeFn invoke{ nullptr };     //<! Invoke the stored callable.
        FCopyFn copy{ nullptr };         //<! Copy-construct into pre-allocated destination.
        FMoveFn move{ nullptr };         //<! Move-construct into pre-allocated destination, then destroy src.
        FDestroyFn destroy{ nullptr };   //<! Destroy the stored callable in-place.
        SizeT size{ 0 };                 //<! sizeof(TCallable), for heap reallocation during copy.
        SizeT align{ 0 };                //<! alignof(TCallable), for aligned heap operations.
    };

private:
    const FVTable* m_vtable{ nullptr };   //<! Pointer to the VTable for the currently stored callable type.
    void* m_heapStorage{ nullptr };       //<! Pointer to heap storage if the callable exceeds SBO capacity;
    alignas(kSBOAlignment) Byte m_sbo[kSBOCapacity]{};   //<! Inline storage buffer for small

public:
    ///
    /// @section Constructors / Destructor
    ///

    TBinding() noexcept = default;

    TBinding(const TBinding& other) { CopyFrom(other); }

    TBinding(TBinding&& other) noexcept { MoveFrom(GP::Move(other)); }

    ~TBinding() noexcept { Destroy(); }

public:
    ///
    /// @section Operators
    ///

    TBinding& operator=(const TBinding& other)
    {
        if (this != &other)
        {
            Destroy();
            CopyFrom(other);
        }
        return *this;
    }

    TBinding& operator=(TBinding&& other) noexcept
    {
        if (this != &other)
        {
            Destroy();
            MoveFrom(std::move(other));
        }
        return *this;
    }

public:
    /// @brief Returns true if a callable is currently stored.
    GP_NODISCARD GP_FORCEINLINE bool IsBound() const noexcept { return m_vtable != nullptr; }

    /// @brief Invokes the stored callable with the provided arguments.
    /// @warning Asserts in debug if no callable is bound. Use ExecuteIfBound() for safe invocation.
    GP_FORCEINLINE TRetVal Invoke(TArgs&&... args) const
    {
        GP_ASSERT(IsBound(), "Attempting to invoke an unbound TBinding.");
        return m_vtable->invoke(GetStorage(), GP::Forward<TArgs>(args)...);
    }

    /// @brief Stores a callable, replacing any existing binding.
    ///
    /// Generates a static VTable for \c TCallable the first time this type is bound.
    /// The callable is stored inline (SBO) if it fits; otherwise heap-allocated.
    ///
    /// @tparam TCallable Any callable type invocable as \c TRetVal(TArgs...).
    template <typename TCallable>
    void Bind(TCallable&& callable)
    {
        using FDecayed = std::decay_t<TCallable>;

        static_assert(
            TIsInvocableR_V<TRetVal, FDecayed, TArgs...>,
            "TCallable must be invocable with the signature TRetVal(TArgs...)."
        );
        static_assert(
            TIsCopyConstructible_V<FDecayed>,
            "TCallable must be CopyConstructible. Use a shared_ptr wrapper for move-only captures."
        );

        Destroy();

        // Static VTable: lambdas are captureless so this is safe across TUs.
        static const FVTable s_vtable{
            .invoke = [](void* storage, TArgs&&... args) -> TRetVal
            { return (*static_cast<FDecayed*>(storage))(GP::Forward<TArgs>(args)...); },
            .copy = [](void* dst, const void* src) { new (dst) FDecayed(*static_cast<const FDecayed*>(src)); },
            .move =
                [](void* dst, void* src) noexcept
            {
                new (dst) FDecayed(GP::Move(*static_cast<FDecayed*>(src)));
                static_cast<FDecayed*>(src)->~FDecayed();
            },
            .destroy = [](void* storage) noexcept { static_cast<FDecayed*>(storage)->~FDecayed(); },
            .size = sizeof(FDecayed),
            .align = alignof(FDecayed),
        };

        m_vtable = &s_vtable;

        if constexpr (FitsInSbo<FDecayed>())
        {
            new (m_sbo) FDecayed(GP::Forward<TCallable>(callable));
            m_heapStorage = nullptr;
        }
        else
        {
            m_heapStorage = ::operator new(sizeof(FDecayed), AlignVal_T{ alignof(FDecayed) });
            new (m_heapStorage) FDecayed(GP::Forward<TCallable>(callable));
        }
    }

    /// @brief Destroys the stored callable and resets to unbound state.
    void Unbind() noexcept { Destroy(); }

private:
    template <typename T>
    GP_NODISCARD static constexpr bool FitsInSbo() noexcept
    {
        return sizeof(T) <= kSBOCapacity && alignof(T) <= kSBOAlignment;
    }

    /// @brief Returns a pointer to the active storage region (SBO or heap).
    GP_NODISCARD GP_FORCEINLINE void* GetStorage() const noexcept
    {
        return (m_heapStorage != nullptr) ? m_heapStorage : static_cast<void*>(const_cast<Byte*>(m_sbo));
    }

    void Destroy() noexcept
    {
        if (m_vtable == nullptr) { return; }

        m_vtable->destroy(GetStorage());

        if (m_heapStorage != nullptr)
        {
            ::operator delete(m_heapStorage, AlignVal_T{ m_vtable->align });
            m_heapStorage = nullptr;
        }

        m_vtable = nullptr;
    }

    void CopyFrom(const TBinding& other)
    {
        if (other.m_vtable == nullptr) { return; }

        m_vtable = other.m_vtable;

        if (other.m_heapStorage != nullptr)
        {
            m_heapStorage = ::operator new(m_vtable->size, AlignVal_T{ m_vtable->align });
            m_vtable->copy(m_heapStorage, other.m_heapStorage);
        }
        else
        {
            m_heapStorage = nullptr;
            m_vtable->copy(m_sbo, other.m_sbo);
        }
    }

    void MoveFrom(TBinding&& other) noexcept
    {
        if (other.m_vtable == nullptr) { return; }

        m_vtable = other.m_vtable;

        if (other.m_heapStorage != nullptr)
        {
            // Transfer ownership of the heap block without touching the object.
            m_heapStorage = other.m_heapStorage;
            other.m_heapStorage = nullptr;
        }
        else
        {
            m_heapStorage = nullptr;
            m_vtable->move(m_sbo, other.m_sbo);
        }

        other.m_vtable = nullptr;
    }
};

}   // namespace GP::Detail
