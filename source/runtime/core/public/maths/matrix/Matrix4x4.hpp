// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"
#include "maths/MathForward.hpp"
#include <xmmintrin.h>  // SSE
#include <immintrin.h>  // if you go AVX later

namespace gp::math
{

/// @brief
/// @tparam T
template <concepts::IsFloatingPoint T>
struct Matrix4x4
{
private:
    __m128 col[4];   // col[0..3], each holding 4 floats (one column)


//setter and getter for col
public:
    
    /// @brief Gets a reference to a column of the matrix by index.
    /// @param[in] index The index of the column to access (0 to 3).
    /// @return A reference to the column at the specified index.
    /// @note The behavior is undefined if the index is out of range
    [[nodiscard]] constexpr __m128& getCol(const Int32 index) noexcept
    {
        GP_ASSERT(index >= 0 && index < 4, "Index out of range");
        return col[index];
    }

    /// @brief Gets a const reference to a column of the matrix by index.
    /// @param[in] index The index of the column to access (0 to 3  ).
    /// @return A const reference to the column at the specified index.
    /// @note The behavior is undefined if the index is out of range
    [[nodiscard]] constexpr const __m128& getCol(const Int32 index) const noexcept
    {
        GP_ASSERT(index >= 0 && index < 4, "Index out of range");
        return col[index];
    }

    /// @brief Sets a column of the matrix by index.
    /// @param[in] index The index of the column to set (0 to 3).
    /// @param[in] value The value to set the column to.
    /// @note The behavior is undefined if the index is out of range
    constexpr void setCol(const Int32 index, const __m128& value) noexcept
    {
        GP_ASSERT(index >= 0 && index < 4, "Index out of range");
        col[index] = value;
    }
}   // namespace gp::math




/*
so the issue at the moment is that it is using xmmintrin and immintrin.

the issue with that is that we need to import them. how do we define the package we make them
install? 

is it possible to remake it? how difficult would it be. could it be optimal to do 


why to use friend.

what does friend do:
    - it allows specific function and classes to access the private and protected classes of another class
    
    what does that mean exactly in practice

    you define a class or a function, and then class or function has access to the private area of the class 
    why would we want that?

    so in theory. when calling a operator. it is not a member-class. so it doesnt have access to the classes
    normally you would have to do a get or set. but in this case, it has automatic access to the private function: col.

*/

/*

begnning implementation of a 4x4 matrix class using SIMD intrinsics for efficient matrix multiplication. The class is templated to support floating-point types and uses the __m128 type to store columns of the matrix. The operator* function is defined as a friend function to allow it to access the private members of the Matrix4x4 class directly, enabling efficient multiplication without the need for getter methods.
*/