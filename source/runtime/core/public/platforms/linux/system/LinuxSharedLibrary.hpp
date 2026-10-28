// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/unix/system/UnixSharedLibrary.hpp"

namespace gp::platform::linux
{

struct SharedLibrary final : public gp::platform::unix::SharedLibrary
{};

}   // namespace gp::platform::linux
