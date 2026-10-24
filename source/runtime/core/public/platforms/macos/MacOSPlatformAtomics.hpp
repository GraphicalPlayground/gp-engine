// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/apple/ApplePlatformAtomics.hpp"

namespace gp::platform::macos
{

/// @brief macOS platform atomic operations implementation.
class Atomics final : public gp::platform::apple::Atomics
{
public:
    Atomics() = delete;
    ~Atomics() = delete;
};

}   // namespace gp::platform::macos

