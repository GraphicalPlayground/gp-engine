// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "display/DisplayTypes.hpp"
#include <gtest/gtest.h>
#include <type_traits>   // IWYU pragma: keep

namespace gp::hal::tests
{

static_assert(kInvalidDisplayId == 0u, "kInvalidDisplayId must be 0.");

constexpr bool testConstexprPoint2D()
{
    Point2D p1;
    if (p1.x != 0 || p1.y != 0)
    {
        return false;
    }

    Point2D p2(10, 20);
    Point2D p3 = -p2;
    if (p3.x != -10 || p3.y != -20)
    {
        return false;
    }

    Point2D p4 = p2 + p2;
    if (p4.x != 20 || p4.y != 40)
    {
        return false;
    }

    Point2D p5 = p4 - p2;
    if (p5 != p2)
    {
        return false;
    }

    return true;
}

static_assert(testConstexprPoint2D(), "Point2D constexpr tests failed.");

constexpr bool testConstexprExtent2D()
{
    Extent2D e1;
    if (e1.width != 0u || e1.height != 0u)
    {
        return false;
    }

    Extent2D e2(1920u, 1080u);
    if (e2.width != 1920u || e2.height != 1080u)
    {
        return false;
    }

    Extent2D e3(e2);
    if (e3 != e2)
    {
        return false;
    }

    return true;
}

static_assert(testConstexprExtent2D(), "Extent2D constexpr tests failed.");

constexpr bool testConstexprDisplayRect()
{
    DisplayRect rect(Point2D(100, 100), Extent2D(800u, 600u));
    if (!rect.contains(Point2D(100, 100)))
    {
        return false;
    }
    if (!rect.contains(Point2D(899, 699)))
    {
        return false;
    }
    if (rect.contains(Point2D(900, 700)))
    {
        return false;
    }
    if (rect.contains(Point2D(99, 100)))
    {
        return false;
    }
    return true;
}

static_assert(testConstexprDisplayRect(), "DisplayRect constexpr tests failed.");

TEST(Point2DTest, Constructors)
{
    const Point2D defaultPoint;
    EXPECT_EQ(defaultPoint.x, 0);
    EXPECT_EQ(defaultPoint.y, 0);

    const Point2D coordPoint(15, -25);
    EXPECT_EQ(coordPoint.x, 15);
    EXPECT_EQ(coordPoint.y, -25);

    const Int32 rawArray[2] = { 42, 99 };
    const Int32* rawPtr = rawArray;
    const Point2D arrayPointerPoint(rawPtr);
    EXPECT_EQ(arrayPointerPoint.x, 42);
    EXPECT_EQ(arrayPointerPoint.y, 99);
}

TEST(Point2DTest, Operators)
{
    const Point2D a(10, 20);
    const Point2D b(5, 15);

    EXPECT_TRUE(a == Point2D(10, 20));
    EXPECT_TRUE(a != b);

    const Point2D negA = -a;
    EXPECT_EQ(negA.x, -10);
    EXPECT_EQ(negA.y, -20);

    const Point2D sum = a + b;
    EXPECT_EQ(sum.x, 15);
    EXPECT_EQ(sum.y, 35);

    const Point2D diff = a - b;
    EXPECT_EQ(diff.x, 5);
    EXPECT_EQ(diff.y, 5);
}

TEST(Extent2DTest, ConstructorsAndEquality)
{
    const Extent2D defaultExtent;
    EXPECT_EQ(defaultExtent.width, 0u);
    EXPECT_EQ(defaultExtent.height, 0u);

    const Extent2D explicitExtent(1280u, 720u);
    EXPECT_EQ(explicitExtent.width, 1280u);
    EXPECT_EQ(explicitExtent.height, 720u);

    const UInt32 rawArray[2] = { 3840u, 2160u };
    const UInt32* rawPtr = rawArray;
    const Extent2D arrayPointerExtent(rawPtr);
    EXPECT_EQ(arrayPointerExtent.width, 3840u);
    EXPECT_EQ(arrayPointerExtent.height, 2160u);

    EXPECT_TRUE(explicitExtent == Extent2D(1280u, 720u));
    EXPECT_TRUE(explicitExtent != arrayPointerExtent);
}

TEST(DisplayRectTest, ConstructorsAndEquality)
{
    const DisplayRect defaultRect;
    EXPECT_EQ(defaultRect.origin.x, 0);
    EXPECT_EQ(defaultRect.origin.y, 0);
    EXPECT_EQ(defaultRect.extent.width, 0u);
    EXPECT_EQ(defaultRect.extent.height, 0u);

    const DisplayRect rectFromParts(Point2D(50, 100), Extent2D(1920u, 1080u));
    EXPECT_EQ(rectFromParts.origin.x, 50);
    EXPECT_EQ(rectFromParts.origin.y, 100);
    EXPECT_EQ(rectFromParts.extent.width, 1920u);
    EXPECT_EQ(rectFromParts.extent.height, 1080u);

    const DisplayRect rectFromScalars(50, 100, 1920u, 1080u);
    EXPECT_TRUE(rectFromParts == rectFromScalars);
    EXPECT_FALSE(rectFromParts != rectFromScalars);
}

TEST(DisplayRectTest, ContainsPoint)
{
    const DisplayRect rect(100, 200, 400u, 300u);

    // Top-left boundary is inclusive
    EXPECT_TRUE(rect.contains(Point2D(100, 200)));

    // Interior points
    EXPECT_TRUE(rect.contains(Point2D(250, 350)));

    // Bottom-right edge is exclusive
    EXPECT_TRUE(rect.contains(Point2D(499, 499)));
    EXPECT_FALSE(rect.contains(Point2D(500, 500)));
    EXPECT_FALSE(rect.contains(Point2D(500, 350)));
    EXPECT_FALSE(rect.contains(Point2D(250, 500)));

    // Points outside other borders
    EXPECT_FALSE(rect.contains(Point2D(99, 200)));
    EXPECT_FALSE(rect.contains(Point2D(100, 199)));
}

TEST(DisplayModeTest, ConstructorsAndEquality)
{
    const DisplayMode defaultMode;
    EXPECT_EQ(defaultMode.resolution.width, 0u);
    EXPECT_EQ(defaultMode.resolution.height, 0u);
    EXPECT_FLOAT_EQ(defaultMode.refreshRate, 0.0f);
    EXPECT_FLOAT_EQ(defaultMode.pixelDensity, 1.0f);
    EXPECT_EQ(defaultMode.pixelFormat, DisplayPixelFormat::Unknown);

    const DisplayMode mode60(Extent2D(1920u, 1080u), 60.0f, 1.0f, DisplayPixelFormat::RGBA8888);
    const DisplayMode mode144(Extent2D(1920u, 1080u), 144.0f, 1.0f, DisplayPixelFormat::RGBA8888);
    const DisplayMode duplicate60(Extent2D(1920u, 1080u), 60.0f, 1.0f, DisplayPixelFormat::RGBA8888);

    EXPECT_TRUE(mode60 == duplicate60);
    EXPECT_TRUE(mode60 != mode144);
}

TEST(DisplayHDRMetadataTest, DefaultsAndCustomValues)
{
    const DisplayHDRMetadata defaultHdr;
    EXPECT_FLOAT_EQ(defaultHdr.sdrWhiteLevelNits, 80.0f);
    EXPECT_FLOAT_EQ(defaultHdr.hdrHeadroom, 1.0f);

    const DisplayHDRMetadata customHdr(200.0f, 4.5f);
    EXPECT_FLOAT_EQ(customHdr.sdrWhiteLevelNits, 200.0f);
    EXPECT_FLOAT_EQ(customHdr.hdrHeadroom, 4.5f);
}

TEST(DisplayDescTest, DefaultState)
{
    const DisplayDesc desc;
    EXPECT_EQ(desc.id, kInvalidDisplayId);
    EXPECT_EQ(desc.name, "Generic Display");
    EXPECT_EQ(desc.bounds.origin, Point2D(0, 0));
    EXPECT_EQ(desc.bounds.extent, Extent2D(0u, 0u));
}

}   // namespace gp::hal::tests
