// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreTypes.hpp"
#include "RHIBuild.hpp"

namespace GP
{

/// @brief Maximum number of simultaneously bound descriptor set layouts per pipeline layout.
constexpr UInt32 kMaxDescriptorSetLayouts = 8u;

/// @brief Maximum number of push-constant bytes per pipeline layout.
constexpr UInt32 kMaxPushConstantSize = 128u;

/// @brief Maximum number of color attachments per render pass.
constexpr UInt32 kMaxColorAttachments = 8u;

/// @brief Maximum number of vertex input bindings.
constexpr UInt32 kMaxVertexBindings = 16u;

/// @brief Maximum number of vertex input attributes.
constexpr UInt32 kMaxVertexAttributes = 32u;

/// @brief Maximum number of viewports and scissors.
constexpr UInt32 kMaxViewports = 16u;

/// @brief Maximum number of frames that may be in flight simultaneously.
constexpr UInt32 kMaxFramesInFlight = 3u;

/// @brief Maximum number of mip levels for any texture.
constexpr UInt32 kMaxMipLevels = 16u;

/// @brief Maximum number of texture array layers.
constexpr UInt32 kMaxArrayLayers = 2048u;

/// @brief Maximum number of subpasses per render pass (legacy render-pass API path).
constexpr UInt32 kMaxSubpasses = 8u;

}   // namespace GP
