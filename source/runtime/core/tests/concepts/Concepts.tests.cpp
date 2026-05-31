// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "concepts/Concepts.hpp"
#include <array>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace gp::concepts::tests
{

TEST(ConceptsTest, Fundamental)
{
    // Floating Point
    EXPECT_TRUE(IsFloatingPoint<float>);
    EXPECT_TRUE(IsFloatingPoint<double>);
    EXPECT_FALSE(IsFloatingPoint<int>);

    // Integral
    EXPECT_TRUE(IsIntegral<int>);
    EXPECT_TRUE(IsIntegral<uint32_t>);
    EXPECT_TRUE(IsIntegral<bool>);
    EXPECT_FALSE(IsIntegral<float>);

    // Arithmetic
    EXPECT_TRUE(IsArithmetic<int>);
    EXPECT_TRUE(IsArithmetic<float>);
    EXPECT_FALSE(IsArithmetic<void*>);

    // IsNumeric (Excludes bool and char)
    EXPECT_TRUE(IsNumeric<int>);
    EXPECT_TRUE(IsNumeric<float>);
    EXPECT_FALSE(IsNumeric<bool>);
    EXPECT_FALSE(IsNumeric<char>);
    EXPECT_FALSE(IsNumeric<wchar_t>);

    // Pointer
    EXPECT_TRUE(IsPointer<int*>);
    EXPECT_FALSE(IsPointer<int>);

    // Enum
    enum StandardEnum
    {
        A
    };
    enum class ScopedEnum
    {
        B
    };
    EXPECT_TRUE(IsEnum<StandardEnum>);
    EXPECT_TRUE(IsEnum<ScopedEnum>);
    EXPECT_TRUE(IsScopedEnum<ScopedEnum>);
    EXPECT_FALSE(IsScopedEnum<StandardEnum>);

    // Class
    struct S
    {};

    class C
    {};

    EXPECT_TRUE(IsClass<S>);
    EXPECT_TRUE(IsClass<C>);
    EXPECT_FALSE(IsClass<int>);

    // Character
    EXPECT_TRUE(IsCharacter<char>);
    EXPECT_TRUE(IsCharacter<wchar_t>);
    EXPECT_TRUE(IsCharacter<char8_t>);
    EXPECT_TRUE(IsCharacter<char16_t>);
    EXPECT_TRUE(IsCharacter<char32_t>);
    EXPECT_FALSE(IsCharacter<int>);

    // Byte
    EXPECT_TRUE(IsByte<std::byte>);
    EXPECT_TRUE(IsByte<unsigned char>);
    EXPECT_TRUE(IsByte<char>);
    EXPECT_FALSE(IsByte<int>);

    // Void
    EXPECT_TRUE(IsVoid<void>);
    EXPECT_FALSE(IsVoid<int>);

    // NullPointer
    EXPECT_TRUE(IsNullPointer<std::nullptr_t>);
    EXPECT_FALSE(IsNullPointer<void*>);

    // Function
    EXPECT_TRUE(IsFunction<void(int)>);
    EXPECT_FALSE(IsFunction<int>);

    // Object
    EXPECT_TRUE(IsObject<int>);
    EXPECT_TRUE(IsObject<S>);
    EXPECT_FALSE(IsObject<int&>);
    EXPECT_FALSE(IsObject<void>);

    // Scalar
    EXPECT_TRUE(IsScalar<int>);
    EXPECT_TRUE(IsScalar<float>);
    EXPECT_TRUE(IsScalar<int*>);
    EXPECT_TRUE(IsScalar<StandardEnum>);
    EXPECT_FALSE(IsScalar<S>);

    // Fundamental
    EXPECT_TRUE(IsFundamental<int>);
    EXPECT_TRUE(IsFundamental<float>);
    EXPECT_TRUE(IsFundamental<void>);
    EXPECT_FALSE(IsFundamental<S>);

    // Reference
    EXPECT_TRUE(IsReference<int&>);
    EXPECT_TRUE(IsReference<int&&>);
    EXPECT_FALSE(IsReference<int>);

    // Array
    EXPECT_TRUE(IsArray<int[10]>);
    EXPECT_TRUE(IsArray<int[]>);
    EXPECT_FALSE(IsArray<std::vector<int>>);
}

TEST(ConceptsTest, Memory)
{
    struct Trivial
    {
        int x;
    };

    struct NonTrivial
    {
        NonTrivial()
        {}

        virtual ~NonTrivial()
        {}
    };

    // Trivially Copyable
    EXPECT_TRUE(IsTriviallyCopyable<int>);
    EXPECT_TRUE(IsTriviallyCopyable<Trivial>);
    EXPECT_FALSE(IsTriviallyCopyable<NonTrivial>);

    // Trivially Destructible
    EXPECT_TRUE(IsTriviallyDestructible<int>);
    EXPECT_TRUE(IsTriviallyDestructible<Trivial>);
    EXPECT_FALSE(IsTriviallyDestructible<NonTrivial>);

    // Trivially Constructible
    EXPECT_TRUE(IsTriviallyConstructible<int>);
    EXPECT_TRUE(IsTriviallyConstructible<Trivial>);
    EXPECT_FALSE(IsTriviallyConstructible<NonTrivial>);

    // Standard Layout
    EXPECT_TRUE(IsStandardLayout<Trivial>);
    EXPECT_TRUE(IsStandardLayout<int>);

    // POD
    EXPECT_TRUE(IsPod<Trivial>);
    EXPECT_TRUE(IsPod<int>);
    EXPECT_FALSE(IsPod<NonTrivial>);

    // Bitwise Copyable
    EXPECT_TRUE(IsBitwiseCopyable<Trivial>);
    EXPECT_TRUE(IsBitwiseCopyable<int>);

    // Relocatable
    EXPECT_TRUE(IsRelocatable<Trivial>);
    EXPECT_TRUE(IsRelocatable<int>);

    // Raw Pointer
    EXPECT_TRUE(IsRawPointer<int*>);
    EXPECT_FALSE(IsRawPointer<std::vector<int>>);
}

TEST(ConceptsTest, Relationship)
{
    // Same As
    EXPECT_TRUE((IsSameAs<int, int>));
    EXPECT_FALSE((IsSameAs<int, float>));

    // Derived From
    struct Base
    {};

    struct Derived : Base
    {};

    EXPECT_TRUE((IsDerivedFrom<Derived, Base>));
    EXPECT_FALSE((IsDerivedFrom<Base, Derived>));

    // Convertible To
    EXPECT_TRUE((IsConvertibleTo<int, float>));
    EXPECT_FALSE((IsConvertibleTo<int, std::string>));

    // Decayed Same As
    EXPECT_TRUE((IsDecayedSameAs<const int&, int>));
    EXPECT_TRUE((IsDecayedSameAs<int&&, int>));
    EXPECT_TRUE((IsDecayedSameAs<volatile int, int>));

    // One Of
    EXPECT_TRUE((IsOneOf<int, float, char, int, double>));
    EXPECT_FALSE((IsOneOf<int, float, char, double>));
}

TEST(ConceptsTest, Container)
{
    // Iterable
    EXPECT_TRUE(IsIterable<std::vector<int>>);
    EXPECT_TRUE((IsIterable<std::array<int, 5>>));
    EXPECT_TRUE(IsIterable<int[10]>);
    EXPECT_FALSE(IsIterable<int>);

    // Contiguous Range
    EXPECT_TRUE(IsContiguousRange<std::vector<int>>);
    EXPECT_TRUE((IsContiguousRange<std::array<int, 5>>));
    EXPECT_TRUE(IsContiguousRange<int[10]>);

    // Sized Range
    EXPECT_TRUE(IsSizedRange<std::vector<int>>);
    EXPECT_TRUE((IsSizedRange<std::array<int, 5>>));
    EXPECT_TRUE(IsSizedRange<int[10]>);
}

TEST(ConceptsTest, Functional)
{
    auto lambda = [](int x)
    {
        return x > 0;
    };
    auto voidLambda = []()
    {
    };

    // Invocable
    EXPECT_TRUE((IsInvocable<decltype(lambda), int>));
    EXPECT_TRUE(IsInvocable<decltype(voidLambda)>);
    EXPECT_FALSE((IsInvocable<decltype(lambda), std::string>));

    // Predicate
    EXPECT_TRUE((IsPredicate<decltype(lambda), int>));
    EXPECT_FALSE(IsPredicate<decltype(voidLambda)>);
}

TEST(ConceptsTest, Comparison)
{
    // Equality Comparable
    EXPECT_TRUE(IsEqualityComparable<int>);
    EXPECT_TRUE(IsEqualityComparable<std::string>);

    // Three Way Comparable
    EXPECT_TRUE(IsThreeWayComparable<int>);
    EXPECT_TRUE(IsThreeWayComparable<std::string>);

    // Equality Comparable With
    EXPECT_TRUE((IsEqualityComparableWith<int, float>));
    EXPECT_FALSE((IsEqualityComparableWith<int, std::string>));

    struct NoCompare
    {};

    EXPECT_FALSE(IsEqualityComparable<NoCompare>);
}

TEST(ConceptsTest, Math)
{
    // Signed
    EXPECT_TRUE(IsSigned<int>);
    EXPECT_TRUE(IsSigned<float>);
    EXPECT_FALSE(IsSigned<unsigned int>);

    // Unsigned
    EXPECT_TRUE(IsUnsigned<unsigned int>);
    EXPECT_TRUE(IsUnsigned<uint32_t>);
    EXPECT_FALSE(IsUnsigned<int>);
}

TEST(ConceptsTest, Construction)
{
    struct Default
    {
        Default() = default;
    };

    struct NoDefault
    {
        NoDefault(int)
        {}
    };

    // Default Constructible
    EXPECT_TRUE(IsDefaultConstructible<int>);
    EXPECT_TRUE(IsDefaultConstructible<Default>);
    EXPECT_FALSE(IsDefaultConstructible<NoDefault>);

    // Copy Constructible
    EXPECT_TRUE(IsCopyConstructible<int>);

    struct NoCopy
    {
        NoCopy(const NoCopy&) = delete;
    };

    EXPECT_FALSE(IsCopyConstructible<NoCopy>);

    // Move Constructible
    EXPECT_TRUE(IsMoveConstructible<int>);

    struct NoMove
    {
        NoMove(NoMove&&) = delete;
    };

    EXPECT_FALSE(IsMoveConstructible<NoMove>);

    // Copy Assignable
    EXPECT_TRUE(IsCopyAssignable<int>);

    struct NoCopyAssign
    {
        NoCopyAssign& operator=(const NoCopyAssign&) = delete;
    };

    EXPECT_FALSE(IsCopyAssignable<NoCopyAssign>);

    // Move Assignable
    EXPECT_TRUE(IsMoveAssignable<int>);

    struct NoMoveAssign
    {
        NoMoveAssign& operator=(NoMoveAssign&&) = delete;
    };

    EXPECT_FALSE(IsMoveAssignable<NoMoveAssign>);

    // Constructible With
    EXPECT_TRUE((IsConstructibleWith<int, int>));
    EXPECT_TRUE((IsConstructibleWith<std::string, const char*>));
    EXPECT_FALSE((IsConstructibleWith<int, std::string>));

    // Destructible
    EXPECT_TRUE(IsDestructible<int>);

    struct NoDestruct
    {
        ~NoDestruct() = delete;
    };

    EXPECT_FALSE(IsDestructible<NoDestruct>);
}

}   // namespace gp::concepts::tests
