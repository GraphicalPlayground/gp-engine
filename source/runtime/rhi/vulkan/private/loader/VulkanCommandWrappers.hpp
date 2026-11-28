// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "loader/VulkanEntryPoints.hpp"   // IWYU pragma: export

#if GP_VULKAN_DYNAMICALLY_LOADED

namespace gp::rhi::vulkan
{

    #define GP_IMPORT_VK_ENTRYPOINT(Type, Func) using dynamic_api::Func;
GP_ENUM_VK_ENTRYPOINTS_ALL(GP_IMPORT_VK_ENTRYPOINT)
    #undef GP_IMPORT_VK_ENTRYPOINT

}   // namespace gp::rhi::vulkan

#else
    #error "Statically linked Vulkan API must be wrapped!"
#endif
