// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/base/system/PathEnums.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::platform::tests
{

static_assert(std::is_same_v<std::underlying_type_t<PathFlags>, gp::UInt8>, "PathFlags must be backed by gp::UInt8.");
static_assert(
    std::is_same_v<std::underlying_type_t<PathValidationError>, gp::UInt8>,
    "PathValidationError must be backed by gp::UInt8."
);
static_assert(gp::concepts::IsBitwiseEnum<PathFlags>, "PathFlags must satisfy IsBitwiseEnum.");

static_assert(
    gp::enums::hasAllFlags(PathFlags::IncludeDot | PathFlags::AllowCompoundExtension, PathFlags::IncludeDot),
    "Bitwise OR and hasAllFlags must work at compile time."
);
static_assert(gp::enums::hasNoFlags(PathFlags::None, PathFlags::IncludeDot), "PathFlags::None must have no flags set.");

TEST(PathEnumsTest, PathFlagsBitwiseOperations)
{
    PathFlags flags = PathFlags::None;
    EXPECT_FALSE(gp::enums::hasAnyFlags(flags));

    flags |= PathFlags::IncludeDot;
    EXPECT_TRUE(gp::enums::hasAnyFlags(flags));
    EXPECT_TRUE(gp::enums::hasAllFlags(flags, PathFlags::IncludeDot));
    EXPECT_TRUE(gp::enums::hasNoFlags(flags, PathFlags::AllowCompoundExtension));

    flags = gp::enums::setFlags(flags, PathFlags::AllowCompoundExtension);
    EXPECT_TRUE(gp::enums::hasAllFlags(flags, PathFlags::IncludeDot | PathFlags::AllowCompoundExtension));

    flags = gp::enums::clearFlags(flags, PathFlags::IncludeDot);
    EXPECT_FALSE(gp::enums::hasAllFlags(flags, PathFlags::IncludeDot));
    EXPECT_TRUE(gp::enums::hasAllFlags(flags, PathFlags::AllowCompoundExtension));

    flags = gp::enums::toggleFlags(flags, PathFlags::AllowCompoundExtension);
    EXPECT_EQ(flags, PathFlags::None);
}

TEST(PathEnumsTest, PathValidationErrorValuesAreDistinct)
{
    EXPECT_EQ(static_cast<gp::UInt8>(PathValidationError::None), 0u);
    EXPECT_EQ(static_cast<gp::UInt8>(PathValidationError::EmptyPath), 1u);
    EXPECT_EQ(static_cast<gp::UInt8>(PathValidationError::ExceedsMaxLength), 2u);
    EXPECT_EQ(static_cast<gp::UInt8>(PathValidationError::InvalidCharacter), 3u);
    EXPECT_EQ(static_cast<gp::UInt8>(PathValidationError::ReservedDeviceName), 4u);
    EXPECT_EQ(static_cast<gp::UInt8>(PathValidationError::TrailingDotOrSpace), 5u);
    EXPECT_EQ(static_cast<gp::UInt8>(PathValidationError::UnrootedDriveSpecifier), 6u);
}

}   // namespace gp::platform::tests
