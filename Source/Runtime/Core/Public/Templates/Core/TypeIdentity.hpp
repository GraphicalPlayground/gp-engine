// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

namespace GP
{

/// @brief Identity transform — always yields T unchanged. Useful for suppressing template argument deduction.
///
/// @example
///   void Scale(float val, TTypeIdentity_T<float> factor);
///   Scale(1.0f, 2.0f);   // factor is NOT deduced; must match exactly.
///
/// @tparam T Any type.
template <typename T>
struct TTypeIdentity
{
    using Type = T;
};

/// @brief Helper alias for the identity transform.
template <typename T>
using TTypeIdentity_T = typename TTypeIdentity<T>::Type;

}   // namespace GP
