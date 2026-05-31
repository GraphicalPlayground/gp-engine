// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"
#include <functional>

namespace gp
{

/// @brief A lightweight wrapper that explicitly tags a value as an error.
/// @note Pedagogical Note: Without this tag type, constructing an Expected in the error
/// state would be deeply ambiguous whenever T and E happen to share the same underlying
/// type (e.g. Expected<int, int>). By forcing callers to wrap their error in Unexpected<E>,
/// we push the disambiguation into the call site, readable, unambiguous, zero-cost.
/// It plays exactly the same role as NullOptT in Optional: its sole purpose is to carry
/// semantic intent to the constructor, so the compiler can dispatch correctly.
/// @tparam E Error type. Must be destructible and not a reference.
template <typename E>
requires concepts::IsDestructible<E> && (!concepts::IsReference<E>)class Unexpected
{
public:
    using ErrorType = E;

private:
    E m_error;

public:
    /// @brief Constructs an Unexpected by copying the given error.
    /// @param[in] error Error value to wrap.
    explicit constexpr Unexpected(const E& error) noexcept(noexcept(E(error)))
        : m_error(error)
    {}

    /// @brief Constructs an Unexpected by moving the given error.
    /// @param[in] error Error value to move.
    explicit constexpr Unexpected(E&& error) noexcept(noexcept(E(std::move(error))))
        : m_error(std::move(error))
    {}

    /// @brief Constructs an Unexpected in-place, forwarding arguments directly to E's constructor.
    /// @tparam Args Constructor argument types.
    /// @param[in] args Arguments forwarded to E's constructor.
    template <typename... Args>
    requires concepts::IsConstructibleWith<E, Args...> &&
             (sizeof...(Args) > 1 || (!std::is_same_v<std::remove_cvref_t<Args>, Unexpected> && ...))
    explicit constexpr Unexpected(Args&&... args) noexcept(noexcept(E(std::forward<Args>(args)...)))
        : m_error(std::forward<Args>(args)...)
    {}

    /// @brief Default copy constructor.
    constexpr Unexpected(const Unexpected&) = default;

    /// @brief Default move constructor.
    constexpr Unexpected(Unexpected&&) = default;

public:
    /// @brief Default copy assignment operator.
    /// @return Reference to this Unexpected.
    constexpr Unexpected& operator=(const Unexpected&) = default;

    /// @brief Default move assignment operator.
    /// @return Reference to this Unexpected.
    constexpr Unexpected& operator=(Unexpected&&) = default;

    /// @brief Checks equality of two Unexpected wrappers.
    /// @param[in] other Unexpected to compare with.
    /// @return True if both wrapped errors are equal, false otherwise.
    GP_NODISCARD constexpr bool operator==(const Unexpected& other) const noexcept
        requires concepts::IsEqualityComparable<E>
    {
        return m_error == other.m_error;
    }

    /// @brief Checks inequality of two Unexpected wrappers.
    /// @param[in] other Unexpected to compare with.
    /// @return True if the wrapped errors differ, false otherwise.
    GP_NODISCARD constexpr bool operator!=(const Unexpected& other) const noexcept
        requires concepts::IsEqualityComparable<E>
    {
        return !(*this == other);
    }

public:
    /// @brief Accesses the wrapped error (lvalue).
    /// @return Reference to the wrapped error.
    GP_NODISCARD constexpr E& error() & noexcept
    {
        return m_error;
    }

    /// @brief Accesses the wrapped error (const lvalue).
    /// @return Const reference to the wrapped error.
    GP_NODISCARD constexpr const E& error() const& noexcept
    {
        return m_error;
    }

    /// @brief Accesses the wrapped error (rvalue).
    /// @return Rvalue reference to the wrapped error.
    GP_NODISCARD constexpr E&& error() && noexcept
    {
        return std::move(m_error);
    }
};

/// @brief Class template argument deduction guide for Unexpected.
template <typename E>
Unexpected(E) -> Unexpected<std::decay_t<E>>;

/// @brief Factory function to create an Unexpected<E> with perfect forwarding.
/// @note Symmetric with makeOptional and makePair.
/// @tparam E Error type, deduced from the argument.
/// @param[in] error Error value to wrap.
/// @return Unexpected<decay_t<E>> holding the provided error.
template <typename E>
GP_NODISCARD constexpr Unexpected<std::decay_t<E>>
    makeUnexpected(E&& error) noexcept(noexcept(Unexpected<std::decay_t<E>>(std::forward<E>(error))))
{
    return Unexpected<std::decay_t<E>>(std::forward<E>(error));
}

/// @brief Holds either a success value of type T, or a failure error of type E. Never both.
/// @note No heap allocation. T and E share a single aligned inline storage buffer sized for
/// whichever type is larger. Use hasValue() / operator bool to check the active state before
/// accessing either track.
/// @note Pedagogical Note: C++ exceptions are widely disabled in AAA game engines, they
/// inflate binary size, produce non-deterministic frame times on the throw path (stack
/// unwinding), and make control flow hard to reason about at a glance. Expected<T, E> solves
/// the same problem as a plain value: errors become first-class citizens in the type system,
/// GP_NODISCARD forces the caller to acknowledge the result, and the generated machine code
/// reduces to a flag check and a branch, no runtime magic, no hidden vtable dispatch.
/// @note Railway-Oriented Programming: picture a railway with two parallel tracks, the
/// "value track" (success) and the "error track" (failure). The four monadic operations let
/// you build a pipeline that stays on the value track as long as every step succeeds, and
/// silently diverts to the error track at the first failure, propagating it all the way to
/// the end without a single manual if-check in the middle:
/// @code
///   Expected<Mesh, AssetError> mesh =
///       openFile(path)
///           .andThen(parseHeader)
///           .andThen(buildMesh)
///           .transformError([](IoError e){ return AssetError{e}; });
/// @endcode
/// @tparam T Success value type. Must not be a reference. T = void is not supported here;
///           use Expected<bool, E> or a unit struct as a workaround.
/// @tparam E Error type. Must not be a reference.
template <typename T, typename E>
requires concepts::IsDestructible<T> && (!concepts::IsReference<T>) && concepts::IsDestructible<E> &&
         (!concepts::IsReference<E>)class Expected
{
public:
    using ValueType = T;
    using ErrorType = E;
    using UnexpectedType = Unexpected<E>;
    using ThisType = Expected<T, E>;

private:
    static constexpr std::size_t kStorageSize = sizeof(T) > sizeof(E) ? sizeof(T) : sizeof(E);
    static constexpr std::size_t kStorageAlign = alignof(T) > alignof(E) ? alignof(T) : alignof(E);

    GP_ALIGN(kStorageAlign) Byte m_storage[kStorageSize];
    bool m_hasValue = false;

public:
    /// @brief Default-constructs a successful Expected holding a default-constructed T.
    /// @note A default-constructed Expected is always in the value state, "success until
    /// proven otherwise" is a safer default and mirrors std::expected's design decision.
    constexpr Expected() noexcept(noexcept(T())) requires concepts::IsDefaultConstructible<T>
        : m_hasValue(true)
    {
        ::new (&m_storage) T();
    }

    /// @brief Copy-constructs a successful Expected holding @p value.
    /// @param[in] value Value to store.
    constexpr Expected(const T& value) noexcept(noexcept(T(value)))
        : m_hasValue(true)
    {
        ::new (&m_storage) T(value);
    }

    /// @brief Move-constructs a successful Expected holding @p value.
    /// @param[in] value Value to move.
    constexpr Expected(T&& value) noexcept(noexcept(T(std::move(value))))
        : m_hasValue(true)
    {
        ::new (&m_storage) T(std::move(value));
    }

    /// @brief Constructs a successful Expected from a compatible type via perfect forwarding.
    /// @note This constructor is intentionally conditionally explicit: when the conversion
    /// from U to T is itself explicit, we propagate that explicitness so the semantic
    /// contract of the underlying types is preserved.
    /// @tparam U Source type. Must be constructible to T, and must not itself be an Expected
    ///           or Unexpected (prevents ambiguity with the dedicated overloads below).
    /// @param[in] value Value to forward.
    template <typename U = T>
    requires concepts::IsConstructibleWith<T, U&&> && (!std::is_same_v<std::remove_cvref_t<U>, Expected>) &&
             (!std::is_same_v<std::remove_cvref_t<U>, Unexpected<E>>) && (!std::is_same_v<std::remove_cvref_t<U>, T>)
    explicit(!std::is_convertible_v<U&&, T>) constexpr Expected(U&& value) noexcept(noexcept(T(std::forward<U>(value))))
        : m_hasValue(true)
    {
        ::new (&m_storage) T(std::forward<U>(value));
    }

    /// @brief Constructs a failed Expected from a copied Unexpected<E>.
    /// @param[in] unexpected Unexpected wrapper holding the error to copy.
    constexpr Expected(const Unexpected<E>& unexpected) noexcept(noexcept(E(unexpected.error())))
        : m_hasValue(false)
    {
        ::new (&m_storage) E(unexpected.error());
    }

    /// @brief Constructs a failed Expected from a moved Unexpected<E>.
    /// @param[in] unexpected Unexpected wrapper holding the error to move.
    constexpr Expected(Unexpected<E>&& unexpected) noexcept(noexcept(E(std::move(unexpected.error()))))
        : m_hasValue(false)
    {
        ::new (&m_storage) E(std::move(unexpected.error()));
    }

    /// @brief Copy-constructs from another Expected.
    /// @param[in] other Expected to copy from.
    constexpr Expected(const Expected& other)
        : m_hasValue(other.m_hasValue)
    {
        if (m_hasValue)
        {
            ::new (&m_storage) T(*other.valuePtr());
        }
        else
        {
            ::new (&m_storage) E(*other.errorPtr());
        }
    }

    /// @brief Move-constructs from another Expected.
    /// @param[in] other Expected to move from.
    constexpr Expected(
        Expected&& other
    ) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<E>)
        : m_hasValue(other.m_hasValue)
    {
        if (m_hasValue)
        {
            ::new (&m_storage) T(std::move(*other.valuePtr()));
        }
        else
        {
            ::new (&m_storage) E(std::move(*other.errorPtr()));
        }
    }

    /// @brief Destroys whichever object (value or error) is currently alive.
    ~Expected()
    {
        destroyActive();
    }

public:
    /// @brief Copy-assigns from another Expected.
    /// @note When source and destination are on different tracks, the live object must be
    /// destroyed and the new one placement-new'd into the same storage. The flag is only
    /// flipped after the construction succeeds to keep the object in a consistent state.
    /// @param[in] other Expected to copy from.
    /// @return Reference to this Expected.
    Expected& operator=(const Expected& other)
    {
        if (this == &other)
        {
            return *this;
        }
        if (m_hasValue && other.m_hasValue)
        {
            *valuePtr() = *other.valuePtr();
        }
        else if (!m_hasValue && !other.m_hasValue)
        {
            *errorPtr() = *other.errorPtr();
        }
        else if (m_hasValue && !other.m_hasValue)
        {
            destroyActive();
            ::new (&m_storage) E(*other.errorPtr());
            m_hasValue = false;
        }
        else   // !m_hasValue && other.m_hasValue
        {
            destroyActive();
            ::new (&m_storage) T(*other.valuePtr());
            m_hasValue = true;
        }
        return *this;
    }

    /// @brief Move-assigns from another Expected.
    /// @param[in] other Expected to move from.
    /// @return Reference to this Expected.
    Expected& operator=(Expected&& other) noexcept(
        std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<E> &&
        std::is_nothrow_move_assignable_v<T> && std::is_nothrow_move_assignable_v<E>
    )
    {
        if (this == &other)
        {
            return *this;
        }
        if (m_hasValue && other.m_hasValue)
        {
            *valuePtr() = std::move(*other.valuePtr());
        }
        else if (!m_hasValue && !other.m_hasValue)
        {
            *errorPtr() = std::move(*other.errorPtr());
        }
        else if (m_hasValue && !other.m_hasValue)
        {
            destroyActive();
            ::new (&m_storage) E(std::move(*other.errorPtr()));
            m_hasValue = false;
        }
        else   // !m_hasValue && other.m_hasValue
        {
            destroyActive();
            ::new (&m_storage) T(std::move(*other.valuePtr()));
            m_hasValue = true;
        }
        return *this;
    }

    /// @brief Assigns a value directly, constructing or reassigning T as needed.
    /// @tparam U Type of the incoming value.
    /// @param[in] inValue Value to assign.
    /// @return Reference to this Expected.
    template <typename U = T>
    requires concepts::IsConstructibleWith<T, U&&> && std::is_assignable_v<T&, U&&> &&
             (!std::is_same_v<std::remove_cvref_t<U>, Expected>) &&
             (!std::is_same_v<std::remove_cvref_t<U>, Unexpected<E>>)Expected&
        operator=(U&& inValue)
    {
        if (m_hasValue)
        {
            *valuePtr() = std::forward<U>(inValue);
        }
        else
        {
            destroyActive();
            ::new (&m_storage) T(std::forward<U>(inValue));
            m_hasValue = true;
        }
        return *this;
    }

    /// @brief Assigns an error from a copied Unexpected<E>, destroying any current state.
    /// @param[in] unexpected Error wrapper to copy.
    /// @return Reference to this Expected.
    Expected& operator=(const Unexpected<E>& unexpected)
    {
        if (!m_hasValue)
        {
            *errorPtr() = unexpected.error();
        }
        else
        {
            destroyActive();
            ::new (&m_storage) E(unexpected.error());
            m_hasValue = false;
        }
        return *this;
    }

    /// @brief Assigns an error from a moved Unexpected<E>, destroying any current state.
    /// @param[in] unexpected Error wrapper to move.
    /// @return Reference to this Expected.
    Expected& operator=(Unexpected<E>&& unexpected)
    {
        if (!m_hasValue)
        {
            *errorPtr() = std::move(unexpected.error());
        }
        else
        {
            destroyActive();
            ::new (&m_storage) E(std::move(unexpected.error()));
            m_hasValue = false;
        }
        return *this;
    }

    /// @brief Checks equality between two Expected instances.
    /// @note Two Expecteds are equal only when they share the same track AND their contents
    /// compare equal. A value-state Expected is never equal to an error-state Expected, even
    /// if T and E are the same type and the bit patterns happen to match, state is part of
    /// the identity.
    /// @param[in] other Expected to compare with.
    /// @return True if both are on the same track with equal contents, false otherwise.
    GP_NODISCARD bool operator==(const Expected& other) const noexcept
        requires concepts::IsEqualityComparable<T> && concepts::IsEqualityComparable<E>
    {
        if (m_hasValue != other.m_hasValue)
        {
            return false;
        }
        return m_hasValue ? (*valuePtr() == *other.valuePtr()) : (*errorPtr() == *other.errorPtr());
    }

    /// @brief Checks inequality between two Expected instances.
    /// @param[in] other Expected to compare with.
    /// @return True if the instances differ, false otherwise.
    GP_NODISCARD bool operator!=(const Expected& other) const noexcept
        requires concepts::IsEqualityComparable<T> && concepts::IsEqualityComparable<E>
    {
        return !(*this == other);
    }

    /// @brief Checks if this Expected holds a value equal to @p inValue.
    /// @param[in] inValue Value to compare against.
    /// @return True if in value state and the stored value equals @p inValue, false otherwise.
    GP_NODISCARD bool operator==(const T& inValue) const noexcept requires concepts::IsEqualityComparable<T>
    {
        return m_hasValue && *valuePtr() == inValue;
    }

    /// @brief Checks if this Expected holds the error contained in @p unexpected.
    /// @param[in] unexpected Unexpected to compare against.
    /// @return True if in error state and the stored error equals the wrapped error, false otherwise.
    GP_NODISCARD bool operator==(const Unexpected<E>& unexpected) const noexcept
        requires concepts::IsEqualityComparable<E>
    {
        return !m_hasValue && *errorPtr() == unexpected.error();
    }

    /// @brief Dereferences to the contained value (lvalue).
    /// @return Reference to the contained value.
    GP_NODISCARD constexpr T& operator*() & noexcept
    {
        return value();
    }

    /// @brief Dereferences to the contained value (const lvalue).
    /// @return Const reference to the contained value.
    GP_NODISCARD constexpr const T& operator*() const& noexcept
    {
        return value();
    }

    /// @brief Dereferences to the contained value (rvalue).
    /// @return Rvalue reference to the contained value.
    GP_NODISCARD constexpr T&& operator*() && noexcept
    {
        return std::move(value());
    }

    /// @brief Accesses members of the contained value.
    /// @return Pointer to the contained value.
    GP_NODISCARD constexpr T* operator->() noexcept
    {
        return &value();
    }

    /// @brief Accesses members of the contained value (const).
    /// @return Const pointer to the contained value.
    GP_NODISCARD constexpr const T* operator->() const noexcept
    {
        return &value();
    }

    /// @brief Converts to bool: true if in the success (value) state.
    /// @return True if holding a value, false if holding an error.
    GP_NODISCARD constexpr explicit operator bool() const noexcept
    {
        return m_hasValue;
    }

public:
    /// @brief Returns true if the Expected holds a value (success state).
    /// @return True if in value state, false otherwise.
    GP_NODISCARD constexpr bool hasValue() const noexcept
    {
        return m_hasValue;
    }

    /// @brief Accesses the contained value (lvalue), asserting if in error state.
    /// @note We assert rather than throw to honour the engine's "no exceptions" contract.
    /// In a debug build GP_ASSERT catches misuse loudly; in release it is a zero-cost no-op.
    /// @return Reference to the contained value.
    GP_NODISCARD constexpr T& value() & noexcept
    {
        GP_ASSERT(m_hasValue);
        return *valuePtr();
    }

    /// @brief Accesses the contained value (const lvalue), asserting if in error state.
    /// @return Const reference to the contained value.
    GP_NODISCARD constexpr const T& value() const& noexcept
    {
        GP_ASSERT(m_hasValue);
        return *valuePtr();
    }

    /// @brief Accesses the contained value (rvalue), asserting if in error state.
    /// @return Rvalue reference to the contained value.
    GP_NODISCARD constexpr T&& value() && noexcept
    {
        GP_ASSERT(m_hasValue);
        return std::move(*valuePtr());
    }

    /// @brief Accesses the contained error (lvalue), asserting if in value state.
    /// @return Reference to the contained error.
    GP_NODISCARD constexpr E& error() & noexcept
    {
        GP_ASSERT(!m_hasValue);
        return *errorPtr();
    }

    /// @brief Accesses the contained error (const lvalue), asserting if in value state.
    /// @return Const reference to the contained error.
    GP_NODISCARD constexpr const E& error() const& noexcept
    {
        GP_ASSERT(!m_hasValue);
        return *errorPtr();
    }

    /// @brief Accesses the contained error (rvalue), asserting if in value state.
    /// @return Rvalue reference to the contained error.
    GP_NODISCARD constexpr E&& error() && noexcept
    {
        GP_ASSERT(!m_hasValue);
        return std::move(*errorPtr());
    }

    /// @brief Returns the contained value if in value state, otherwise returns @p defaultValue.
    /// @param[in] defaultValue Fallback value returned when in error state.
    /// @return The stored value or @p defaultValue.
    GP_NODISCARD constexpr T valueOr(T&& defaultValue) const& noexcept
    {
        return m_hasValue ? *valuePtr() : std::move(defaultValue);
    }

    /// @brief Returns the contained value if in value state, otherwise returns @p defaultValue (rvalue overload).
    /// @param[in] defaultValue Fallback value returned when in error state.
    /// @return The stored value or @p defaultValue.
    GP_NODISCARD constexpr T valueOr(T&& defaultValue) && noexcept
    {
        return m_hasValue ? std::move(*valuePtr()) : std::move(defaultValue);
    }

    /// @brief Returns the contained error if in error state, otherwise returns @p defaultError.
    /// @param[in] defaultError Fallback error returned when in value state.
    /// @return The stored error or @p defaultError.
    GP_NODISCARD constexpr E errorOr(E&& defaultError) const& noexcept
    {
        return !m_hasValue ? *errorPtr() : std::move(defaultError);
    }

    /// @brief Returns the contained error if in error state, otherwise returns @p defaultError (rvalue overload).
    /// @param[in] defaultError Fallback error returned when in value state.
    /// @return The stored error or @p defaultError.
    GP_NODISCARD constexpr E errorOr(E&& defaultError) && noexcept
    {
        return !m_hasValue ? std::move(*errorPtr()) : std::move(defaultError);
    }

    /// @brief Destroys the current contents and constructs a new value in-place.
    /// @tparam Args Constructor argument types.
    /// @param[in] args Arguments forwarded to T's constructor.
    /// @return Reference to the newly constructed value.
    template <typename... Args>
    requires concepts::IsConstructibleWith<T, Args...> T& emplaceValue(Args&&... args)
    {
        destroyActive();
        ::new (&m_storage) T(std::forward<Args>(args)...);
        m_hasValue = true;
        return *valuePtr();
    }

    /// @brief Destroys the current contents and constructs a new error in-place.
    /// @tparam Args Constructor argument types.
    /// @param[in] args Arguments forwarded to E's constructor.
    /// @return Reference to the newly constructed error.
    template <typename... Args>
    requires concepts::IsConstructibleWith<E, Args...> E& emplaceError(Args&&... args)
    {
        destroyActive();
        ::new (&m_storage) E(std::forward<Args>(args)...);
        m_hasValue = false;
        return *errorPtr();
    }

    /// @brief Swaps the contents of this Expected with another.
    /// @note Pedagogical Note: Swapping two Expecteds that are on different tracks requires
    /// a temporary because T and E share the same storage region. The cross-track path saves
    /// one side into a stack temporary, constructs the other side in the freed slot, then
    /// constructs from the temporary. Three moves, zero heap allocations. On same-track swaps
    /// we delegate to std::swap which is free to use optimised intrinsics for trivial types.
    /// @param[in] other Expected to swap with.
    void swap(
        Expected& other
    ) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<E>)
    {
        if (m_hasValue && other.m_hasValue)
        {
            std::swap(*valuePtr(), *other.valuePtr());
        }
        else if (!m_hasValue && !other.m_hasValue)
        {
            std::swap(*errorPtr(), *other.errorPtr());
        }
        else if (m_hasValue && !other.m_hasValue)
        {
            // this = value  |  other = error
            // Step 1: save other's error on the stack.
            // Step 2: destroy other's error and construct this's value in other.
            // Step 3: destroy this's value and construct saved error in this.
            E tempError(std::move(*other.errorPtr()));
            other.destroyActive();
            ::new (&other.m_storage) T(std::move(*valuePtr()));
            other.m_hasValue = true;
            destroyActive();
            ::new (&m_storage) E(std::move(tempError));
            m_hasValue = false;
        }
        else   // !m_hasValue && other.m_hasValue  →  symmetric, delegate
        {
            other.swap(*this);
        }
    }

    /// @brief Chains a fallible continuation on the value track (lvalue).
    /// @note Pedagogical Note: andThen is the "flatMap" / "bind" operator, the cornerstone of
    /// Railway-Oriented Programming with Expected. On the value track, @p f is called with
    /// the stored value and its own Expected result (which must share the same error type E) is
    /// returned. On the error track, @p f is skipped and the error is forwarded unchanged.
    /// This lets you compose an arbitrary number of fallible steps without a single manual check:
    /// @code
    ///   openFile(path).andThen(parseHeader).andThen(buildMesh)
    /// @endcode
    /// @tparam F Callable: (T&) -> Expected<U, E> for some U.
    /// @param[in] f Continuation invoked with the stored value.
    /// @return The result of @p f, or the existing error forwarded unchanged.
    template <typename F>
    requires concepts::IsInvocable<F, T&> GP_NODISCARD auto andThen(F&& f) &
    {
        using ResultType = std::invoke_result_t<F, T&>;
        if (m_hasValue)
        {
            return std::invoke(std::forward<F>(f), *valuePtr());
        }
        return ResultType(Unexpected<E>(*errorPtr()));
    }

    /// @brief Chains a fallible continuation on the value track (const lvalue).
    /// @tparam F Callable: (const T&) -> Expected<U, E> for some U.
    /// @param[in] f Continuation invoked with the stored value.
    /// @return The result of @p f, or the existing error forwarded unchanged.
    template <typename F>
    requires concepts::IsInvocable<F, const T&> GP_NODISCARD auto andThen(F&& f) const&
    {
        using ResultType = std::invoke_result_t<F, const T&>;
        if (m_hasValue)
        {
            return std::invoke(std::forward<F>(f), *valuePtr());
        }
        return ResultType(Unexpected<E>(*errorPtr()));
    }

    /// @brief Chains a fallible continuation on the value track (rvalue).
    /// @tparam F Callable: (T&&) -> Expected<U, E> for some U.
    /// @param[in] f Continuation invoked with the stored value.
    /// @return The result of @p f, or the existing error forwarded unchanged.
    template <typename F>
    requires concepts::IsInvocable<F, T&&> GP_NODISCARD auto andThen(F&& f) &&
    {
        using ResultType = std::invoke_result_t<F, T&&>;
        if (m_hasValue)
        {
            return std::invoke(std::forward<F>(f), std::move(*valuePtr()));
        }
        return ResultType(Unexpected<E>(std::move(*errorPtr())));
    }

    /// @brief Provides a recovery continuation on the error track (lvalue).
    /// @note Pedagogical Note: orElse is the "recovery" complement to andThen. On the error
    /// track, @p f is called with the stored error and may return a new Expected, either
    /// recovering with a fresh value or replacing the error with a different type. On the
    /// value track, the value passes through untouched. Ideal for fallback chains:
    /// @code
    ///   loadFromDisk(path).orElse(loadFromCache).orElse(generateProcedurally)
    /// @endcode
    /// @tparam F Callable: (E&) -> Expected<T, G> for some G.
    /// @param[in] f Recovery function invoked with the stored error.
    /// @return The result of @p f, or the existing value forwarded unchanged.
    template <typename F>
    requires concepts::IsInvocable<F, E&> GP_NODISCARD auto orElse(F&& f) &
    {
        using ResultType = std::invoke_result_t<F, E&>;
        if (!m_hasValue)
        {
            return std::invoke(std::forward<F>(f), *errorPtr());
        }
        return ResultType(*valuePtr());
    }

    /// @brief Provides a recovery continuation on the error track (const lvalue).
    /// @tparam F Callable: (const E&) -> Expected<T, G> for some G.
    /// @param[in] f Recovery function invoked with the stored error.
    /// @return The result of @p f, or the existing value forwarded unchanged.
    template <typename F>
    requires concepts::IsInvocable<F, const E&> GP_NODISCARD auto orElse(F&& f) const&
    {
        using ResultType = std::invoke_result_t<F, const E&>;
        if (!m_hasValue)
        {
            return std::invoke(std::forward<F>(f), *errorPtr());
        }
        return ResultType(*valuePtr());
    }

    /// @brief Provides a recovery continuation on the error track (rvalue).
    /// @tparam F Callable: (E&&) -> Expected<T, G> for some G.
    /// @param[in] f Recovery function invoked with the stored error.
    /// @return The result of @p f, or the existing value forwarded unchanged.
    template <typename F>
    requires concepts::IsInvocable<F, E&&> GP_NODISCARD auto orElse(F&& f) &&
    {
        using ResultType = std::invoke_result_t<F, E&&>;
        if (!m_hasValue)
        {
            return std::invoke(std::forward<F>(f), std::move(*errorPtr()));
        }
        return ResultType(std::move(*valuePtr()));
    }

    /// @brief Applies an infallible transformation to the value track (lvalue).
    /// @note Pedagogical Note: transform is the "map" over the value track. Unlike andThen,
    /// @p f does not return an Expected, the result is automatically wrapped in Expected<U, E>.
    /// On the error track the error propagates unchanged. Use this for pure, non-failing
    /// transformations where you want to reshape the data without changing the error type:
    /// @code
    ///   parseFloat(str).transform([](float f){ return f * 2.0f; })
    /// @endcode
    /// @tparam F Callable: (T&) -> U (any non-Expected return type).
    /// @param[in] f Transformation applied to the stored value.
    /// @return Expected<U, E> holding the transformed value, or the existing error.
    template <typename F>
    requires concepts::IsInvocable<F, T&> GP_NODISCARD auto transform(F&& f) &
    {
        using U = std::invoke_result_t<F, T&>;
        using ResultType = Expected<U, E>;
        if (m_hasValue)
        {
            return ResultType(std::invoke(std::forward<F>(f), *valuePtr()));
        }
        return ResultType(Unexpected<E>(*errorPtr()));
    }

    /// @brief Applies an infallible transformation to the value track (const lvalue).
    /// @tparam F Callable: (const T&) -> U.
    /// @param[in] f Transformation applied to the stored value.
    /// @return Expected<U, E> holding the transformed value, or the existing error.
    template <typename F>
    requires concepts::IsInvocable<F, const T&> GP_NODISCARD auto transform(F&& f) const&
    {
        using U = std::invoke_result_t<F, const T&>;
        using ResultType = Expected<U, E>;
        if (m_hasValue)
        {
            return ResultType(std::invoke(std::forward<F>(f), *valuePtr()));
        }
        return ResultType(Unexpected<E>(*errorPtr()));
    }

    /// @brief Applies an infallible transformation to the value track (rvalue).
    /// @tparam F Callable: (T&&) -> U.
    /// @param[in] f Transformation applied to the stored value.
    /// @return Expected<U, E> holding the transformed value, or the existing error.
    template <typename F>
    requires concepts::IsInvocable<F, T&&> GP_NODISCARD auto transform(F&& f) &&
    {
        using U = std::invoke_result_t<F, T&&>;
        using ResultType = Expected<U, E>;
        if (m_hasValue)
        {
            return ResultType(std::invoke(std::forward<F>(f), std::move(*valuePtr())));
        }
        return ResultType(Unexpected<E>(std::move(*errorPtr())));
    }

    /// @brief Applies an infallible transformation to the error track (lvalue).
    /// @note Pedagogical Note: transformError is the dual of transform, it maps over the
    /// error track while leaving the value type T completely unchanged. This is indispensable
    /// for lifting low-level implementation errors into rich domain errors without breaking
    /// the pipeline:
    /// @code
    ///   readSocket(fd).transformError([](SyscallError e){ return NetworkError{e.errno, peer}; })
    /// @endcode
    /// @tparam F Callable: (E&) -> G (any non-Expected return type).
    /// @param[in] f Transformation applied to the stored error.
    /// @return Expected<T, G> holding the transformed error, or the existing value.
    template <typename F>
    requires concepts::IsInvocable<F, E&> GP_NODISCARD auto transformError(F&& f) &
    {
        using G = std::invoke_result_t<F, E&>;
        using ResultType = Expected<T, G>;
        if (!m_hasValue)
        {
            return ResultType(Unexpected<G>(std::invoke(std::forward<F>(f), *errorPtr())));
        }
        return ResultType(*valuePtr());
    }

    /// @brief Applies an infallible transformation to the error track (const lvalue).
    /// @tparam F Callable: (const E&) -> G.
    /// @param[in] f Transformation applied to the stored error.
    /// @return Expected<T, G> holding the transformed error, or the existing value.
    template <typename F>
    requires concepts::IsInvocable<F, const E&> GP_NODISCARD auto transformError(F&& f) const&
    {
        using G = std::invoke_result_t<F, const E&>;
        using ResultType = Expected<T, G>;
        if (!m_hasValue)
        {
            return ResultType(Unexpected<G>(std::invoke(std::forward<F>(f), *errorPtr())));
        }
        return ResultType(*valuePtr());
    }

    /// @brief Applies an infallible transformation to the error track (rvalue).
    /// @tparam F Callable: (E&&) -> G.
    /// @param[in] f Transformation applied to the stored error.
    /// @return Expected<T, G> holding the transformed error, or the existing value.
    template <typename F>
    requires concepts::IsInvocable<F, E&&> GP_NODISCARD auto transformError(F&& f) &&
    {
        using G = std::invoke_result_t<F, E&&>;
        using ResultType = Expected<T, G>;
        if (!m_hasValue)
        {
            return ResultType(Unexpected<G>(std::invoke(std::forward<F>(f), std::move(*errorPtr()))));
        }
        return ResultType(std::move(*valuePtr()));
    }

private:
    GP_NODISCARD T* valuePtr() noexcept
    {
        return reinterpret_cast<T*>(&m_storage);
    }

    GP_NODISCARD const T* valuePtr() const noexcept
    {
        return reinterpret_cast<const T*>(&m_storage);
    }

    GP_NODISCARD E* errorPtr() noexcept
    {
        return reinterpret_cast<E*>(&m_storage);
    }

    GP_NODISCARD const E* errorPtr() const noexcept
    {
        return reinterpret_cast<const E*>(&m_storage);
    }

    /// @brief Destroys whichever object is currently alive in the shared storage.
    /// @note Skips the destructor call entirely for trivially destructible types,
    /// the compiler sees this as a no-op and eliminates it, which matters in tight
    /// loops that process arrays of Expecteds (e.g. bulk asset load results).
    void destroyActive() noexcept
    {
        if (m_hasValue)
        {
            if constexpr (!concepts::IsTriviallyDestructible<T>)
            {
                valuePtr()->~T();
            }
        }
        else
        {
            if constexpr (!concepts::IsTriviallyDestructible<E>)
            {
                errorPtr()->~E();
            }
        }
    }
};

/// @brief Creates an Expected<T, E> in the success state by forwarding arguments to T's constructor.
/// @note Symmetric with makeOptional and makePair. The error type E must be specified explicitly
/// at the call site since it cannot be deduced from a successful construction:
/// @code
///   auto result = makeExpected<Texture, RenderError>(width, height, format);
/// @endcode
/// @tparam T Value type.
/// @tparam E Error type (must be stated explicitly).
/// @tparam Args Constructor argument types.
/// @param[in] args Arguments forwarded to T's constructor.
/// @return Expected<T, E> in the success state.
template <typename T, typename E, typename... Args>
requires concepts::IsConstructibleWith<T, Args...>
GP_NODISCARD constexpr Expected<T, E> makeExpected(Args&&... args) noexcept(noexcept(T(std::forward<Args>(args)...)))
{
    return Expected<T, E>(T(std::forward<Args>(args)...));
}

}   // namespace gp
