// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "pipeline/BlendState.hpp"
#include "pipeline/BlendStateDesc.hpp"
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "RHIDefinitions.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Blend state translated once into one `VkPipelineColorBlendAttachmentState` per render target slot.
/// @details Every slot is translated, even when independent blending is off; the pipeline builder decides how many
///          attachments it copies. Blending is disabled for a slot whose equation is the identity (`Add`, `One`,
///          `Zero` on both color and alpha), so the hardware can skip the blend unit.
class GP_RHI_VULKAN_API VulkanBlendState final : public pipeline::BlendState
{
public:
    /// @brief Translates `desc` into native attachment states.
    /// @param[in] desc RHI blend description, kept for `getInitializer`.
    explicit VulkanBlendState(const pipeline::BlendStateDesc& desc) noexcept;

    /// @brief Translates one render target blend description.
    /// @param[in] desc RHI render target blend description.
    /// @return Native attachment state.
    [[nodiscard]] static VkPipelineColorBlendAttachmentState
        translateRenderTarget(const pipeline::RenderTargetBlendDesc& desc) noexcept;

    /// @copydoc pipeline::BlendState::getInitializer()
    [[nodiscard]] virtual bool getInitializer(pipeline::BlendStateDesc& outDesc) const noexcept override;

    /// @brief Returns the native attachment state of one render target slot.
    /// @param[in] renderTargetIndex Slot in `[0, kMaxSimultaneousRenderTargets)`.
    /// @return Native attachment state.
    [[nodiscard]] const VkPipelineColorBlendAttachmentState&
        getAttachmentState(gp::UInt32 renderTargetIndex) const noexcept;

    /// @brief Returns the native attachment states of every slot.
    /// @return Pointer to `kMaxSimultaneousRenderTargets` contiguous states.
    [[nodiscard]] const VkPipelineColorBlendAttachmentState* getAttachmentStates() const noexcept
    {
        return m_attachmentStates.data();
    }

    /// @brief Returns the RHI description.
    /// @return Description passed at creation.
    [[nodiscard]] const pipeline::BlendStateDesc& getDesc() const noexcept
    {
        return m_desc;
    }

protected:
    /// @brief Destroys the state.
    virtual ~VulkanBlendState() noexcept override;

private:
    /// @brief Native attachment states, one per render target slot.
    gp::Array<VkPipelineColorBlendAttachmentState, kMaxSimultaneousRenderTargets> m_attachmentStates{};

    /// @brief RHI description.
    pipeline::BlendStateDesc m_desc;
};

}   // namespace gp::rhi::vulkan
