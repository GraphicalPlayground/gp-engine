// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Math/MathForward.hpp"
#include "Math/Vector/Vector2.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Math
{

/// \brief
/// \tparam T
template <Concepts::IsFloatingPoint T>
struct TRectangle
{
public:
    TVector2<T> position;   //<! The position of the rectangle (usually the top-left corner).
    TVector2<T> size;       //<! The size of the rectangle (width and height).
};

}   // namespace GP::Math
