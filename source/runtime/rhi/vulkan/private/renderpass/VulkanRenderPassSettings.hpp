// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Reports whether deferred shading dependencies also wait for shader reads
/// (`r.Vulkan.InputAttachmentShaderRead`).
/// @details The specification only needs `INPUT_ATTACHMENT_READ` between the G-buffer and lighting subpasses; some
///          drivers flicker without the extra `SHADER_READ` access.
/// @return True when the console variable is non-zero.
[[nodiscard]] GP_RHI_VULKAN_API bool getVulkanInputAttachmentShaderRead() noexcept;

/// @brief Reports whether render passes are skipped in favor of `VK_KHR_dynamic_rendering`
/// (`r.Vulkan.DynamicRendering`).
/// @details The extension still has to be enabled on the device; the console variable only opts in.
/// @return True when the console variable is non-zero.
[[nodiscard]] GP_RHI_VULKAN_API bool getVulkanAllowDynamicRendering() noexcept;

}   // namespace gp::rhi::vulkan
