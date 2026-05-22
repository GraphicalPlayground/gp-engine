// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/ContainerForward.hpp"   // IWYU pragma: keep
#include "containers/details/BaseKeyFunctions.hpp"
#include "CoreMinimal.hpp"                   // IWYU pragma: keep

namespace gp::container
{

/// @brief
/// @tparam InValueType
/// @tparam InAllowDuplicateKeys
/// @details
template <typename InValueType, bool InAllowDuplicateKeys = false>
struct DefaultKeyFunctions : public BaseKeyFunctions<InValueType, InValueType, InAllowDuplicateKeys>
{
public:
    // TODO: Implement DefaultKeyFunctions
};

}   // namespace gp::container
