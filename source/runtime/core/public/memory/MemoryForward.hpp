// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::memory
{

/// @section Malloc primitives forward declarations

class Malloc;
class MallocAnsi;

}   // namespace gp::memory

namespace gp
{

/// @section Pointers forward declarations

template <typename T>
class UniquePtr;

template <typename T>
class SharedPtr;

template <typename T>
class WeakPtr;

template <typename T>
class RefCountedPtr;

}   // namespace gp
