// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "RHIDefinitions.hpp"
#include "RHIVulkan.hpp"
#include "states/VulkanStateForward.hpp"   // IWYU pragma: keep
#include "VulkanThirdParty.hpp"            // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Native vertex input layout of one pipeline: compacted bindings plus the attributes the shader reads.
/// @details `generate` keeps only the elements whose attribute index is set in the shader's input mask. Every stream
///          they use becomes one `VkVertexInputBindingDescription` (binding = stream, input rate, stride). Bindings
///          are then compacted so they have no gaps, and the stream-to-binding map is kept so draw calls can bind
///          vertex buffers to the right slots. `getInfo()` points into this object, so copies re-point their
///          `VkPipelineVertexInputStateCreateInfo` at their own arrays.
class GP_RHI_VULKAN_API VulkanVertexInputStateInfo final
{
public:
    /// @brief Marks a stream or binding that is not mapped.
    static constexpr gp::UInt8 kInvalidMapping = 0xFFu;

    /// @brief Creates an empty, not yet generated layout.
    VulkanVertexInputStateInfo() noexcept;

    /// @brief Copies `other` and re-points the native create info at this object's arrays.
    /// @param[in] other Layout to copy.
    VulkanVertexInputStateInfo(const VulkanVertexInputStateInfo& other) noexcept;

    /// @brief Copies `other` and re-points the native create info at this object's arrays.
    /// @param[in] other Layout to copy.
    /// @return This layout.
    VulkanVertexInputStateInfo& operator=(const VulkanVertexInputStateInfo& other) noexcept;

    /// @brief Destroys the layout.
    ~VulkanVertexInputStateInfo() noexcept = default;

    /// @brief Builds the bindings and attributes of `declaration` filtered by `attributeMask`.
    /// @note Must be called once, on a freshly constructed object.
    /// @param[in] declaration Vertex declaration.
    /// @param[in] attributeMask Bit `i` is set when the vertex shader reads attribute location `i`.
    void generate(const VulkanVertexDeclaration& declaration, gp::UInt32 attributeMask) noexcept;

    /// @brief Returns the hash of the bindings followed by the attributes.
    /// @return CRC32, or 0 when there are no bindings.
    [[nodiscard]] gp::UInt32 getHash() const noexcept
    {
        return m_hash;
    }

    /// @brief Returns the native create info.
    /// @return Create info pointing into this object.
    [[nodiscard]] const VkPipelineVertexInputStateCreateInfo& getInfo() const noexcept
    {
        return m_info;
    }

    /// @brief Returns the mask of the streams the compacted bindings came from.
    /// @return Bit `i` is set when stream `i` is bound.
    [[nodiscard]] gp::UInt32 getBindingsMask() const noexcept
    {
        return m_bindingsMask;
    }

    /// @brief Returns the number of compacted bindings.
    /// @return Binding count.
    [[nodiscard]] gp::UInt32 getNumBindings() const noexcept
    {
        return m_numBindings;
    }

    /// @brief Returns the number of attributes.
    /// @return Attribute count.
    [[nodiscard]] gp::UInt32 getNumAttributes() const noexcept
    {
        return m_numAttributes;
    }

    /// @brief Returns one compacted binding.
    /// @param[in] bindingIndex Binding in `[0, getNumBindings())`.
    /// @return Native binding description.
    [[nodiscard]] const VkVertexInputBindingDescription& getBinding(gp::UInt32 bindingIndex) const noexcept;

    /// @brief Returns one attribute.
    /// @param[in] attributeIndex Attribute in `[0, getNumAttributes())`.
    /// @return Native attribute description.
    [[nodiscard]] const VkVertexInputAttributeDescription& getAttribute(gp::UInt32 attributeIndex) const noexcept;

    /// @brief Returns the compacted binding of a stream.
    /// @param[in] streamIndex Vertex stream in `[0, kMaxVertexElementCount)`.
    /// @return Binding index, or `kInvalidMapping` when the stream is not bound.
    [[nodiscard]] gp::UInt8 getBindingForStream(gp::UInt32 streamIndex) const noexcept;

    /// @brief Returns the stream a compacted binding came from.
    /// @param[in] bindingIndex Binding in `[0, kMaxVertexElementCount)`.
    /// @return Stream index, or `kInvalidMapping` when the binding is unused.
    [[nodiscard]] gp::UInt8 getStreamForBinding(gp::UInt32 bindingIndex) const noexcept;

    /// @brief Compares the attributes of two layouts.
    /// @param[in] rhs Layout to compare against.
    /// @return True when both layouts declare the same attributes in the same order.
    [[nodiscard]] bool operator==(const VulkanVertexInputStateInfo& rhs) const noexcept;

private:
    /// @brief Points `m_info` at this object's arrays.
    void repointInfo() noexcept;

private:
    /// @brief Native create info; its arrays point into this object.
    VkPipelineVertexInputStateCreateInfo m_info;

    /// @brief CRC32 of the bindings then the attributes.
    gp::UInt32 m_hash{ 0u };

    /// @brief Number of compacted bindings.
    gp::UInt32 m_numBindings{ 0u };

    /// @brief Streams the compacted bindings came from.
    gp::UInt32 m_bindingsMask{ 0u };

    /// @brief Number of attributes.
    gp::UInt32 m_numAttributes{ 0u };

    /// @brief Compacted binding index to source stream.
    gp::Array<gp::UInt8, kMaxVertexElementCount> m_bindingToStream{};

    /// @brief Source stream to compacted binding index.
    gp::Array<gp::UInt8, kMaxVertexElementCount> m_streamToBinding{};

    /// @brief Compacted bindings.
    gp::Array<VkVertexInputBindingDescription, kMaxVertexElementCount> m_bindings{};

    /// @brief Attributes.
    gp::Array<VkVertexInputAttributeDescription, kMaxVertexElementCount> m_attributes{};
};

}   // namespace gp::rhi::vulkan
