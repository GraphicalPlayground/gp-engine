// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/generic/system/SharedLibrary.hpp"

namespace gp::platform::unix
{

struct SharedLibrary : public gp::platform::generic::SharedLibrary
{};

}   // namespace gp::platform::unix
