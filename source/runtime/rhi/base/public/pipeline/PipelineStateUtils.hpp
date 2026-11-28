// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi::pipeline
{

/// @brief Compares two RHI state resources by querying their creation descriptors via `getInitializer()`.
/// @details Passing `nullptr` for either state compares against a default-constructed `InDescType`.
/// @tparam InStateType Concrete state resource type providing `getInitializer(InDescType&)`.
/// @tparam InDescType Creation descriptor type supporting default construction and `operator==`.
/// @param[in] lhsState First state resource pointer (may be `nullptr`).
/// @param[in] rhsState Second state resource pointer (may be `nullptr`).
/// @return True when the retrieved descriptors compare equal.
template <typename InStateType, typename InDescType>
[[nodiscard]] inline bool
    matchPipelineState(const InStateType* const lhsState, const InStateType* const rhsState) noexcept
{
    InDescType lhsDesc{};
    InDescType rhsDesc{};

    if (lhsState != nullptr)
    {
        static_cast<void>(lhsState->getInitializer(lhsDesc));
    }
    if (rhsState != nullptr)
    {
        static_cast<void>(rhsState->getInitializer(rhsDesc));
    }

    return lhsDesc == rhsDesc;
}

}   // namespace gp::rhi::pipeline
