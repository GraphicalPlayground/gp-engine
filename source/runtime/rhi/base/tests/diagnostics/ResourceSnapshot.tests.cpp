// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "diagnostics/ResourceSnapshot.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "containers/strings/String.hpp"
#include "core/Resource.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "platforms/base/system/FileSystem.hpp"
#include "resources/Buffer.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "sync/SyncEnums.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::diagnostics::tests
{

namespace
{

class SnapshotTestBuffer final : public resource::Buffer
{
public:
    explicit SnapshotTestBuffer(const resource::BufferCreateDesc& desc) noexcept
        : Buffer(desc)
    {}

protected:
    virtual ~SnapshotTestBuffer() noexcept override = default;
};

class SnapshotTestTexture final : public resource::Texture
{
public:
    explicit SnapshotTestTexture(const resource::TextureCreateDesc& desc) noexcept
        : Texture(desc)
    {}

protected:
    virtual ~SnapshotTestTexture() noexcept override = default;
};

}   // namespace

#if GP_RHI_ENABLE_RESOURCE_INFO

TEST(RHIResourceSnapshotTests, TrackedResourcesAreCapturedAndSummarized)
{
    resource::Resource::startTrackingAllResources();

    {
        resource::BufferCreateDesc bufDesc =
            resource::BufferCreateDesc::createStructured("TestScenePrimitives", 8192u, 64u);
        bufDesc.usage |= resource::BufferUsageFlags::UnorderedAccess;
        bufDesc.initialState = sync::ResourceAccess::UAVMask;
        bufDesc.ownerName = gp::Name("Renderer.Scene");
        resource::BufferRef buf(new SnapshotTestBuffer(bufDesc));

        resource::TextureCreateDesc texDesc =
            resource::TextureCreateDesc::create2D("TestSceneColor", 256, 256, PixelFormat::R16G16B16A16_Float);
        texDesc.flags = resource::TextureCreateFlags::RenderTargetable | resource::TextureCreateFlags::ShaderResource;
        texDesc.initialState = sync::ResourceAccess::RTV;
        texDesc.ownerName = gp::Name("Renderer.GBuffer");
        resource::TextureRef tex(new SnapshotTestTexture(texDesc));

        gp::Vector<ResourceStats> stats{};
        getTrackedResourceStats(stats);
        EXPECT_GE(stats.size(), 2u);

        bool foundBuffer = false;
        bool foundTexture = false;
        for (const ResourceStats& item: stats)
        {
            if (item.name == "TestScenePrimitives")
            {
                foundBuffer = true;
                EXPECT_EQ(item.sizeInBytes, 8192u);
                EXPECT_TRUE(item.hasUnorderedAccessFlag);
                EXPECT_EQ(item.ownerName, "Renderer.Scene");
            }
            else if (item.name == "TestSceneColor")
            {
                foundTexture = true;
                EXPECT_GT(item.sizeInBytes, 0u);
                EXPECT_TRUE(item.hasRenderTargetFlag);
                EXPECT_EQ(item.ownerName, "Renderer.GBuffer");
            }
        }
        EXPECT_TRUE(foundBuffer);
        EXPECT_TRUE(foundTexture);

        const gp::String summary = dumpResourceMemorySummary(10);
        EXPECT_NE(summary.view().find("TestSceneColor"), gp::StringView::npos);
        EXPECT_NE(summary.view().find("TestScenePrimitives"), gp::StringView::npos);
    }

    resource::ResourceDeletionQueue::flushPendingDeletes(true);
    resource::Resource::stopTrackingAllResources();
}

#endif

TEST(RHIResourceSnapshotTests, SaveAndLoadRoundTripAndRejectCorruptFiles)
{
    const gp::StringView snapshotPath(".cache/test_rhi_resource_snapshot.bin");
    const gp::StringView corruptPath(".cache/test_rhi_resource_snapshot_corrupt.bin");
    static_cast<void>(gp::platform::FileSystem::createDirectoryTree(".cache"));

    ResourceSnapshot original{};
    original.metadata.rhiName = gp::String("Null");
    original.metadata.adapterName = gp::String("Test Virtual GPU");
    original.metadata.driverVersion = gp::String("560.94");
    original.metadata.featureLevel = gp::String("SM6");
    original.metadata.dedicatedVideoMemory = 12ULL * 1024ULL * 1024ULL * 1024ULL;
    original.metadata.textureMemoryStats.streamingMemorySize = 64 * 1024 * 1024;

    ResourceStats entryA{};
    entryA.name = gp::String("GBufferA");
    entryA.ownerName = gp::String("DeferredShading");
    entryA.type = gp::String("Texture2D");
    entryA.pixelFormat = gp::String("R8G8B8A8_UNorm");
    entryA.sizeInBytes = 8ULL * 1024ULL * 1024ULL;
    entryA.hasRenderTargetFlag = true;
    original.resources.emplaceBack(entryA);

    ResourceStats entryB{};
    entryB.name = gp::String("TLASBuffer");
    entryB.ownerName = gp::String("RayTracing");
    entryB.type = gp::String("Buffer");
    entryB.pixelFormat = gp::String("Unknown");
    entryB.sizeInBytes = 2ULL * 1024ULL * 1024ULL;
    entryB.hasRayTracingAccelerationStructure = true;
    entryB.isTransient = true;
    original.resources.emplaceBack(entryB);

    ASSERT_TRUE(original.saveToFile(snapshotPath));
    EXPECT_EQ(original.metadata.totalResourceSize, 10ULL * 1024ULL * 1024ULL);

    ResourceSnapshot loaded{};
    ASSERT_TRUE(ResourceSnapshot::loadFromFile(snapshotPath, loaded));
    EXPECT_EQ(loaded.metadata, original.metadata);
    ASSERT_EQ(loaded.resources.size(), 2u);
    EXPECT_EQ(loaded.resources[0], original.resources[0]);
    EXPECT_EQ(loaded.resources[1], original.resources[1]);

    // Corrupt magic must fail cleanly.
    const gp::UInt32 badBytes[4] = { 0xDEADBEEFu, 1u, 0u, 0u };
    static_cast<void>(gp::platform::FileSystem::writeAllBytes(corruptPath, badBytes, sizeof(badBytes)));
    ResourceSnapshot rejected{};
    EXPECT_FALSE(ResourceSnapshot::loadFromFile(corruptPath, rejected));

    static_cast<void>(gp::platform::FileSystem::removeFile(snapshotPath));
    static_cast<void>(gp::platform::FileSystem::removeFile(corruptPath));
}

}   // namespace gp::rhi::diagnostics::tests
