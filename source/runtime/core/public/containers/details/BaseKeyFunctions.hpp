// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/ContainerForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "templates/traits/CallTraits.hpp"

namespace gp::container
{

/// @brief
/// @tparam InValueType
/// @tparam InKeyType
/// @tparam InAllowDuplicateKeys
/// @details
template <typename InValueType, typename InKeyType, bool InAllowDuplicateKeys = false>
struct BaseKeyFunctions
{
public:
    using ValueType = InValueType;
    using ValueInitType = trait::CallTraits<ValueType>::ParamType;
    using KeyType = InKeyType;
    using KeyInitType = trait::CallTraits<KeyType>::ParamType;
    static constexpr bool AllowDuplicateKeys = InAllowDuplicateKeys;
};

}   // namespace gp::container
