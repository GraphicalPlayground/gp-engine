// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::container
{

/// @brief Controls whether string comparisons, searches, and hashes distinguish between uppercase and lowercase ASCII.
/// @details Modeled after Unreal Engine's `ESearchCase::Type`. `CaseSensitive` compares exact character code units,
///          whereas `IgnoreCase` folds ASCII `'A'`..`'Z'` to `'a'`..`'z'` without consulting OS locales.
enum class SearchCase : gp::UInt8
{
    /// @brief Exact code-unit comparison (`'A' != 'a'`).
    CaseSensitive = 0u,

    /// @brief Locale-independent ASCII case-insensitive comparison (`'A' == 'a'`).
    IgnoreCase = 1u
};

/// @brief Controls the scan direction of string search and replacement operations.
/// @details Modeled after Unreal Engine's `ESearchDir::Type`.
enum class SearchDir : gp::UInt8
{
    /// @brief Scans forward from the beginning of the string toward the end.
    FromStart = 0u,

    /// @brief Scans backward from the end of the string toward the beginning.
    FromEnd = 1u
};

/// @brief Bitmask flags controlling token emission during zero-allocation string splitting (`splitView`, `splitLines`).
enum class StringSplitOptions : gp::UInt8
{
    /// @brief Emits all tokens unchanged, including empty tokens between adjacent delimiters.
    None = 0x00u,

    /// @brief Omits empty tokens (after optional whitespace trimming).
    SkipEmpty = 0x01u,

    /// @brief Alias for `SkipEmpty` (Unreal Engine `ParseIntoArray` terminology).
    CullEmpty = SkipEmpty,

    /// @brief Trims leading and trailing ASCII whitespace from each token before emission.
    TrimTokens = 0x02u,

    /// @brief Convenience combination of `TrimTokens` and `SkipEmpty`.
    SkipEmptyAndTrim = SkipEmpty | TrimTokens,

    /// @brief Alias for `SkipEmptyAndTrim`.
    CullEmptyAndTrim = SkipEmptyAndTrim
};

}   // namespace gp::container

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::container::StringSplitOptions);

namespace gp
{

/// @brief Convenience alias for `gp::container::SearchCase` in the root `gp` namespace.
using SearchCase = container::SearchCase;

/// @brief Convenience alias for `gp::container::SearchDir` in the root `gp` namespace.
using SearchDir = container::SearchDir;

/// @brief Convenience alias for `gp::container::StringSplitOptions` in the root `gp` namespace.
using StringSplitOptions = container::StringSplitOptions;

}   // namespace gp
