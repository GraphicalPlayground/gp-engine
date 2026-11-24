// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Translates an RHI `VertexElementType` attribute format to its Vulkan `VkFormat`.
/// @param[in] type Vertex input element component type.
/// @return Matching `VkFormat`, or `VK_FORMAT_UNDEFINED` for `VertexElementType::None` or out-of-range values.
[[nodiscard]] inline constexpr VkFormat vertexElementTypeToVkFormat(const pipeline::VertexElementType type) noexcept
{
    switch (type)
    {
    case pipeline::VertexElementType::Float1:
        return VK_FORMAT_R32_SFLOAT;
    case pipeline::VertexElementType::Float2:
        return VK_FORMAT_R32G32_SFLOAT;
    case pipeline::VertexElementType::Float3:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case pipeline::VertexElementType::Float4:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case pipeline::VertexElementType::PackedNormal:
        return VK_FORMAT_R8G8B8A8_SNORM;
    case pipeline::VertexElementType::UByte4:
        return VK_FORMAT_R8G8B8A8_UINT;
    case pipeline::VertexElementType::UByte4N:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case pipeline::VertexElementType::Color:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case pipeline::VertexElementType::Short2:
        return VK_FORMAT_R16G16_SINT;
    case pipeline::VertexElementType::Short4:
        return VK_FORMAT_R16G16B16A16_SINT;
    case pipeline::VertexElementType::Short2N:
        return VK_FORMAT_R16G16_SNORM;
    case pipeline::VertexElementType::Half2:
        return VK_FORMAT_R16G16_SFLOAT;
    case pipeline::VertexElementType::Half4:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case pipeline::VertexElementType::Short4N:
        return VK_FORMAT_R16G16B16A16_SNORM;
    case pipeline::VertexElementType::UShort2:
        return VK_FORMAT_R16G16_UINT;
    case pipeline::VertexElementType::UShort4:
        return VK_FORMAT_R16G16B16A16_UINT;
    case pipeline::VertexElementType::UShort2N:
        return VK_FORMAT_R16G16_UNORM;
    case pipeline::VertexElementType::UShort4N:
        return VK_FORMAT_R16G16B16A16_UNORM;
    case pipeline::VertexElementType::URGB10A2N:
        return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case pipeline::VertexElementType::UInt:
        return VK_FORMAT_R32_UINT;
    case pipeline::VertexElementType::UInt2:
        return VK_FORMAT_R32G32_UINT;
    case pipeline::VertexElementType::None:
    default:
        return VK_FORMAT_UNDEFINED;
    }
}

/// @brief Returns the tightly packed byte size of a single vertex attribute element of `type`.
/// @param[in] type Vertex input element component type.
/// @return Size in bytes (`4`, `8`, `12`, or `16`), or `0u` for `VertexElementType::None` or out-of-range values.
[[nodiscard]] inline constexpr gp::UInt32 getVertexElementByteSize(const pipeline::VertexElementType type) noexcept
{
    switch (type)
    {
    case pipeline::VertexElementType::Float1:
    case pipeline::VertexElementType::PackedNormal:
    case pipeline::VertexElementType::UByte4:
    case pipeline::VertexElementType::UByte4N:
    case pipeline::VertexElementType::Color:
    case pipeline::VertexElementType::Short2:
    case pipeline::VertexElementType::Short2N:
    case pipeline::VertexElementType::Half2:
    case pipeline::VertexElementType::UShort2:
    case pipeline::VertexElementType::UShort2N:
    case pipeline::VertexElementType::URGB10A2N:
    case pipeline::VertexElementType::UInt:
        return 4u;

    case pipeline::VertexElementType::Float2:
    case pipeline::VertexElementType::Short4:
    case pipeline::VertexElementType::Half4:
    case pipeline::VertexElementType::Short4N:
    case pipeline::VertexElementType::UShort4:
    case pipeline::VertexElementType::UShort4N:
    case pipeline::VertexElementType::UInt2:
        return 8u;

    case pipeline::VertexElementType::Float3:
        return 12u;

    case pipeline::VertexElementType::Float4:
        return 16u;

    case pipeline::VertexElementType::None:
    default:
        return 0u;
    }
}

}   // namespace gp::rhi::vulkan
