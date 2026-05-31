// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "templates/Pair.hpp"
#include <gtest/gtest.h>
#include <string>
#include <utility>

namespace gp::tests
{

// Dummy struct to track constructions, destructions, and assignments
struct PairTracker
{
    static int constructions;
    static int destructions;
    static int copies;
    static int moves;
    static int copyAssignments;
    static int moveAssignments;

    int value;

    PairTracker(int v = 0)
        : value(v)
    {
        constructions++;
    }

    PairTracker(const PairTracker& other)
        : value(other.value)
    {
        copies++;
        constructions++;
    }

    PairTracker(PairTracker&& other) noexcept
        : value(other.value)
    {
        moves++;
        constructions++;
        other.value = -1;
    }

    PairTracker& operator=(const PairTracker& other)
    {
        copyAssignments++;
        value = other.value;
        return *this;
    }

    PairTracker& operator=(PairTracker&& other) noexcept
    {
        moveAssignments++;
        value = other.value;
        other.value = -1;
        return *this;
    }

    ~PairTracker()
    {
        destructions++;
    }

    bool operator==(const PairTracker& other) const
    {
        return value == other.value;
    }

    auto operator<=>(const PairTracker& other) const
    {
        return value <=> other.value;
    }

    static void reset()
    {
        constructions = 0;
        destructions = 0;
        copies = 0;
        moves = 0;
        copyAssignments = 0;
        moveAssignments = 0;
    }
};

int PairTracker::constructions = 0;
int PairTracker::destructions = 0;
int PairTracker::copies = 0;
int PairTracker::moves = 0;
int PairTracker::copyAssignments = 0;
int PairTracker::moveAssignments = 0;

TEST(PairTest, DefaultConstruction)
{
    Pair<int, float> p;
    EXPECT_EQ(p.first, 0);
    EXPECT_EQ(p.second, 0.0f);
}

TEST(PairTest, ValueConstruction)
{
    Pair<int, std::string> p(42, "hello");
    EXPECT_EQ(p.first, 42);
    EXPECT_EQ(p.second, "hello");
}

TEST(PairTest, MoveConstruction)
{
    PairTracker::reset();
    PairTracker t1(10);
    PairTracker t2(20);
    PairTracker::reset();

    Pair<PairTracker, PairTracker> p(std::move(t1), std::move(t2));
    EXPECT_EQ(p.first.value, 10);
    EXPECT_EQ(p.second.value, 20);
    EXPECT_EQ(PairTracker::moves, 2);
}

TEST(PairTest, ForwardingConstruction)
{
    Pair<std::string, std::string> p("first", "second");
    EXPECT_EQ(p.first, "first");
    EXPECT_EQ(p.second, "second");
}

TEST(PairTest, CopyConstruction)
{
    Pair<int, int> p1(1, 2);
    Pair<int, int> p2(p1);
    EXPECT_EQ(p2.first, 1);
    EXPECT_EQ(p2.second, 2);
}

TEST(PairTest, Assignment)
{
    Pair<int, int> p1(1, 2);
    Pair<int, int> p2;
    p2 = p1;
    EXPECT_EQ(p2.first, 1);
    EXPECT_EQ(p2.second, 2);

    Pair<int, int> p3;
    p3 = std::move(p1);
    EXPECT_EQ(p3.first, 1);
    EXPECT_EQ(p3.second, 2);
}

TEST(PairTest, Equality)
{
    Pair<int, int> p1(1, 2);
    Pair<int, int> p2(1, 2);
    Pair<int, int> p3(2, 1);

    EXPECT_TRUE(p1 == p2);
    EXPECT_FALSE(p1 == p3);
    EXPECT_TRUE(p1 != p3);
}

TEST(PairTest, Comparison)
{
    Pair<int, int> p1(1, 2);
    Pair<int, int> p2(1, 3);
    Pair<int, int> p3(2, 1);

    EXPECT_TRUE(p1 < p2);
    EXPECT_TRUE(p2 < p3);
    EXPECT_TRUE(p1 < p3);
    EXPECT_TRUE(p3 > p1);
    EXPECT_TRUE(p1 <= p2);
    EXPECT_TRUE(p2 >= p1);
}

TEST(PairTest, Swap)
{
    Pair<int, int> p1(1, 2);
    Pair<int, int> p2(3, 4);

    p1.swap(p2);
    EXPECT_EQ(p1.first, 3);
    EXPECT_EQ(p1.second, 4);
    EXPECT_EQ(p2.first, 1);
    EXPECT_EQ(p2.second, 2);
}

TEST(PairTest, MakePair)
{
    auto p = makePair(42, std::string("test"));
    static_assert(std::is_same_v<decltype(p), Pair<int, std::string>>);
    EXPECT_EQ(p.first, 42);
    EXPECT_EQ(p.second, "test");
}

TEST(PairTest, DeductionGuide)
{
    Pair p(10, 20.5f);
    static_assert(std::is_same_v<decltype(p), Pair<int, float>>);
    EXPECT_EQ(p.first, 10);
    EXPECT_EQ(p.second, 20.5f);
}

}   // namespace gp::tests
