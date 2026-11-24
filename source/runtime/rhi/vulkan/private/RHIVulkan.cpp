// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "RHIVulkan.hpp"
#include "logging/LogEnums.hpp"
#include "modules/ModuleMacros.hpp"

namespace gp
{

GP_DEFINE_LOG_CATEGORY(LogVulkanRHI, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);
GP_DEFINE_LOG_CATEGORY(LogVulkan, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);

}   // namespace gp

GP_IMPLEMENT_DEFAULT_MODULE("rhi/vulkan")
