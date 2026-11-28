// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceForward.hpp"
#include "CoreMinimal.hpp"
#include "pipeline/SamplerState.hpp"
#include "resources/Texture.hpp"
#include "resources/UniformBuffer.hpp"
#include "resources/UniformBufferEnums.hpp"
#include "resources/UniformBufferLayout.hpp"
#include "shaders/ShaderData.hpp"
#include "shaders/ShaderTypes.hpp"
#include "views/ShaderResourceView.hpp"
#include "views/UnorderedAccessView.hpp"
#include <bit>

namespace gp::rhi::vulkan
{

/// @brief Sentinel closing every list inside a shader resource table map.
inline constexpr gp::UInt32 kVulkanResourceTableEndOfTable = 0xFFFFFFFFu;

/// @brief Packs one resource table entry: uniform buffer index, resource index inside that buffer, binding slot.
/// @details Layout is `[31..24] uniform buffer | [23..8] resource index | [7..0] bind index`. The shader compiler
///          emits these words; each map stores, at index `ubIndex`, the offset of that buffer's entry list.
/// @param[in] uniformBufferIndex Shader-local uniform buffer index (8 bits).
/// @param[in] resourceIndex Index into the uniform buffer's resource table (16 bits).
/// @param[in] bindIndex Shader binding slot (8 bits).
/// @return Packed entry.
[[nodiscard]] constexpr gp::UInt32 encodeVulkanResourceTableEntry(
    const gp::UInt32 uniformBufferIndex, const gp::UInt32 resourceIndex, const gp::UInt32 bindIndex
) noexcept
{
    return ((uniformBufferIndex & 0xFFu) << 24u) | ((resourceIndex & 0xFFFFu) << 8u) | (bindIndex & 0xFFu);
}

/// @brief Extracts the uniform buffer index of a packed entry.
/// @param[in] entry Packed entry.
/// @return Uniform buffer index (0xFF for the end-of-table sentinel).
[[nodiscard]] constexpr gp::UInt32 getVulkanResourceTableUniformBufferIndex(const gp::UInt32 entry) noexcept
{
    return entry >> 24u;
}

/// @brief Extracts the resource index of a packed entry.
/// @param[in] entry Packed entry.
/// @return Index into the uniform buffer's resource table.
[[nodiscard]] constexpr gp::UInt32 getVulkanResourceTableResourceIndex(const gp::UInt32 entry) noexcept
{
    return (entry >> 8u) & 0xFFFFu;
}

/// @brief Extracts the binding slot of a packed entry.
/// @param[in] entry Packed entry.
/// @return Shader binding slot.
[[nodiscard]] constexpr gp::UInt32 getVulkanResourceTableBindIndex(const gp::UInt32 entry) noexcept
{
    return entry & 0xFFu;
}

/// @brief Calls `callback(resourceIndex, bindIndex)` for every entry `map` lists for `uniformBufferIndex`.
/// @details `map[uniformBufferIndex]` holds the offset of the buffer's list (zero means none); the list ends at the
///          first entry of another buffer or at the sentinel. Out-of-range offsets are ignored.
/// @tparam InCallback Callable as `void(gp::UInt32 resourceIndex, gp::UInt32 bindIndex)`.
/// @param[in] map One resource-type map of a `shader::ShaderResourceTable`.
/// @param[in] uniformBufferIndex Shader-local uniform buffer index.
/// @param[in] callback Visitor.
/// @return Number of entries visited.
template <typename InCallback>
gp::UInt32 enumerateVulkanResourceTableEntries(
    const gp::ConstVectorView<gp::UInt32> map, const gp::UInt32 uniformBufferIndex, InCallback&& callback
) noexcept
{
    if (uniformBufferIndex >= map.size())
    {
        return 0u;
    }

    gp::UInt32 count = 0u;
    for (gp::USize offset = map[uniformBufferIndex]; (offset > 0u) && (offset < map.size()); ++offset)
    {
        const gp::UInt32 entry = map[offset];
        if ((entry == kVulkanResourceTableEndOfTable) ||
            (getVulkanResourceTableUniformBufferIndex(entry) != uniformBufferIndex))
        {
            break;
        }
        callback(getVulkanResourceTableResourceIndex(entry), getVulkanResourceTableBindIndex(entry));
        ++count;
    }
    return count;
}

/// @brief Binds every resource `uniformBuffer` carries into the slots the shader expects them in.
/// @tparam InBinder Type providing `setTexture(UInt32, resource::Texture*)`, `setSRV(UInt32,
/// view::ShaderResourceView*)`,
///         `setSampler(UInt32, pipeline::SamplerState*)` and `setUAV(UInt32, view::UnorderedAccessView*)`.
/// @param[in,out] binder Destination of the bindings.
/// @param[in] table Shader resource table of the bound shader.
/// @param[in] uniformBufferIndex Shader-local index `uniformBuffer` is bound at.
/// @param[in] uniformBuffer Uniform buffer whose resource table is read.
/// @return Number of resources bound.
template <typename InBinder>
gp::UInt32 bindVulkanUniformBufferResources(
    InBinder& binder,
    const shader::ShaderResourceTable& table,
    const gp::UInt32 uniformBufferIndex,
    const resource::UniformBuffer& uniformBuffer
) noexcept
{
    const gp::ConstVectorView<resource::ResourceRef> resources = uniformBuffer.getResourceTable();
    const auto fetch = [&resources](const gp::UInt32 resourceIndex) -> resource::Resource*
    {
        return (resourceIndex < resources.size()) ? resources[resourceIndex].get() : nullptr;
    };

    gp::UInt32 count = 0u;
    // UAVs bind first: an SRV of the same resource in the same buffer must not overwrite the writable binding.
    count += enumerateVulkanResourceTableEntries(
        table.unorderedAccessViewMap,
        uniformBufferIndex,
        [&binder, &fetch](const gp::UInt32 resourceIndex, const gp::UInt32 bindIndex) -> void
    {
        binder.setUAV(bindIndex, static_cast<view::UnorderedAccessView*>(fetch(resourceIndex)));
    }
    );
    count += enumerateVulkanResourceTableEntries(
        table.textureMap,
        uniformBufferIndex,
        [&binder, &fetch](const gp::UInt32 resourceIndex, const gp::UInt32 bindIndex) -> void
    {
        binder.setTexture(bindIndex, static_cast<resource::Texture*>(fetch(resourceIndex)));
    }
    );
    count += enumerateVulkanResourceTableEntries(
        table.shaderResourceViewMap,
        uniformBufferIndex,
        [&binder, &fetch](const gp::UInt32 resourceIndex, const gp::UInt32 bindIndex) -> void
    {
        binder.setSRV(bindIndex, static_cast<view::ShaderResourceView*>(fetch(resourceIndex)));
    }
    );
    count += enumerateVulkanResourceTableEntries(
        table.samplerMap,
        uniformBufferIndex,
        [&binder, &fetch](const gp::UInt32 resourceIndex, const gp::UInt32 bindIndex) -> void
    {
        binder.setSampler(bindIndex, static_cast<pipeline::SamplerState*>(fetch(resourceIndex)));
    }
    );
    return count;
}

/// @brief Binds the resource tables of every dirty uniform buffer the shader reads resources from.
/// @details Only buffers both dirty and flagged in `resourceTableBits` are walked; their bits are cleared from
///          `inOutDirtyBits` so a later draw with the same bindings skips the walk.
/// @tparam InBinder See `bindVulkanUniformBufferResources()`.
/// @param[in,out] binder Destination of the bindings.
/// @param[in] shader Shader whose resource table drives the walk.
/// @param[in] boundUniformBuffers Uniform buffers by shader-local index; null entries are skipped.
/// @param[in,out] inOutDirtyBits Bitmask of dirty uniform buffer indices.
/// @return Number of resources bound.
template <typename InBinder>
gp::UInt32 setVulkanUniformBufferResourcesFromTables(
    InBinder& binder,
    const shader::ShaderData& shader,
    const gp::ConstVectorView<resource::UniformBuffer*> boundUniformBuffers,
    gp::UInt32& inOutDirtyBits
) noexcept
{
    const shader::ShaderResourceTable& table = shader.getShaderResourceTable();
    gp::UInt32 pending = inOutDirtyBits & table.resourceTableBits;
    inOutDirtyBits &= ~pending;

    gp::UInt32 count = 0u;
    while (pending != 0u)
    {
        const gp::UInt32 bufferIndex = static_cast<gp::UInt32>(std::countr_zero(pending));
        pending &= pending - 1u;

        const resource::UniformBuffer* const uniformBuffer =
            (bufferIndex < boundUniformBuffers.size()) ? boundUniformBuffers[bufferIndex] : nullptr;
        if (uniformBuffer != nullptr)
        {
            count += bindVulkanUniformBufferResources(binder, table, bufferIndex, *uniformBuffer);
        }
    }
    return count;
}

/// @brief Calls `callback(bufferIndex, uniformBuffer)` for every static slot the shader reads.
/// @details Static uniform buffers are bound once per pass by slot; each shader declares which of its buffer
///          indices map to which slot. When the shader recorded a layout hash for the index, a mismatching buffer is
///          skipped so a stale binding never feeds the wrong layout to the shader.
/// @tparam InCallback Callable as `void(gp::UInt32 bufferIndex, resource::UniformBuffer* uniformBuffer)`.
/// @param[in] shader Shader whose static slots are read.
/// @param[in] staticUniformBuffers Uniform buffers indexed by static slot.
/// @param[in] callback Visitor.
/// @return Number of buffers visited.
template <typename InCallback>
gp::UInt32 applyVulkanStaticUniformBuffers(
    const shader::ShaderData& shader,
    const gp::ConstVectorView<resource::UniformBuffer*> staticUniformBuffers,
    InCallback&& callback
) noexcept
{
    const gp::ConstVectorView<resource::UniformBufferStaticSlot> slots = shader.getStaticSlots();
    const gp::ConstVectorView<gp::UInt32> hashes = shader.getShaderResourceTable().resourceTableLayoutHashes;

    gp::UInt32 count = 0u;
    for (gp::USize bufferIndex = 0u; bufferIndex < slots.size(); ++bufferIndex)
    {
        const resource::UniformBufferStaticSlot slot = slots[bufferIndex];
        if (!resource::isUniformBufferStaticSlotValid(slot) ||
            (static_cast<gp::USize>(slot) >= staticUniformBuffers.size()))
        {
            continue;
        }

        resource::UniformBuffer* const uniformBuffer = staticUniformBuffers[static_cast<gp::USize>(slot)];
        if (uniformBuffer == nullptr)
        {
            continue;
        }

        const bool hasHash = (bufferIndex < hashes.size()) && (hashes[bufferIndex] != 0u);
        if (hasHash && (hashes[bufferIndex] != uniformBuffer->getLayout().getHash()))
        {
            continue;
        }

        callback(static_cast<gp::UInt32>(bufferIndex), uniformBuffer);
        ++count;
    }
    return count;
}

}   // namespace gp::rhi::vulkan
