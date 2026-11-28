// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanLinkedView.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"

namespace gp::rhi::vulkan
{

VulkanLinkedView::~VulkanLinkedView() noexcept
{
    unlink();
}

void VulkanLinkedView::linkHead(VulkanLinkedView*& head) noexcept
{
    GP_CHECKF(!isLinked(), "VulkanLinkedView is already linked to a resource.");

    m_next = head;
    m_prevLink = &head;
    if (head != nullptr)
    {
        head->m_prevLink = &m_next;
    }
    head = this;
}

void VulkanLinkedView::unlink() noexcept
{
    if (!isLinked())
    {
        return;
    }

    if (m_next != nullptr)
    {
        m_next->m_prevLink = m_prevLink;
    }
    *m_prevLink = m_next;

    m_next = nullptr;
    m_prevLink = nullptr;
}

}   // namespace gp::rhi::vulkan

