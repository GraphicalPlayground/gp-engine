// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Smallest anisotropic tap count a sampler can request.
inline constexpr gp::Int32 kMinSamplerAnisotropy = 1;

/// @brief Largest anisotropic tap count a sampler can request, before the device limit is applied.
inline constexpr gp::Int32 kMaxSamplerAnisotropy = 16;

/// @brief Returns the project-wide default anisotropy (`r.MaxAnisotropy`).
/// @return Raw console variable value; it is not clamped.
[[nodiscard]] GP_RHI_VULKAN_API gp::Int32 getVulkanDefaultMaxAnisotropy() noexcept;

/// @brief Resolves the anisotropic tap count of a sampler.
/// @details A positive `requestedAnisotropy` wins; zero or negative falls back to `r.MaxAnisotropy`. The result is
///          clamped to `[kMinSamplerAnisotropy, kMaxSamplerAnisotropy]`. The device limit is applied by the caller.
/// @param[in] requestedAnisotropy `SamplerStateDesc::maxAnisotropy`.
/// @return Tap count in `[1, 16]`.
[[nodiscard]] GP_RHI_VULKAN_API gp::Int32 computeVulkanSamplerAnisotropy(gp::Int32 requestedAnisotropy) noexcept;

}   // namespace gp::rhi::vulkan
