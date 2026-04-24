// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::rhi
{

/// @brief The type of the graphics interface.
enum class InterfaceType
{
    Hidden,
    Null,
    D3D11,
    D3D12,
    Vulkan,
    Metal,
    OpenGL
};

/// @brief The feature support level of the graphics interface.
enum class FeatureSupport : gp::UInt8
{
    Unsupported,        //<! The RHI feature is completely unavailable at runtime.
    RuntimeDependent,   //<! The RHI feature can be available at runtime based on hardware or driver.
    RuntimeGuaranteed   //<! The RHI feature is guaranteed to be available at runtime.
};

}   // namespace gp::rhi
