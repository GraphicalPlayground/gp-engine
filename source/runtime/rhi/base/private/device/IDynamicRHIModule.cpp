// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/IDynamicRHIModule.hpp"

namespace gp::rhi
{

bool IDynamicRHIModule::isSupported(const FeatureLevel requestedFeatureLevel) const noexcept
{
    static_cast<void>(requestedFeatureLevel);
    return isSupported();
}

}   // namespace gp::rhi
