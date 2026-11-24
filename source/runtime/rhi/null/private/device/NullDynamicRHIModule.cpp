// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/NullDynamicRHIModule.hpp"
#include "device/NullDynamicRHI.hpp"

namespace gp::rhi::null
{

DynamicRHI* NullDynamicRHIModule::createRHI(const FeatureLevel requestedFeatureLevel) noexcept
{
    static_cast<void>(requestedFeatureLevel);
    return new NullDynamicRHI();
}

}   // namespace gp::rhi::null
