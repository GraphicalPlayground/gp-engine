// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/VulkanDynamicRHIModule.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/DeviceEnums.hpp"
#include "device/RHIGlobals.hpp"
#include "device/VulkanDynamicRHI.hpp"
#include "device/VulkanFeatureLevelSupport.hpp"
#include "platform/VulkanPlatform.hpp"

namespace gp::rhi::vulkan
{

bool VulkanDynamicRHIModule::isSupported() const noexcept
{
    return VulkanPlatform::isSupported() && isVulkanBackendAvailable(getVulkanAdapterProbe());
}

bool VulkanDynamicRHIModule::isSupported(const FeatureLevel requestedFeatureLevel) const noexcept
{
    return VulkanPlatform::isSupported() && isVulkanFeatureLevelAvailable(
                                                requestedFeatureLevel,
                                                getVulkanAdapterProbe(),
                                                VulkanPlatform::supportsProfileChecks(),
                                                GP_WITH_EDITOR != 0
                                            );
}

DynamicRHI* VulkanDynamicRHIModule::createRHI(const FeatureLevel requestedFeatureLevel) noexcept
{
    const FeatureLevel platformFeatureLevel = VulkanPlatform::getFeatureLevel(requestedFeatureLevel);
    getRHIGlobals().maxSupportedFeatureLevel = resolveVulkanFeatureLevel(platformFeatureLevel, getVulkanAdapterProbe());
    return new VulkanDynamicRHI();
}

}   // namespace gp::rhi::vulkan
