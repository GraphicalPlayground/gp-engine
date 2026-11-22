// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "logging/LogMacros.hpp"
#include "RHIBase.hpp"

namespace gp
{

/// @brief Built-in log category for `gp::rhi` diagnostics.
GP_DECLARE_EXPORTED_LOG_CATEGORY_EXTERN(GP_RHI_API, LogRHI);

}   // namespace gp

namespace gp::rhi
{

/// @brief Required byte alignment for shader parameter structs.
inline constexpr gp::UInt32 kShaderParameterStructAlignment = 16u;

/// @brief Byte alignment between consecutive elements of array shader parameters.
inline constexpr gp::UInt32 kShaderArrayElementAlignBytes = 16u;

/// @brief Byte alignment required for raw RHI buffer views.
inline constexpr gp::UInt32 kRawViewAlignment = 16u;

/// @brief Minimum byte alignment enforced on pointer fields in uniform buffer structs across C++ and shaders.
inline constexpr gp::UInt32 kShaderParameterPointerAlignment = sizeof(gp::UInt64);
static_assert(
    sizeof(void*) <= kShaderParameterPointerAlignment, "Pointer size exceeds shader parameter pointer alignment."
);

/// @brief Maximum number of mip levels supported by an RHI texture.
inline constexpr gp::UInt32 kMaxTextureMipCount = 15u;

/// @brief Maximum number of LOD levels supported for static and skeletal meshes.
inline constexpr gp::UInt32 kMaxMeshLodCount = 8u;

/// @brief Maximum number of vertex attributes in a single vertex declaration.
inline constexpr gp::UInt32 kMaxVertexElementCount = 16u;

/// @brief Maximum number of color render targets bound simultaneously.
inline constexpr gp::UInt32 kMaxSimultaneousRenderTargets = 8u;

/// @brief Maximum number of global static uniform buffer binding slots.
inline constexpr gp::UInt8 kMaxUniformBufferStaticSlots = 255u;

}   // namespace gp::rhi
