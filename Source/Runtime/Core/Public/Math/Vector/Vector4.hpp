// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Math/MathForward.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Math
{

/// \brief A 4D vector template struct that can represent both geometric vectors and colors.
/// \tparam T The type of the vector components, constrained to floating-point types.
/// \warning This struct is not designed for SIMD operations and may not be optimal for performance-critical code.
/// Consider using specialized SIMD types for such cases.
template <Concepts::IsFloatingPoint T>
struct TVector4
{
public:
    union
    {
        struct
        {
            T x, y, z, w;   //<! Components of the vector.
        };

        struct
        {
            T r, g, b, a;   //<! Color representation.
        };

        T data[4];   //<! Array access to components.
    };
};

}   // namespace GP::Math
