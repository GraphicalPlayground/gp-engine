// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/generic/GenericPlatformAtomics.hpp"

namespace gp::platform::unix
{

/// @brief Unix platform atomic operations implementation.
/// @note Inherits generic built-in atomic implementations.
class Atomics : public gp::platform::generic::Atomics
{
public:
    Atomics() = delete;
    ~Atomics() = delete;
};

}   // namespace gp::platform::unix

