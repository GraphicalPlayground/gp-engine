// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi::shader
{

/// @brief Identifies a target shader compilation platform.
enum class ShaderPlatform : gp::UInt16;

/// @brief Programmable pipeline stage represented by a shader object.
enum class ShaderFrequency : gp::UInt8;

/// @brief Reflected shader resource binding kind extracted from compiled shader metadata.
enum class ShaderCodeResourceBindingType : gp::UInt8;

/// @brief Member types permitted inside a reflected uniform buffer parameter layout.
enum class UniformBufferBaseType : gp::UInt8;

/// @brief Binding models permitted for a uniform buffer layout.
enum class UniformBufferBindingFlags : gp::UInt8;

/// @brief Execution path used to dispatch shader bundles.
enum class ShaderBundleMethod : gp::UInt8;

}   // namespace gp::rhi::shader
