// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "sync/SyncEnums.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::sync::tests
{

static_assert(isValidAccess(ResourceAccess::Unknown), "Unknown access must be valid.");
static_assert(isValidAccess(ResourceAccess::Present), "Present access must be valid.");
static_assert(isValidAccess(ResourceAccess::RTV), "RTV access must be valid.");
static_assert(isValidAccess(ResourceAccess::DSVRead | ResourceAccess::DSVWrite), "DSVRead | DSVWrite must be valid.");
static_assert(
    isInvalidAccess(ResourceAccess::RTV | ResourceAccess::SRVGraphics), "RTV combined with SRVGraphics must be invalid."
);
static_assert(
    isInvalidAccess(ResourceAccess::Present | ResourceAccess::CopyDest),
    "Present combined with CopyDest must be invalid."
);

static_assert(!isSinglePipelineStage(PipelineStage::None), "None is not a single pipeline stage.");
static_assert(isSinglePipelineStage(PipelineStage::Graphics), "Graphics must be a single pipeline stage.");
static_assert(isSinglePipelineStage(PipelineStage::AsyncCompute), "AsyncCompute must be a single pipeline stage.");
static_assert(!isSinglePipelineStage(PipelineStage::All), "All is not a single pipeline stage.");
static_assert(getPipelineStageIndex(PipelineStage::Graphics) == 0u, "Graphics pipeline index must be 0.");
static_assert(getPipelineStageIndex(PipelineStage::AsyncCompute) == 1u, "AsyncCompute pipeline index must be 1.");
static_assert(getPipelineStageCount() == 2u, "Pipeline stage count must be 2.");

TEST(SyncEnumsTest, ResourceAccessReadWriteClassification)
{
    EXPECT_TRUE(isReadOnlyExclusiveAccess(ResourceAccess::Present));
    EXPECT_TRUE(isReadOnlyExclusiveAccess(ResourceAccess::SRVGraphics));
    EXPECT_TRUE(isReadOnlyExclusiveAccess(ResourceAccess::CopySrc | ResourceAccess::SRVCompute));
    EXPECT_FALSE(isReadOnlyExclusiveAccess(ResourceAccess::DSVRead));
    EXPECT_FALSE(isReadOnlyExclusiveAccess(ResourceAccess::RTV));

    EXPECT_TRUE(isReadOnlyAccess(ResourceAccess::DSVRead));
    EXPECT_TRUE(isReadOnlyAccess(ResourceAccess::Present));
    EXPECT_FALSE(isReadOnlyAccess(ResourceAccess::UAVGraphics));
    EXPECT_FALSE(isReadOnlyAccess(ResourceAccess::None));

    EXPECT_TRUE(isWriteOnlyAccess(ResourceAccess::RTV));
    EXPECT_TRUE(isWriteOnlyAccess(ResourceAccess::CopyDest));
    EXPECT_TRUE(isWriteOnlyAccess(ResourceAccess::DSVWrite));
    EXPECT_FALSE(isWriteOnlyAccess(ResourceAccess::UAVCompute));

    EXPECT_TRUE(isWritableAccess(ResourceAccess::RTV));
    EXPECT_TRUE(isWritableAccess(ResourceAccess::UAVCompute));
    EXPECT_TRUE(isWritableAccess(ResourceAccess::BVHWrite));
    EXPECT_FALSE(isWritableAccess(ResourceAccess::SRVGraphics));

    EXPECT_TRUE(isReadableAccess(ResourceAccess::SRVGraphics));
    EXPECT_TRUE(isReadableAccess(ResourceAccess::UAVGraphics));
    EXPECT_FALSE(isReadableAccess(ResourceAccess::CopyDest));
}

TEST(SyncEnumsTest, ResourceAccessConflictDetection)
{
    EXPECT_TRUE(isValidAccess(ResourceAccess::Discard));
    EXPECT_TRUE(isValidAccess(ResourceAccess::SRVGraphics | ResourceAccess::CopySrc));
    EXPECT_TRUE(isValidAccess(ResourceAccess::DSVRead | ResourceAccess::DSVWrite));
    EXPECT_TRUE(isValidAccess(ResourceAccess::UAVGraphics | ResourceAccess::UAVCompute));

    EXPECT_TRUE(isInvalidAccess(ResourceAccess::SRVGraphics | ResourceAccess::RTV));
    EXPECT_TRUE(isInvalidAccess(ResourceAccess::CopySrc | ResourceAccess::CopyDest));
    EXPECT_TRUE(isInvalidAccess(ResourceAccess::Present | ResourceAccess::UAVGraphics));
    EXPECT_TRUE(isInvalidAccess(ResourceAccess::RTV | ResourceAccess::DSVRead));
}

TEST(SyncEnumsTest, PipelineStageQueries)
{
    EXPECT_FALSE(isSinglePipelineStage(PipelineStage::None));
    EXPECT_TRUE(isSinglePipelineStage(PipelineStage::Graphics));
    EXPECT_TRUE(isSinglePipelineStage(PipelineStage::AsyncCompute));
    EXPECT_FALSE(isSinglePipelineStage(PipelineStage::All));

    EXPECT_EQ(getPipelineStageIndex(PipelineStage::Graphics), 0u);
    EXPECT_EQ(getPipelineStageIndex(PipelineStage::AsyncCompute), 1u);
    EXPECT_EQ(getPipelineStageCount(), kPipelineStageCount);
}

}   // namespace gp::rhi::sync::tests
