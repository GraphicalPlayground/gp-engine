// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include <concepts>
#include <type_traits>

namespace GP::Concepts
{

/// \brief Concept to check if a type is the same as another type.
template <typename T, typename U>
concept IsSameAs = std::is_same_v<T, U>;

/// \brief Concept to check if a type is derived from another type.
template <typename Derived, typename Base>
concept IsDerivedFrom = std::is_base_of_v<Base, Derived>;

/// \brief Concept to check if a type is convertible to another type.
template <typename From, typename To>
concept IsConvertibleTo = std::is_convertible_v<From, To>;

/// \brief Concept to check if a type is implicitly convertible to another type.
template <typename From, typename To>
concept IsImplicitlyConvertibleTo = std::is_convertible_v<From, To>;

/// \brief Concept to check if a type is arithmetic (integral or floating point).
template <typename T>
concept IsArithmetic = std::is_arithmetic_v<T>;

/// \brief Concept to check if a type is a floating point type.
template <typename T>
concept IsFloatingPoint = std::is_floating_point_v<T>;

/// \brief Concept to check if a type is an integral type.
template <typename T>
concept IsIntegral = std::is_integral_v<T>;

/// \brief Concept to check if a type is a signed type.
template <typename T>
concept IsSigned = std::is_signed_v<T>;

/// \brief Concept to check if a type is an unsigned type.
template <typename T>
concept IsUnsigned = std::is_unsigned_v<T>;

/// \brief Concept to check if a type is a pointer type.
template <typename T>
concept IsPointer = std::is_pointer_v<T>;

/// \brief Concept to check if a type is a class type.
template <typename T>
concept IsClass = std::is_class_v<T>;

/// \brief Concept to check if a type is destructible.
template <typename T>
concept IsDestructible = std::is_destructible_v<T>;

/// \brief Concept to check if a type is constructible with given arguments.
template <typename T, typename... Args>
concept IsConstructible = std::is_constructible_v<T, Args...>;

/// \brief Concept to check if a type is default constructible.
template <typename T>
concept IsDefaultConstructible = std::is_default_constructible_v<T>;

/// \brief Concept to check if a type is move constructible.
template <typename T>
concept IsMoveConstructible = std::is_move_constructible_v<T>;

/// \brief Concept to check if a type is copy constructible.
template <typename T>
concept IsCopyConstructible = std::is_copy_constructible_v<T>;

/// \brief Concept to check if a type is move assignable.
template <typename T>
concept IsMoveAssignable = std::is_move_assignable_v<T>;

/// \brief Concept to check if a type is copy assignable.
template <typename T>
concept IsCopyAssignable = std::is_copy_assignable_v<T>;

/// \brief Concept to check if a type is swappable.
template <typename T>
concept IsSwappable = std::is_swappable_v<T>;

/// \brief Concept to check if a type is an enum type.
template <typename T>
concept IsEnum = std::is_enum_v<T>;

/// \brief Concept to check if a type is a union type.
template <typename T>
concept IsUnion = std::is_union_v<T>;

/// \brief Concept to check if a type is a function type.
template <typename T>
concept IsFunction = std::is_function_v<T>;

/// \brief Concept to check if a type is a reference type.
template <typename T>
concept IsReference = std::is_reference_v<T>;

/// \brief Concept to check if a type is an lvalue reference type.
template <typename T>
concept IsLValueReference = std::is_lvalue_reference_v<T>;

/// \brief Concept to check if a type is an rvalue reference type.
template <typename T>
concept IsRValueReference = std::is_rvalue_reference_v<T>;

/// \brief Concept to check if a type is const-qualified.
template <typename T>
concept IsConst = std::is_const_v<T>;

/// \brief Concept to check if a type is volatile-qualified.
template <typename T>
concept IsVolatile = std::is_volatile_v<T>;

/// \brief Concept to check if a type is trivially copyable.
template <typename T>
concept IsTriviallyCopyable = std::is_trivially_copyable_v<T>;

/// \brief Concept to check if a type is trivially constructible.
template <typename T, typename... Args>
concept IsTriviallyConstructible = std::is_trivially_constructible_v<T, Args...>;

/// \brief Concept to check if a type is trivially default constructible.
template <typename T>
concept IsTriviallyDefaultConstructible = std::is_trivially_default_constructible_v<T>;

/// \brief Concept to check if a type is trivially destructible.
template <typename T>
concept IsTriviallyDestructible = std::is_trivially_destructible_v<T>;

/// \brief Concept to check if a type is polymorphic.
template <typename T>
concept IsPolymorphic = std::is_polymorphic_v<T>;

/// \brief Concept to check if a type is abstract.
template <typename T>
concept IsAbstract = std::is_abstract_v<T>;

/// \brief Concept to check if a type is final.
template <typename T>
concept IsFinal = std::is_final_v<T>;

/// \brief Concept to check if a type is empty.
template <typename T>
concept IsEmpty = std::is_empty_v<T>;

/// \brief Concept to check if a type is trivial.
template <typename T>
concept IsTrivial = std::is_trivial_v<T>;

/// \brief Concept to check if a type is standard layout.
template <typename T>
concept IsStandardLayout = std::is_standard_layout_v<T>;

/// \brief Concept to check if a type is a valid character type (char, wchar_t, char16_t, char32_t, ...).
template <typename T>
concept IsCharacter = std::same_as<std::remove_cv_t<T>, char> || std::same_as<std::remove_cv_t<T>, char8_t> ||
                      std::same_as<std::remove_cv_t<T>, char16_t> || std::same_as<std::remove_cv_t<T>, char32_t> ||
                      std::same_as<std::remove_cv_t<T>, wchar_t>;

}   // namespace GP::Concepts
