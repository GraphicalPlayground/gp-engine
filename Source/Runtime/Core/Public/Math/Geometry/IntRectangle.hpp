// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Math/MathForward.hpp"
#include "Math/Vector/IntExtent2D.hpp"
#include "Math/Vector/IntPoint2D.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Math
{

/// \brief
/// \tparam T
template <Concepts::IsIntegral T>
struct TIntRectangle
{
public:
    TIntPoint2D<T> position;   //<! The position of the rectangle (usually the top-left corner).
    TIntExtent2D<T> size;      //<! The size of the rectangle (width and height).
};

}   // namespace GP::Math
