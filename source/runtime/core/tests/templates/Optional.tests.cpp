// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "templates/Optional.hpp"
#include <gtest/gtest.h>
#include <string>
#include <utility>

namespace gp::tests
{

// Dummy struct to track constructions, destructions, and assignments
struct OptionalTracker
{
    static int constructions;
    static int destructions;
    static int copies;
    static int moves;
    static int copyAssignments;
    static int moveAssignments;

    int value;

    OptionalTracker(int v = 0)
        : value(v)
    {
        constructions++;
    }

    OptionalTracker(const OptionalTracker& other)
        : value(other.value)
    {
        copies++;
        constructions++;
    }

    OptionalTracker(OptionalTracker&& other) noexcept
        : value(other.value)
    {
        moves++;
        constructions++;
        other.value = -1;
    }

    OptionalTracker& operator=(const OptionalTracker& other)
    {
        copyAssignments++;
        value = other.value;
        return *this;
    }

    OptionalTracker& operator=(OptionalTracker&& other) noexcept
    {
        moveAssignments++;
        value = other.value;
        other.value = -1;
        return *this;
    }

    ~OptionalTracker()
    {
        destructions++;
    }

    bool operator==(const OptionalTracker& other) const
    {
        return value == other.value;
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

int OptionalTracker::constructions = 0;
int OptionalTracker::destructions = 0;
int OptionalTracker::copies = 0;
int OptionalTracker::moves = 0;
int OptionalTracker::copyAssignments = 0;
int OptionalTracker::moveAssignments = 0;

TEST(OptionalTest, DefaultConstruction)
{
    Optional<int> opt;
    EXPECT_FALSE(opt.hasValue());
    EXPECT_FALSE(static_cast<bool>(opt));
    EXPECT_EQ(opt, nullopt);
}

TEST(OptionalTest, NullOptConstruction)
{
    Optional<int> opt(nullopt);
    EXPECT_FALSE(opt.hasValue());
    EXPECT_FALSE(static_cast<bool>(opt));
}

TEST(OptionalTest, ValueConstruction)
{
    Optional<int> opt(42);
    EXPECT_TRUE(opt.hasValue());
    EXPECT_TRUE(static_cast<bool>(opt));
    EXPECT_EQ(opt.value(), 42);
    EXPECT_EQ(*opt, 42);
}

TEST(OptionalTest, VariadicConstruction)
{
    Optional<std::string> opt(5, 'a');
    EXPECT_TRUE(opt.hasValue());
    EXPECT_EQ(opt.value(), "aaaaa");
}

TEST(OptionalTest, CopyConstruction)
{
    OptionalTracker::reset();
    Optional<OptionalTracker> opt1(OptionalTracker{ 42 });
    OptionalTracker::reset();

    Optional<OptionalTracker> opt2(opt1);
    EXPECT_TRUE(opt2.hasValue());
    EXPECT_EQ(opt2->value, 42);
    EXPECT_EQ(OptionalTracker::copies, 1);
    EXPECT_EQ(OptionalTracker::moves, 0);
    EXPECT_EQ(OptionalTracker::constructions, 1);
}

TEST(OptionalTest, MoveConstruction)
{
    OptionalTracker::reset();
    Optional<OptionalTracker> opt1(OptionalTracker{ 42 });
    OptionalTracker::reset();

    Optional<OptionalTracker> opt2(std::move(opt1));
    EXPECT_TRUE(opt2.hasValue());
    EXPECT_EQ(opt2->value, 42);

    // gp::Optional specific behavior: move constructor resets the source
    EXPECT_FALSE(opt1.hasValue());
    EXPECT_EQ(OptionalTracker::copies, 0);
    EXPECT_EQ(OptionalTracker::moves, 1);
    EXPECT_EQ(OptionalTracker::constructions, 1);
}

TEST(OptionalTest, NullOptAssignment)
{
    OptionalTracker::reset();
    Optional<OptionalTracker> opt(OptionalTracker{ 42 });
    EXPECT_TRUE(opt.hasValue());
    EXPECT_EQ(OptionalTracker::destructions, 1);   // Temporary was destroyed
    OptionalTracker::reset();

    opt = nullopt;
    EXPECT_FALSE(opt.hasValue());
    EXPECT_EQ(OptionalTracker::destructions, 1);
}

TEST(OptionalTest, CopyAssignment)
{
    OptionalTracker::reset();
    Optional<OptionalTracker> opt1(OptionalTracker{ 42 });
    Optional<OptionalTracker> opt2;
    OptionalTracker::reset();

    opt2 = opt1;
    EXPECT_TRUE(opt2.hasValue());
    EXPECT_EQ(opt2->value, 42);
    EXPECT_EQ(OptionalTracker::copies, 1);
    EXPECT_EQ(OptionalTracker::copyAssignments, 0);   // Copy constructed into empty

    Optional<OptionalTracker> opt3(OptionalTracker{ 10 });
    OptionalTracker::reset();
    opt3 = opt1;
    EXPECT_TRUE(opt3.hasValue());
    EXPECT_EQ(opt3->value, 42);
    EXPECT_EQ(OptionalTracker::copies, 0);
    EXPECT_EQ(OptionalTracker::copyAssignments, 1);   // Assigned into existing
}

TEST(OptionalTest, MoveAssignment)
{
    OptionalTracker::reset();
    Optional<OptionalTracker> opt1(OptionalTracker{ 42 });
    Optional<OptionalTracker> opt2;
    OptionalTracker::reset();

    opt2 = std::move(opt1);
    EXPECT_TRUE(opt2.hasValue());
    EXPECT_EQ(opt2->value, 42);
    EXPECT_FALSE(opt1.hasValue());   // Custom behavior: resets source
    EXPECT_EQ(OptionalTracker::moves, 1);
    EXPECT_EQ(OptionalTracker::moveAssignments, 0);

    Optional<OptionalTracker> opt3(OptionalTracker{ 10 });
    Optional<OptionalTracker> opt4(OptionalTracker{ 20 });
    OptionalTracker::reset();

    opt4 = std::move(opt3);
    EXPECT_TRUE(opt4.hasValue());
    EXPECT_EQ(opt4->value, 10);
    EXPECT_FALSE(opt3.hasValue());
    EXPECT_EQ(OptionalTracker::moves, 0);
    EXPECT_EQ(OptionalTracker::moveAssignments, 1);
}

TEST(OptionalTest, ValueAssignment)
{
    OptionalTracker::reset();
    Optional<OptionalTracker> opt;
    opt = OptionalTracker{ 42 };
    EXPECT_TRUE(opt.hasValue());
    EXPECT_EQ(opt->value, 42);
    EXPECT_EQ(OptionalTracker::moves, 1);   // Temporary constructed, then moved into storage

    OptionalTracker::reset();
    OptionalTracker t(10);
    opt = t;
    EXPECT_EQ(opt->value, 10);
    EXPECT_EQ(OptionalTracker::copyAssignments, 1);
}

TEST(OptionalTest, EqualityComparison)
{
    Optional<int> opt1(42);
    Optional<int> opt2(42);
    Optional<int> opt3(10);
    Optional<int> optEmpty1;
    Optional<int> optEmpty2;

    EXPECT_TRUE(opt1 == opt2);
    EXPECT_FALSE(opt1 == opt3);
    EXPECT_FALSE(opt1 == optEmpty1);
    EXPECT_TRUE(optEmpty1 == optEmpty2);

    EXPECT_TRUE(opt1 != opt3);
    EXPECT_TRUE(opt1 != optEmpty1);
    EXPECT_FALSE(optEmpty1 != optEmpty2);

    EXPECT_TRUE(opt1 == 42);
    EXPECT_FALSE(opt1 == 10);
    EXPECT_FALSE(optEmpty1 == 42);

    EXPECT_TRUE(opt1 != 10);
    EXPECT_FALSE(opt1 != 42);
    EXPECT_TRUE(optEmpty1 != 42);

    EXPECT_TRUE(optEmpty1 == nullopt);
    EXPECT_FALSE(opt1 == nullopt);
    EXPECT_TRUE(opt1 != nullopt);
    EXPECT_FALSE(optEmpty1 != nullopt);
}

TEST(OptionalTest, Accessors)
{
    Optional<std::string> opt("hello");
    EXPECT_EQ(*opt, "hello");
    EXPECT_EQ(opt->length(), 5);

    const Optional<std::string> copt("world");
    EXPECT_EQ(*copt, "world");
    EXPECT_EQ(copt->length(), 5);

    EXPECT_EQ(std::move(opt).value(), "hello");
    EXPECT_EQ(std::move(opt).valueOr(std::string("default")), "hello");

    Optional<std::string> emptyOpt;
    EXPECT_EQ(emptyOpt.valueOr(std::string("default")), "default");
}

TEST(OptionalTest, Emplace)
{
    OptionalTracker::reset();
    Optional<OptionalTracker> opt;
    opt.emplace(42);
    EXPECT_TRUE(opt.hasValue());
    EXPECT_EQ(opt->value, 42);
    EXPECT_EQ(OptionalTracker::constructions, 1);

    OptionalTracker::reset();
    opt.emplace(10);
    EXPECT_TRUE(opt.hasValue());
    EXPECT_EQ(opt->value, 10);
    EXPECT_EQ(OptionalTracker::destructions, 1);   // Previous value destroyed
    EXPECT_EQ(OptionalTracker::constructions, 1);
}

TEST(OptionalTest, Reset)
{
    OptionalTracker::reset();
    Optional<OptionalTracker> opt(OptionalTracker{ 42 });
    EXPECT_TRUE(opt.hasValue());
    OptionalTracker::reset();

    opt.reset();
    EXPECT_FALSE(opt.hasValue());
    EXPECT_EQ(OptionalTracker::destructions, 1);

    // Calling reset again should be safe
    opt.reset();
    EXPECT_EQ(OptionalTracker::destructions, 1);
}

TEST(OptionalTest, Swap)
{
    OptionalTracker::reset();
    Optional<OptionalTracker> opt1(OptionalTracker{ 42 });
    Optional<OptionalTracker> opt2(OptionalTracker{ 10 });
    OptionalTracker::reset();

    opt1.swap(opt2);
    EXPECT_EQ(opt1->value, 10);
    EXPECT_EQ(opt2->value, 42);

    Optional<OptionalTracker> opt3;
    opt1.swap(opt3);
    EXPECT_FALSE(opt1.hasValue());
    EXPECT_TRUE(opt3.hasValue());
    EXPECT_EQ(opt3->value, 10);

    opt3.swap(opt1);
    EXPECT_TRUE(opt1.hasValue());
    EXPECT_EQ(opt1->value, 10);
    EXPECT_FALSE(opt3.hasValue());
}

TEST(OptionalTest, MakeOptional)
{
    auto opt = makeOptional<std::string>(5, 'x');
    EXPECT_TRUE(opt.hasValue());
    EXPECT_EQ(opt.value(), "xxxxx");
}

}   // namespace gp::tests
