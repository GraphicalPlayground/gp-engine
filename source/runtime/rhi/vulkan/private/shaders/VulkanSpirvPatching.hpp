// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/ContainerForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Rewrites a SPIR-V module so its subpass inputs read multisampled attachments.
/// @details Shaders are compiled once against single-sampled subpass inputs. When the pipeline rasterizes with more
///          than one sample, the attachments become multisampled and a `subpassInput` must become a
///          `subpassInputMS`. The patch performs a single linear pass over the instruction stream:
///          - Every `OpTypeImage` with `Dim == SubpassData` gets its `MS` operand set to 1.
///          - `OpLoad` results of those images are tracked.
///          - Every 5-word `OpImageRead` of a tracked image gains a `Sample` image operand (0x40) whose value is the
///            module's existing signed `int 0` constant, growing the instruction from 5 to 7 words.
///          No new result IDs are introduced, so the module header (including its ID bound) stays valid.
/// @param[in] spirv Source module.
/// @param[out] outPatched Receives the patched module; unspecified on failure.
/// @return False when the module is truncated, contains a zero-length instruction, or reads a subpass input without
///         the signed `int 0` constant the rewrite needs.
GP_RHI_VULKAN_API bool
    patchSpirvInputAttachments(gp::ConstVectorView<gp::UInt32> spirv, gp::Vector<gp::UInt32>& outPatched) noexcept;

}   // namespace gp::rhi::vulkan
