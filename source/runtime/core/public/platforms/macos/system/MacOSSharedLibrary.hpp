// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/apple/system/AppleSharedLibrary.hpp"

namespace gp::platform::macos
{

struct SharedLibrary final : public gp::platform::apple::SharedLibrary
{};

}   // namespace gp::platform::macos
