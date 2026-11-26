// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "pipeline/DepthStencilState.hpp"
#include "pipeline/DepthStencilStateDesc.hpp"
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"           // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Depth-stencil state translated into `VkPipelineDepthStencilStateCreateInfo` when a pipeline is built.
/// @details The native struct depends on the pipeline initializer (depth bounds testing), so only the RHI
///          description is stored and `setupCreateInfo` runs per pipeline.
class GP_RHI_VULKAN_API VulkanDepthStencilState final : public pipeline::DepthStencilState
{
public:
    /// @brief Stores the RHI description.
    /// @param[in] desc RHI depth-stencil description.
    explicit VulkanDepthStencilState(const pipeline::DepthStencilStateDesc& desc) noexcept;

    /// @brief Fills the native depth-stencil create info of a graphics pipeline.
    /// @details The depth test is enabled when it is not `Always`, or when depth is written. Stencil faces are
    ///          swapped on purpose: the engine's front-face ops go to Vulkan's `back` face and its back-face ops to
    ///          `front`, which keeps stencil faces consistent with the winding convention of
    ///          `VulkanRasterizerState` (clockwise front faces). Without back-face stencil both faces share the ops.
    /// @param[in] pipelineInitializer Pipeline initializer providing the depth bounds flag.
    /// @param[out] outCreateInfo Receives a fully initialized create info with no `pNext` chain.
    void setupCreateInfo(
        const pipeline::GraphicsPipelineStateInitializer& pipelineInitializer,
        VkPipelineDepthStencilStateCreateInfo& outCreateInfo
    ) const noexcept;

    /// @brief Fills the native depth-stencil create info from the description alone.
    /// @param[in] enableDepthBounds True to enable the depth bounds test over `[0, 1]`.
    /// @param[out] outCreateInfo Receives a fully initialized create info with no `pNext` chain.
    void setupCreateInfo(bool enableDepthBounds, VkPipelineDepthStencilStateCreateInfo& outCreateInfo) const noexcept;

    /// @copydoc pipeline::DepthStencilState::getInitializer()
    [[nodiscard]] virtual bool getInitializer(pipeline::DepthStencilStateDesc& outDesc) const noexcept override;

    /// @brief Returns the RHI description.
    /// @return Description passed at creation.
    [[nodiscard]] const pipeline::DepthStencilStateDesc& getDesc() const noexcept
    {
        return m_desc;
    }

protected:
    /// @brief Destroys the state.
    virtual ~VulkanDepthStencilState() noexcept override;

private:
    /// @brief RHI description.
    pipeline::DepthStencilStateDesc m_desc;
};

}   // namespace gp::rhi::vulkan
