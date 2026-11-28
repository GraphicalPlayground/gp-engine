// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "views/ViewEnums.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::view::tests
{

static_assert(isBindlessDisabled(BindlessConfiguration::Disabled), "Disabled must report disabled.");
static_assert(
    isBindlessEnabledForOnlyRayTracing(BindlessConfiguration::RayTracing), "RayTracing must report ray-tracing-only."
);
static_assert(
    isBindlessEnabledForRayTracing(BindlessConfiguration::Minimal), "Minimal must include ray tracing bindless."
);
static_assert(
    isBindlessEnabledForAnyGraphics(BindlessConfiguration::Minimal), "Minimal must enable graphics bindless."
);
static_assert(isBindlessFullyEnabled(BindlessConfiguration::All), "All must report fully enabled.");

static_assert(
    descriptorTypeMaskFromType(DescriptorType::BufferSRV) == DescriptorTypeMask::BufferSRV,
    "BufferSRV mask conversion failed."
);
static_assert(
    descriptorTypeMaskFromType(DescriptorType::Invalid) == DescriptorTypeMask::None,
    "Invalid descriptor type must map to DescriptorTypeMask::None."
);

TEST(ViewEnumsTest, BindlessConfigurationDisabledTier)
{
    const BindlessConfiguration config = BindlessConfiguration::Disabled;

    EXPECT_TRUE(isBindlessDisabled(config));
    EXPECT_FALSE(isBindlessEnabledForOnlyRayTracing(config));
    EXPECT_FALSE(isBindlessEnabledForRayTracing(config));
    EXPECT_FALSE(isBindlessEnabledForAnyGraphics(config));
    EXPECT_FALSE(isBindlessFullyEnabled(config));
}

TEST(ViewEnumsTest, BindlessConfigurationRayTracingTier)
{
    const BindlessConfiguration config = BindlessConfiguration::RayTracing;

    EXPECT_FALSE(isBindlessDisabled(config));
    EXPECT_TRUE(isBindlessEnabledForOnlyRayTracing(config));
    EXPECT_TRUE(isBindlessEnabledForRayTracing(config));
    EXPECT_FALSE(isBindlessEnabledForAnyGraphics(config));
    EXPECT_FALSE(isBindlessFullyEnabled(config));
}

TEST(ViewEnumsTest, BindlessConfigurationMinimalTier)
{
    const BindlessConfiguration config = BindlessConfiguration::Minimal;

    EXPECT_FALSE(isBindlessDisabled(config));
    EXPECT_FALSE(isBindlessEnabledForOnlyRayTracing(config));
    EXPECT_TRUE(isBindlessEnabledForRayTracing(config));
    EXPECT_TRUE(isBindlessEnabledForAnyGraphics(config));
    EXPECT_FALSE(isBindlessFullyEnabled(config));
}

TEST(ViewEnumsTest, BindlessConfigurationAllTier)
{
    const BindlessConfiguration config = BindlessConfiguration::All;

    EXPECT_FALSE(isBindlessDisabled(config));
    EXPECT_FALSE(isBindlessEnabledForOnlyRayTracing(config));
    EXPECT_TRUE(isBindlessEnabledForRayTracing(config));
    EXPECT_TRUE(isBindlessEnabledForAnyGraphics(config));
    EXPECT_TRUE(isBindlessFullyEnabled(config));
}

TEST(ViewEnumsTest, BindlessConfigurationOrderingComparisons)
{
    EXPECT_LT(BindlessConfiguration::Disabled, BindlessConfiguration::RayTracing);
    EXPECT_LT(BindlessConfiguration::RayTracing, BindlessConfiguration::Minimal);
    EXPECT_LT(BindlessConfiguration::Minimal, BindlessConfiguration::All);
    EXPECT_GE(BindlessConfiguration::All, BindlessConfiguration::Minimal);
}

TEST(ViewEnumsTest, DescriptorTypeMaskFromTypeMapsEveryEnumerator)
{
    EXPECT_EQ(descriptorTypeMaskFromType(DescriptorType::BufferSRV), DescriptorTypeMask::BufferSRV);
    EXPECT_EQ(descriptorTypeMaskFromType(DescriptorType::BufferUAV), DescriptorTypeMask::BufferUAV);
    EXPECT_EQ(descriptorTypeMaskFromType(DescriptorType::TypedBufferSRV), DescriptorTypeMask::TypedBufferSRV);
    EXPECT_EQ(descriptorTypeMaskFromType(DescriptorType::TypedBufferUAV), DescriptorTypeMask::TypedBufferUAV);
    EXPECT_EQ(descriptorTypeMaskFromType(DescriptorType::TextureSRV), DescriptorTypeMask::TextureSRV);
    EXPECT_EQ(descriptorTypeMaskFromType(DescriptorType::TextureUAV), DescriptorTypeMask::TextureUAV);
    EXPECT_EQ(descriptorTypeMaskFromType(DescriptorType::CBV), DescriptorTypeMask::CBV);
    EXPECT_EQ(descriptorTypeMaskFromType(DescriptorType::Sampler), DescriptorTypeMask::Sampler);
    EXPECT_EQ(
        descriptorTypeMaskFromType(DescriptorType::AccelerationStructure), DescriptorTypeMask::AccelerationStructure
    );
    EXPECT_EQ(descriptorTypeMaskFromType(DescriptorType::Invalid), DescriptorTypeMask::None);
}

TEST(ViewEnumsTest, CompositeDescriptorTypeMasksMatchIndividualBits)
{
    const DescriptorTypeMask srvBits = descriptorTypeMaskFromType(DescriptorType::BufferSRV) |
                                       descriptorTypeMaskFromType(DescriptorType::TypedBufferSRV) |
                                       descriptorTypeMaskFromType(DescriptorType::TextureSRV);
    const DescriptorTypeMask uavBits = descriptorTypeMaskFromType(DescriptorType::BufferUAV) |
                                       descriptorTypeMaskFromType(DescriptorType::TypedBufferUAV) |
                                       descriptorTypeMaskFromType(DescriptorType::TextureUAV);
    const DescriptorTypeMask standardBits = srvBits | uavBits | descriptorTypeMaskFromType(DescriptorType::CBV) |
                                            descriptorTypeMaskFromType(DescriptorType::AccelerationStructure);

    EXPECT_EQ(DescriptorTypeMask::SRV, srvBits);
    EXPECT_EQ(DescriptorTypeMask::UAV, uavBits);
    EXPECT_EQ(DescriptorTypeMask::Standard, standardBits);
    EXPECT_EQ(DescriptorTypeMask::Standard & DescriptorTypeMask::Sampler, DescriptorTypeMask::None);
}

}   // namespace gp::rhi::view::tests
