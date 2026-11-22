// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/DeviceEnums.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::tests
{

static_assert(convertToGpuVendorId(0x00000000u) == GpuVendorId::NotQueried, "Zero vendor ID must map to NotQueried.");
static_assert(convertToGpuVendorId(0x10DEu) == GpuVendorId::Nvidia, "0x10DE must map to Nvidia.");
static_assert(convertToGpuVendorId(0x1002u) == GpuVendorId::Amd, "0x1002 must map to Amd.");
static_assert(convertToGpuVendorId(0x8086u) == GpuVendorId::Intel, "0x8086 must map to Intel.");
static_assert(convertToGpuVendorId(0xFFFFFFFFu) == GpuVendorId::Unknown, "0xFFFFFFFF must map to Unknown.");
static_assert(convertToGpuVendorId(0x12345678u) == GpuVendorId::Unknown, "Unrecognized ID must map to Unknown.");

TEST(DeviceEnumsTest, ConvertToGpuVendorIdRecognizesAllKnownVendors)
{
    EXPECT_EQ(convertToGpuVendorId(0x00000000u), GpuVendorId::NotQueried);
    EXPECT_EQ(convertToGpuVendorId(0x1002u), GpuVendorId::Amd);
    EXPECT_EQ(convertToGpuVendorId(0x1010u), GpuVendorId::ImgTec);
    EXPECT_EQ(convertToGpuVendorId(0x10DEu), GpuVendorId::Nvidia);
    EXPECT_EQ(convertToGpuVendorId(0x13B5u), GpuVendorId::Arm);
    EXPECT_EQ(convertToGpuVendorId(0x14E4u), GpuVendorId::Broadcom);
    EXPECT_EQ(convertToGpuVendorId(0x5143u), GpuVendorId::Qualcomm);
    EXPECT_EQ(convertToGpuVendorId(0x8086u), GpuVendorId::Intel);
    EXPECT_EQ(convertToGpuVendorId(0x106Bu), GpuVendorId::Apple);
    EXPECT_EQ(convertToGpuVendorId(0x7A05u), GpuVendorId::Vivante);
    EXPECT_EQ(convertToGpuVendorId(0x1EB1u), GpuVendorId::VeriSilicon);
    EXPECT_EQ(convertToGpuVendorId(0x144Du), GpuVendorId::SamsungAMD);
    EXPECT_EQ(convertToGpuVendorId(0x1414u), GpuVendorId::Microsoft);
    EXPECT_EQ(convertToGpuVendorId(0x10003u), GpuVendorId::Kazan);
    EXPECT_EQ(convertToGpuVendorId(0x10004u), GpuVendorId::Codeplay);
    EXPECT_EQ(convertToGpuVendorId(0x10005u), GpuVendorId::Mesa);
}

TEST(DeviceEnumsTest, ConvertToGpuVendorIdMapsUnknownValuesToUnknown)
{
    EXPECT_EQ(convertToGpuVendorId(0xFFFFFFFFu), GpuVendorId::Unknown);
    EXPECT_EQ(convertToGpuVendorId(0x00000001u), GpuVendorId::Unknown);
    EXPECT_EQ(convertToGpuVendorId(0xCAFEBABEu), GpuVendorId::Unknown);
}

}   // namespace gp::rhi::tests
