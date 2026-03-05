// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Memory/SharedPtr.hpp"
#include "Memory/WeakPtr.hpp"

namespace GP
{

/// @brief Mixin base class that allows an object managed by TSharedPtr to safely create additional TSharedPtr instances
///        that refer to itself.
///
///        Without this, constructing a TSharedPtr from `this` would create a second control block - causing a
///        double-free.  This class ensures that all such pointers share a single control block.
///
///        Usage:
///        @code
///            class FMyActor : public TEnableSharedFromThis<FMyActor>
///            {
///            public:
///                void Subscribe(FEventBus& Bus)
///                {
///                    Bus.Register(SharedFromThis());   // safe - same control block
///                }
///            };
///
///            auto Actor = MakeShared<FMyActor>();
///            Actor->Subscribe(EventBus);
///        @endcode
///
/// @warning SharedFromThis() is only valid AFTER the object is managed by a TSharedPtr. Calling it on a raw or
///          stack-allocated instance is undefined behaviour.
///
/// @tparam T The derived class (CRTP parameter).
template <typename T>
class TEnableSharedFromThis
{
private:
    mutable TWeakPtr<T> m_weakThis;

protected:
    template <typename U>
    friend class TSharedPtr;

protected:
    TEnableSharedFromThis() noexcept = default;
    ~TEnableSharedFromThis() noexcept = default;

    TEnableSharedFromThis(const TEnableSharedFromThis&) noexcept {}

protected:
    TEnableSharedFromThis& operator=(const TEnableSharedFromThis&) noexcept { return *this; }

public:
    /// @brief Returns a TSharedPtr<T> sharing ownership with the existing owner.
    GP_NODISCARD TSharedPtr<T> SharedFromThis()
    {
        TSharedPtr<T> strong = m_weakThis.Lock();
        GP_ASSUME(strong.Get() != nullptr);   // UB if called before a TSharedPtr exists
        return strong;
    }

    /// @brief Returns a TSharedPtr<const T> sharing ownership with the existing owner.
    GP_NODISCARD TSharedPtr<const T> SharedFromThis() const
    {
        TSharedPtr<const T> strong = m_weakThis.Lock();
        GP_ASSUME(strong.Get() != nullptr);
        return strong;
    }

    /// @brief Returns a TWeakPtr<T> observing the managed object.
    GP_NODISCARD TWeakPtr<T> WeakFromThis() noexcept { return m_weakThis; }

    /// @brief Returns a TWeakPtr<const T> observing the managed object.
    GP_NODISCARD TWeakPtr<const T> WeakFromThis() const noexcept { return TWeakPtr<const T>(m_weakThis); }

protected:
    /// @brief Called by TSharedPtr constructor (via duck-typing detection)
    ///        to inject the initial weak reference.  Only injected once —
    ///        subsequent calls from aliasing or converting constructors are
    ///        ignored if the weak ref is already valid.
    void InternalInitWeakThis(const TSharedPtr<T>& shared) const noexcept
    {
        if (m_weakThis.IsExpired()) { m_weakThis = TWeakPtr<T>(shared); }
    }
};

}   // namespace GP
