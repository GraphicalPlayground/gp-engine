// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "states/VulkanVertexInputStateInfo.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "diagnostics/AssertionMacros.hpp"
#include "hashing/Crc32.hpp"
#include "memory/Memory.hpp"
#include "pipeline/VertexDeclarationDesc.hpp"
#include "states/VulkanVertexDeclaration.hpp"
#include "translation/VulkanVertexFormat.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Reports whether `attributeIndex` is set in `attributeMask`.
/// @param[in] attributeMask Shader input mask.
/// @param[in] attributeIndex Attribute location.
/// @return True when the shader reads the attribute.
[[nodiscard]] constexpr bool isAttributeUsed(const gp::UInt32 attributeMask, const gp::UInt32 attributeIndex) noexcept
{
    return (attributeIndex < 32u) && (((1u << attributeIndex) & attributeMask) != 0u);
}

/// @brief Returns the native input rate of an element.
/// @param[in] element Vertex element.
/// @return Per-instance or per-vertex input rate.
[[nodiscard]] constexpr VkVertexInputRate getInputRate(const pipeline::VertexElement& element) noexcept
{
    return element.isInstanced() ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
}

}   // namespace

VulkanVertexInputStateInfo::VulkanVertexInputStateInfo() noexcept
{
    // An all-zero create info (sType included) marks a layout that has not been generated yet.
    gp::memory::zeroMemory(&m_info, sizeof(m_info));
    m_bindingToStream.fill(kInvalidMapping);
    m_streamToBinding.fill(kInvalidMapping);
}

VulkanVertexInputStateInfo::VulkanVertexInputStateInfo(const VulkanVertexInputStateInfo& other) noexcept
    : m_info(other.m_info)
    , m_hash(other.m_hash)
    , m_numBindings(other.m_numBindings)
    , m_bindingsMask(other.m_bindingsMask)
    , m_numAttributes(other.m_numAttributes)
    , m_bindingToStream(other.m_bindingToStream)
    , m_streamToBinding(other.m_streamToBinding)
    , m_bindings(other.m_bindings)
    , m_attributes(other.m_attributes)
{
    repointInfo();
}

VulkanVertexInputStateInfo& VulkanVertexInputStateInfo::operator=(const VulkanVertexInputStateInfo& other) noexcept
{
    if (this != &other)
    {
        m_info = other.m_info;
        m_hash = other.m_hash;
        m_numBindings = other.m_numBindings;
        m_bindingsMask = other.m_bindingsMask;
        m_numAttributes = other.m_numAttributes;
        m_bindingToStream = other.m_bindingToStream;
        m_streamToBinding = other.m_streamToBinding;
        m_bindings = other.m_bindings;
        m_attributes = other.m_attributes;
        repointInfo();
    }
    return *this;
}

void VulkanVertexInputStateInfo::generate(
    const VulkanVertexDeclaration& declaration, const gp::UInt32 attributeMask
) noexcept
{
    GP_CHECKF(m_info.sType == 0, "VulkanVertexInputStateInfo::generate must only be called once.");

    const pipeline::VertexDeclarationElementList& elements = declaration.getElements();

    // 1. One binding per stream that feeds at least one used attribute. Bindings start at their stream index.
    for (gp::USize index = 0u; index < elements.size(); ++index)
    {
        const pipeline::VertexElement& element = elements[index];
        if (!isAttributeUsed(attributeMask, element.attributeIndex))
        {
            continue;
        }

        GP_CHECK(element.streamIndex < kMaxVertexElementCount);
        VkVertexInputBindingDescription& binding = m_bindings[element.streamIndex];
        const gp::UInt32 streamBit = 1u << element.streamIndex;
        if ((m_bindingsMask & streamBit) != 0u)
        {
            // Every element of a stream must agree on the stream layout.
            GP_CHECK(binding.binding == element.streamIndex);
            GP_CHECK(binding.inputRate == getInputRate(element));
            GP_CHECK(binding.stride == element.stride);
        }
        else
        {
            binding.binding = element.streamIndex;
            binding.inputRate = getInputRate(element);
            binding.stride = element.stride;
            m_bindingsMask |= streamBit;
        }
    }

    // 2. Remove the gaps between bindings. Compaction only moves entries down, so reading slot `stream` after
    //    writing slot `m_numBindings <= stream` is safe.
    m_numBindings = 0u;
    for (gp::UInt32 stream = 0u; stream < kMaxVertexElementCount; ++stream)
    {
        if ((m_bindingsMask & (1u << stream)) == 0u)
        {
            continue;
        }

        m_bindingToStream[m_numBindings] = static_cast<gp::UInt8>(stream);
        m_streamToBinding[stream] = static_cast<gp::UInt8>(m_numBindings);
        VkVertexInputBindingDescription& binding = m_bindings[m_numBindings];
        binding = m_bindings[stream];
        binding.binding = m_numBindings;
        ++m_numBindings;
    }
    for (gp::UInt32 index = m_numBindings; index < kMaxVertexElementCount; ++index)
    {
        m_bindings[index] = VkVertexInputBindingDescription{};
    }

    // 3. One attribute per used element whose stream is bound.
    GP_CHECK(m_numAttributes == 0u);
    for (gp::USize index = 0u; index < elements.size(); ++index)
    {
        const pipeline::VertexElement& element = elements[index];
        if (!isAttributeUsed(attributeMask, element.attributeIndex) ||
            (m_streamToBinding[element.streamIndex] == kInvalidMapping))
        {
            continue;
        }

        VkVertexInputAttributeDescription& attribute = m_attributes[m_numAttributes];
        ++m_numAttributes;
        attribute.binding = m_streamToBinding[element.streamIndex];
        attribute.location = element.attributeIndex;
        attribute.format = vertexElementTypeToVkFormat(element.type);
        attribute.offset = element.offset;
    }

    // 4. Native create info. A pipeline can have no vertex buffers at all; its hash stays 0.
    m_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    if (m_numBindings == 0u)
    {
        GP_CHECK(m_hash == 0u);
        return;
    }

    GP_CHECK(m_numAttributes > 0u);
    repointInfo();

    gp::hashing::Crc32 hasher;
    hasher.update(static_cast<const void*>(m_bindings.data()), m_numBindings * sizeof(VkVertexInputBindingDescription));
    hasher.update(
        static_cast<const void*>(m_attributes.data()), m_numAttributes * sizeof(VkVertexInputAttributeDescription)
    );
    m_hash = hasher.finalize();
}

const VkVertexInputBindingDescription&
    VulkanVertexInputStateInfo::getBinding(const gp::UInt32 bindingIndex) const noexcept
{
    GP_CHECK(bindingIndex < m_numBindings);
    return m_bindings[bindingIndex];
}

const VkVertexInputAttributeDescription&
    VulkanVertexInputStateInfo::getAttribute(const gp::UInt32 attributeIndex) const noexcept
{
    GP_CHECK(attributeIndex < m_numAttributes);
    return m_attributes[attributeIndex];
}

gp::UInt8 VulkanVertexInputStateInfo::getBindingForStream(const gp::UInt32 streamIndex) const noexcept
{
    return (streamIndex < kMaxVertexElementCount) ? m_streamToBinding[streamIndex] : kInvalidMapping;
}

gp::UInt8 VulkanVertexInputStateInfo::getStreamForBinding(const gp::UInt32 bindingIndex) const noexcept
{
    return (bindingIndex < kMaxVertexElementCount) ? m_bindingToStream[bindingIndex] : kInvalidMapping;
}

bool VulkanVertexInputStateInfo::operator==(const VulkanVertexInputStateInfo& rhs) const noexcept
{
    if (m_numAttributes != rhs.m_numAttributes)
    {
        return false;
    }
    return gp::memory::compareMemory(
               m_attributes.data(), rhs.m_attributes.data(), m_numAttributes * sizeof(VkVertexInputAttributeDescription)
           ) == 0;
}

void VulkanVertexInputStateInfo::repointInfo() noexcept
{
    if (m_numBindings == 0u)
    {
        m_info.vertexBindingDescriptionCount = 0u;
        m_info.pVertexBindingDescriptions = nullptr;
        m_info.vertexAttributeDescriptionCount = 0u;
        m_info.pVertexAttributeDescriptions = nullptr;
        return;
    }

    m_info.vertexBindingDescriptionCount = m_numBindings;
    m_info.pVertexBindingDescriptions = m_bindings.data();
    m_info.vertexAttributeDescriptionCount = m_numAttributes;
    m_info.pVertexAttributeDescriptions = m_attributes.data();
}

}   // namespace gp::rhi::vulkan
