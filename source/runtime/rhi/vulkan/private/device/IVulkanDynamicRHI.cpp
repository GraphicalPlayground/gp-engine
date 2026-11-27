// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "IVulkanDynamicRHI.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/DeviceEnums.hpp"
#include "device/DynamicRHI.hpp"

namespace gp::rhi::vulkan
{

IVulkanDynamicRHI* getVulkanDynamicRHI() noexcept
{
    DynamicRHI* const activeRHI = getDynamicRHI();
    if (activeRHI == nullptr)
    {
        return nullptr;
    }

    // Validation wrappers report the inner interface type, so the check runs on the unwrapped RHI.
    DynamicRHI* const innerRHI = activeRHI->getNonValidationRHI();
    if ((innerRHI == nullptr) || (innerRHI->getInterfaceType() != InterfaceType::Vulkan))
    {
        return nullptr;
    }
    return static_cast<IVulkanDynamicRHI*>(innerRHI);
}

}   // namespace gp::rhi::vulkan
