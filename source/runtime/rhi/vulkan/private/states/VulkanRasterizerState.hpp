// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "pipeline/RasterizerState.hpp"
#include "pipeline/RasterizerStateDesc.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Rasterizer state translated once into a `VkPipelineRasterizationStateCreateInfo`.
/// @details Graphics pipelines copy `getCreateInfo()` verbatim, so the translation cost is paid at state creation
///          and never at pipeline build time. Front faces are clockwise and lines are one pixel wide.
class GP_RHI_VULKAN_API VulkanRasterizerState final : public pipeline::RasterizerState
{
public:
    /// @brief Translates `desc` into its native create info.
    /// @param[in] desc RHI rasterizer description, kept for `getInitializer`.
    explicit VulkanRasterizerState(const pipeline::RasterizerStateDesc& desc) noexcept;

    /// @brief Resets `outCreateInfo` to the engine defaults: fill, no culling, clockwise front faces, 1 px lines.
    /// @param[out] outCreateInfo Create info to reset.
    static void resetCreateInfo(VkPipelineRasterizationStateCreateInfo& outCreateInfo) noexcept;

    /// @copydoc pipeline::RasterizerState::getInitializer()
    [[nodiscard]] virtual bool getInitializer(pipeline::RasterizerStateDesc& outDesc) const noexcept override;

    /// @brief Returns the native create info.
    /// @return Create info with no `pNext` chain.
    [[nodiscard]] const VkPipelineRasterizationStateCreateInfo& getCreateInfo() const noexcept
    {
        return m_createInfo;
    }

    /// @brief Returns the RHI description.
    /// @return Description passed at creation.
    [[nodiscard]] const pipeline::RasterizerStateDesc& getDesc() const noexcept
    {
        return m_desc;
    }

protected:
    /// @brief Destroys the state.
    virtual ~VulkanRasterizerState() noexcept override;

private:
    /// @brief Native create info.
    VkPipelineRasterizationStateCreateInfo m_createInfo;

    /// @brief RHI description.
    pipeline::RasterizerStateDesc m_desc;
};

}   // namespace gp::rhi::vulkan
