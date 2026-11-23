// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "sync/TransitionDesc.hpp"
#include "sync/SyncEnums.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::sync::tests
{

static_assert(SubresourceRange{}.isWholeResource(), "Default SubresourceRange must cover the whole resource.");
static_assert(SubresourceRange{}.isAllMips(), "Default SubresourceRange must cover all mips.");
static_assert(SubresourceRange{}.isAllArraySlices(), "Default SubresourceRange must cover all array slices.");
static_assert(SubresourceRange{}.isAllPlaneSlices(), "Default SubresourceRange must cover all plane slices.");
static_assert(!SubresourceRange{}.isZeroIndexResource(), "Default SubresourceRange must not be zero-indexed.");
static_assert(SubresourceRange(0u, 0u, 0u).isZeroIndexResource(), "(0, 0, 0) must report isZeroIndexResource.");
static_assert(
    SubresourceRange(0u, 0u, SubresourceRange::kDepthPlaneSlice).ignoreStencilPlane(),
    "Depth plane slice must report ignoreStencilPlane."
);
static_assert(
    SubresourceRange(0u, 0u, SubresourceRange::kStencilPlaneSlice).ignoreDepthPlane(),
    "Stencil plane slice must report ignoreDepthPlane."
);
static_assert(CommitResourceInfo(4096u).sizeInBytes == 4096u, "CommitResourceInfo must store byte size.");

TEST(TransitionDescTest, SubresourceRangeQueriesAndEquality)
{
    const SubresourceRange whole{};
    const SubresourceRange mip2Slice3(2u, 3u, SubresourceRange::kAllSubresources);
    const SubresourceRange depthOnly(0u, 0u, SubresourceRange::kDepthPlaneSlice);
    const SubresourceRange stencilOnly(0u, 0u, SubresourceRange::kStencilPlaneSlice);

    EXPECT_TRUE(whole.isWholeResource());
    EXPECT_FALSE(mip2Slice3.isWholeResource());
    EXPECT_FALSE(mip2Slice3.isAllMips());
    EXPECT_FALSE(mip2Slice3.isAllArraySlices());
    EXPECT_TRUE(mip2Slice3.isAllPlaneSlices());

    EXPECT_TRUE(depthOnly.isZeroIndexResource());
    EXPECT_TRUE(depthOnly.ignoreStencilPlane());
    EXPECT_FALSE(depthOnly.ignoreDepthPlane());

    EXPECT_FALSE(stencilOnly.isZeroIndexResource());
    EXPECT_TRUE(stencilOnly.ignoreDepthPlane());
    EXPECT_FALSE(stencilOnly.ignoreStencilPlane());

    EXPECT_EQ(mip2Slice3, SubresourceRange(2u, 3u, SubresourceRange::kAllSubresources));
    EXPECT_NE(depthOnly, stencilOnly);
}

TEST(TransitionDescTest, TransitionInfoConstructorsAndEquality)
{
    resource::Texture* const fakeTexA = reinterpret_cast<resource::Texture*>(static_cast<gp::UIntPtr>(0x1000u));
    resource::Texture* const fakeTexB = reinterpret_cast<resource::Texture*>(static_cast<gp::UIntPtr>(0x2000u));
    resource::Buffer* const fakeBuf = reinterpret_cast<resource::Buffer*>(static_cast<gp::UIntPtr>(0x3000u));
    view::UnorderedAccessView* const fakeUav =
        reinterpret_cast<view::UnorderedAccessView*>(static_cast<gp::UIntPtr>(0x4000u));
    resource::RayTracingAccelerationStructure* const fakeBvh =
        reinterpret_cast<resource::RayTracingAccelerationStructure*>(static_cast<gp::UIntPtr>(0x5000u));

    const TransitionInfo defaultInfo{};
    EXPECT_EQ(defaultInfo.type, TransitionResourceType::Unknown);
    EXPECT_EQ(defaultInfo.resource, nullptr);
    EXPECT_EQ(defaultInfo.accessBefore, ResourceAccess::Unknown);
    EXPECT_EQ(defaultInfo.accessAfter, ResourceAccess::Unknown);
    EXPECT_FALSE(defaultInfo.commitInfo.hasValue());

    const TransitionInfo texInfo(
        fakeTexA,
        ResourceAccess::RTV,
        ResourceAccess::SRVGraphics,
        ResourceTransitionFlags::MaintainCompression,
        1u,
        2u,
        SubresourceRange::kDepthPlaneSlice
    );
    EXPECT_EQ(texInfo.type, TransitionResourceType::Texture);
    EXPECT_EQ(texInfo.texture, fakeTexA);
    EXPECT_EQ(texInfo.accessBefore, ResourceAccess::RTV);
    EXPECT_EQ(texInfo.accessAfter, ResourceAccess::SRVGraphics);
    EXPECT_EQ(texInfo.flags, ResourceTransitionFlags::MaintainCompression);
    EXPECT_EQ(texInfo.mipIndex, 1u);
    EXPECT_EQ(texInfo.arraySlice, 2u);
    EXPECT_EQ(texInfo.planeSlice, SubresourceRange::kDepthPlaneSlice);

    const TransitionInfo texShort(fakeTexB, ResourceAccess::CopySrc);
    EXPECT_EQ(texShort.type, TransitionResourceType::Texture);
    EXPECT_EQ(texShort.accessBefore, ResourceAccess::Unknown);
    EXPECT_EQ(texShort.accessAfter, ResourceAccess::CopySrc);
    EXPECT_TRUE(texShort.isWholeResource());

    const TransitionInfo bufCommit(
        fakeBuf, ResourceAccess::CopyDest, ResourceAccess::SRVCompute, CommitResourceInfo(65536u)
    );
    EXPECT_EQ(bufCommit.type, TransitionResourceType::Buffer);
    EXPECT_EQ(bufCommit.buffer, fakeBuf);
    ASSERT_TRUE(bufCommit.commitInfo.hasValue());
    EXPECT_EQ(bufCommit.commitInfo.value().sizeInBytes, 65536u);

    const TransitionInfo bufNoCommit(fakeBuf, ResourceAccess::CopyDest, ResourceAccess::SRVCompute);
    EXPECT_NE(bufCommit, bufNoCommit);

    const TransitionInfo uavInfo(fakeUav, ResourceAccess::UAVCompute, ResourceAccess::SRVCompute);
    EXPECT_EQ(uavInfo.type, TransitionResourceType::UAV);
    EXPECT_EQ(uavInfo.uav, fakeUav);

    const TransitionInfo bvhInfo(fakeBvh, ResourceAccess::BVHWrite, ResourceAccess::BVHRead);
    EXPECT_EQ(bvhInfo.type, TransitionResourceType::BVH);
    EXPECT_EQ(bvhInfo.bvh, fakeBvh);
}

TEST(TransitionDescTest, TransientAliasingAndCreateInfoEquality)
{
    resource::Texture* const fakeTex = reinterpret_cast<resource::Texture*>(static_cast<gp::UIntPtr>(0x1000u));
    resource::Buffer* const fakeBuf = reinterpret_cast<resource::Buffer*>(static_cast<gp::UIntPtr>(0x2000u));

    const TransientAliasingOverlap overlapList[2] = { TransientAliasingOverlap(fakeTex),
                                                      TransientAliasingOverlap(fakeBuf) };
    EXPECT_TRUE(overlapList[0].isTexture());
    EXPECT_FALSE(overlapList[0].isBuffer());
    EXPECT_TRUE(overlapList[1].isBuffer());
    EXPECT_NE(overlapList[0], overlapList[1]);

    const TransientAliasingInfo acquireTex = TransientAliasingInfo::acquire(fakeTex, overlapList);
    const TransientAliasingInfo discardBuf = TransientAliasingInfo::discard(fakeBuf);

    EXPECT_TRUE(acquireTex.isAcquire());
    EXPECT_FALSE(acquireTex.isDiscard());
    EXPECT_TRUE(acquireTex.isTexture());
    EXPECT_EQ(acquireTex.overlaps.size(), 2u);

    EXPECT_TRUE(discardBuf.isDiscard());
    EXPECT_FALSE(discardBuf.isAcquire());
    EXPECT_TRUE(discardBuf.isBuffer());
    EXPECT_EQ(discardBuf.overlaps.size(), 0u);

    const TransitionInfo transitionsA[2] = {
        TransitionInfo(fakeTex, ResourceAccess::RTV, ResourceAccess::SRVGraphics),
        TransitionInfo(fakeBuf, ResourceAccess::UAVCompute, ResourceAccess::IndirectArgs)
    };
    const TransitionInfo transitionsB[1] = {
        TransitionInfo(fakeTex, ResourceAccess::RTV, ResourceAccess::SRVGraphics)
    };
    const TransientAliasingInfo aliasInfos[2] = { acquireTex, discardBuf };

    const TransitionCreateInfo createA(
        PipelineStage::Graphics,
        PipelineStage::AsyncCompute,
        TransitionCreateFlags::AllowDuringRenderPass,
        transitionsA,
        aliasInfos
    );
    const TransitionCreateInfo createACopy(
        PipelineStage::Graphics,
        PipelineStage::AsyncCompute,
        TransitionCreateFlags::AllowDuringRenderPass,
        transitionsA,
        aliasInfos
    );
    const TransitionCreateInfo createDifferentLength(
        PipelineStage::Graphics,
        PipelineStage::AsyncCompute,
        TransitionCreateFlags::AllowDuringRenderPass,
        transitionsB,
        aliasInfos
    );

    EXPECT_EQ(createA, createACopy);
    EXPECT_NE(createA, createDifferentLength);
}

TEST(TransitionDescTest, TrackedAccessAndTrackedAccessInfo)
{
    resource::ViewableResource* const fakeViewable =
        reinterpret_cast<resource::ViewableResource*>(static_cast<gp::UIntPtr>(0x9000u));

    const TrackedAccess defaultTracked{};
    EXPECT_EQ(defaultTracked.access, ResourceAccess::Unknown);
    EXPECT_EQ(defaultTracked.pipelines, PipelineStage::None);

    const TrackedAccessInfo infoA(fakeViewable, ResourceAccess::SRVGraphics, PipelineStage::Graphics);
    const TrackedAccessInfo infoB(fakeViewable, ResourceAccess::SRVGraphics, PipelineStage::Graphics);
    const TrackedAccessInfo infoC(fakeViewable, ResourceAccess::UAVCompute, PipelineStage::AsyncCompute);

    EXPECT_EQ(infoA.resource, fakeViewable);
    EXPECT_EQ(infoA.access, ResourceAccess::SRVGraphics);
    EXPECT_EQ(infoA.pipelines, PipelineStage::Graphics);
    EXPECT_EQ(infoA, infoB);
    EXPECT_NE(infoA, infoC);
}

}   // namespace gp::rhi::sync::tests
