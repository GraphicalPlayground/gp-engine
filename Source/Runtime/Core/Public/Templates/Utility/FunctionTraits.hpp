// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreTypes.hpp"
#include "Templates/Core/IntegerSequence.hpp"
#include "Templates/Core/TemplateTypes.hpp"
#include "Templates/Utility/TypeList.hpp"

namespace GP
{

// clang-format off

namespace Detail
{

/// @brief Primary template, not a function type.
template <typename T>
struct TFunctionTraitsImpl;

/// @brief Extracts function traits from a free or static member function type.
#define GP_FUNC_TRAITS_IMPL(...)                                                                    \
    template <typename TRet, typename... TArgs>                                                     \
    struct TFunctionTraitsImpl<TRet(TArgs...) __VA_ARGS__>                                          \
    {                                                                                               \
        using ReturnType                 = TRet;                                                    \
        using ArgTypes                   = TTypeList<TArgs...>;                                     \
        using ClassType                  = void;                                                    \
        static constexpr SizeT Arity     = sizeof...(TArgs);                                        \
        static constexpr bool IsNoexcept = false;                                                   \
        static constexpr bool IsConst    = false;                                                   \
        static constexpr bool IsVariadic = false;                                                   \
        template <SizeT I>                                                                          \
        using ArgType = TTypeListGet_T<I, ArgTypes>;                                                \
    };

GP_FUNC_TRAITS_IMPL(/* plain */)
GP_FUNC_TRAITS_IMPL(const)
GP_FUNC_TRAITS_IMPL(&)
GP_FUNC_TRAITS_IMPL(const &)
GP_FUNC_TRAITS_IMPL(&&)
GP_FUNC_TRAITS_IMPL(const &&)

#undef GP_FUNC_TRAITS_IMPL

/// @brief Extracts function traits from a free or static member function type with noexcept specifier.
#define GP_FUNC_TRAITS_NX(...)                                                                      \
    template <typename TRet, typename... TArgs>                                                     \
    struct TFunctionTraitsImpl<TRet(TArgs...) __VA_ARGS__ noexcept>                                 \
    {                                                                                               \
        using ReturnType                  = TRet;                                                   \
        using ArgTypes                    = TTypeList<TArgs...>;                                    \
        using ClassType                   = void;                                                   \
        static constexpr SizeT Arity      = sizeof...(TArgs);                                       \
        static constexpr bool IsNoexcept  = true;                                                   \
        static constexpr bool IsConst     = false;                                                  \
        static constexpr bool IsVariadic  = false;                                                  \
        template <SizeT I>                                                                          \
        using ArgType = TTypeListGet_T<I, ArgTypes>;                                                \
    };

GP_FUNC_TRAITS_NX(/* plain */)
GP_FUNC_TRAITS_NX(const)
GP_FUNC_TRAITS_NX(&)
GP_FUNC_TRAITS_NX(const &)
GP_FUNC_TRAITS_NX(&&)
GP_FUNC_TRAITS_NX(const &&)

#undef GP_FUNC_TRAITS_NX

/// @brief Extracts function traits from a free or static member function type.
#define GP_MFP_TRAITS(QUALIFIERS, IS_CONST)                                                         \
    template <typename TClass, typename TRet, typename... TArgs>                                    \
    struct TFunctionTraitsImpl<TRet (TClass::*)(TArgs...) QUALIFIERS>                               \
    {                                                                                               \
        using ReturnType                 = TRet;                                                    \
        using ArgTypes                   = TTypeList<TArgs...>;                                     \
        using ClassType                  = TClass;                                                  \
        static constexpr SizeT Arity     = sizeof...(TArgs);                                        \
        static constexpr bool IsNoexcept = false;                                                   \
        static constexpr bool IsConst    = IS_CONST;                                                \
        static constexpr bool IsVariadic = false;                                                   \
        template <SizeT I>                                                                          \
        using ArgType = TTypeListGet_T<I, ArgTypes>;                                                \
    };                                                                                              \
    template <typename TClass, typename TRet, typename... TArgs>                                    \
    struct TFunctionTraitsImpl<TRet (TClass::*)(TArgs...) QUALIFIERS noexcept>                      \
    {                                                                                               \
        using ReturnType                 = TRet;                                                    \
        using ArgTypes                   = TTypeList<TArgs...>;                                     \
        using ClassType                  = TClass;                                                  \
        static constexpr SizeT Arity     = sizeof...(TArgs);                                        \
        static constexpr bool IsNoexcept = true;                                                    \
        static constexpr bool IsConst    = IS_CONST;                                                \
        static constexpr bool IsVariadic = false;                                                   \
        template <SizeT I>                                                                          \
        using ArgType = TTypeListGet_T<I, ArgTypes>;                                                \
    };

GP_MFP_TRAITS(/* plain */, false)
GP_MFP_TRAITS(const,       true)
GP_MFP_TRAITS(&,           false)
GP_MFP_TRAITS(const &,     true)
GP_MFP_TRAITS(&&,          false)
GP_MFP_TRAITS(const &&,    true)

#undef GP_MFP_TRAITS

/// @brief Extracts function traits from a callable type (e.g., lambda, functor).
template <typename T>
struct TStripCallable : TFunctionTraitsImpl<decltype(&T::operator())> {};

}   // namespace Detail

/// @brief Decomposes any callable type into its constituent return and argument types.
///
///   TFunctionTraits<TFn>::ReturnType        — Return type.
///   TFunctionTraits<TFn>::ArgTypes          — TTypeList of argument types.
///   TFunctionTraits<TFn>::ArgType<I>        — I-th argument type.
///   TFunctionTraits<TFn>::ClassType         — Owning class (void for free functions).
///   TFunctionTraits<TFn>::Arity             — Number of parameters.
///   TFunctionTraits<TFn>::bIsNoexcept       — Whether declared noexcept.
///   TFunctionTraits<TFn>::bIsConst          — Whether the call is const-qualified.
///
/// @tparam TFn   Any callable: free function, function pointer, member function pointer, functor, or lambda.
template <typename TFn>
struct TFunctionTraits : Detail::TFunctionTraitsImpl<TFn> {};

/// @brief Specialisation for function pointers
template <typename TFn>
struct TFunctionTraits<TFn*> : TFunctionTraits<TFn> {};

/// @brief Specialisation for functors/lambdas
template <typename TFn> requires requires { &TFn::operator(); }
struct TFunctionTraits<TFn> : Detail::TStripCallable<TFn> {};

// clang-format on

}   // namespace GP
