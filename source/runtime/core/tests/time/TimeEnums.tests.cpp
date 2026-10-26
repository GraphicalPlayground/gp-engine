// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "time/TimeEnums.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::time::tests
{

// Compile-time static assertions on enumeration contracts
static_assert(std::is_scoped_enum_v<ClockDomain>, "ClockDomain must be an enum class.");
static_assert(
    std::is_same_v<std::underlying_type_t<ClockDomain>, gp::UInt8>, "ClockDomain underlying type must be gp::UInt8."
);
static_assert(sizeof(ClockDomain) == 1u, "ClockDomain must be 1 byte in size.");
static_assert(static_cast<gp::UInt8>(ClockDomain::RealTime) == 0u, "RealTime domain must equal 0.");
static_assert(static_cast<gp::UInt8>(ClockDomain::UI) == 1u, "UI domain must equal 1.");
static_assert(static_cast<gp::UInt8>(ClockDomain::Gameplay) == 2u, "Gameplay domain must equal 2.");
static_assert(static_cast<gp::UInt8>(ClockDomain::Physics) == 3u, "Physics domain must equal 3.");
static_assert(static_cast<gp::UInt8>(ClockDomain::Animation) == 4u, "Animation domain must equal 4.");
static_assert(static_cast<gp::UInt8>(ClockDomain::Audio) == 5u, "Audio domain must equal 5.");
static_assert(static_cast<gp::UInt8>(ClockDomain::Count) == 6u, "ClockDomain count must equal 6.");

static_assert(std::is_scoped_enum_v<DeltaSmoothingMode>, "DeltaSmoothingMode must be an enum class.");
static_assert(
    std::is_same_v<std::underlying_type_t<DeltaSmoothingMode>, gp::UInt8>,
    "DeltaSmoothingMode underlying type must be gp::UInt8."
);
static_assert(sizeof(DeltaSmoothingMode) == 1u, "DeltaSmoothingMode must be 1 byte in size.");
static_assert(static_cast<gp::UInt8>(DeltaSmoothingMode::None) == 0u, "None mode must equal 0.");
static_assert(
    static_cast<gp::UInt8>(DeltaSmoothingMode::ExponentialMovingAverage) == 1u,
    "ExponentialMovingAverage mode must equal 1."
);
static_assert(
    static_cast<gp::UInt8>(DeltaSmoothingMode::OutlierRejectedAverage) == 2u,
    "OutlierRejectedAverage mode must equal 2."
);
static_assert(
    static_cast<gp::UInt8>(DeltaSmoothingMode::FixedDeterministic) == 3u, "FixedDeterministic mode must equal 3."
);

static_assert(std::is_scoped_enum_v<FramePacingMode>, "FramePacingMode must be an enum class.");
static_assert(
    std::is_same_v<std::underlying_type_t<FramePacingMode>, gp::UInt8>,
    "FramePacingMode underlying type must be gp::UInt8."
);
static_assert(sizeof(FramePacingMode) == 1u, "FramePacingMode must be 1 byte in size.");
static_assert(static_cast<gp::UInt8>(FramePacingMode::Uncapped) == 0u, "Uncapped mode must equal 0.");
static_assert(
    static_cast<gp::UInt8>(FramePacingMode::HybridSleepAndSpin) == 1u, "HybridSleepAndSpin mode must equal 1."
);
static_assert(static_cast<gp::UInt8>(FramePacingMode::SleepOnly) == 2u, "SleepOnly mode must equal 2.");
static_assert(static_cast<gp::UInt8>(FramePacingMode::SpinOnly) == 3u, "SpinOnly mode must equal 3.");

TEST(TimeEnumsTest, ClockDomainValuesAndCount)
{
    EXPECT_EQ(static_cast<gp::UInt8>(ClockDomain::RealTime), 0u);
    EXPECT_EQ(static_cast<gp::UInt8>(ClockDomain::UI), 1u);
    EXPECT_EQ(static_cast<gp::UInt8>(ClockDomain::Gameplay), 2u);
    EXPECT_EQ(static_cast<gp::UInt8>(ClockDomain::Physics), 3u);
    EXPECT_EQ(static_cast<gp::UInt8>(ClockDomain::Animation), 4u);
    EXPECT_EQ(static_cast<gp::UInt8>(ClockDomain::Audio), 5u);
    EXPECT_EQ(static_cast<gp::UInt8>(ClockDomain::Count), 6u);
}

TEST(TimeEnumsTest, DeltaSmoothingModeValues)
{
    EXPECT_EQ(static_cast<gp::UInt8>(DeltaSmoothingMode::None), 0u);
    EXPECT_EQ(static_cast<gp::UInt8>(DeltaSmoothingMode::ExponentialMovingAverage), 1u);
    EXPECT_EQ(static_cast<gp::UInt8>(DeltaSmoothingMode::OutlierRejectedAverage), 2u);
    EXPECT_EQ(static_cast<gp::UInt8>(DeltaSmoothingMode::FixedDeterministic), 3u);
}

TEST(TimeEnumsTest, FramePacingModeValues)
{
    EXPECT_EQ(static_cast<gp::UInt8>(FramePacingMode::Uncapped), 0u);
    EXPECT_EQ(static_cast<gp::UInt8>(FramePacingMode::HybridSleepAndSpin), 1u);
    EXPECT_EQ(static_cast<gp::UInt8>(FramePacingMode::SleepOnly), 2u);
    EXPECT_EQ(static_cast<gp::UInt8>(FramePacingMode::SpinOnly), 3u);
}

}   // namespace gp::time::tests
