// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Concepts/CoreConcepts.hpp"

namespace GP
{

template <CFloatingPoint T>
struct TDualQuaternion
{};

}   // namespace GP

#include "Math/Rotation/DualQuaternion.inl"
