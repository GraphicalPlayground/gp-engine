// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "presentation/VulkanViewportRegistry.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "presentation/VulkanViewport.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/UniqueLock.hpp"
#include <utility>

namespace gp::rhi::vulkan
{

VulkanViewportRegistry::~VulkanViewportRegistry() noexcept
{
    GP_CHECK_SLOW(m_viewports.isEmpty());
}

void VulkanViewportRegistry::add(VulkanViewport* const viewport) noexcept
{
    if (viewport == nullptr)
    {
        return;
    }
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    if (!m_viewports.contains(viewport))
    {
        m_viewports.pushBack(viewport);
    }
}

bool VulkanViewportRegistry::remove(VulkanViewport* const viewport) noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_viewports.removeFirstSwap(viewport);
}

bool VulkanViewportRegistry::contains(const VulkanViewport* const viewport) const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    for (const VulkanViewport* const registered: m_viewports)
    {
        if (registered == viewport)
        {
            return true;
        }
    }
    return false;
}

gp::USize VulkanViewportRegistry::getNumViewports() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_viewports.size();
}

gp::USize VulkanViewportRegistry::releaseAll() noexcept
{
    gp::Vector<VulkanViewport*> viewports{};
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
        viewports = std::move(m_viewports);
        m_viewports.clear();
    }

    for (VulkanViewport* const viewport: viewports)
    {
        viewport->releaseNativeResources();
    }
    return viewports.size();
}

}   // namespace gp::rhi::vulkan
