// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi::null
{

/// @brief Dynamic RHI module exposing the headless `NullDynamicRHI` backend.
class NullDynamicRHIModule;

/// @brief Headless no-op `DynamicRHI` and `ICommandContext` backend implementation.
class NullDynamicRHI;

/// @brief Concrete no-op GPU buffer allocated by `NullDynamicRHI`.
class NullBuffer;

/// @brief Concrete no-op GPU texture allocated by `NullDynamicRHI`.
class NullTexture;

/// @brief Concrete no-op GPU uniform buffer allocated by `NullDynamicRHI`.
class NullUniformBuffer;

/// @brief Concrete immediately-signaled GPU fence allocated by `NullDynamicRHI`.
class NullGPUFence;

}   // namespace gp::rhi::null
