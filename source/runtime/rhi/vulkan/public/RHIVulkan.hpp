// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "logging/LogMacros.hpp"

/// @brief DLL export/import visibility macro for the `rhi/vulkan` module.
#if GP_IS_MONOLITHIC
    #define GP_RHI_VULKAN_API
#else
    #if defined(GP_RHI_VULKAN_EXPORTS)
        #define GP_RHI_VULKAN_API GP_DLLEXPORT
    #else
        #define GP_RHI_VULKAN_API GP_DLLIMPORT
    #endif
#endif

namespace gp
{

/// @brief Log category for general Vulkan RHI runtime diagnostics and device lifecycle events.
GP_DECLARE_EXPORTED_LOG_CATEGORY_EXTERN(GP_RHI_VULKAN_API, LogVulkanRHI);

/// @brief Log category for shared Vulkan shader and low-level translation diagnostics.
GP_DECLARE_EXPORTED_LOG_CATEGORY_EXTERN(GP_RHI_VULKAN_API, LogVulkan);

}   // namespace gp
