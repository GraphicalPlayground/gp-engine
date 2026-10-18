// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/apple/system/SharedLibrary.hpp"

namespace gp::platform::windows
{

struct SharedLibrary final : public gp::platform::apple::SharedLibrary
{};

}   // namespace gp::platform::windows
