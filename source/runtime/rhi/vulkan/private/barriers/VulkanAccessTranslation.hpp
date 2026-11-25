// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "barriers/VulkanBarrierEnums.hpp"
#include "barriers/VulkanBarrierForward.hpp"   // IWYU pragma: keep
#include "barriers/VulkanBarrierTypes.hpp"
#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "sync/SyncEnums.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Confirms that a texture tagged as a shading rate source can be consumed by the device.
/// @note Fails a check when the device reports `VulkanShadingRateImageType::NotSupported`.
/// @param[in] type Device shading rate image encoding.
/// @return True for `Palette` and `Fractional`; false otherwise.
[[nodiscard]] GP_RHI_VULKAN_API bool validateShadingRateImageType(VulkanShadingRateImageType type) noexcept;

/// @brief Translates a `sync::ResourceAccess` value into the stage mask, access mask and image layout to use.
/// @details States that cannot be combined (`Discard`, `Unknown`, `CPURead`, `Present`, `RTV`, `ResolveDst`,
///          `CopyDest`, `BVHRead`, `BVHWrite`) return straight away. `DSVWrite` overrides every other flag because it
///          decides the layout. The remaining read flags are accumulated; a texture mixing `CopySrc` with any other
///          read falls back to `VK_IMAGE_LAYOUT_GENERAL`.
/// @note `RTV | ResolveDst` is treated as `RTV` (a render pass resolve attachment) and `BVHRead` drops any SRV bits.
/// @param[in] access Access state to translate.
/// @param[in] resourceType Underlying resource kind (`Texture` or `Buffer` once UAVs are unwrapped).
/// @param[in] usageFlags `VkBufferUsageFlags` of the buffer, zero for textures.
/// @param[in] isDepthStencil True when the image has a depth or stencil aspect.
/// @param[in] supportsReadOnlyOptimal True when the image may use `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`.
/// @param[in] supportsLocalRead True when render targets stay in `VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ`.
/// @param[in] isSourceState True when translating the state before the barrier.
/// @param[in] desc Device-wide translation inputs.
/// @return Stage mask, access mask and layout for `access`.
[[nodiscard]] GP_RHI_VULKAN_API VulkanStageAccessLayout getVkStageAndAccessFlags(
    sync::ResourceAccess access,
    sync::TransitionResourceType resourceType,
    VkBufferUsageFlags usageFlags,
    bool isDepthStencil,
    bool supportsReadOnlyOptimal,
    bool supportsLocalRead,
    bool isSourceState,
    const VulkanBarrierTranslationDesc& desc
) noexcept;

/// @brief Translates the access state of a single depth or stencil plane into stage and access masks.
/// @note `ResolveDst` maps to the color attachment output stage because resolves run there even for depth targets.
/// @param[in] access Access state of the plane.
/// @return Stage mask and access mask for that plane.
[[nodiscard]] GP_RHI_VULKAN_API VulkanStageAccessFlags
    getDepthOrStencilStageAndAccessFlags(sync::ResourceAccess access) noexcept;

}   // namespace gp::rhi::vulkan
