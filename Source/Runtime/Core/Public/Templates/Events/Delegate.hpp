// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include "Templates/Core/Utility.hpp"
#include "Templates/Events/DelegateHandle.hpp"
#include "Templates/Events/DelegateInternals.hpp"
#include <utility>

namespace GP
{

/// @brief Single-cast delegate: stores and invokes exactly one callable at a time.
///
/// Supports binding free functions, mutable member functions, const member functions,
/// lambdas, and any functor matching the declared signature. The binding is
/// type-erased with Small Buffer Optimization (SBO) so typical bindings incur
/// zero heap allocation.
///
/// Unlike TMulticastDelegate, TDelegate supports non-void return types and is
/// copyable and movable.
///
/// Example:
/// @code
///   TDelegate<Float32(Int32, Float32)> lerp;
///   lerp.BindLambda([](Int32 steps, Float32 t) -> Float32 { return t / steps; });
///
///   if (lerp.IsBound())
///       Float32 result = lerp.Execute(10, 3.0f);
/// @endcode
///
/// @tparam TRetVal  Return type of the callable.
/// @tparam TArgs    Argument types forwarded on each invocation.
template <typename Signature>
class TDelegate;

template <typename TRetVal, typename... TArgs>
class TDelegate<TRetVal(TArgs...)>
{
private:
    using FBinding = Detail::TBinding<TRetVal, TArgs...>;

private:
    FBinding m_binding;   //<! Type-erased storage for the currently bound callable.

public:
    ///
    /// @section Constructors / Destructor
    ///

    TDelegate() noexcept = default;
    TDelegate(const TDelegate&) = default;
    TDelegate(TDelegate&&) noexcept = default;
    TDelegate& operator=(const TDelegate&) = default;
    TDelegate& operator=(TDelegate&&) noexcept = default;
    ~TDelegate() noexcept = default;

public:
    /// @brief Binds a free or static member function.
    /// @param fn Non-null function pointer.
    void BindFunction(TRetVal (*fn)(TArgs...))
    {
        GP_ASSERT(fn != nullptr, "TDelegate::BindFunction - function pointer must not be null.");
        m_binding.Bind(fn);
    }

    /// @brief Binds a mutable member function to a specific object instance.
    /// @tparam T Instance type.
    /// @param instance Non-null pointer to the object that will receive the call.
    /// @param method Pointer to the member function to invoke.
    template <typename T>
    void BindMethod(T* instance, TRetVal (T::*method)(TArgs...))
    {
        GP_ASSERT(instance != nullptr, "TDelegate::BindMethod - instance pointer must not be null.");
        GP_ASSERT(method != nullptr, "TDelegate::BindMethod - method pointer must not be null.");

        using FWrapper = Detail::TMemberBinding<T, TRetVal (T::*)(TArgs...), TRetVal, TArgs...>;
        m_binding.Bind(FWrapper{ instance, method });
    }

    /// @brief Binds a const-qualified member function to a specific object instance.
    /// @tparam T Instance type.
    /// @param instance Non-null const pointer to the object that will receive the call.
    /// @param method Pointer to the const member function to invoke.
    template <typename T>
    void BindMethod(const T* instance, TRetVal (T::*method)(TArgs...) const)
    {
        GP_ASSERT(instance != nullptr, "TDelegate::BindMethod - instance pointer must not be null.");
        GP_ASSERT(method != nullptr, "TDelegate::BindMethod - method pointer must not be null.");

        using FWrapper = Detail::TConstMemberBinding<T, TRetVal (T::*)(TArgs...) const, TRetVal, TArgs...>;
        m_binding.Bind(FWrapper{ instance, method });
    }

    /// @brief Binds any callable object: lambda, functor, std::function, etc.
    /// @tparam TCallable Type of the callable.
    /// @param callable Callable to store. Copied or moved into the delegate.
    template <typename TCallable>
    void BindCallable(TCallable&& callable)
    {
        m_binding.Bind(GP::Forward<TCallable>(callable));
    }

    /// @brief Alias for BindCallable, provided for readability when binding lambdas.
    template <typename TCallable>
    GP_FORCEINLINE void BindLambda(TCallable&& lambda)
    {
        BindCallable(GP::Forward<TCallable>(lambda));
    }

    /// @brief Removes the current binding and resets the delegate to unbound state.
    void Unbind() noexcept { m_binding.Unbind(); }

    /// @brief Returns true if a callable is currently bound.
    GP_NODISCARD GP_FORCEINLINE bool IsBound() const noexcept { return m_binding.IsBound(); }

    /// @brief Invokes the bound callable and returns the result.
    /// @warning Asserts (debug) or invokes undefined behaviour (release) if unbound.
    ///          Prefer ExecuteIfBound() when the binding may be absent.
    /// @return  The return value produced by the callable.
    GP_FORCEINLINE TRetVal Execute(TArgs... args) const
    {
        GP_ASSERT(IsBound(), "TDelegate::Execute - delegate is not bound.");
        return m_binding.Invoke(GP::Forward<TArgs>(args)...);
    }

    /// @brief Invokes the bound callable if one exists.
    ///
    /// For void-return delegates this is a no-op when unbound.
    /// For non-void delegates, \p outResult receives the callable's return value
    /// when bound; it is left unchanged when unbound.
    ///
    /// @param args Arguments forwarded to the callable.
    /// @param outResult Optional out-pointer that receives the return value when bound.
    /// @return True if the callable was invoked.
    bool ExecuteIfBound(TArgs... args, TRetVal* outResult = nullptr) const
    {
        if (!IsBound()) { return false; }

        TRetVal result = m_binding.Invoke(std::forward<TArgs>(args)...);
        if (outResult != nullptr) { *outResult = std::move(result); }
        return true;
    }

public:
    ///
    /// @section Static Factory Methods
    ///

    /// @brief Creates a delegate bound to a free function.
    /// @param fn  Pointer to a free (or static member) function.
    GP_NODISCARD static TDelegate CreateFromFunction(TRetVal (*fn)(TArgs...))
    {
        TDelegate d;
        d.BindFunction(fn);
        return d;
    }

    /// @brief Creates a delegate bound to a mutable member function.
    /// @tparam T Instance type.
    /// @param instance Non-null pointer to the object instance.
    /// @param method Pointer-to-member-function.
    template <typename T>
    GP_NODISCARD static TDelegate CreateFromMethod(T* instance, TRetVal (T::*method)(TArgs...))
    {
        TDelegate d;
        d.BindMethod(instance, method);
        return d;
    }

    /// @brief Creates a delegate bound to a const member function.
    /// @tparam T Instance type.
    /// @param instance Non-null const pointer to the object instance.
    /// @param method Pointer-to-const-member-function.
    template <typename T>
    GP_NODISCARD static TDelegate CreateFromMethod(const T* instance, TRetVal (T::*method)(TArgs...) const)
    {
        TDelegate d;
        d.BindMethod(instance, method);
        return d;
    }

    /// @brief Creates a delegate bound to an arbitrary callable (lambda, functor).
    /// @tparam TCallable Any callable type matching \c TRetVal(TArgs...).
    /// @param callable Callable to copy/move into the delegate.
    template <typename TCallable>
    GP_NODISCARD static TDelegate CreateFromCallable(TCallable&& callable)
    {
        TDelegate d;
        d.BindCallable(GP::Forward<TCallable>(callable));
        return d;
    }
};

/// @brief Partial specialisation for void-return single-cast delegates. Provides a clean ExecuteIfBound() overload that
///        returns bool only.
template <typename... TArgs>
class TDelegate<void(TArgs...)>
{
private:
    using FBinding = Detail::TBinding<void, TArgs...>;

private:
    FBinding m_binding;   //<! Type-erased storage for the currently bound callable.

public:
    ///
    /// @section Constructors / Destructor
    ///

    TDelegate() noexcept = default;
    TDelegate(const TDelegate&) = default;
    TDelegate(TDelegate&&) noexcept = default;
    TDelegate& operator=(const TDelegate&) = default;
    TDelegate& operator=(TDelegate&&) noexcept = default;
    ~TDelegate() noexcept = default;

public:
    void BindFunction(void (*fn)(TArgs...))
    {
        GP_ASSERT(fn != nullptr, "TDelegate::BindFunction - function pointer must not be null.");
        m_binding.Bind(fn);
    }

    template <typename T>
    void BindMethod(T* instance, void (T::*method)(TArgs...))
    {
        GP_ASSERT(instance != nullptr, "TDelegate::BindMethod - instance pointer must not be null.");
        GP_ASSERT(method != nullptr, "TDelegate::BindMethod - method pointer must not be null.");

        using FWrapper = Detail::TMemberBinding<T, void (T::*)(TArgs...), void, TArgs...>;
        m_binding.Bind(FWrapper{ instance, method });
    }

    template <typename T>
    void BindMethod(const T* instance, void (T::*method)(TArgs...) const)
    {
        GP_ASSERT(instance != nullptr, "TDelegate::BindMethod - instance pointer must not be null.");
        GP_ASSERT(method != nullptr, "TDelegate::BindMethod - method pointer must not be null.");

        using FWrapper = Detail::TConstMemberBinding<T, void (T::*)(TArgs...) const, void, TArgs...>;
        m_binding.Bind(FWrapper{ instance, method });
    }

    template <typename TCallable>
    void BindCallable(TCallable&& callable)
    {
        m_binding.Bind(GP::Forward<TCallable>(callable));
    }

    template <typename TCallable>
    GP_FORCEINLINE void BindLambda(TCallable&& lambda)
    {
        BindCallable(GP::Forward<TCallable>(lambda));
    }

    void Unbind() noexcept { m_binding.Unbind(); }

    GP_NODISCARD GP_FORCEINLINE bool IsBound() const noexcept { return m_binding.IsBound(); }

    /// @brief Invokes the bound callable. Asserts if unbound.
    GP_FORCEINLINE void Execute(TArgs... args) const
    {
        GP_ASSERT(IsBound(), "TDelegate::Execute - delegate is not bound.");
        m_binding.Invoke(GP::Forward<TArgs>(args)...);
    }

    /// @brief Invokes the bound callable if one exists, otherwise does nothing.
    /// @return True if the callable was invoked.
    GP_FORCEINLINE bool ExecuteIfBound(TArgs... args) const
    {
        if (!IsBound()) { return false; }

        m_binding.Invoke(GP::Forward<TArgs>(args)...);
        return true;
    }

public:
    ///
    /// @section Static Factory Methods
    ///

    GP_NODISCARD static TDelegate CreateFromFunction(void (*fn)(TArgs...))
    {
        TDelegate d;
        d.BindFunction(fn);
        return d;
    }

    template <typename T>
    GP_NODISCARD static TDelegate CreateFromMethod(T* instance, void (T::*method)(TArgs...))
    {
        TDelegate d;
        d.BindMethod(instance, method);
        return d;
    }

    template <typename T>
    GP_NODISCARD static TDelegate CreateFromMethod(const T* instance, void (T::*method)(TArgs...) const)
    {
        TDelegate d;
        d.BindMethod(instance, method);
        return d;
    }

    template <typename TCallable>
    GP_NODISCARD static TDelegate CreateFromCallable(TCallable&& callable)
    {
        TDelegate d;
        d.BindCallable(GP::Forward<TCallable>(callable));
        return d;
    }
};

}   // namespace GP
