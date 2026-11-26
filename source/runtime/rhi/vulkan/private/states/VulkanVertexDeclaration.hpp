// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "pipeline/VertexDeclaration.hpp"
#include "pipeline/VertexDeclarationDesc.hpp"
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Vertex declaration: an element list plus the two hashes pipelines key on.
/// @details Declarations are deduplicated process-wide by `findOrCreate`, which keys on the element bytes. The native
///          binding and attribute descriptions depend on which attributes the vertex shader reads, so they are built
///          per pipeline by `VulkanVertexInputStateInfo` instead of being stored here.
class GP_RHI_VULKAN_API VulkanVertexDeclaration final : public pipeline::VertexDeclaration
{
public:
    /// @brief Stores the elements and their precomputed hashes.
    /// @param[in] elements Vertex declaration elements.
    /// @param[in] hash CRC32 of the element bytes.
    /// @param[in] hashNoStrides CRC32 of the sorted, stride-less elements.
    VulkanVertexDeclaration(
        const pipeline::VertexDeclarationElementList& elements, gp::UInt32 hash, gp::UInt32 hashNoStrides
    ) noexcept;

    /// @brief Returns the declaration matching `elements`, creating it on first use.
    /// @note Thread safe. The process-wide cache keeps every declaration alive until `emptyCache()`.
    /// @param[in] elements Vertex declaration elements.
    /// @return Shared declaration; never null.
    [[nodiscard]] static pipeline::VertexDeclarationRef
        findOrCreate(const pipeline::VertexDeclarationElementList& elements) noexcept;

    /// @brief Drops every cached declaration reference; declarations still referenced elsewhere stay alive.
    static void emptyCache() noexcept;

    /// @brief Returns the number of distinct cached declarations.
    /// @return Cached declaration count.
    [[nodiscard]] static gp::USize getCacheSize() noexcept;

    /// @copydoc pipeline::VertexDeclaration::getInitializer()
    [[nodiscard]] virtual bool
        getInitializer(pipeline::VertexDeclarationElementList& outElements) const noexcept override;

    /// @copydoc pipeline::VertexDeclaration::getPrecachePSOHash()
    /// @note Returns the stride-less hash, so declarations that differ only by stride share precached pipelines.
    [[nodiscard]] virtual gp::UInt32 getPrecachePSOHash() const noexcept override;

    /// @brief Returns the elements.
    /// @return Elements in declaration order.
    [[nodiscard]] const pipeline::VertexDeclarationElementList& getElements() const noexcept
    {
        return m_elements;
    }

    /// @brief Returns the hash of the element bytes.
    /// @return CRC32 of the elements in declaration order.
    [[nodiscard]] gp::UInt32 getHash() const noexcept
    {
        return m_hash;
    }

    /// @brief Returns the hash of the sorted, stride-less elements.
    /// @return CRC32 used for PSO precaching.
    [[nodiscard]] gp::UInt32 getHashNoStrides() const noexcept
    {
        return m_hashNoStrides;
    }

protected:
    /// @brief Destroys the declaration.
    virtual ~VulkanVertexDeclaration() noexcept override;

private:
    /// @brief Elements in declaration order.
    pipeline::VertexDeclarationElementList m_elements;

    /// @brief CRC32 of the element bytes.
    gp::UInt32 m_hash{ 0u };

    /// @brief CRC32 of the sorted, stride-less elements.
    gp::UInt32 m_hashNoStrides{ 0u };
};

}   // namespace gp::rhi::vulkan
