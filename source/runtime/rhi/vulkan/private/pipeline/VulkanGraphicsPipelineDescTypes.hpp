// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"             // IWYU pragma: keep
#include "CoreMinimal.hpp"                          // IWYU pragma: keep
#include "pipeline/VulkanPipelineForward.hpp"       // IWYU pragma: keep
#include "renderpass/VulkanRenderPassForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"                     // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Element-wise equality of two description arrays.
/// @tparam InElementType Element type providing `operator==`.
/// @param[in] lhs First array.
/// @param[in] rhs Second array.
/// @return True when both arrays have the same length and equal elements.
template <typename InElementType>
[[nodiscard]] bool
    areVulkanDescArraysEqual(const gp::Vector<InElementType>& lhs, const gp::Vector<InElementType>& rhs) noexcept
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }
    for (gp::USize index = 0u; index < static_cast<gp::USize>(lhs.size()); ++index)
    {
        if (!(lhs[index] == rhs[index]))
        {
            return false;
        }
    }
    return true;
}

/// @brief Compact copy of one `VkPipelineColorBlendAttachmentState`.
/// @details Enumerations are narrowed to 8 bits: every blend op, factor and write mask value used by the RHI fits.
struct GP_RHI_VULKAN_API VulkanBlendAttachmentDesc
{
    /// @brief Blending is enabled.
    bool blend{ false };

    /// @brief `VkBlendOp` for color.
    gp::UInt8 colorBlendOp{ 0u };

    /// @brief `VkBlendFactor` for the source color.
    gp::UInt8 srcColorBlendFactor{ 0u };

    /// @brief `VkBlendFactor` for the destination color.
    gp::UInt8 dstColorBlendFactor{ 0u };

    /// @brief `VkBlendOp` for alpha.
    gp::UInt8 alphaBlendOp{ 0u };

    /// @brief `VkBlendFactor` for the source alpha.
    gp::UInt8 srcAlphaBlendFactor{ 0u };

    /// @brief `VkBlendFactor` for the destination alpha.
    gp::UInt8 dstAlphaBlendFactor{ 0u };

    /// @brief `VkColorComponentFlags`.
    gp::UInt8 colorWriteMask{ 0u };

    /// @brief Copies a native attachment state.
    /// @param[in] state Native state.
    void readFrom(const VkPipelineColorBlendAttachmentState& state) noexcept;

    /// @brief Writes the native attachment state.
    /// @param[out] outState Native state.
    void writeInto(VkPipelineColorBlendAttachmentState& outState) const noexcept;

    /// @brief Appends every field to a key.
    /// @param[in,out] writer Key writer.
    void writeKey(VulkanPipelineKeyWriter& writer) const noexcept;

    /// @brief Memberwise equality.
    /// @param[in] rhs Description to compare against.
    /// @return True when every field matches.
    [[nodiscard]] bool operator==(const VulkanBlendAttachmentDesc& rhs) const noexcept = default;
};

/// @brief One descriptor binding of a pipeline layout set (the descriptor count is always 1).
struct GP_RHI_VULKAN_API VulkanDescriptorSetLayoutBindingDesc
{
    /// @brief Binding index inside the set.
    gp::UInt32 binding{ 0u };

    /// @brief `VkDescriptorType`.
    gp::UInt8 descriptorType{ 0u };

    /// @brief `VkShaderStageFlags`.
    VkShaderStageFlags stageFlags{ 0u };

    /// @brief Copies a native binding.
    /// @param[in] nativeBinding Native binding; its descriptor count must be 1.
    void readFrom(const VkDescriptorSetLayoutBinding& nativeBinding) noexcept;

    /// @brief Writes the native binding (descriptor count 1, no immutable samplers).
    /// @param[out] outBinding Native binding.
    void writeInto(VkDescriptorSetLayoutBinding& outBinding) const noexcept;

    /// @brief Appends every field to a key.
    /// @param[in,out] writer Key writer.
    void writeKey(VulkanPipelineKeyWriter& writer) const noexcept;

    /// @brief Memberwise equality.
    /// @param[in] rhs Description to compare against.
    /// @return True when every field matches.
    [[nodiscard]] bool operator==(const VulkanDescriptorSetLayoutBindingDesc& rhs) const noexcept = default;
};

/// @brief Compact copy of one `VkVertexInputBindingDescription`.
struct GP_RHI_VULKAN_API VulkanVertexBindingDesc
{
    /// @brief Stride in bytes.
    gp::UInt32 stride{ 0u };

    /// @brief Binding index.
    gp::UInt16 binding{ 0u };

    /// @brief `VkVertexInputRate`.
    gp::UInt16 inputRate{ 0u };

    /// @brief Copies a native binding.
    /// @param[in] nativeBinding Native binding.
    void readFrom(const VkVertexInputBindingDescription& nativeBinding) noexcept;

    /// @brief Writes the native binding.
    /// @param[out] outBinding Native binding.
    void writeInto(VkVertexInputBindingDescription& outBinding) const noexcept;

    /// @brief Appends every field to a key.
    /// @param[in,out] writer Key writer.
    void writeKey(VulkanPipelineKeyWriter& writer) const noexcept;

    /// @brief Memberwise equality.
    /// @param[in] rhs Description to compare against.
    /// @return True when every field matches.
    [[nodiscard]] bool operator==(const VulkanVertexBindingDesc& rhs) const noexcept = default;
};

/// @brief Copy of one `VkVertexInputAttributeDescription`.
struct GP_RHI_VULKAN_API VulkanVertexAttributeDesc
{
    /// @brief Shader input location.
    gp::UInt32 location{ 0u };

    /// @brief Vertex binding the attribute reads from.
    gp::UInt32 binding{ 0u };

    /// @brief `VkFormat`.
    gp::UInt32 format{ 0u };

    /// @brief Byte offset inside the vertex.
    gp::UInt32 offset{ 0u };

    /// @brief Copies a native attribute.
    /// @param[in] attribute Native attribute.
    void readFrom(const VkVertexInputAttributeDescription& attribute) noexcept;

    /// @brief Writes the native attribute.
    /// @param[out] outAttribute Native attribute.
    void writeInto(VkVertexInputAttributeDescription& outAttribute) const noexcept;

    /// @brief Appends every field to a key.
    /// @param[in,out] writer Key writer.
    void writeKey(VulkanPipelineKeyWriter& writer) const noexcept;

    /// @brief Memberwise equality.
    /// @param[in] rhs Description to compare against.
    /// @return True when every field matches.
    [[nodiscard]] bool operator==(const VulkanVertexAttributeDesc& rhs) const noexcept = default;
};

/// @brief Pipeline-relevant part of a rasterization state.
/// @details Front face, depth clamp and rasterizer discard are fixed by the backend and not stored.
struct GP_RHI_VULKAN_API VulkanRasterizerDesc
{
    /// @brief `VkPolygonMode`.
    gp::UInt8 polygonMode{ 0u };

    /// @brief `VkCullModeFlags`.
    gp::UInt8 cullMode{ 0u };

    /// @brief Slope-scaled depth bias.
    gp::Float32 depthBiasSlopeScale{ 0.0f };

    /// @brief Constant depth bias; depth bias is enabled when it is not zero.
    gp::Float32 depthBiasConstantFactor{ 0.0f };

    /// @brief Copies a native rasterization state.
    /// @param[in] state Native state.
    void readFrom(const VkPipelineRasterizationStateCreateInfo& state) noexcept;

    /// @brief Writes the native rasterization state.
    /// @details Front face is `VK_FRONT_FACE_CLOCKWISE`; depth clamp and rasterizer discard are disabled.
    /// @param[in,out] outState Native state, reset by the caller (`VulkanRasterizerState::resetCreateInfo`).
    void writeInto(VkPipelineRasterizationStateCreateInfo& outState) const noexcept;

    /// @brief Appends every field to a key.
    /// @param[in,out] writer Key writer.
    void writeKey(VulkanPipelineKeyWriter& writer) const noexcept;

    /// @brief Memberwise equality.
    /// @param[in] rhs Description to compare against.
    /// @return True when every field matches.
    [[nodiscard]] bool operator==(const VulkanRasterizerDesc& rhs) const noexcept = default;
};

/// @brief Copy of a `VkPipelineDepthStencilStateCreateInfo` (both stencil faces, depth bounds test bit).
struct GP_RHI_VULKAN_API VulkanDepthStencilDesc
{
    /// @brief `VkCompareOp` of the depth test.
    gp::UInt8 depthCompareOp{ 0u };

    /// @brief Depth testing is enabled.
    bool depthTestEnable{ false };

    /// @brief Depth writes are enabled.
    bool depthWriteEnable{ false };

    /// @brief Stencil testing is enabled.
    bool stencilTestEnable{ false };

    /// @brief Depth bounds testing is enabled.
    bool depthBoundsTestEnable{ false };

    /// @brief Front face `VkStencilOp` on stencil failure.
    gp::UInt8 frontFailOp{ 0u };

    /// @brief Front face `VkStencilOp` on pass.
    gp::UInt8 frontPassOp{ 0u };

    /// @brief Front face `VkStencilOp` on depth failure.
    gp::UInt8 frontDepthFailOp{ 0u };

    /// @brief Front face `VkCompareOp`.
    gp::UInt8 frontCompareOp{ 0u };

    /// @brief Front face compare mask.
    gp::UInt32 frontCompareMask{ 0u };

    /// @brief Front face write mask.
    gp::UInt32 frontWriteMask{ 0u };

    /// @brief Front face reference value.
    gp::UInt32 frontReference{ 0u };

    /// @brief Back face `VkStencilOp` on stencil failure.
    gp::UInt8 backFailOp{ 0u };

    /// @brief Back face `VkStencilOp` on pass.
    gp::UInt8 backPassOp{ 0u };

    /// @brief Back face `VkStencilOp` on depth failure.
    gp::UInt8 backDepthFailOp{ 0u };

    /// @brief Back face `VkCompareOp`.
    gp::UInt8 backCompareOp{ 0u };

    /// @brief Back face compare mask.
    gp::UInt32 backCompareMask{ 0u };

    /// @brief Back face write mask.
    gp::UInt32 backWriteMask{ 0u };

    /// @brief Back face reference value.
    gp::UInt32 backReference{ 0u };

    /// @brief Copies a native depth-stencil state.
    /// @note Compare masks keep all 32 bits (an 8-bit copy would merge states that differ in the upper bits).
    /// @param[in] state Native state.
    void readFrom(const VkPipelineDepthStencilStateCreateInfo& state) noexcept;

    /// @brief Writes the native depth-stencil state (sType and pNext are left to the caller).
    /// @param[in,out] outState Native state.
    void writeInto(VkPipelineDepthStencilStateCreateInfo& outState) const noexcept;

    /// @brief Appends every field to a key.
    /// @param[in,out] writer Key writer.
    void writeKey(VulkanPipelineKeyWriter& writer) const noexcept;

    /// @brief Memberwise equality.
    /// @param[in] rhs Description to compare against.
    /// @return True when every field matches.
    [[nodiscard]] bool operator==(const VulkanDepthStencilDesc& rhs) const noexcept = default;
};

/// @brief Copy of one `VkAttachmentReference`.
struct GP_RHI_VULKAN_API VulkanAttachmentRefDesc
{
    /// @brief Attachment index, or `VK_ATTACHMENT_UNUSED`.
    gp::UInt32 attachment{ 0u };

    /// @brief `VkImageLayout`.
    gp::UInt64 layout{ 0u };

    /// @brief Copies a native reference.
    /// @param[in] reference Native reference.
    void readFrom(const VkAttachmentReference& reference) noexcept;

    /// @brief Appends every field to a key.
    /// @param[in,out] writer Key writer.
    void writeKey(VulkanPipelineKeyWriter& writer) const noexcept;

    /// @brief Memberwise equality.
    /// @param[in] rhs Description to compare against.
    /// @return True when every field matches.
    [[nodiscard]] bool operator==(const VulkanAttachmentRefDesc& rhs) const noexcept = default;
};

/// @brief Copy of one `VkAttachmentReferenceStencilLayout`.
struct GP_RHI_VULKAN_API VulkanStencilAttachmentRefDesc
{
    /// @brief `VkImageLayout` of the stencil aspect.
    gp::UInt64 layout{ 0u };

    /// @brief Copies a native stencil reference.
    /// @param[in] reference Native reference.
    void readFrom(const VkAttachmentReferenceStencilLayout& reference) noexcept;

    /// @brief Appends every field to a key.
    /// @param[in,out] writer Key writer.
    void writeKey(VulkanPipelineKeyWriter& writer) const noexcept;

    /// @brief Memberwise equality.
    /// @param[in] rhs Description to compare against.
    /// @return True when every field matches.
    [[nodiscard]] bool operator==(const VulkanStencilAttachmentRefDesc& rhs) const noexcept = default;
};

/// @brief Compact copy of one `VkAttachmentDescription`.
struct GP_RHI_VULKAN_API VulkanAttachmentDescriptionDesc
{
    /// @brief `VkFormat`.
    gp::UInt32 format{ 0u };

    /// @brief `VkAttachmentDescriptionFlags`.
    gp::UInt8 flags{ 0u };

    /// @brief `VkSampleCountFlagBits`.
    gp::UInt8 samples{ 0u };

    /// @brief `VkAttachmentLoadOp`.
    gp::UInt8 loadOp{ 0u };

    /// @brief `VkAttachmentStoreOp`.
    gp::UInt8 storeOp{ 0u };

    /// @brief Stencil `VkAttachmentLoadOp`.
    gp::UInt8 stencilLoadOp{ 0u };

    /// @brief Stencil `VkAttachmentStoreOp`.
    gp::UInt8 stencilStoreOp{ 0u };

    /// @brief Initial `VkImageLayout`.
    gp::UInt64 initialLayout{ 0u };

    /// @brief Final `VkImageLayout`.
    gp::UInt64 finalLayout{ 0u };

    /// @brief Copies a native attachment description.
    /// @param[in] description Native description.
    void readFrom(const VkAttachmentDescription& description) noexcept;

    /// @brief Appends every field to a key.
    /// @param[in,out] writer Key writer.
    void writeKey(VulkanPipelineKeyWriter& writer) const noexcept;

    /// @brief Memberwise equality.
    /// @param[in] rhs Description to compare against.
    /// @return True when every field matches.
    [[nodiscard]] bool operator==(const VulkanAttachmentDescriptionDesc& rhs) const noexcept = default;
};

/// @brief Copy of one `VkAttachmentDescriptionStencilLayout`.
struct GP_RHI_VULKAN_API VulkanStencilAttachmentDescriptionDesc
{
    /// @brief Initial stencil `VkImageLayout`.
    gp::UInt64 initialLayout{ 0u };

    /// @brief Final stencil `VkImageLayout`.
    gp::UInt64 finalLayout{ 0u };

    /// @brief Copies a native stencil description.
    /// @param[in] description Native description.
    void readFrom(const VkAttachmentDescriptionStencilLayout& description) noexcept;

    /// @brief Appends every field to a key.
    /// @param[in,out] writer Key writer.
    void writeKey(VulkanPipelineKeyWriter& writer) const noexcept;

    /// @brief Memberwise equality.
    /// @param[in] rhs Description to compare against.
    /// @return True when every field matches.
    [[nodiscard]] bool operator==(const VulkanStencilAttachmentDescriptionDesc& rhs) const noexcept = default;
};

/// @brief Render target layout snapshot stored in a graphics pipeline description.
/// @details Holds everything a pipeline depends on: attachment references, attachment descriptions, the compatible
///          render pass hash and the extent. With dynamic rendering, the color and depth formats are read back from
///          here when the pipeline is created.
struct GP_RHI_VULKAN_API VulkanRenderTargetsDesc
{
    /// @brief One reference per color attachment.
    gp::Vector<VulkanAttachmentRefDesc> colorAttachments{};

    /// @brief One resolve reference per color attachment, or empty without resolves.
    gp::Vector<VulkanAttachmentRefDesc> resolveAttachments{};

    /// @brief Depth-stencil reference (zero without depth-stencil).
    VulkanAttachmentRefDesc depth{};

    /// @brief Stencil layout of the depth-stencil reference (zero without depth-stencil).
    VulkanStencilAttachmentRefDesc stencil{};

    /// @brief Fragment density map reference (zero without one).
    VulkanAttachmentRefDesc fragmentDensity{};

    /// @brief One description per attachment.
    gp::Vector<VulkanAttachmentDescriptionDesc> descriptions{};

    /// @brief Stencil layouts of the depth-stencil attachment (zero without depth-stencil).
    VulkanStencilAttachmentDescriptionDesc stencilDescription{};

    /// @brief Number of attachment descriptions.
    gp::UInt8 numAttachments{ 0u };

    /// @brief Number of color attachments.
    gp::UInt8 numColorAttachments{ 0u };

    /// @brief A depth-stencil attachment is bound.
    bool hasDepthStencil{ false };

    /// @brief Color resolve attachments are bound.
    bool hasResolveAttachments{ false };

    /// @brief A depth-stencil resolve attachment is bound.
    bool hasDepthStencilResolve{ false };

    /// @brief A fragment density map or shading rate attachment is bound.
    bool hasFragmentDensityAttachment{ false };

    /// @brief Number of clear values the matching render pass consumes.
    gp::UInt8 numUsedClearValues{ 0u };

    /// @brief Compatible render pass hash of the layout.
    gp::UInt32 renderPassCompatibleHash{ 0u };

    /// @brief Width, height and depth of the layout extent.
    gp::Array<gp::UInt32, 3u> extent{};

    /// @brief Copies everything a pipeline depends on from a render target layout.
    /// @param[in] layout Layout built from the pipeline initializer.
    void readFrom(const VulkanRenderTargetLayout& layout) noexcept;

    /// @brief Appends every field to a key.
    /// @note `hasFragmentDensityAttachment` is part of the key: two pipelines that differ only by it must not share
    ///       a native pipeline, since one of them declares a shading rate attachment.
    /// @param[in,out] writer Key writer.
    void writeKey(VulkanPipelineKeyWriter& writer) const noexcept;

    /// @brief Memberwise equality.
    /// @param[in] rhs Description to compare against.
    /// @return True when every field matches.
    [[nodiscard]] bool operator==(const VulkanRenderTargetsDesc& rhs) const noexcept;
};

}   // namespace gp::rhi::vulkan
