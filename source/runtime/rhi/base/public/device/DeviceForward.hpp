// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi
{

/// @brief Defines the RHI feature level tiers supported by the engine.
enum class FeatureLevel : gp::Int32;

/// @brief Graphics backend API implemented by the active RHI.
enum class InterfaceType : gp::UInt8;

/// @brief Runtime availability of an RHI hardware or driver feature.
enum class FeatureSupport : gp::UInt8;

/// @brief Hardware support level for static shader binding layouts.
enum class StaticShaderBindingLayoutSupport : gp::UInt8;

/// @brief PCI vendor IDs for known GPU manufacturers and software rasterizers.
enum class GpuVendorId : gp::UInt32;

}   // namespace gp::rhi
