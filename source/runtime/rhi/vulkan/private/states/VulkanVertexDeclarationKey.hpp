// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "pipeline/VertexDeclarationDesc.hpp"
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Cache key of a vertex declaration.
/// @details `hash` covers the element bytes in declaration order and identifies the declaration itself.
///          `hashNoStrides` sorts the elements by (stream, offset, attribute) and zeroes their strides first: two
///          declarations that only differ by vertex buffer stride or element order produce the same pipeline vertex
///          layout, so PSO precaching keys on that hash.
struct GP_RHI_VULKAN_API VulkanVertexDeclarationKey final
{
    /// @brief Elements in declaration order.
    pipeline::VertexDeclarationElementList elements{};

    /// @brief CRC32 of the element bytes in declaration order.
    gp::UInt32 hash{ 0u };

    /// @brief CRC32 of the sorted elements with their strides zeroed.
    gp::UInt32 hashNoStrides{ 0u };

public:
    /// @brief Copies `inElements` and computes both hashes.
    /// @param[in] inElements Vertex declaration elements.
    explicit VulkanVertexDeclarationKey(const pipeline::VertexDeclarationElementList& inElements) noexcept;

    /// @brief Compares the element lists byte for byte.
    /// @param[in] rhs Key to compare against.
    /// @return True when both keys hold the same elements in the same order.
    [[nodiscard]] bool operator==(const VulkanVertexDeclarationKey& rhs) const noexcept;

    /// @brief Sorts elements by stream index, then offset, then attribute index.
    /// @param[in,out] inOutElements Elements to sort in place; the sort is stable.
    static void sortElements(pipeline::VertexDeclarationElementList& inOutElements) noexcept;
};

}   // namespace gp::rhi::vulkan
