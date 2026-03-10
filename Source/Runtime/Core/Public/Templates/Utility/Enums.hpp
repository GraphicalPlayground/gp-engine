// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Concepts/CoreConcepts.hpp"
#include "Templates/Core/UnderlyingType.hpp"

#define GP_ENABLE_ENUM_BITWISE_OPERATIONS(EnumType) \
    template <> \
    struct TIsBitwiseEnumeration<EnumType> : FTrueType \
    {}

namespace GP
{

/// @brief Type trait to determine if a type is a bitwise enumeration.
/// @tparam T The type to check.
template <typename T>
struct TIsBitwiseEnumeration : FFalseType
{};

/// @brief Variable template for TIsBitwiseEnumeration to simplify usage.
template <typename T>
inline constexpr bool TIsBitwiseEnumeration_V = TIsBitwiseEnumeration<T>::Value;

/// @brief Concept to check if a type is a bitwise enumeration.
template <typename T>
concept CBitwiseEnum = CEnum<T> && TIsBitwiseEnumeration_V<T>;

/// @brief Bitwise OR operator for bitwise enumerations.
/// @return The result of the bitwise OR operation on the underlying values, cast back to the enumeration type.
template <CBitwiseEnum E>
GP_INLINE constexpr E operator|(E lhs, E rhs) noexcept
{
    using Underlying = TUnderlyingType_T<E>;
    return static_cast<E>(static_cast<Underlying>(lhs) | static_cast<Underlying>(rhs));
}

/// @brief Bitwise AND operator for bitwise enumerations.
/// @return The result of the bitwise AND operation on the underlying values, cast back to the enumeration type.
template <CBitwiseEnum E>
GP_INLINE constexpr E operator&(E lhs, E rhs) noexcept
{
    using Underlying = TUnderlyingType_T<E>;
    return static_cast<E>(static_cast<Underlying>(lhs) & static_cast<Underlying>(rhs));
}

/// @brief Bitwise XOR operator for bitwise enumerations.
/// @return The result of the bitwise XOR operation on the underlying values, cast back to the enumeration type.
template <CBitwiseEnum E>
GP_INLINE constexpr E operator^(E lhs, E rhs) noexcept
{
    using Underlying = TUnderlyingType_T<E>;
    return static_cast<E>(static_cast<Underlying>(lhs) ^ static_cast<Underlying>(rhs));
}

/// @brief Bitwise NOT operator for bitwise enumerations.
/// @return The result of the bitwise NOT operation on the underlying value, cast back to the enumeration type.
template <CBitwiseEnum E>
GP_INLINE constexpr E operator~(E value) noexcept
{
    using Underlying = TUnderlyingType_T<E>;
    return static_cast<E>(~static_cast<Underlying>(value));
}

/// @brief Compound bitwise OR assignment operator for bitwise enumerations.
template <CBitwiseEnum E>
GP_INLINE E& operator|=(E& lhs, E rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

/// @brief Compound bitwise AND assignment operator for bitwise enumerations.
template <CBitwiseEnum E>
GP_INLINE E& operator&=(E& lhs, E rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}

/// @brief Compound bitwise XOR assignment operator for bitwise enumerations.
template <CBitwiseEnum E>
GP_INLINE E& operator^=(E& lhs, E rhs) noexcept
{
    lhs = lhs ^ rhs;
    return lhs;
}

/// @brief Utility function to check if any of the specified flags are set in a bitwise enumeration value.
/// @param value The enumeration value to check.
/// @param flags The flags to check for.
/// @return True if any of the specified flags are set in the value, false otherwise.
template <CBitwiseEnum E>
GP_INLINE constexpr bool EnumHasAnyFlags(E value, E flags) noexcept
{
    using Underlying = TUnderlyingType_T<E>;
    return (static_cast<Underlying>(value) & static_cast<Underlying>(flags)) != 0;
}

/// @brief Utility function to check if all of the specified flags are set in a bitwise enumeration value.
/// @param value The enumeration value to check.
/// @param flags The flags to check for.
/// @return True if all of the specified flags are set in the value, false otherwise.
template <CBitwiseEnum E>
GP_INLINE constexpr bool EnumHasAllFlags(E value, E flags) noexcept
{
    using Underlying = TUnderlyingType_T<E>;
    return (static_cast<Underlying>(value) & static_cast<Underlying>(flags)) == static_cast<Underlying>(flags);
}

/// @brief Utility function to check if none of the specified flags are set in a bitwise enumeration value.
/// @param value The enumeration value to check.
/// @param flags The flags to check for.
/// @return True if none of the specified flags are set in the value, false otherwise.
template <CBitwiseEnum E>
GP_INLINE constexpr bool EnumHasNoFlags(E value, E flags) noexcept
{
    using Underlying = TUnderlyingType_T<E>;
    return (static_cast<Underlying>(value) & static_cast<Underlying>(flags)) == 0;
}

/// @brief Utility function to set specific flags in a bitwise enumeration value.
/// @param value The enumeration value to modify.
/// @param flags The flags to set in the value.
template <CBitwiseEnum E>
GP_INLINE void EnumSetFlags(E& value, E flags) noexcept
{
    value |= flags;
}

/// @brief Utility function to clear specific flags in a bitwise enumeration value.
/// @param value The enumeration value to modify.
/// @param flags The flags to clear in the value.
template <CBitwiseEnum E>
GP_INLINE void EnumClearFlags(E& value, E flags) noexcept
{
    value &= ~flags;
}

/// @brief Utility function to toggle specific flags in a bitwise enumeration value.
/// @param value The enumeration value to modify.
/// @param flags The flags to toggle in the value.
template <CBitwiseEnum E>
GP_INLINE void EnumToggleFlags(E& value, E flags) noexcept
{
    value ^= flags;
}

}   // namespace GP
