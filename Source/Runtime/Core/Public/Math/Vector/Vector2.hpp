// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Math/MathForward.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Math
{

/// \brief A 2D vector with components of type T.
/// \tparam T The type of the vector components. Must be a floating-point type.
/// \warning This struct is not designed for SIMD optimizations. Use with care in performance-critical code.
template <Concepts::IsFloatingPoint T>
struct TVector2
{
public:
    union
    {
        struct
        {
            T x, y;   //<! Components of the vector.
        };

        T data[2];   //<! Array access to components.
    };
};

}   // namespace GP::Math
