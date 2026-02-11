// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Math/MathForward.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Math
{

/// \brief A 2x2 matrix of floating-point values.
/// \tparam T The type of the matrix components. Must be a floating-point type.
/// \warning This structure is intended for mathematical operations and does not include any special optimizations for
/// graphics or other specific use cases. Use with care in performance-critical code.
template <Concepts::IsFloatingPoint T>
struct TMatrix2x2
{
public:
    union
    {
        struct
        {
            T m00, m01;   //<! First row.
            T m10, m11;   //<! Second row.
        };

        T data[4];   //<! Array access to components.
    };
};

}   // namespace GP::Math
