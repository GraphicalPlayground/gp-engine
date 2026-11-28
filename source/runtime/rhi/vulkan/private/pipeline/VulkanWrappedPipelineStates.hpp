// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"
#include "pipeline/ComputePipelineState.hpp"
#include "pipeline/GraphicsPipelineState.hpp"
#include "pipeline/VulkanComputePipelineState.hpp"
#include "pipeline/VulkanGraphicsPipelineState.hpp"
#include "RHIVulkan.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/ShaderForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief RHI graphics pipeline handed to the renderer; one per creation request.
/// @details A thin handle on the shared `VulkanGraphicsPipelineState`. Its only job is lifetime: it keeps the inner
///          state alive and, when it dies, tells the cache manager one wrapper fewer uses that state, so the manager
///          can drop pipelines nobody uses anymore.
/// @note The cache manager counts the wrapper in `VulkanCommonPipelineState::m_numWrappers` before constructing it.
class GP_RHI_VULKAN_API VulkanWrappedGraphicsPipelineState final : public pipeline::GraphicsPipelineState
{
public:
    /// @brief Wraps a counted inner state.
    /// @param[in] state Inner state; a reference is held.
    explicit VulkanWrappedGraphicsPipelineState(VulkanGraphicsPipelineState* state) noexcept;

    /// @copydoc pipeline::GraphicsPipelineState::getShader()
    [[nodiscard]] virtual shader::GraphicsShader* getShader(shader::ShaderFrequency frequency) const noexcept override;

    /// @brief Returns the shared inner state.
    /// @return Inner state.
    [[nodiscard]] VulkanGraphicsPipelineState* getState() const noexcept
    {
        return m_state.get();
    }

protected:
    /// @brief Uncounts the wrapper from its inner state, if the cache manager is still alive.
    virtual ~VulkanWrappedGraphicsPipelineState() noexcept override;

private:
    /// @brief Shared inner state.
    gp::RefCountedPtr<VulkanGraphicsPipelineState> m_state{};
};

/// @brief RHI compute pipeline handed to the renderer; one per creation request.
/// @details Same role as `VulkanWrappedGraphicsPipelineState`. The RHI validity flag mirrors whether the native
/// pipeline
///          could be created.
class GP_RHI_VULKAN_API VulkanWrappedComputePipelineState final : public pipeline::ComputePipelineState
{
public:
    /// @brief Wraps a counted inner state.
    /// @param[in] state Inner state; a reference is held.
    explicit VulkanWrappedComputePipelineState(VulkanComputePipelineState* state) noexcept;

    /// @brief Returns the shared inner state.
    /// @return Inner state.
    [[nodiscard]] VulkanComputePipelineState* getState() const noexcept
    {
        return m_state.get();
    }

protected:
    /// @brief Uncounts the wrapper from its inner state, if the cache manager is still alive.
    virtual ~VulkanWrappedComputePipelineState() noexcept override;

private:
    /// @brief Shared inner state.
    gp::RefCountedPtr<VulkanComputePipelineState> m_state{};
};

}   // namespace gp::rhi::vulkan
