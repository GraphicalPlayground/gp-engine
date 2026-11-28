// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "views/DescriptorHandle.hpp"
#include "hashing/HashOf.hpp"
#include "views/ViewEnums.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::view::tests
{

static_assert(!DescriptorHandle{}.isValid(), "Default DescriptorHandle must be invalid.");
static_assert(
    DescriptorHandle{}.getIndex() == DescriptorHandle::kInvalidIndex,
    "Default DescriptorHandle index must equal kInvalidIndex."
);
static_assert(
    DescriptorHandle{}.getType() == DescriptorType::Invalid, "Default DescriptorHandle type must be Invalid."
);
static_assert(
    DescriptorHandle{}.getTypeMask() == DescriptorTypeMask::None, "Default DescriptorHandle type mask must be None."
);
static_assert(DescriptorHandle(DescriptorType::Sampler, 0u).isValid(), "Sampler descriptor at slot 0 must be valid.");
static_assert(
    !DescriptorHandle(DescriptorType::Invalid, 0u).isValid(), "Invalid type with valid index must remain invalid."
);
static_assert(
    !DescriptorHandle(DescriptorType::Sampler, DescriptorHandle::kInvalidIndex).isValid(),
    "Valid type with kInvalidIndex must remain invalid."
);

TEST(DescriptorHandleTest, ValidityTypeMaskEqualityAndHashing)
{
    const DescriptorHandle invalid{};
    const DescriptorHandle samplerA(DescriptorType::Sampler, 12u);
    const DescriptorHandle samplerB(DescriptorType::Sampler, 12u);
    const DescriptorHandle samplerC(DescriptorType::Sampler, 13u);
    const DescriptorHandle textureSrv(DescriptorType::TextureSRV, 12u);

    EXPECT_FALSE(invalid.isValid());
    EXPECT_TRUE(samplerA.isValid());
    EXPECT_EQ(samplerA.getIndex(), 12u);
    EXPECT_EQ(samplerA.getType(), DescriptorType::Sampler);
    EXPECT_EQ(samplerA.getTypeMask(), DescriptorTypeMask::Sampler);
    EXPECT_EQ(textureSrv.getTypeMask(), DescriptorTypeMask::TextureSRV);

    EXPECT_EQ(samplerA, samplerB);
    EXPECT_EQ(gp::hashing::hashOf(samplerA), gp::hashing::hashOf(samplerB));

    EXPECT_NE(samplerA, samplerC);
    EXPECT_NE(samplerA, textureSrv);
    EXPECT_NE(gp::hashing::hashOf(samplerA), gp::hashing::hashOf(samplerC));
    EXPECT_NE(gp::hashing::hashOf(samplerA), gp::hashing::hashOf(textureSrv));
}

}   // namespace gp::rhi::view::tests
