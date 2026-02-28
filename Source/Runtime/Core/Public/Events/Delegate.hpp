// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Events/DelegateHandle.hpp"
#include "Templates/Concepts.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>

namespace GP::Events
{

/// \brief Primary template — intentionally left undefined.
/// \note Specialize on a function signature: TDelegate<ReturnType(ArgTypes...)>.
template <typename Signature>
class TDelegate;

/// \brief A single-binding, type-safe callable wrapper supporting free functions, lambdas, and member function
/// pointers.
///
/// TDelegate holds at most one binding at a time. Binding a new callable automatically replaces any previously bound
/// one. For one-to-many notification patterns, prefer TMulticastDelegate<void(Args...)>.
///
/// TDelegate supports non-void return types via Execute() and ExecuteIfBound(). For void return types, ExecuteIfBound()
/// returns a bool indicating whether the call was made.
///
/// \par Usage Example
/// \code
/// TDelegate<float(UInt32, UInt32)> onAspectRatioQuery;
/// onAspectRatioQuery.BindLambda([](UInt32 w, UInt32 h) -> float {
///     return static_cast<float>(w) / static_cast<float>(h);
/// });
/// if (onAspectRatioQuery.IsBound())
///     float ratio = onAspectRatioQuery.Execute(1920, 1080);
/// \endcode
///
/// \tparam R The return type of the callable.
/// \tparam Args The parameter types of the callable.
template <typename R, typename... Args>
class TDelegate<R(Args...)>
{
public:
    using FReturnType = R;
    using FSignature = R(Args...);
    using FStorage = std::move_only_function<FSignature>;
    using FOptionalReturn = std::conditional_t<std::is_void_v<R>, bool, std::optional<R>>;

private:
    FStorage m_storage;       //<! Storage for the currently bound callable. Empty if no callable is bound.
    std::function<bool()>
        m_weakPtrValidator;   //<! Optional predicate returning false when a bound weak_ptr has expired.

public:
    /// \brief Default constructor.
    TDelegate() = default;

    /// \brief Default destructor.
    ~TDelegate() = default;

    /// \brief Default move constructor.
    TDelegate(TDelegate&&) = default;

    /// \brief Deleted copy constructor.
    TDelegate(const TDelegate&) = delete;

public:
    /// \brief Default move assignment operator.
    TDelegate& operator=(TDelegate&&) = default;

    /// \brief Deleted copy assignment operator.
    TDelegate& operator=(const TDelegate&) = delete;

public:
    /// \brief Binds a free function or stateless function pointer.
    /// \param func A non-null pointer to a free function matching the delegate's signature.
    GP_INLINE void BindStatic(R (*func)(Args...)) noexcept
    {
        GP_ASSERT(func != nullptr);
        m_storage = FStorage{ func };
        m_weakPtrValidator = nullptr;
    }

    /// \brief Binds any callable: lambda, functor, or std::bind expression.
    /// \tparam TCallable Any callable type whose signature is compatible with R(Args...).
    /// \param callable The callable to bind. Forwarded into internal storage.
    template <typename TCallable>
    requires Concepts::IsInvocable<TCallable, Args...> GP_INLINE void BindLambda(TCallable&& callable)
    {
        m_storage = FStorage{ std::forward<TCallable>(callable) };
        m_weakPtrValidator = nullptr;
    }

    /// \brief Binds a non-const member function on a raw object pointer.
    /// \tparam T The class that owns the member function.
    /// \param object A non-null raw pointer to the target object. Lifetime must outlive the delegate.
    /// \param func Pointer to the member function to invoke.
    /// \warning The delegate does NOT extend the object's lifetime. Ensure the object remains valid for the lifetime of
    /// this binding.
    template <typename T>
    GP_INLINE void BindMember(T* object, R (T::*func)(Args...))
    {
        GP_ASSERT(object != nullptr);
        GP_ASSERT(func != nullptr);
        m_storage =
            FStorage{ [object, func](Args&&... args) -> R { return (object->*func)(std::forward<Args>(args)...); } };
        m_weakPtrValidator = nullptr;
    }

    /// \brief Binds a const member function on a raw object pointer.
    /// \tparam T The class that owns the member function.
    /// \param object A non-null raw pointer to the target const object. Lifetime must outlive the delegate.
    /// \param func Pointer to the const member function to invoke.
    template <typename T>
    GP_INLINE void BindMember(const T* object, R (T::*func)(Args...) const)
    {
        GP_ASSERT(object != nullptr);
        GP_ASSERT(func != nullptr);
        m_storage =
            FStorage{ [object, func](Args&&... args) -> R { return (object->*func)(std::forward<Args>(args)...); } };
        m_weakPtrValidator = nullptr;
    }

    /// \brief Binds a non-const member function via a shared_ptr, using a weak reference internally. IsBound() returns
    /// false once the shared_ptr's managed object has been destroyed, making it safe to use ExecuteIfBound() without
    /// worrying about dangling pointers.
    /// \tparam T The class that owns the member function.
    /// \param sp A non-null shared_ptr to the target object.
    /// \param func Pointer to the member function to invoke.
    /// \warning Execute() will GP_ASSERT if called after the tracked object has been destroyed. Prefer ExecuteIfBound()
    /// to handle the expired case gracefully.
    template <typename T>
    GP_INLINE void BindWeakSP(const std::shared_ptr<T>& sp, R (T::*func)(Args...))
    {
        GP_ASSERT(sp != nullptr);
        GP_ASSERT(func != nullptr);
        std::weak_ptr<T> wp = sp;
        m_storage = FStorage{ [wp, func](Args&&... args) -> R
                              {
                                  auto locked = wp.lock();
                                  GP_ASSERT(
                                      locked != nullptr,
                                      "BindWeakSP: the tracked object has been destroyed. Use ExecuteIfBound()."
                                  );
                                  return (locked.get()->*func)(std::forward<Args>(args)...);
                              } };
        m_weakPtrValidator = [wp]() -> bool { return !wp.expired(); };
    }

    /// \brief Binds a const member function via a shared_ptr, using a weak reference internally.
    /// IsBound() returns false once the shared_ptr's managed object has been destroyed.
    /// \tparam T The class that owns the member function.
    /// \param sp A non-null shared_ptr to the target object.
    /// \param func Pointer to the const member function to invoke.
    template <typename T>
    GP_INLINE void BindWeakSP(const std::shared_ptr<T>& sp, R (T::*func)(Args...) const)
    {
        GP_ASSERT(sp != nullptr);
        GP_ASSERT(func != nullptr);
        std::weak_ptr<T> wp = sp;
        m_storage = FStorage{ [wp, func](Args&&... args) -> R
                              {
                                  auto locked = wp.lock();
                                  GP_ASSERT(
                                      locked != nullptr,
                                      "BindWeakSP: the tracked object has been destroyed. Use ExecuteIfBound()."
                                  );
                                  return (locked.get()->*func)(std::forward<Args>(args)...);
                              } };
        m_weakPtrValidator = [wp]() -> bool { return !wp.expired(); };
    }

    /// \brief Removes the current binding. After this call, IsBound() returns false.
    GP_INLINE void Unbind() noexcept
    {
        m_storage = nullptr;
        m_weakPtrValidator = nullptr;
    }

    /// \brief Returns true if a callable is currently bound and, for weak-pointer bindings, the tracked object
    /// is still alive.
    /// \return True if the delegate is ready to be invoked, false otherwise.
    GP_NODISCARD GP_INLINE bool IsBound() const noexcept
    {
        if (!static_cast<bool>(m_storage)) { return false; }
        if (m_weakPtrValidator) { return m_weakPtrValidator(); }
        return true;
    }

    /// \brief Invokes the bound callable unconditionally.
    /// \param args Arguments forwarded to the callable.
    /// \return The return value of the callable.
    /// \warning Triggers a GP_ASSERT if no callable is bound. Prefer ExecuteIfBound() for optional invocation.
    GP_INLINE R Execute(Args... args)
    {
        GP_ASSERT(IsBound(), "TDelegate::Execute called on an unbound delegate.");
        return m_storage(std::forward<Args>(args)...);
    }

    /// \brief Invokes the bound callable only if one is bound. For non-void return types, returns std::optional<R>
    /// containing the result, or std::nullopt if no callable is bound. For void return types, returns true if the
    /// callable was invoked, false otherwise.
    /// \param args Arguments forwarded to the callable.
    /// \return std::optional<R> (non-void) or bool (void).
    GP_INLINE FOptionalReturn ExecuteIfBound(Args... args)
    {
        if constexpr (std::is_void_v<R>)
        {
            if (IsBound())
            {
                m_storage(std::forward<Args>(args)...);
                return true;
            }
            return false;
        }
        else
        {
            if (IsBound()) { return std::optional<R>{ m_storage(std::forward<Args>(args)...) }; }
            return std::nullopt;
        }
    }
};

}   // namespace GP::Events
