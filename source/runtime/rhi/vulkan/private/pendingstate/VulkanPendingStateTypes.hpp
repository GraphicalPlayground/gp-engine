// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"                 // IWYU pragma: keep
#include "shaders/VulkanShaderHeaderEnums.hpp"
#include "states/VulkanStateForward.hpp"   // IWYU pragma: keep
#include "views/VulkanViewForward.hpp"     // IWYU pragma: keep
#include "VulkanThirdParty.hpp"            // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Number of engine-wide immutable samplers a shader may reference.
inline constexpr gp::USize kVulkanNumGlobalSamplers = static_cast<gp::USize>(VulkanGlobalSamplerType::Count);

/// @brief Vertex buffer bound to one vertex stream.
struct VulkanVertexStream
{
    /// @brief Native buffer; `VK_NULL_HANDLE` when the stream is unbound.
    VkBuffer buffer{ VK_NULL_HANDLE };

    /// @brief Byte offset of the first vertex.
    VkDeviceSize offset{ 0u };
};

/// @brief Mapped transient memory that receives one packed uniform buffer upload.
/// @details Packed globals are rewritten on every draw that changed a parameter, so they live in per-frame transient
///          memory (a temporary block allocator) instead of in a persistent buffer. The memory stays valid until the
///          command buffer that reads it completes.
struct VulkanUniformUpload
{
    /// @brief Buffer that holds the allocation.
    VkBuffer buffer{ VK_NULL_HANDLE };

    /// @brief Unique id of `buffer`, used by descriptor set keys.
    gp::UInt32 handleId{ 0u };

    /// @brief Byte offset of the allocation in `buffer`.
    gp::UInt32 offset{ 0u };

    /// @brief CPU pointer to the allocation.
    gp::UInt8* mappedData{ nullptr };
};

/// @brief Objects bound to descriptors that have not been written yet, and the engine-wide immutable samplers.
/// @details Every image and sampler descriptor starts out pointing at a valid default so that a partially written set
///          never reads a destroyed object. The defaults are device-wide and outlive every descriptor state.
struct VulkanDescriptorDefaults
{
    /// @brief Sampler bound until a sampler binding is written; must not be null.
    const VulkanSamplerState* defaultSampler{ nullptr };

    /// @brief Image view bound until an image binding is written; must not be null.
    const VulkanTextureView* defaultImageView{ nullptr };

    /// @brief Global sampler per `VulkanGlobalSamplerType`; a null entry leaves the binding on the default sampler.
    gp::Array<const VulkanSamplerState*, kVulkanNumGlobalSamplers> globalSamplers{};
};

}   // namespace gp::rhi::vulkan
