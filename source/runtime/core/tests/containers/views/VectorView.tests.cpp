// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/views/VectorView.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::tests
{

namespace
{

/// @brief Compile-time verification of `gp::VectorView` operations in `constexpr` contexts.
[[nodiscard]] constexpr bool testConstexprVectorView() noexcept
{
    constexpr gp::Int32 raw[6] = { 10, 20, 30, 40, 30, 60 };
    gp::ConstVectorView<gp::Int32> view(raw);

    if (view.size() != 6u || view.isEmpty() || view.front() != 10 || view.back() != 60)
    {
        return false;
    }

    if (view.indexOf(30) != 2u || view.lastIndexOf(30) != 4u || view.count(30) != 2u)
    {
        return false;
    }

    const gp::ConstVectorView<gp::Int32> middle = view.slice(1u, 3u);
    if (middle.size() != 3u || middle[0] != 20 || middle[2] != 40)
    {
        return false;
    }

    const gp::ConstVectorView<gp::Int32> dropped = view.dropFirst(2u).dropLast(1u);
    return dropped.size() == 3u && dropped.front() == 30 && dropped.back() == 30;
}

static_assert(testConstexprVectorView(), "Constexpr gp::VectorView evaluation failed.");

}   // namespace

template <typename InT>
class VectorViewTypedTest : public ::testing::Test
{};

using ViewNumericTypes = ::testing::Types<gp::Int32, gp::UInt64, gp::Float32, gp::Float64>;
TYPED_TEST_SUITE(VectorViewTypedTest, ViewNumericTypes);

TYPED_TEST(VectorViewTypedTest, SlicingIterationAndMutation)
{
    using Elem = TypeParam;
    Elem buffer[5] = {
        static_cast<Elem>(1), static_cast<Elem>(2), static_cast<Elem>(3), static_cast<Elem>(4), static_cast<Elem>(5)
    };

    gp::VectorView<Elem> mutableView(buffer);
    EXPECT_EQ(mutableView.size(), 5u);
    EXPECT_EQ(mutableView.sizeInBytes(), 5u * sizeof(Elem));

    // Mutate through view
    mutableView[2] = static_cast<Elem>(30);
    EXPECT_EQ(buffer[2], static_cast<Elem>(30));

    // Implicit conversion to ConstVectorView
    gp::ConstVectorView<Elem> constView = mutableView;
    EXPECT_EQ(constView.size(), 5u);
    EXPECT_EQ(constView[2], static_cast<Elem>(30));
    EXPECT_EQ(constView.lastElement(0u), static_cast<Elem>(5));
    EXPECT_EQ(constView.lastElement(1u), static_cast<Elem>(4));

    gp::ConstVectorView<Elem> head = constView.first(2u);
    EXPECT_EQ(head.size(), 2u);
    EXPECT_EQ(head[0], static_cast<Elem>(1));
    EXPECT_EQ(head[1], static_cast<Elem>(2));

    gp::ConstVectorView<Elem> tail = constView.last(2u);
    EXPECT_EQ(tail.size(), 2u);
    EXPECT_EQ(tail[0], static_cast<Elem>(4));
    EXPECT_EQ(tail[1], static_cast<Elem>(5));
}

TEST(VectorViewTests, FootprintAndConcepts)
{
    static_assert(sizeof(gp::VectorView<gp::Int32>) == 16u);
    static_assert(sizeof(gp::ConstVectorView<gp::Int32>) == 16u);
    static_assert(sizeof(gp::VectorView64<gp::Int32>) == 16u);
    static_assert(sizeof(gp::ConstVectorView64<gp::Int32>) == 16u);

    static_assert(gp::concepts::IsVectorView<gp::VectorView<gp::Int32>>);
    static_assert(gp::concepts::IsVectorView<gp::ConstVectorView<gp::Int32>>);
    static_assert(!gp::concepts::IsVectorView<gp::Vector<gp::Int32>>);
    static_assert(gp::concepts::IsRelocatable<gp::VectorView<gp::Int32>>);
    static_assert(gp::concepts::IsRelocatable<gp::ConstVectorView<gp::Int32>>);
}

TEST(VectorViewTests, ContainerInteropAcrossAllocators)
{
    const auto sumElements = [](const gp::ConstVectorView<gp::Int32> view) -> gp::Int32
    {
        gp::Int32 total = 0;
        for (const gp::Int32 v: view)
        {
            total += v;
        }
        return total;
    };

    gp::Vector<gp::Int32> heapVec{ 1, 2, 3, 4 };
    gp::InlineVector<gp::Int32, 8u> inlineVec{ 10, 20, 30 };
    gp::FixedVector<gp::Int32, 4u> fixedVec{ 5, 5, 5 };
    gp::Array<gp::Int32, 3u> stdArr{ 7, 8, 9 };
    const gp::Int32 cArr[2] = { 50, 50 };

    EXPECT_EQ(sumElements(heapVec), 10);
    EXPECT_EQ(sumElements(inlineVec), 60);
    EXPECT_EQ(sumElements(fixedVec), 15);
    EXPECT_EQ(sumElements(stdArr), 24);
    EXPECT_EQ(sumElements(cArr), 100);
    EXPECT_EQ(sumElements({ 100, 200, 300 }), 600);
}

TEST(VectorViewTests, SearchPrefixSuffixBinarySearchAndBytes)
{
    gp::Int32 data[6] = { 10, 20, 30, 40, 50, 60 };
    gp::VectorView<gp::Int32> view(data);

    EXPECT_TRUE(view.contains(40));
    EXPECT_FALSE(view.contains(99));
    EXPECT_TRUE(view.containsIf(
        [](const gp::Int32 v) -> bool
    {
        return v > 55;
    }
    ));
    EXPECT_EQ(
        view.countIf(
            [](const gp::Int32 v) -> bool
    {
        return (v % 20) == 0;
    }
        ),
        3u
    );

    const gp::Int32 prefixArr[2] = { 10, 20 };
    const gp::Int32 suffixArr[2] = { 50, 60 };
    EXPECT_TRUE(view.startsWith(gp::ConstVectorView<gp::Int32>(prefixArr)));
    EXPECT_TRUE(view.endsWith(gp::ConstVectorView<gp::Int32>(suffixArr)));
    EXPECT_FALSE(view.startsWith(gp::ConstVectorView<gp::Int32>(suffixArr)));

    EXPECT_EQ(view.binarySearch(30), 2u);
    EXPECT_EQ(view.binarySearch(35), gp::VectorView<gp::Int32>::npos);
    EXPECT_EQ(view.lowerBound(35), 3u);
    EXPECT_EQ(view.upperBound(30), 3u);

    gp::VectorView<const gp::UInt8> roBytes = view.asBytes();
    EXPECT_EQ(roBytes.size(), 6u * sizeof(gp::Int32));

    gp::VectorView<gp::UInt8> rwBytes = view.asWritableBytes();
    EXPECT_EQ(rwBytes.size(), 6u * sizeof(gp::Int32));
    EXPECT_EQ(rwBytes.data(), reinterpret_cast<gp::UInt8*>(data));

    gp::VectorView<gp::Int32> trimmed = view;
    trimmed.removePrefix(1u);
    trimmed.removeSuffix(2u);
    ASSERT_EQ(trimmed.size(), 3u);
    EXPECT_EQ(trimmed.front(), 20);
    EXPECT_EQ(trimmed.back(), 40);

    trimmed.fill(77);
    EXPECT_EQ(data[1], 77);
    EXPECT_EQ(data[2], 77);
    EXPECT_EQ(data[3], 77);
}

}   // namespace gp::tests
