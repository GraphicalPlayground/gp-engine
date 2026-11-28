// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "logging/LogMacros.hpp"

namespace gp::imgui::detail
{

/// @brief Log category shared by every translation unit of `plugin/imgui`.
GP_DEFINE_LOG_CATEGORY_INLINE(LogImGui, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);

}   // namespace gp::imgui::detail
