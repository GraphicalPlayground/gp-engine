// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Math/Geometry/Plane.hpp"
#include "Math/MathForward.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Math
{

/// \brief
/// \tparam T
template <Concepts::IsFloatingPoint T>
struct TFrustum
{
public:
    union
    {
        struct
        {
            TPlane<T> left;     //<! The left plane of the frustum.
            TPlane<T> right;    //<! The right plane of the frustum.
            TPlane<T> top;      //<! The top plane of the frustum.
            TPlane<T> bottom;   //<! The bottom plane of the frustum.
            TPlane<T> near;     //<! The near plane of the frustum.
            TPlane<T> far;      //<! The far plane of the frustum.
        };

        TPlane<T> planes[6];   //<! The six planes of the frustum (left, right, top, bottom, near, far).
    };
};

}   // namespace GP::Math
