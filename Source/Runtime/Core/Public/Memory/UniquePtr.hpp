// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include "Memory/DefaultDeleter.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"
#include "Templates/Core/RemoveExtent.hpp"
#include "Templates/Core/RemoveReference.hpp"
#include "Templates/Core/Utility.hpp"
#include <compare>

namespace GP
{

/// @brief Exclusive-ownership smart pointer with zero overhead.
///
///        - Non-copyable: ownership is unique.
///        - Movable: ownership transfers on move.
///        - The deleter is stored via [[no_unique_address]]; stateless deleters
///          (e.g. TDefaultDeleter) add zero bytes to the object size.
///        - Destroying or resetting the pointer calls the deleter exactly once.
///
/// @tparam T The managed type.
/// @tparam TDeleter The deleter type. Defaults to TDefaultDeleter<T>.
template <typename T, typename TDeleter = TDefaultDeleter<T>>
class TUniquePtr
{
public:
    using ElementType = T;
    using DeleterType = TDeleter;
    using PointerType = T*;

private:
    T* m_ptr{ nullptr };
    GP_NO_UNIQUE_ADDRESS TDeleter m_deleter{};

private:
    template <typename U, typename UDeleter>
    friend class TUniquePtr;

public:
    constexpr TUniquePtr() noexcept = default;

    constexpr TUniquePtr(decltype(nullptr)) noexcept {}

    explicit TUniquePtr(T* inPtr) noexcept
        : m_ptr(inPtr)
    {}

    TUniquePtr(T* inPtr, const TDeleter& inDeleter) noexcept
        : m_ptr(inPtr)
        , m_deleter(inDeleter)
    {}

    TUniquePtr(T* inPtr, TDeleter&& inDeleter) noexcept
        : m_ptr(inPtr)
        , m_deleter(GP::Move(inDeleter))
    {}

    TUniquePtr(TUniquePtr&& other) noexcept
        : m_ptr(other.Release())
        , m_deleter(GP::Move(other.m_deleter))
    {}

    /// @brief Converting move constructor: TUniquePtr<Derived> → TUniquePtr<Base>.
    template <typename U, typename UDeleter>
    requires CDerivedFrom<U, T> && CConvertibleTo<UDeleter, TDeleter>
    TUniquePtr(TUniquePtr<U, UDeleter>&& other) noexcept
        : m_ptr(other.Release())
        , m_deleter(GP::Forward<UDeleter>(other.GetDeleter()))
    {}

    TUniquePtr(const TUniquePtr&) = delete;

    ~TUniquePtr() noexcept
    {
        if (m_ptr) { m_deleter(m_ptr); }
    }

public:
    TUniquePtr& operator=(TUniquePtr&& other) noexcept
    {
        if GP_LIKELY (this != &other)
        {
            Reset(other.Release());
            m_deleter = GP::Move(other.m_deleter);
        }
        return *this;
    }

    template <typename U, typename UDeleter>
    requires CDerivedFrom<U, T> && CConvertibleTo<UDeleter, TDeleter>
    TUniquePtr& operator=(TUniquePtr<U, UDeleter>&& other) noexcept
    {
        Reset(other.Release());
        m_deleter = GP::Forward<UDeleter>(other.GetDeleter());
        return *this;
    }

    TUniquePtr& operator=(decltype(nullptr)) noexcept
    {
        Reset();
        return *this;
    }

    TUniquePtr& operator=(const TUniquePtr&) = delete;

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

    GP_NODISCARD TDeleter& GetDeleter() noexcept { return m_deleter; }

    GP_NODISCARD const TDeleter& GetDeleter() const noexcept { return m_deleter; }

    /// @brief Releases ownership. The caller becomes responsible for deletion.
    /// @return The raw pointer. Caller must manage its lifetime.
    GP_NODISCARD T* Release() noexcept
    {
        T* ptr = m_ptr;
        m_ptr = nullptr;
        return ptr;
    }

    /// @brief Destroys the current object (if any) and takes ownership of inPtr.
    void Reset(T* inPtr = nullptr) noexcept
    {
        T* old = m_ptr;
        m_ptr = inPtr;
        if (old) { m_deleter(old); }
    }

    void Swap(TUniquePtr& other) noexcept
    {
        T* tempPtr = m_ptr;
        m_ptr = other.m_ptr;
        other.m_ptr = tempPtr;
        TDeleter tempDeleter = GP::Move(m_deleter);
        m_deleter = GP::Move(other.m_deleter);
        other.m_deleter = GP::Move(tempDeleter);
    }
};

/// @note TUniquePtr<T[], TDeleter>  —  Array Specialisation
template <typename T, typename TDeleter>
class TUniquePtr<T[], TDeleter>
{
public:
    using ElementType = T;
    using DeleterType = TDeleter;
    using PointerType = T*;

private:
    T* m_ptr{ nullptr };
    GP_NO_UNIQUE_ADDRESS TDeleter m_deleter{};

public:
    constexpr TUniquePtr() noexcept = default;

    constexpr TUniquePtr(decltype(nullptr)) noexcept {}

    explicit TUniquePtr(T* inPtr) noexcept
        : m_ptr(inPtr)
    {}

    TUniquePtr(T* inPtr, const TDeleter& inDeleter) noexcept
        : m_ptr(inPtr)
        , m_deleter(inDeleter)
    {}

    TUniquePtr(T* inPtr, TDeleter&& inDeleter) noexcept
        : m_ptr(inPtr)
        , m_deleter(GP::Move(inDeleter))
    {}

    TUniquePtr(TUniquePtr&& other) noexcept
        : m_ptr(other.Release())
        , m_deleter(GP::Move(other.m_deleter))
    {}

    TUniquePtr(const TUniquePtr&) = delete;

    ~TUniquePtr() noexcept
    {
        if (m_ptr) { m_deleter(m_ptr); }
    }

public:
    TUniquePtr& operator=(TUniquePtr&& other) noexcept
    {
        if GP_LIKELY (this != &other)
        {
            Reset(other.Release());
            m_deleter = GP::Move(other.m_deleter);
        }
        return *this;
    }

    TUniquePtr& operator=(decltype(nullptr)) noexcept
    {
        Reset();
        return *this;
    }

    TUniquePtr& operator=(const TUniquePtr&) = delete;

    GP_NODISCARD T& operator[](SizeT index) const noexcept { return m_ptr[index]; }

public:
    GP_NODISCARD T* Get() const noexcept { return m_ptr; }

    GP_NODISCARD explicit operator bool() const noexcept { return m_ptr != nullptr; }

    GP_NODISCARD TDeleter& GetDeleter() noexcept { return m_deleter; }

    GP_NODISCARD const TDeleter& GetDeleter() const noexcept { return m_deleter; }

    GP_NODISCARD T* Release() noexcept
    {
        T* ptr = m_ptr;
        m_ptr = nullptr;
        return ptr;
    }

    void Reset(T* inPtr = nullptr) noexcept
    {
        T* old = m_ptr;
        m_ptr = inPtr;
        if (old) { m_deleter(old); }
    }

    void Swap(TUniquePtr& other) noexcept
    {
        T* tempPTr = m_ptr;
        m_ptr = other.m_ptr;
        other.m_ptr = tempPTr;
        TDeleter tempDeleter = GP::Move(m_deleter);
        m_deleter = GP::Move(other.m_deleter);
        other.m_deleter = GP::Move(tempDeleter);
    }
};

///
/// @section Non-member comparison operators
///

template <typename T, typename TD, typename U, typename UD>
GP_NODISCARD bool operator==(const TUniquePtr<T, TD>& lhs, const TUniquePtr<U, UD>& rhs) noexcept
{
    return lhs.Get() == rhs.Get();
}

template <typename T, typename TD, typename U, typename UD>
GP_NODISCARD auto operator<=>(const TUniquePtr<T, TD>& lhs, const TUniquePtr<U, UD>& rhs) noexcept
{
    return lhs.Get() <=> rhs.Get();
}

template <typename T, typename TD>
GP_NODISCARD bool operator==(const TUniquePtr<T, TD>& lhs, decltype(nullptr)) noexcept
{
    return !lhs;
}

template <typename T, typename TD>
GP_NODISCARD auto operator<=>(const TUniquePtr<T, TD>& lhs, decltype(nullptr)) noexcept
{
    return lhs.Get() <=> static_cast<T*>(nullptr);
}

/// @brief Swap specialization for TUniquePtr. Swaps both the pointer and the deleter.
template <typename T, typename TD>
void Swap(TUniquePtr<T, TD>& a, TUniquePtr<T, TD>& b) noexcept
{
    a.Swap(b);
}

/// @brief Constructs T with Args and wraps it in a TUniquePtr.
///        Equivalent to TUniquePtr<T>(new T(Args...)) but exception-safe
///        and avoids naked new at the call site.
///
/// @tparam T The type to construct.
/// @tparam TArgs Constructor argument types.
template <typename T, typename... TArgs>
requires(!TIsArray_V<T>) GP_NODISCARD TUniquePtr<T> MakeUnique(TArgs&&... args)
{
    return TUniquePtr<T>(new T(GP::Forward<TArgs>(args)...));
}

/// @brief Constructs a default-initialised array of Count elements.
///
/// @tparam T The array element type (must be T[]).
/// @param count Number of elements to allocate.
template <typename T>
requires TIsUnboundedArray_V<T> GP_NODISCARD TUniquePtr<T> MakeUnique(SizeT count)
{
    using TElem = TRemoveExtent_T<T>;
    return TUniquePtr<T>(new TElem[count]());
}

/// @brief Constructs a value-initialised array of N elements (bounded array).
///
/// @tparam T Bounded array type, e.g. int[4].
template <typename T>
requires TIsBoundedArray_V<T>
GP_NODISCARD TUniquePtr<T> MakeUnique() = delete;   // standard mandates this overload be deleted

/// @brief Constructs T with Args, leaving the object default-initialised (no zero-fill).
///        Slightly faster than MakeUnique for trivial types when zeroing is unnecessary.
template <typename T, typename... TArgs>
requires(!TIsArray_V<T>) GP_NODISCARD TUniquePtr<T> MakeUniqueForOverwrite()
{
    return TUniquePtr<T>(new T);
}

}   // namespace GP
