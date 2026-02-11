// Copyright (c) - Mallory SCOTTON. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include <type_traits>

/// \brief Internal trait to check if bitwise operations are enabled for an enum
/// By default, all enums have this trait set to false_type.
/// Use GP_ENABLE_ENUM_BITWISE_OPERATIONS macro to enable bitwise operations.
template <typename E>
struct IsBitwiseEnumeration : std::false_type
{};

#define GP_ENABLE_ENUM_BITWISE_OPERATIONS(EnumType) \
    template <> \
    struct IsBitwiseEnumeration<EnumType> : std::true_type \
    {}

/// \brief Bitwise OR operator for enum classes
/// \tparam E Enum type (must be enabled with GP_ENABLE_ENUM_BITWISE_OPERATIONS)
/// \param lhs Left-hand side enum value
/// \param rhs Right-hand side enum value
/// \return Combined enum value with both flags set
template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && IsBitwiseEnumeration<E>::value>>
inline E operator|(E lhs, E rhs)
{
    using Underlying = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<Underlying>(lhs) | static_cast<Underlying>(rhs));
}

/// \brief Bitwise AND operator for enum classes
/// \tparam E Enum type (must be enabled with GP_ENABLE_ENUM_BITWISE_OPERATIONS)
/// \param lhs Left-hand side enum value
/// \param rhs Right-hand side enum value
/// \return Enum value with only common flags set
template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && IsBitwiseEnumeration<E>::value>>
inline E operator&(E lhs, E rhs)
{
    using Underlying = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<Underlying>(lhs) & static_cast<Underlying>(rhs));
}

/// \brief Bitwise XOR operator for enum classes
/// \tparam E Enum type (must be enabled with GP_ENABLE_ENUM_BITWISE_OPERATIONS)
/// \param lhs Left-hand side enum value
/// \param rhs Right-hand side enum value
/// \return Enum value with flags that are in one value but not both
template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && IsBitwiseEnumeration<E>::value>>
inline E operator^(E lhs, E rhs)
{
    using Underlying = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<Underlying>(lhs) ^ static_cast<Underlying>(rhs));
}

/// \brief Bitwise NOT operator for enum classes
/// \tparam E Enum type (must be enabled with GP_ENABLE_ENUM_BITWISE_OPERATIONS)
/// \param lhs Enum value to invert
/// \return Enum value with all bits inverted
template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && IsBitwiseEnumeration<E>::value>>
inline E operator~(E lhs)
{
    using Underlying = std::underlying_type_t<E>;
    return static_cast<E>(~static_cast<Underlying>(lhs));
}

/// \brief Bitwise OR assignment operator for enum classes
/// \tparam E Enum type (must be enabled with GP_ENABLE_ENUM_BITWISE_OPERATIONS)
/// \param lhs Left-hand side enum value (will be modified)
/// \param rhs Right-hand side enum value to OR with
/// \return Reference to the modified lhs
template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && IsBitwiseEnumeration<E>::value>>
inline E& operator|=(E& lhs, E rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

/// \brief Bitwise AND assignment operator for enum classes
/// \tparam E Enum type (must be enabled with GP_ENABLE_ENUM_BITWISE_OPERATIONS)
/// \param lhs Left-hand side enum value (will be modified)
/// \param rhs Right-hand side enum value to AND with
/// \return Reference to the modified lhs
template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && IsBitwiseEnumeration<E>::value>>
inline E& operator&=(E& lhs, E rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

/// \brief Bitwise XOR assignment operator for enum classes
/// \tparam E Enum type (must be enabled with GP_ENABLE_ENUM_BITWISE_OPERATIONS)
/// \param lhs Left-hand side enum value (will be modified)
/// \param rhs Right-hand side enum value to XOR with
/// \return Reference to the modified lhs
template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && IsBitwiseEnumeration<E>::value>>
inline E& operator^=(E& lhs, E rhs)
{
    lhs = lhs ^ rhs;
    return lhs;
}

namespace GP::Enums
{

/// \brief Check if any bit is set in the enum value
/// \tparam E Enum type (must be enabled with
/// GP_ENABLE_ENUM_BITWISE_OPERATIONS)
/// \param val Enum value to check
/// \return true if any bit is set, false otherwise
template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && IsBitwiseEnumeration<E>::value>>
inline bool Any(E val)
{
    using Underlying = std::underlying_type_t<E>;
    return static_cast<Underlying>(val) != 0;
}

/// \brief Check if no bits are set in the enum value
/// \tparam E Enum type (must be enabled with GP_ENABLE_ENUM_BITWISE_OPERATIONS)
/// \param val Enum value to check
/// \return true if no bits are set, false otherwise
template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && IsBitwiseEnumeration<E>::value>>
inline bool None(E val)
{
    return !Any(val);
}

/// \brief Check if all specified flags are set in the enum value
/// \tparam E Enum type (must be enabled with GP_ENABLE_ENUM_BITWISE_OPERATIONS)
/// \param val Enum value to check
/// \param flags Flags to check for
/// \return true if all flags are set, false otherwise
template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && IsBitwiseEnumeration<E>::value>>
inline bool HasAll(E val, E flags)
{
    return (val & flags) == flags;
}

/// \brief Check if any of the specified flags are set in the enum value
/// \tparam E Enum type (must be enabled with GP_ENABLE_ENUM_BITWISE_OPERATIONS)
/// \param val Enum value to check
/// \param flags Flags to check for
/// \return true if at least one flag is set, false otherwise
template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && IsBitwiseEnumeration<E>::value>>
inline bool HasAny(E val, E flags)
{
    return Any(val & flags);
}

///////////////////////////////////////////////////////////////////////////////
/// \brief Check if none of the specified flags are set in the enum value
/// \tparam E Enum type (must be enabled with GP_ENABLE_ENUM_BITWISE_OPERATIONS)
/// \param val Enum value to check
/// \param flags Flags to check for
/// \return true if none of the flags are set, false otherwise
template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && IsBitwiseEnumeration<E>::value>>
inline bool HasNone(E val, E flags)
{
    return !HasAny(val, flags);
}

/// \brief Set specified flags in the enum value
/// \tparam E Enum type (must be enabled with GP_ENABLE_ENUM_BITWISE_OPERATIONS)
/// \param val Enum value to modify
/// \param flags Flags to set
/// \return Modified enum value with flags set
template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && IsBitwiseEnumeration<E>::value>>
inline E SetFlags(E val, E flags)
{
    return val | flags;
}

/// \brief Clear specified flags in the enum value
/// \tparam E Enum type (must be enabled with GP_ENABLE_ENUM_BITWISE_OPERATIONS)
/// \param val Enum value to modify
/// \param flags Flags to clear
/// \return Modified enum value with flags cleared
template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && IsBitwiseEnumeration<E>::value>>
inline E ClearFlags(E val, E flags)
{
    return val & ~flags;
}

/// \brief Toggle specified flags in the enum value
/// \tparam E Enum type (must be enabled with GP_ENABLE_ENUM_BITWISE_OPERATIONS)
/// \param val Enum value to modify
/// \param flags Flags to toggle
/// \return Modified enum value with flags toggled
template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && IsBitwiseEnumeration<E>::value>>
inline E ToggleFlags(E val, E flags)
{
    return val ^ flags;
}

/// \brief Get the underlying integer value of an enum
/// \tparam E Enum type (must be enabled with GP_ENABLE_ENUM_BITWISE_OPERATIONS)
/// \param val Enum value to convert
/// \return The underlying integer value
template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && IsBitwiseEnumeration<E>::value>>
inline std::underlying_type_t<E> ToUnderlying(E val)
{
    return static_cast<std::underlying_type_t<E>>(val);
}

}   // namespace GP::Enums
