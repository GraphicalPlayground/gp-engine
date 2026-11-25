// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanLayoutManager.hpp"
#include "barriers/VulkanBarrierTypes.hpp"
#include "barriers/VulkanImageLayout.hpp"
#include "barriers/VulkanPipelineBarrier.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"

namespace gp::rhi::vulkan
{

VulkanLayoutManager::VulkanLayoutManager(const bool inWriteOnly, VulkanLayoutManager* const inFallback) noexcept
    : m_isWriteOnly(inWriteOnly)
    , m_fallback(inFallback)
{}

void VulkanLayoutManager::notifyDeletedImage(const VkImage image) noexcept
{
    const gp::USize index = findIndex(image);
    if (index != kInvalidIndex)
    {
        m_images.removeAtSwap(index);
        m_layouts.removeAtSwap(index);
    }
}

const VulkanImageLayout* VulkanLayoutManager::getFullLayout(const VkImage image) const noexcept
{
    GP_CHECK(!m_isWriteOnly);
    const gp::USize index = findIndex(image);
    if (index == kInvalidIndex && m_fallback != nullptr)
    {
        return m_fallback->getFullLayout(image);
    }
    return (index != kInvalidIndex) ? &m_layouts[index] : nullptr;
}

const VulkanImageLayout* VulkanLayoutManager::getFullLayout(
    const VulkanBarrierResource& texture, const bool addIfNotFound, const VkImageLayout layoutIfNotFound
) noexcept
{
    GP_CHECK(!m_isWriteOnly);
    const VulkanImageLayout* layout = findLayout(texture.image);

    if (layout == nullptr && m_fallback != nullptr)
    {
        layout = m_fallback->getFullLayout(texture.image);

        // Carry a layout found in the fallback into this manager so later changes are tracked here.
        if (layout != nullptr)
        {
            const VulkanImageLayout importedLayout = *layout;
            return &addLayout(texture.image, importedLayout);
        }
    }

    if (layout != nullptr)
    {
        return layout;
    }
    else if (!addIfNotFound)
    {
        return nullptr;
    }

    return &addLayout(
        texture.image,
        VulkanImageLayout(layoutIfNotFound, texture.numMips, texture.numArrayLayers, texture.fullAspectMask)
    );
}

void VulkanLayoutManager::setFullLayout(const VkImage image, const VulkanImageLayout& newLayout) noexcept
{
    addOrReplaceLayout(image, newLayout);
}

void VulkanLayoutManager::setFullLayout(
    const VulkanBarrierResource& texture, const VkImageLayout layout, const bool onlyIfNotFound
) noexcept
{
    VulkanImageLayout* const existingLayout = findLayout(texture.image);
    if (existingLayout != nullptr)
    {
        if (!onlyIfNotFound)
        {
            existingLayout->set(layout, VulkanPipelineBarrier::makeSubresourceRange(texture.fullAspectMask));
        }
    }
    else
    {
        addLayout(
            texture.image, VulkanImageLayout(layout, texture.numMips, texture.numArrayLayers, texture.fullAspectMask)
        );
    }
}

void VulkanLayoutManager::setLayout(
    const VulkanBarrierResource& texture, const VkImageSubresourceRange& subresourceRange, const VkImageLayout layout
) noexcept
{
    VulkanImageLayout* const existingLayout = findLayout(texture.image);
    if (existingLayout != nullptr)
    {
        existingLayout->set(layout, subresourceRange);
    }
    else
    {
        VulkanImageLayout newLayout(
            VK_IMAGE_LAYOUT_UNDEFINED, texture.numMips, texture.numArrayLayers, texture.fullAspectMask
        );
        newLayout.set(layout, subresourceRange);
        addLayout(texture.image, newLayout);
    }
}

void VulkanLayoutManager::transferTo(VulkanLayoutManager& destination) noexcept
{
    if (m_images.isEmpty())
    {
        return;
    }

    for (gp::USize index = 0u; index < m_images.size(); ++index)
    {
        destination.addOrReplaceLayout(m_images[index], m_layouts[index]);
    }

    m_images.reset();
    m_layouts.reset();
}

gp::USize VulkanLayoutManager::findIndex(const VkImage image) const noexcept
{
    for (gp::USize index = 0u; index < m_images.size(); ++index)
    {
        if (m_images[index] == image)
        {
            return index;
        }
    }
    return kInvalidIndex;
}

VulkanImageLayout* VulkanLayoutManager::findLayout(const VkImage image) noexcept
{
    const gp::USize index = findIndex(image);
    return (index != kInvalidIndex) ? &m_layouts[index] : nullptr;
}

VulkanImageLayout& VulkanLayoutManager::addLayout(const VkImage image, const VulkanImageLayout& layout) noexcept
{
    m_images.pushBack(image);
    return m_layouts.pushBack(layout);
}

void VulkanLayoutManager::addOrReplaceLayout(const VkImage image, const VulkanImageLayout& layout) noexcept
{
    VulkanImageLayout* const existingLayout = findLayout(image);
    if (existingLayout != nullptr)
    {
        *existingLayout = layout;
    }
    else
    {
        addLayout(image, layout);
    }
}

}   // namespace gp::rhi::vulkan
