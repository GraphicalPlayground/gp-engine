// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::container
{

/// @brief Lookup policy when constructing or querying a `gp::Name` in `gp::NamePool` (ADR-0046).
enum class NameFindMode : gp::UInt8
{
    /// @brief Finds an existing entry or inserts a new entry into the global `NamePool` if not yet registered.
    Add = 0u,

    /// @brief Queries the global `NamePool` without inserting; returns `Name()` (`isNone() == true`) if not found.
    Find = 1u
};

}   // namespace gp::container

namespace gp
{

using NameFindMode = container::NameFindMode;

}   // namespace gp
