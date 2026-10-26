// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "hardware/GenericHardwareInfo.hpp"
#include <gtest/gtest.h>

namespace gp::hal::generic::tests
{

TEST(GenericHardwareInfoTest, DefaultQueries)
{
    HardwareInfo hwInfo;

    EXPECT_FALSE(hwInfo.initialize());
    (void)hwInfo.getCPUInfo();
    (void)hwInfo.getGPUInfo();
    (void)hwInfo.getOSInfo();

    EXPECT_FALSE(hwInfo.getMemoryStatus().has_value());
    EXPECT_FALSE(hwInfo.getDiskStatus("C:/").has_value());
    EXPECT_FALSE(hwInfo.getPowerState().has_value());
}

}   // namespace gp::hal::generic::tests
