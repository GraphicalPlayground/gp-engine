// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "templates/Expected.hpp"
#include <gtest/gtest.h>
#include <string>
#include <type_traits>

namespace gp::tests
{

// Generic tracker to monitor object lifecycle
struct LifecycleTracker
{
    static int constructions;
    static int destructions;
    static int copies;
    static int moves;
    static int copyAssignments;
    static int moveAssignments;

    int value;

    explicit LifecycleTracker(int v = 0)
        : value(v)
    {
        constructions++;
    }

    LifecycleTracker(const LifecycleTracker& other)
        : value(other.value)
    {
        copies++;
        constructions++;
    }

    LifecycleTracker(LifecycleTracker&& other) noexcept
        : value(other.value)
    {
        moves++;
        constructions++;
        other.value = -1;
    }

    LifecycleTracker& operator=(const LifecycleTracker& other)
    {
        copyAssignments++;
        value = other.value;
        return *this;
    }

    LifecycleTracker& operator=(LifecycleTracker&& other) noexcept
    {
        moveAssignments++;
        value = other.value;
        other.value = -1;
        return *this;
    }

    ~LifecycleTracker()
    {
        destructions++;
    }

    bool operator==(const LifecycleTracker& other) const
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

int LifecycleTracker::constructions = 0;
int LifecycleTracker::destructions = 0;
int LifecycleTracker::copies = 0;
int LifecycleTracker::moves = 0;
int LifecycleTracker::copyAssignments = 0;
int LifecycleTracker::moveAssignments = 0;

TEST(UnexpectedTest, Construction)
{
    LifecycleTracker::reset();
    {
        Unexpected<LifecycleTracker> unexp(LifecycleTracker{ 42 });
        EXPECT_EQ(unexp.error().value, 42);
        EXPECT_EQ(LifecycleTracker::constructions, 2);   // 1 temp + 1 moved
        EXPECT_EQ(LifecycleTracker::moves, 1);
    }
    EXPECT_EQ(LifecycleTracker::destructions, 2);
}

TEST(UnexpectedTest, VariadicConstruction)
{
    struct MultiArg
    {
        int a;
        float b;

        MultiArg(int i, float f)
            : a(i)
            , b(f)
        {}
    };

    Unexpected<MultiArg> unexp(42, 3.14f);
    EXPECT_EQ(unexp.error().a, 42);
    EXPECT_EQ(unexp.error().b, 3.14f);
}

TEST(UnexpectedTest, Assignment)
{
    LifecycleTracker::reset();
    Unexpected<LifecycleTracker> unexp1(LifecycleTracker{ 1 });
    Unexpected<LifecycleTracker> unexp2(LifecycleTracker{ 2 });
    LifecycleTracker::reset();

    unexp1 = unexp2;
    EXPECT_EQ(unexp1.error().value, 2);
    EXPECT_EQ(LifecycleTracker::copyAssignments, 1);

    unexp1 = std::move(unexp2);
    EXPECT_EQ(unexp1.error().value, 2);
    EXPECT_EQ(LifecycleTracker::moveAssignments, 1);
}

TEST(UnexpectedTest, Equality)
{
    Unexpected<int> unexp1(42);
    Unexpected<int> unexp2(42);
    Unexpected<int> unexp3(10);

    EXPECT_TRUE(unexp1 == unexp2);
    EXPECT_FALSE(unexp1 == unexp3);
    EXPECT_TRUE(unexp1 != unexp3);
}

TEST(UnexpectedTest, Accessors)
{
    Unexpected<std::string> unexp("error");
    EXPECT_EQ(unexp.error(), "error");

    const Unexpected<std::string> cunexp("const error");
    EXPECT_EQ(cunexp.error(), "const error");

    EXPECT_EQ(std::move(unexp).error(), "error");
}

TEST(UnexpectedTest, CTAD)
{
    Unexpected unexp(42);
    static_assert(std::is_same_v<decltype(unexp), Unexpected<int>>);
    EXPECT_EQ(unexp.error(), 42);
}

TEST(UnexpectedTest, Factory)
{
    auto unexp = makeUnexpected(std::string("failed"));
    EXPECT_EQ(unexp.error(), "failed");
}

TEST(ExpectedTest, DefaultConstruction)
{
    Expected<int, int> ex;
    EXPECT_TRUE(ex.hasValue());
    EXPECT_TRUE(static_cast<bool>(ex));
    EXPECT_EQ(ex.value(), 0);
}

TEST(ExpectedTest, ValueConstruction)
{
    Expected<int, int> ex(42);
    EXPECT_TRUE(ex.hasValue());
    EXPECT_EQ(ex.value(), 42);

    Expected<std::string, int> exStr("aaaaa");
    EXPECT_TRUE(exStr.hasValue());
    EXPECT_EQ(exStr.value(), "aaaaa");
}

TEST(ExpectedTest, ConvertingConstructor)
{
    // Test implicit conversion
    Expected<double, int> ex1 = 42;
    EXPECT_TRUE(ex1.hasValue());
    EXPECT_EQ(ex1.value(), 42.0);

    // Test explicit conversion
    struct ExplicitInt
    {
        int val;

        explicit ExplicitInt(int v)
            : val(v)
        {}
    };

    // Expected<ExplicitInt, int> ex2 = 42; // Should not compile
    Expected<ExplicitInt, int> ex2(42);
    EXPECT_TRUE(ex2.hasValue());
    EXPECT_EQ(ex2.value().val, 42);
}

TEST(ExpectedTest, ErrorConstruction)
{
    Expected<int, int> ex(makeUnexpected(404));
    EXPECT_FALSE(ex.hasValue());
    EXPECT_EQ(ex.error(), 404);

    Expected<int, std::string> exStr(Unexpected<std::string>("fail"));
    EXPECT_FALSE(exStr.hasValue());
    EXPECT_EQ(exStr.error(), "fail");
}

TEST(ExpectedTest, CopyMoveConstruction)
{
    LifecycleTracker::reset();
    {
        Expected<LifecycleTracker, int> ex1(LifecycleTracker{ 42 });
        EXPECT_EQ(LifecycleTracker::constructions, 2);   // 1 temp + 1 moved into storage
        LifecycleTracker::reset();

        Expected<LifecycleTracker, int> ex2(ex1);
        EXPECT_TRUE(ex2.hasValue());
        EXPECT_EQ(ex2.value().value, 42);
        EXPECT_EQ(LifecycleTracker::copies, 1);

        Expected<LifecycleTracker, int> ex3(std::move(ex2));
        EXPECT_TRUE(ex3.hasValue());
        EXPECT_EQ(ex3.value().value, 42);
        EXPECT_EQ(LifecycleTracker::moves, 1);
    }

    LifecycleTracker::reset();
    {
        Expected<int, LifecycleTracker> err1(makeUnexpected(LifecycleTracker{ 404 }));
        LifecycleTracker::reset();

        Expected<int, LifecycleTracker> err2(err1);
        EXPECT_FALSE(err2.hasValue());
        EXPECT_EQ(err2.error().value, 404);
        EXPECT_EQ(LifecycleTracker::copies, 1);

        Expected<int, LifecycleTracker> err3(std::move(err2));
        EXPECT_FALSE(err3.hasValue());
        EXPECT_EQ(err3.error().value, 404);
        EXPECT_EQ(LifecycleTracker::moves, 1);
    }
}

TEST(ExpectedTest, Assignment)
{
    Expected<int, int> ex;
    ex = 42;
    EXPECT_TRUE(ex.hasValue());
    EXPECT_EQ(ex.value(), 42);

    ex = makeUnexpected(500);
    EXPECT_FALSE(ex.hasValue());
    EXPECT_EQ(ex.error(), 500);

    ex = 10;
    EXPECT_TRUE(ex.hasValue());
    EXPECT_EQ(ex.value(), 10);

    Expected<int, int> ex2(100);
    ex = ex2;
    EXPECT_EQ(ex.value(), 100);

    ex = makeUnexpected(404);
    Expected<int, int> ex3(makeUnexpected(505));
    ex = std::move(ex3);
    EXPECT_EQ(ex.error(), 505);
}

TEST(ExpectedTest, Equality)
{
    Expected<int, int> ex1(42);
    Expected<int, int> ex2(42);
    Expected<int, int> ex3(10);
    Expected<int, int> err1(makeUnexpected(404));
    Expected<int, int> err2(makeUnexpected(404));

    EXPECT_EQ(ex1, ex2);
    EXPECT_NE(ex1, ex3);
    EXPECT_NE(ex1, err1);
    EXPECT_EQ(err1, err2);

    EXPECT_EQ(ex1, 42);
    EXPECT_NE(ex1, 10);
    EXPECT_NE(err1, 42);

    EXPECT_EQ(err1, makeUnexpected(404));
    EXPECT_NE(err1, makeUnexpected(500));
    EXPECT_NE(ex1, makeUnexpected(404));
}

TEST(ExpectedTest, Accessors)
{
    Expected<std::string, int> ex("hello");
    EXPECT_EQ(*ex, "hello");
    EXPECT_EQ(ex->length(), 5);

    const Expected<std::string, int> cex("world");
    EXPECT_EQ(*cex, "world");
    EXPECT_EQ(cex->length(), 5);

    Expected<int, std::string> err(makeUnexpected(std::string("fail")));
    EXPECT_EQ(err.error(), "fail");

    const Expected<int, std::string> cerr(makeUnexpected(std::string("error")));
    EXPECT_EQ(cerr.error(), "error");
}

TEST(ExpectedTest, ValueOrErrorOr)
{
    Expected<int, int> ex(42);
    EXPECT_EQ(ex.valueOr(0), 42);
    EXPECT_EQ(ex.errorOr(500), 500);

    Expected<int, int> err(makeUnexpected(404));
    EXPECT_EQ(err.valueOr(0), 0);
    EXPECT_EQ(err.errorOr(500), 404);

    // Test rvalue move for valueOr
    LifecycleTracker::reset();
    Expected<LifecycleTracker, int> exLT(LifecycleTracker{ 42 });
    LifecycleTracker::reset();
    auto val = std::move(exLT).valueOr(LifecycleTracker{ 0 });
    EXPECT_EQ(val.value, 42);
    EXPECT_EQ(LifecycleTracker::moves, 1);

    // Test rvalue move for errorOr
    LifecycleTracker::reset();
    Expected<int, LifecycleTracker> errLT(makeUnexpected(LifecycleTracker{ 404 }));
    LifecycleTracker::reset();
    auto errVal = std::move(errLT).errorOr(LifecycleTracker{ 0 });
    EXPECT_EQ(errVal.value, 404);
    EXPECT_EQ(LifecycleTracker::moves, 1);
}

TEST(ExpectedTest, Emplace)
{
    Expected<LifecycleTracker, LifecycleTracker> ex(LifecycleTracker{ 1 });
    LifecycleTracker::reset();

    ex.emplaceValue(42);
    EXPECT_TRUE(ex.hasValue());
    EXPECT_EQ(ex.value().value, 42);
    EXPECT_EQ(LifecycleTracker::destructions, 1);
    EXPECT_EQ(LifecycleTracker::constructions, 1);

    LifecycleTracker::reset();
    ex.emplaceError(500);
    EXPECT_FALSE(ex.hasValue());
    EXPECT_EQ(ex.error().value, 500);
    EXPECT_EQ(LifecycleTracker::destructions, 1);
    EXPECT_EQ(LifecycleTracker::constructions, 1);
}

TEST(ExpectedTest, Swap)
{
    Expected<int, int> ex1(42);
    Expected<int, int> ex2(10);
    ex1.swap(ex2);
    EXPECT_EQ(ex1.value(), 10);
    EXPECT_EQ(ex2.value(), 42);

    Expected<int, int> err1(makeUnexpected(404));
    ex1.swap(err1);
    EXPECT_FALSE(ex1.hasValue());
    EXPECT_EQ(ex1.error(), 404);
    EXPECT_TRUE(err1.hasValue());
    EXPECT_EQ(err1.value(), 10);

    // Lifecycle check for cross-track swap
    LifecycleTracker::reset();
    Expected<LifecycleTracker, LifecycleTracker> exLT(LifecycleTracker{ 1 });
    Expected<LifecycleTracker, LifecycleTracker> errLT(makeUnexpected(LifecycleTracker{ 2 }));
    LifecycleTracker::reset();

    exLT.swap(errLT);
    EXPECT_FALSE(exLT.hasValue());
    EXPECT_EQ(exLT.error().value, 2);
    EXPECT_TRUE(errLT.hasValue());
    EXPECT_EQ(errLT.value().value, 1);

    EXPECT_EQ(LifecycleTracker::moves, 3);
    EXPECT_EQ(LifecycleTracker::constructions, 3);
    EXPECT_EQ(LifecycleTracker::destructions, 3);
}

TEST(ExpectedMonadicTest, AndThen)
{
    auto f = [](int x) -> Expected<std::string, int>
    {
        if (x > 0)
        {
            return std::string("positive");
        }
        return makeUnexpected(-1);
    };

    Expected<int, int> ex1(42);
    EXPECT_EQ(ex1.andThen(f).value(), "positive");

    Expected<int, int> ex2(-10);
    EXPECT_FALSE(ex2.andThen(f).hasValue());
    EXPECT_EQ(ex2.andThen(f).error(), -1);

    Expected<int, int> err(makeUnexpected(404));
    EXPECT_FALSE(err.andThen(f).hasValue());
    EXPECT_EQ(err.andThen(f).error(), 404);

    // Test rvalue move - Success path
    LifecycleTracker::reset();
    Expected<LifecycleTracker, int> exLT(LifecycleTracker{ 42 });
    LifecycleTracker::reset();
    auto res = std::move(exLT).andThen([](LifecycleTracker lt) -> Expected<int, int> {
        return lt.value;
    });
    EXPECT_EQ(res.value(), 42);
    EXPECT_EQ(LifecycleTracker::moves, 1);

    // Test rvalue move - Error path (should move error into result)
    LifecycleTracker::reset();
    Expected<int, LifecycleTracker> errLT(makeUnexpected(LifecycleTracker{ 404 }));
    LifecycleTracker::reset();
    auto resErr = std::move(errLT).andThen([](int x) -> Expected<int, LifecycleTracker> {
        return x;
    });
    EXPECT_FALSE(resErr.hasValue());
    EXPECT_EQ(resErr.error().value, 404);
    // 1 move from storage to Unexpected temp, 1 move from Unexpected temp to new Expected storage
    EXPECT_EQ(LifecycleTracker::moves, 2);
}

TEST(ExpectedMonadicTest, OrElse)
{
    auto f = [](int e) -> Expected<int, std::string>
    {
        if (e == 404)
        {
            return 0;   // Recover
        }
        return makeUnexpected(std::string("unrecoverable"));
    };

    Expected<int, int> ex(42);
    EXPECT_EQ(ex.orElse(f).value(), 42);

    Expected<int, int> err1(makeUnexpected(404));
    EXPECT_EQ(err1.orElse(f).value(), 0);

    Expected<int, int> err2(makeUnexpected(500));
    EXPECT_FALSE(err2.orElse(f).hasValue());
    EXPECT_EQ(err2.orElse(f).error(), "unrecoverable");

    // Test rvalue move - Error path
    LifecycleTracker::reset();
    Expected<int, LifecycleTracker> errLT(makeUnexpected(LifecycleTracker{ 404 }));
    LifecycleTracker::reset();
    auto res = std::move(errLT).orElse([](LifecycleTracker lt) -> Expected<int, std::string> {
        return lt.value;
    });
    EXPECT_EQ(res.value(), 404);
    EXPECT_EQ(LifecycleTracker::moves, 1);

    // Test rvalue move - Success path
    LifecycleTracker::reset();
    Expected<LifecycleTracker, int> exLT(LifecycleTracker{ 42 });
    LifecycleTracker::reset();
    auto resEx = std::move(exLT).orElse([](int e) -> Expected<LifecycleTracker, int> {
        return makeUnexpected(e);
    });
    EXPECT_TRUE(resEx.hasValue());
    EXPECT_EQ(resEx.value().value, 42);
    EXPECT_EQ(LifecycleTracker::moves, 1);
}

TEST(ExpectedMonadicTest, Transform)
{
    auto f = [](int x)
    {
        return std::to_string(x);
    };

    Expected<int, int> ex(42);
    EXPECT_EQ(ex.transform(f).value(), "42");

    Expected<int, int> err(makeUnexpected(404));
    EXPECT_FALSE(err.transform(f).hasValue());
    EXPECT_EQ(err.transform(f).error(), 404);

    // Test rvalue move - Success path
    LifecycleTracker::reset();
    Expected<LifecycleTracker, int> exLT(LifecycleTracker{ 42 });
    LifecycleTracker::reset();
    auto res = std::move(exLT).transform([](LifecycleTracker lt) {
        return lt.value;
    });
    EXPECT_EQ(res.value(), 42);
    EXPECT_EQ(LifecycleTracker::moves, 1);

    // Test rvalue move - Error path
    LifecycleTracker::reset();
    Expected<int, LifecycleTracker> errLT(makeUnexpected(LifecycleTracker{ 404 }));
    LifecycleTracker::reset();
    auto resErr = std::move(errLT).transform([](int x) {
        return x;
    });
    EXPECT_FALSE(resErr.hasValue());
    EXPECT_EQ(resErr.error().value, 404);
    EXPECT_EQ(LifecycleTracker::moves, 2);
}

TEST(ExpectedMonadicTest, TransformError)
{
    auto f = [](int e)
    {
        return std::to_string(e);
    };

    Expected<int, int> ex(42);
    EXPECT_EQ(ex.transformError(f).value(), 42);

    Expected<int, int> err(makeUnexpected(404));
    EXPECT_FALSE(err.transformError(f).hasValue());
    EXPECT_EQ(err.transformError(f).error(), "404");

    // Test rvalue move - Error path
    LifecycleTracker::reset();
    Expected<int, LifecycleTracker> errLT(makeUnexpected(LifecycleTracker{ 404 }));
    LifecycleTracker::reset();
    auto res = std::move(errLT).transformError([](LifecycleTracker lt) {
        return lt.value;
    });
    EXPECT_FALSE(res.hasValue());
    EXPECT_EQ(res.error(), 404);
    EXPECT_EQ(LifecycleTracker::moves, 1);

    // Test rvalue move - Success path
    LifecycleTracker::reset();
    Expected<LifecycleTracker, int> exLT(LifecycleTracker{ 42 });
    LifecycleTracker::reset();
    auto resEx = std::move(exLT).transformError([](int e) {
        return e;
    });
    EXPECT_TRUE(resEx.hasValue());
    EXPECT_EQ(resEx.value().value, 42);
    EXPECT_EQ(LifecycleTracker::moves, 1);
}

TEST(ExpectedTest, MakeExpected)
{
    auto ex = makeExpected<std::string, int>(5, 'x');
    EXPECT_TRUE(ex.hasValue());
    EXPECT_EQ(ex.value(), "xxxxx");
}

}   // namespace gp::tests
