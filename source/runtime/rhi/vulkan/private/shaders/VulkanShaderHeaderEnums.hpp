// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::rhi::vulkan
{

/// @brief Render target an input attachment binding reads through subpass fetch.
/// @details The enumerator order is part of the shader compiler contract: bit `N` of
///          `VulkanShaderHeader::inputAttachmentsMask` refers to the enumerator whose value is `N`.
enum class VulkanShaderAttachmentType : gp::UInt8
{
    Depth = 0,
    Color0,
    Color1,
    Color2,
    Color3,
    Color4,
    Color5,
    Color6,
    Color7,

    Count
};

/// @brief Engine-wide immutable samplers a shader may reference without an RHI sampler binding.
/// @details The order must match the global sampler declarations emitted by the shader compiler.
enum class VulkanGlobalSamplerType : gp::UInt8
{
    PointClampedSampler = 0,
    PointWrappedSampler,
    BilinearClampedSampler,
    BilinearWrappedSampler,
    TrilinearClampedSampler,
    TrilinearWrappedSampler,

    Count,
    Invalid
};

/// @brief How a ray hit group stores its any-hit and intersection entry points.
enum class VulkanRayHitGroupEntrypoint : gp::UInt8
{
    /// @brief The hit group does not use this entry point.
    NotPresent = 0,

    /// @brief The entry point lives in the main SPIR-V blob under its own name.
    CommonBlob,

    /// @brief The entry point is stored in a separate SPIR-V blob that follows the main one.
    SeparateBlob
};

/// @brief Resource usage facts the shader compiler records about a shader.
enum class VulkanShaderUsageFlags : gp::UInt8
{
    None = 0x00,

    /// @brief The shader performs no screen-space derivative operations (compute shaders only).
    NoDerivativeOps = 0x01,

    /// @brief The shader may be dispatched through a shader bundle.
    ShaderBundle = 0x02,

    /// @brief The shader reads resources through the bindless heap.
    BindlessResources = 0x04,

    /// @brief The shader reads samplers through the bindless heap.
    BindlessSamplers = 0x08
};

}   // namespace gp::rhi::vulkan

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::vulkan::VulkanShaderUsageFlags);
