// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Core/AddCV.hpp"
#include "Templates/Core/Conditional.hpp"
#include "Templates/Core/RemoveCV.hpp"
#include "Templates/Core/TemplateTypes.hpp"

namespace GP
{

namespace Detail
{

// clang-format off

template <typename T> struct TMakeSignedImpl;
template <> struct TMakeSignedImpl<char>               { using Type = signed char; };
template <> struct TMakeSignedImpl<signed char>        { using Type = signed char; };
template <> struct TMakeSignedImpl<unsigned char>      { using Type = signed char; };
template <> struct TMakeSignedImpl<short>              { using Type = short;       };
template <> struct TMakeSignedImpl<unsigned short>     { using Type = short;       };
template <> struct TMakeSignedImpl<int>                { using Type = int;         };
template <> struct TMakeSignedImpl<unsigned int>       { using Type = int;         };
template <> struct TMakeSignedImpl<long>               { using Type = long;        };
template <> struct TMakeSignedImpl<unsigned long>      { using Type = long;        };
template <> struct TMakeSignedImpl<long long>          { using Type = long long;   };
template <> struct TMakeSignedImpl<unsigned long long> { using Type = long long;   };

template <typename T> struct TMakeUnsignedImpl;
template <> struct TMakeUnsignedImpl<char>               { using Type = unsigned char;      };
template <> struct TMakeUnsignedImpl<signed char>        { using Type = unsigned char;      };
template <> struct TMakeUnsignedImpl<unsigned char>      { using Type = unsigned char;      };
template <> struct TMakeUnsignedImpl<short>              { using Type = unsigned short;     };
template <> struct TMakeUnsignedImpl<unsigned short>     { using Type = unsigned short;     };
template <> struct TMakeUnsignedImpl<int>                { using Type = unsigned int;       };
template <> struct TMakeUnsignedImpl<unsigned int>       { using Type = unsigned int;       };
template <> struct TMakeUnsignedImpl<long>               { using Type = unsigned long;      };
template <> struct TMakeUnsignedImpl<unsigned long>      { using Type = unsigned long;      };
template <> struct TMakeUnsignedImpl<long long>          { using Type = unsigned long long; };
template <> struct TMakeUnsignedImpl<unsigned long long> { using Type = unsigned long long; };

// clang-format on

}   // namespace Detail

// clang-format off

/// @brief Transforms an integral type to its signed counterpart, preserving cv-qualifiers.
/// @tparam T An integral type (excluding bool).
template <typename T>
struct TMakeSigned
{
private:
    using TBase = TRemoveCV_T<T>;
    using TSigned = typename Detail::TMakeSignedImpl<TBase>::Type;
public:
    // Reapply const/volatile that were on T.
    using Type = TConditional_T<TIsConst_V<T>,
                    TConditional_T<TIsVolatile_V<T>, const volatile TSigned, const TSigned>,
                    TConditional_T<TIsVolatile_V<T>, volatile TSigned, TSigned>>;
};

/// @brief Helper alias to obtain the signed counterpart of an integral type.
template <typename T>
using TMakeSigned_T = typename TMakeSigned<T>::Type;

/// @brief Transforms an integral type to its unsigned counterpart, preserving cv-qualifiers.
/// @tparam T An integral type (excluding bool).
template <typename T>
struct TMakeUnsigned
{
private:
    using TBase     = TRemoveCV_T<T>;
    using TUnsigned = typename Detail::TMakeUnsignedImpl<TBase>::Type;
public:
    using Type = TConditional_T<TIsConst_V<T>,
                    TConditional_T<TIsVolatile_V<T>, const volatile TUnsigned, const TUnsigned>,
                    TConditional_T<TIsVolatile_V<T>, volatile TUnsigned, TUnsigned>>;
};

/// @brief Helper alias to obtain the unsigned counterpart of an integral type.
template <typename T>
using TMakeUnsigned_T = typename TMakeUnsigned<T>::Type;

// clang-format on

}   // namespace GP
