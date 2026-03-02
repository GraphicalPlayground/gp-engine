// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include "Templates/Core/AddReference.hpp"
#include "Templates/Core/RemoveCVRef.hpp"
#include "Templates/Core/RemoveReference.hpp"
#include "Templates/Traits/TypeCategories.hpp"

namespace GP
{

/// @brief Casts an expression to an rvalue reference, enabling move semantics. Equivalent to std::move.
/// @tparam T Deduced type of the argument.
/// @param obj The object to cast.
/// @return An rvalue reference to t.
template <typename T>
GP_FORCEINLINE constexpr TRemoveReference_T<T>&& Move(T&& obj) noexcept
{
    return static_cast<TRemoveReference_T<T>&&>(obj);
}

/// @brief Perfect-forwards an lvalue as either an lvalue or rvalue. Equivalent to std::forward.
/// @tparam T The forwarding type.
/// @param obj The object to forward.
/// @return A perfectly-forwarded reference.
template <typename T>
GP_FORCEINLINE constexpr T&& Forward(TRemoveReference_T<T>& obj) noexcept
{
    return static_cast<T&&>(obj);
}

/// @brief Rvalue overload of Forward, for forwarding rvalues.
/// @tparam T The forwarding type.
/// @param obj The object to forward.
/// @return A perfectly-forwarded reference.
template <typename T>
GP_FORCEINLINE constexpr T&& Forward(TRemoveReference_T<T>&& obj) noexcept
{
    static_assert(!TIsLValueRef_V<T>, "[GP] Cannot forward an rvalue as an lvalue.");
    return static_cast<T&&>(obj);
}

/// @brief Obtains a reference to a type without constructing an object.
///        Only valid in unevaluated contexts (e.g. decltype, sizeof).
/// @tparam T The type to produce a reference for.
/// @return A reference of type TAddRValueRef_T<T>; never actually called.
template <typename T>
TAddRValueRef_T<T> Declval() noexcept;

/// @brief Replaces the value of an object and returns its old value. Equivalent to std::exchange.
/// @tparam T Type of the object.
/// @tparam U Type of the new value (must be assignable to T).
/// @param obj The object whose value to replace.
/// @param newVal The new value to assign.
/// @return The old value of obj.
template <typename T, typename U = T>
constexpr T Exchange(T& obj, U&& newVal) noexcept(noexcept(T(Move(obj))) && noexcept(obj = Forward<U>(newVal)))
{
    T old = Move(obj);
    obj = Forward<U>(newVal);
    return old;
}

/// @brief Obtains a const reference to an object. Equivalent to std::as_const.
/// @tparam T The type of the object.
/// @param obj The object.
/// @return A const lvalue reference to t.
template <typename T>
GP_FORCEINLINE constexpr const T& AsConst(T& obj) noexcept
{
    return obj;
}

/// @brief Deleted rvalue overload — prevents binding a const ref to a temporary.
/// @tparam T The type of the object.
/// @param obj The object (rvalue).
/// @return Deleted function; cannot be called.
template <typename T>
const T& AsConst(const T&&) = delete;

/// @brief Returns the size of a raw C array at compile time.
/// @tparam T The element type.
/// @tparam N The array size.
/// @return N, the number of elements.
template <typename T, SizeT N>
GP_NODISCARD constexpr SizeT ArraySize(const T (&)[N]) noexcept
{
    return N;
}

}   // namespace GP
