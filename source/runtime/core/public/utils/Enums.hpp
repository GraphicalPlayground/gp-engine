// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include <type_traits>

/// @brief Enables bitwise operators for the specified enum type.
/// @param[in] enumType The enum type for which to enable bitwise operators.
/// @attention This macro should be used with caution, as it may lead to unintended consequences if the enum values are
///            not designed for bitwise operations. Make sure that the enum values are unique and meaningful for bitwise
///            operations before enabling this macro.
/// @example
/// @code
/// enum class MyFlags
/// {
///     None = 0,
///     FlagA = 1 << 0,
///     FlagB = 1 << 1,
///     FlagC = 1 << 2
/// };
/// GP_ENABLE_ENUM_BITWISE_OPERATIONS(MyFlags);
///
/// MyFlags flags = MyFlags::FlagA | MyFlags::FlagB; // Bitwise OR
/// @endcode
#define GP_ENABLE_ENUM_BITWISE_OPERATIONS(enumType) \
    template <> \
    struct ::gp::EnableBitwiseOperators<enumType> : std::true_type {}

/// @brief Enables comparison operators for the specified enum type.
/// @param[in] enumType The enum type for which to enable comparison operators.
/// @attention This macro should be used with caution, as it may lead to unintended consequences if the enum values are
///            not designed for comparison operations. Make sure that the enum values are unique and meaningful for
///            comparison before enabling this macro.
/// @example
/// @code
/// enum class MyEnum
/// {
///     Minimum = 0,
///     Medium = 1,
///     Maximum = 2
/// };
/// GP_ENABLE_ENUM_COMPARISON_OPERATIONS(MyEnum);
///
/// MyEnum a = MyEnum::Minimum;
/// MyEnum b = MyEnum::Medium;
/// bool areEqual = (a <= b); // Comparison operators enabled
/// @endcode
#define GP_ENABLE_ENUM_COMPARISON_OPERATIONS(enumType) \
    template <> \
    struct ::gp::EnableComparisonOperators<enumType> : std::true_type {}

namespace gp
{

/// @brief General template specialization to enable bitwise operators for enum types.
/// @note By default, bitwise operators are disabled for all enum types. To enable bitwise operators for a specific enum
///       type, specialize this template for that type and set it to `std::true_type` or use the
///       `GP_ENABLE_ENUM_BITWISE_OPERATIONS` macro.
template <typename T>
struct EnableBitwiseOperators : std::false_type
{};

/// @brief Variable template to check if bitwise operators are enabled for a given type.
template <typename T>
inline constexpr bool HasBitwiseOperatorsV = EnableBitwiseOperators<T>::value;

/// @brief Concept to check if a type is an enum and has bitwise operators enabled.
template <typename T>
concept BitwiseEnum = std::is_enum_v<T> && HasBitwiseOperatorsV<T>;

/// @brief General template specialization to enable comparison operators for enum types.
/// @note By default, comparison operators are disabled for all enum types. To enable comparison operators for a
///       specific enum type, specialize this template for that type and set it to `std::true_type` or use the
///       `GP_ENABLE_ENUM_COMPARISON_OPERATIONS` macro.
template <typename T>
struct EnableComparisonOperators : std::false_type
{};

/// @brief Variable template to check if comparison operators are enabled for a given type.
template <typename T>
inline constexpr bool HasComparisonOperatorsV = EnableComparisonOperators<T>::value;

/// @brief Concept to check if a type is an enum and has comparison operators enabled.
template <typename T>
concept ComparableEnum = std::is_enum_v<T> && HasComparisonOperatorsV<T>;

namespace enums
{

/// @brief Checks if any of the specified flags are set in the given enum value.
/// @tparam E The enum type, which must satisfy the `BitwiseEnum` concept.
/// @param[in] value The enum value to check for the specified flags.
template <gp::BitwiseEnum E>
GP_INLINE constexpr bool hasAnyFlags(E value) noexcept
{
    using Underlying = std::underlying_type_t<E>;
    return static_cast<Underlying>(value) != 0;
}

/// @brief Checks if all of the specified flags are set in the given enum value.
/// @tparam E The enum type, which must satisfy the `BitwiseEnum` concept.
/// @param[in] value The enum value to check for the specified flags.
/// @param[in] flags The flags to check for in the enum value.
template <gp::BitwiseEnum E>
GP_INLINE constexpr bool hasAllFlags(E value, E flags) noexcept
{
    using Underlying = std::underlying_type_t<E>;
    return (static_cast<Underlying>(value) & static_cast<Underlying>(flags)) == static_cast<Underlying>(flags);
}

/// @brief Checks if none of the specified flags are set in the given enum value.
/// @tparam E The enum type, which must satisfy the `BitwiseEnum` concept.
/// @param[in] value The enum value to check for the specified flags.
/// @param[in] flags The flags to check for in the enum value.
template <gp::BitwiseEnum E>
GP_INLINE constexpr bool hasNoFlags(E value, E flags) noexcept
{
    using Underlying = std::underlying_type_t<E>;
    return (static_cast<Underlying>(value) & static_cast<Underlying>(flags)) == 0;
}

/// @brief Sets the specified flags in the given enum value.
/// @tparam E The enum type, which must satisfy the `BitwiseEnum` concept.
/// @param[in] value The enum value to set the flags in.
/// @param[in] flags The flags to set in the enum value.
template <gp::BitwiseEnum E>
GP_INLINE constexpr E setFlags(E value, E flags) noexcept
{
    return value | flags;
}

/// @brief Clears the specified flags in the given enum value.
/// @tparam E The enum type, which must satisfy the `BitwiseEnum` concept.
/// @param[in] value The enum value to clear the flags in.
/// @param[in] flags The flags to clear in the enum value.
template <gp::BitwiseEnum E>
GP_INLINE constexpr E clearFlags(E value, E flags) noexcept
{
    return value & ~flags;
}

/// @brief Toggles the specified flags in the given enum value.
/// @tparam E The enum type, which must satisfy the `BitwiseEnum` concept.
/// @param[in] value The enum value to toggle the flags in.
/// @param[in] flags The flags to toggle in the enum value.
template <gp::BitwiseEnum E>
GP_INLINE constexpr E toggleFlags(E value, E flags) noexcept
{
    return value ^ flags;
}

}   // namespace enums

}   // namespace gp

/// @brief Bitwise OR operator for enum types that satisfy the `BitwiseEnum` concept.
template <gp::BitwiseEnum E>
GP_INLINE constexpr E operator|(E lhs, E rhs) noexcept
{
    using Underlying = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<Underlying>(lhs) | static_cast<Underlying>(rhs));
}

/// @brief Bitwise AND operator for enum types that satisfy the `BitwiseEnum` concept.
template <gp::BitwiseEnum E>
GP_INLINE constexpr E operator&(E lhs, E rhs) noexcept
{
    using Underlying = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<Underlying>(lhs) & static_cast<Underlying>(rhs));
}

/// @brief Bitwise XOR operator for enum types that satisfy the `BitwiseEnum` concept.
template <gp::BitwiseEnum E>
GP_INLINE constexpr E operator^(E lhs, E rhs) noexcept
{
    using Underlying = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<Underlying>(lhs) ^ static_cast<Underlying>(rhs));
}

/// @brief Bitwise NOT operator for enum types that satisfy the `BitwiseEnum` concept.
template <gp::BitwiseEnum E>
GP_INLINE constexpr E operator~(E value) noexcept
{
    using Underlying = std::underlying_type_t<E>;
    return static_cast<E>(~static_cast<Underlying>(value));
}

/// @brief Bitwise OR assignment operator for enum types that satisfy the `BitwiseEnum` concept.
template <gp::BitwiseEnum E>
GP_INLINE constexpr E& operator|=(E& lhs, E rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

/// @brief Bitwise AND assignment operator for enum types that satisfy the `BitwiseEnum` concept.
template <gp::BitwiseEnum E>
GP_INLINE constexpr E& operator&=(E& lhs, E rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}

/// @brief Bitwise XOR assignment operator for enum types that satisfy the `BitwiseEnum` concept.
template <gp::BitwiseEnum E>
GP_INLINE constexpr E& operator^=(E& lhs, E rhs) noexcept
{
    lhs = lhs ^ rhs;
    return lhs;
}

/// @brief Equality operator for enum types that satisfy the `ComparableEnum` concept.
template <gp::ComparableEnum E>
GP_INLINE constexpr bool operator==(E lhs, E rhs) noexcept
{
    using Underlying = std::underlying_type_t<E>;
    return static_cast<Underlying>(lhs) == static_cast<Underlying>(rhs);
}

/// @brief Inequality operator for enum types that satisfy the `ComparableEnum` concept.
template <gp::ComparableEnum E>
GP_INLINE constexpr bool operator!=(E lhs, E rhs) noexcept
{
    using Underlying = std::underlying_type_t<E>;
    return static_cast<Underlying>(lhs) != static_cast<Underlying>(rhs);
}

/// @brief Less-than operator for enum types that satisfy the `ComparableEnum` concept.
template <gp::ComparableEnum E>
GP_INLINE constexpr bool operator<(E lhs, E rhs) noexcept
{
    using Underlying = std::underlying_type_t<E>;
    return static_cast<Underlying>(lhs) < static_cast<Underlying>(rhs);
}

/// @brief Less-than or equal operator for enum types that satisfy the `ComparableEnum` concept.
template <gp::ComparableEnum E>
GP_INLINE constexpr bool operator<=(E lhs, E rhs) noexcept
{
    using Underlying = std::underlying_type_t<E>;
    return static_cast<Underlying>(lhs) <= static_cast<Underlying>(rhs);
}

/// @brief Greater-than operator for enum types that satisfy the `ComparableEnum` concept.
template <gp::ComparableEnum E>
GP_INLINE constexpr bool operator>(E lhs, E rhs) noexcept
{
    using Underlying = std::underlying_type_t<E>;
    return static_cast<Underlying>(lhs) > static_cast<Underlying>(rhs);
}

/// @brief Greater-than or equal operator for enum types that satisfy the `ComparableEnum` concept.
template <gp::ComparableEnum E>
GP_INLINE constexpr bool operator>=(E lhs, E rhs) noexcept
{
    using Underlying = std::underlying_type_t<E>;
    return static_cast<Underlying>(lhs) >= static_cast<Underlying>(rhs);
}
