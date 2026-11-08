// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::thread
{

enum class FiberResult : gp::UInt8;
enum class FiberState : gp::UInt8;
enum class FiberStackTier : gp::UInt8;

struct FiberDesc;
class Fiber;
class ScopedThreadFiber;

}   // namespace gp::thread
