// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi::resource
{

/// @brief Runtime type tag identifying concrete `Resource` subclasses without RTTI.
enum class ResourceType : gp::UInt8;

/// @brief CPU mapping access mode when locking a GPU buffer or texture.
enum class ResourceLockMode : gp::UInt8;

/// @brief Usage, lifetime, and binding flags for GPU buffers.
enum class BufferUsageFlags : gp::UInt32;

/// @brief Dimensionality of an RHI texture resource.
enum class TextureDimension : gp::UInt8;

/// @brief Creation, memory placement, and usage flags for GPU textures.
enum class TextureCreateFlags : gp::UInt64;

/// @brief Identifies one of the six faces of a cubemap texture.
enum class CubeFace : gp::UInt32;

/// @brief Status of an asynchronous 2D texture reallocation request.
enum class TextureReallocationStatus : gp::UInt8;

/// @brief Expected update frequency and lifetime of a uniform buffer.
enum class UniformBufferUsage : gp::UInt8;

/// @brief Validation policy applied when constructing or updating a uniform buffer.
enum class UniformBufferValidation : gp::UInt8;

/// @brief Behavior and reflection flags for uniform buffer creation.
enum class UniformBufferFlags : gp::UInt8;

}   // namespace gp::rhi::resource
