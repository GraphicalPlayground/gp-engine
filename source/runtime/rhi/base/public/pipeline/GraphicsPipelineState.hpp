// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/Resource.hpp"
#include "CoreMinimal.hpp"
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "RHIBase.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/ShaderForward.hpp"

namespace gp::rhi::pipeline
{

/// @brief Compiled graphics pipeline state object combining shader stages, fixed-function states, and target formats.
/// @details Tagged with `ResourceType::GraphicsPipelineState` so deferred destruction increments
///          `Resource::getNumDeletedPipelineStates()`. Stores an opaque 64-bit `sortKey` used by draw-list sorters to
///          minimize state changes between consecutive draw calls.
class GP_RHI_API GraphicsPipelineState : public resource::Resource
{
public:
    /// @brief Constructs a graphics pipeline state tagged with `ResourceType::GraphicsPipelineState`.
    GraphicsPipelineState() noexcept;

    GraphicsPipelineState(const GraphicsPipelineState&) = delete;
    GraphicsPipelineState& operator=(const GraphicsPipelineState&) = delete;
    GraphicsPipelineState(GraphicsPipelineState&&) = delete;
    GraphicsPipelineState& operator=(GraphicsPipelineState&&) = delete;

protected:
    /// @copydoc resource::Resource::~Resource()
    virtual ~GraphicsPipelineState() noexcept override;

public:
    /// @brief Assigns the draw-call sorting key associated with this compiled pipeline state.
    /// @param[in] sortKey Opaque 64-bit sort key produced by the pipeline state cache.
    GP_FORCEINLINE_HINT void setSortKey(const gp::UInt64 sortKey) noexcept
    {
        m_sortKey = sortKey;
    }

    /// @brief Returns the draw-call sorting key assigned to this pipeline state.
    /// @return 64-bit sort key, or `0` if unassigned.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt64 getSortKey() const noexcept
    {
        return m_sortKey;
    }

    /// @brief Retrieves the graphics shader bound to `frequency` within this graphics pipeline state.
    /// @param[in] frequency Graphics `shader::ShaderFrequency` stage to query.
    /// @return Pointer to the bound `shader::GraphicsShader`, or `nullptr` if that stage is unused.
    [[nodiscard]] virtual shader::GraphicsShader* getShader(shader::ShaderFrequency frequency) const noexcept = 0;

private:
    /// @brief Cache-assigned sort key used to order draw calls by pipeline state.
    gp::UInt64 m_sortKey{ 0u };
};

}   // namespace gp::rhi::pipeline
