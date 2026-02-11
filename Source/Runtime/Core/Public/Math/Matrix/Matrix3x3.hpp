// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Math/MathForward.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Math
{

/// \brief A 3x3 matrix of floating-point values.
/// \tparam T The type of the matrix components. Must be a floating-point type.
/// \warning This structure is intended for mathematical operations and does not include any specific optimizations for
/// graphics or physics applications. For performance-critical code, consider using specialized libraries or SIMD types.
template <Concepts::IsFloatingPoint T>
struct TMatrix3x3
{
public:
    union
    {
        struct
        {
            T m00, m01, m02;   //<! Row 0
            T m10, m11, m12;   //<! Row 1
            T m20, m21, m22;   //<! Row 2
        };

        T data[9];   //<! Array access to components.
    };
};

}   // namespace GP::Math
