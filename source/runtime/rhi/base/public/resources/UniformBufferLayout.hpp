// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/Resource.hpp"
#include "core/ResourceForward.hpp"          // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "resources/UniformBufferEnums.hpp"
#include "resources/UniformBufferLayoutDesc.hpp"
#include "RHIBase.hpp"
#include "shaders/ShaderEnums.hpp"

namespace gp::rhi::resource
{

/// @brief Immutable reference-counted layout describing the constant data and resource table of a uniform buffer.
/// @details Shared across every `UniformBuffer` instance that conforms to the same shader parameter struct. Because
///          `UniformBufferLayout` derives from `Resource`, uniform buffers retain it via `UniformBufferLayoutConstRef`
///          and deferred deletion keeps layout metadata alive until every referencing uniform buffer has finished on
///          the GPU.
class GP_RHI_API UniformBufferLayout : public Resource
{
public:
    UniformBufferLayout() = delete;

    /// @brief Constructs an immutable uniform buffer layout from `desc`.
    /// @details Uses `desc.hash` when non-zero; otherwise computes the layout hash directly from `desc`.
    /// @param[in] desc Populated layout descriptor.
    explicit UniformBufferLayout(const UniformBufferLayoutDesc& desc) noexcept;

    UniformBufferLayout(const UniformBufferLayout&) = delete;
    UniformBufferLayout& operator=(const UniformBufferLayout&) = delete;
    UniformBufferLayout(UniformBufferLayout&&) = delete;
    UniformBufferLayout& operator=(UniformBufferLayout&&) = delete;

protected:
    /// @copydoc Resource::~Resource()
    virtual ~UniformBufferLayout() noexcept override;

public:
    /// @brief Returns the debug name of the uniform buffer parameter structure.
    /// @return Non-owning view of the layout debug name.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::StringView getDebugName() const noexcept
    {
        return m_name.view();
    }

    /// @brief Returns all RHI resource member entries recorded in this layout.
    /// @return Contiguous view of `UniformBufferResource` descriptors.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::ConstVectorView<UniformBufferResource> getResources() const noexcept
    {
        return m_resources;
    }

    /// @brief Returns all render-graph managed resource entries in this layout.
    /// @return Contiguous view of graph resource descriptors.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::ConstVectorView<UniformBufferResource> getGraphResources() const noexcept
    {
        return m_graphResources;
    }

    /// @brief Returns all render-graph managed texture entries in this layout.
    /// @return Contiguous view of graph texture descriptors.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::ConstVectorView<UniformBufferResource> getGraphTextures() const noexcept
    {
        return m_graphTextures;
    }

    /// @brief Returns all render-graph managed buffer entries in this layout.
    /// @return Contiguous view of graph buffer descriptors.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::ConstVectorView<UniformBufferResource> getGraphBuffers() const noexcept
    {
        return m_graphBuffers;
    }

    /// @brief Returns all render-graph managed nested uniform buffer entries in this layout.
    /// @return Contiguous view of graph uniform buffer descriptors.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::ConstVectorView<UniformBufferResource> getGraphUniformBuffers() const noexcept
    {
        return m_graphUniformBuffers;
    }

    /// @brief Returns all non-graph referenced uniform buffer entries in this layout.
    /// @return Contiguous view of nested uniform buffer descriptors.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::ConstVectorView<UniformBufferResource> getUniformBuffers() const noexcept
    {
        return m_uniformBuffers;
    }

    /// @brief Returns the 32-bit structural hash of this layout.
    /// @note Triggers a slow-build assertion if the hash is zero.
    /// @return Precomputed 32-bit hash matching shader resource table hashes.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getHash() const noexcept
    {
        GP_CHECK_SLOW(m_hash != 0u);
        return m_hash;
    }

    /// @brief Returns the size in bytes of the constant data payload.
    /// @return Constant buffer byte size.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getConstantBufferSize() const noexcept
    {
        return m_constantBufferSize;
    }

    /// @brief Returns the byte offset of embedded render target binding slots.
    /// @return Byte offset, or `kUniformBufferInvalidOffset` when absent.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt16 getRenderTargetsOffset() const noexcept
    {
        return m_renderTargetsOffset;
    }

    /// @brief Returns the global static uniform buffer slot assigned to this layout.
    /// @return `UniformBufferStaticSlot`, or `kMaxUniformBufferStaticSlots` when unbound.
    [[nodiscard]] GP_FORCEINLINE_HINT UniformBufferStaticSlot getStaticSlot() const noexcept
    {
        return m_staticSlot;
    }

    /// @brief Returns the binding models permitted for uniform buffers using this layout.
    /// @return Bitmask of `shader::UniformBufferBindingFlags`.
    [[nodiscard]] GP_FORCEINLINE_HINT shader::UniformBufferBindingFlags getBindingFlags() const noexcept
    {
        return m_bindingFlags;
    }

    /// @brief Returns the behavioral and reflection flags assigned to this layout.
    /// @return Bitmask of `UniformBufferFlags`.
    [[nodiscard]] GP_FORCEINLINE_HINT UniformBufferFlags getFlags() const noexcept
    {
        return m_flags;
    }

    /// @brief Checks whether this layout includes embedded render target binding slots.
    /// @return True when `getRenderTargetsOffset() != kUniformBufferInvalidOffset`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool hasRenderTargets() const noexcept
    {
        return m_renderTargetsOffset != kUniformBufferInvalidOffset;
    }

    /// @brief Checks whether this layout declares outputs managed outside the render graph.
    /// @return True when `UniformBufferFlags::HasNonGraphOutputs` is set.
    [[nodiscard]] GP_FORCEINLINE_HINT bool hasExternalOutputs() const noexcept
    {
        return (m_flags & UniformBufferFlags::HasNonGraphOutputs) != UniformBufferFlags::None;
    }

    /// @brief Checks whether this layout targets a valid global static uniform buffer slot.
    /// @return True when `isUniformBufferStaticSlotValid(getStaticSlot())` holds.
    [[nodiscard]] GP_FORCEINLINE_HINT bool hasStaticSlot() const noexcept
    {
        return isUniformBufferStaticSlotValid(m_staticSlot);
    }

    /// @brief Compares two immutable uniform buffer layouts for runtime layout compatibility.
    /// @param[in] rhs Layout to compare against.
    /// @return True when constant buffer size, static slot, binding flags, and resource arrays match.
    [[nodiscard]] bool operator==(const UniformBufferLayout& rhs) const noexcept;

private:
    /// @brief Debug name of the uniform buffer parameter structure.
    gp::String m_name{};

    /// @brief All RHI resource member entries sorted by offset.
    gp::Vector<UniformBufferResource> m_resources{};

    /// @brief Render-graph managed resource entries.
    gp::Vector<UniformBufferResource> m_graphResources{};

    /// @brief Render-graph managed texture entries.
    gp::Vector<UniformBufferResource> m_graphTextures{};

    /// @brief Render-graph managed buffer entries.
    gp::Vector<UniformBufferResource> m_graphBuffers{};

    /// @brief Render-graph managed nested uniform buffer entries.
    gp::Vector<UniformBufferResource> m_graphUniformBuffers{};

    /// @brief Non-graph referenced uniform buffer entries.
    gp::Vector<UniformBufferResource> m_uniformBuffers{};

    /// @brief Precomputed 32-bit structural hash of this layout.
    gp::UInt32 m_hash{ 0u };

    /// @brief Constant data payload size in bytes.
    gp::UInt32 m_constantBufferSize{ 0u };

    /// @brief Byte offset of embedded render target binding slots, or `kUniformBufferInvalidOffset`.
    gp::UInt16 m_renderTargetsOffset{ kUniformBufferInvalidOffset };

    /// @brief Global static uniform buffer slot index, or `kMaxUniformBufferStaticSlots`.
    UniformBufferStaticSlot m_staticSlot{ kMaxUniformBufferStaticSlots };

    /// @brief Permitted binding models for uniform buffers created with this layout.
    shader::UniformBufferBindingFlags m_bindingFlags{ shader::UniformBufferBindingFlags::Shader };

    /// @brief Behavioral and reflection flags assigned to this layout.
    UniformBufferFlags m_flags{ UniformBufferFlags::None };
};

}   // namespace gp::rhi::resource
