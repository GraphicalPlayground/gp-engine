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

public:
    /// @brief Default constructor that initializes the matrix to an identity matrix.
    constexpr Matrix4x4() noexcept
        : col{ _mm_set_ps(0, 0, 0, 1), _mm_set_ps(0, 0, 1, 0), _mm_set_ps(0, 1, 0, 0), _mm_set_ps(1, 0, 0, 0) }
    {}

    /// @brief Constructor that initializes the matrix with specified column vectors.
    /// @param[in] c0 The first column vector.
    /// @param[in] c1 The second column vector.
    /// @param[in] c2 The third column vector.
    /// @param[in] c3 The fourth column vector.
    constexpr Matrix4x4(const __m128& c0, const __m128& c1, const __m128& c2, const __m128& c3) noexcept
        : col{ c0, c1, c2, c3 }
    {}

    constexpr Matrix4x4(const T* const ptr) noexcept
        : col{ _mm_loadu_ps(ptr), _mm_loadu_ps(ptr + 4), _mm_loadu_ps(ptr + 8), _mm_loadu_ps(ptr + 12) }
    {
        GP_ASSERT(ptr != nullptr, "Input pointer cannot be null");
    }

    constexpr Matrix4x4(const Matrix4x4& other) noexcept
        : col{ other.col[0], other.col[1], other.col[2], other.col[3] }
    {}

    constexpr Matrix4x4& operator=(const Matrix4x4& other) noexcept
    {
        if (this != &other)
        {
            col[0] = other.col[0];
            col[1] = other.col[1];
            col[2] = other.col[2];
            col[3] = other.col[3];
        }
        return *this;
    }

    //missing 
    // constexpr Matrix4x4(vector4)

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


public:
    /// @brief Adds two 4x4 matrices using SIMD instructions.
    /// @param[in] A The first matrix.
    /// @param[in] B The second matrix.
    /// @return The sum of the two matrices.
    friend Matrix4x4 operator+(const Matrix4x4& A, const Matrix4x4& B)
    {
        Matrix4x4 result;

        for (int i = 0; i < 4; i++)
            result.col[i] = _mm_add_ps(A.col[i], B.col[i]);
        return result;
    }

    /// @brief Adds a scalar to a 4x4 matrix using SIMD instructions.
    /// @param[in] A The matrix.
    /// @param[in] scalar The scalar value to add.
    /// @return The sum of the matrix and the scalar.
    friend Matrix4x4 operator+(const Matrix4x4& A, const T scalar)
    {
        Matrix4x4 result;
        __m128 scalarVec = _mm_set1_ps(scalar);
        for (int i = 0; i < 4; i++)
            result.col[i] = _mm_add_ps(A.col[i], scalarVec);
        return result;
    }

    friend Matrix4x4 operator+=(const Matrix4x4& A, const Matrix4x4& B)
    {
        return A + B;
    }

    friend Matrix4x4 operator+=(const Matrix4x4& A, const T scalar)
    {
        return A + scalar;
    }

    /// @brief Subtracts two 4x4 matrices using SIMD instructions.
    /// @param[in] A The first matrix.
    /// @param[in] B The second matrix.
    /// @return The difference of the two matrices.
    friend Matrix4x4 operator-(const Matrix4x4& A, const Matrix4x4& B)
    {
        Matrix4x4 result;

        for (int i = 0; i < 4; i++)
            result.col[i] = _mm_sub_ps(A.col[i], B.col[i]);
        return result;
    }

    /// @brief Subtracts a scalar from a 4x4 matrix using SIMD instructions.
    /// @param[in] A The matrix.
    /// @param[in] scalar The scalar value to subtract.
    /// @return The difference of the matrix and the scalar.
    friend Matrix4x4 operator-(const Matrix4x4& A, const T scalar)
    {
        Matrix4x4 result;
        __m128 scalarVec = _mm_set1_ps(scalar);
        for (int i = 0; i < 4; i++)
            result.col[i] = _mm_sub_ps(A.col[i], scalarVec);
        return result;
    }

    friend Matrix4x4 operator-=(const Matrix4x4& A, const Matrix4x4& B)
    {
        return A - B;
    }

    friend Matrix4x4 operator-=(const Matrix4x4& A, const T scalar)
    {
        return A - scalar;
    }

    /// @brief Multiplies two 4x4 matrices using SIMD instructions.
    /// @param[in] A The first matrix.
    /// @param[in] B The second matrix.
    /// @return The product of the two matrices.
    friend Matrix4x4 operator*(const Matrix4x4& A, const Matrix4x4& B)
    {
        Matrix4x4 result;
        for (int i = 0; i < 4; i++)
        {
            __m128 b0 = _mm_set1_ps(B.col[i][0]);
            __m128 b1 = _mm_set1_ps(B.col[i][1]);
            __m128 b2 = _mm_set1_ps(B.col[i][2]);
            __m128 b3 = _mm_set1_ps(B.col[i][3]);

            __m128 acc = _mm_mul_ps(A.col[0], b0);
            acc = _mm_add_ps(acc, _mm_mul_ps(A.col[1], b1));
            acc = _mm_add_ps(acc, _mm_mul_ps(A.col[2], b2));
            acc = _mm_add_ps(acc, _mm_mul_ps(A.col[3], b3));

            result.col[i] = acc;
        }
        return result;
    }

    /// @brief Multiplies a 4x4 matrix by a scalar using SIMD instructions.
    /// @param[in] A The matrix.
    /// @param[in] scalar The scalar value to multiply by.
    /// @return The product of the matrix and the scalar.
    friend Matrix4x4 operator*(const Matrix4x4& A, const T scalar)
    {
        Matrix4x4 result;
        __m128 scalarVec = _mm_set1_ps(scalar);
        for (int i = 0; i < 4; i++)
            result.col[i] = _mm_mul_ps(A.col[i], scalarVec);
        return result;
    }


    /// @brief Multiplies a 4x4 matrix by another 4x4 matrix using SIMD instructions.
    /// @param[in] A The first matrix.
    /// @param[in] B The second matrix.
    /// @return The product of the two matrices.
    friend Matrix4x4 operator*=(const Matrix4x4& A, const Matrix4x4& B)
    {
        return A * B;
    }

    /// @brief Multiplies a 4x4 matrix by a scalar using SIMD instructions.
    /// @param[in] A The matrix.
    /// @param[in] scalar The scalar value to multiply by.
    /// @return The product of the matrix and the scalar.
    friend Matrix4x4 operator*=(const Matrix4x4& A, const T scalar)
    {
        return A * scalar;
    }


public:
    /// @brief Compares two 4x4 matrices for equality using SIMD instructions.
    /// @param[in] A The first matrix.
    /// @param[in] B The second matrix.
    /// @return True if the matrices are equal, false otherwise.
    /// @note there is ZERO tolerance for this comparison. if you want to compare with tolerance, use the equals function
    friend bool operator==(const Matrix4x4& A, const Matrix4x4& B)
    {
        for (int i = 0; i < 4; i++)
            if (!_mm_move_test_pi32(B.col[i], A.col[i])) return false;
        return true;
    }

    /// @brief Compares two 4x4 matrices for inequality using SIMD instructions.
    /// @param[in] A The first matrix.
    /// @param[in] B The second matrix.
    /// @return True if the matrices are not equal, false otherwise.
    friend bool operator!=(const Matrix4x4& A, const Matrix4x4& B)
    {
        return !(A == B);
    }

    /// @brief Compares two 4x4 matrices for equality within a specified tolerance using SIMD instructions.
    /// @param[in] A The first matrix.
    /// @param[in] B The second matrix.
    /// @param[in] tolerance The tolerance for the comparison.
    /// @return True if the matrices are equal within the specified tolerance, false otherwise.
    friend bool equals(const Matrix4x4& A, const Matrix4x4& B, const T tolerance = Constants<T>::kindaSmallNumber)
    {
        for (int i = 0; i < 4; i++)
        {
            __m128 diff = _mm_sub_ps(A.col[i], B.col[i]);
            __m128 absDiff = _mm_andnot_ps(_mm_set1_ps(-0.0f), diff); // absolute value
            __m128 cmp = _mm_cmple_ps(absDiff, _mm_set1_ps(tolerance));
            if (_mm_movemask_ps(cmp) != 0xF) return false; // if any component is greater than tolerance
        }
        return true;
    }

    /// @brief Compares two 4x4 matrices for inequality within a specified tolerance using SIMD instructions.
    /// @param[in] A The first matrix.
    /// @param[in] B The second matrix.
    /// @param[in] tolerance The tolerance for the comparison.
    /// @return True if the matrices are not equal within the specified tolerance, false otherwise.
    friend bool notEquals(const Matrix4x4& A, const Matrix4x4& B, const T tolerance = Constants<T>::kindaSmallNumber)
    {
        return !equals(A, B, tolerance);
    }

};

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