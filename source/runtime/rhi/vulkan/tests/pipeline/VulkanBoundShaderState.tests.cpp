// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanBoundShaderState.hpp"
#include "containers/arrays/Vector.hpp"        // IWYU pragma: keep
#include "containers/views/VectorView.hpp"     // IWYU pragma: keep
#include "core/ResourceDeletionQueue.hpp"
#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "device/VulkanDevice.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "pipeline/BoundShaderState.hpp"       // IWYU pragma: keep
#include "pipeline/VulkanBoundShaderStateCache.hpp"
#include "pipeline/VulkanBoundShaderStateKey.hpp"
#include "shaders/PixelShader.hpp"
#include "shaders/VertexShader.hpp"
#include "shaders/VulkanBaseShader.hpp"
#include "shaders/VulkanShaderFactory.hpp"
#include "VulkanPipelineTestFakes.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Offline fixture owning a vertex and a pixel shader, and emptying the bound state cache around each test.
class VulkanBoundShaderStateTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        emptyCachedVulkanBoundShaderStates();
        const gp::Vector<gp::UInt8> vertexBlob = makeTestShaderBlob(kSpirvExecutionModelVertex, 1u, "BoundVS");
        const gp::Vector<gp::UInt8> pixelBlob = makeTestShaderBlob(kSpirvExecutionModelVertex, 2u, "BoundPS");
        m_vertexShader = m_factory.createShader<VulkanVertexShader>(
            m_offlineDevice, nullptr, gp::ConstVectorView<gp::UInt8>(vertexBlob)
        );
        m_pixelShader = m_factory.createShader<VulkanPixelShader>(
            m_offlineDevice, nullptr, gp::ConstVectorView<gp::UInt8>(pixelBlob)
        );
    }

    virtual void TearDown() override
    {
        emptyCachedVulkanBoundShaderStates();
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
        m_vertexShader.reset();
        m_pixelShader.reset();
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
    }

    /// @brief Device without native handles.
    VulkanDevice m_offlineDevice{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_API_VERSION_1_3, false };

    /// @brief Shader factory; declared after the device so it is destroyed first.
    VulkanShaderFactory m_factory{};

    /// @brief Vertex stage.
    gp::RefCountedPtr<VulkanVertexShader> m_vertexShader{};

    /// @brief Pixel stage.
    gp::RefCountedPtr<VulkanPixelShader> m_pixelShader{};
};

}   // namespace

TEST(VulkanBoundShaderStateLookupKeyTest, KeysCompareByShaderIdentity)
{
    const shader::VertexShader* const fakeVertex = reinterpret_cast<const shader::VertexShader*>(0x1000);
    const shader::PixelShader* const fakePixel = reinterpret_cast<const shader::PixelShader*>(0x2000);

    const VulkanBoundShaderStateLookupKey first(nullptr, fakeVertex, fakePixel);
    const VulkanBoundShaderStateLookupKey second(nullptr, fakeVertex, fakePixel);
    const VulkanBoundShaderStateLookupKey depthOnly(nullptr, fakeVertex, nullptr);

    EXPECT_TRUE(first == second);
    EXPECT_EQ(first.getHash(), second.getHash());
    EXPECT_FALSE(first == depthOnly);
    EXPECT_NE(first.getHash(), depthOnly.getHash());
}

TEST_F(VulkanBoundShaderStateTest, EqualStagesShareOneState)
{
    ASSERT_TRUE(m_vertexShader.isValid());
    ASSERT_TRUE(m_pixelShader.isValid());
    {
        const pipeline::BoundShaderStateRef first =
            createVulkanBoundShaderState(nullptr, m_vertexShader.get(), m_pixelShader.get(), nullptr);
        const pipeline::BoundShaderStateRef second =
            createVulkanBoundShaderState(nullptr, m_vertexShader.get(), m_pixelShader.get(), nullptr);
        const pipeline::BoundShaderStateRef found =
            findCachedVulkanBoundShaderState(nullptr, m_vertexShader.get(), m_pixelShader.get());

        ASSERT_TRUE(first.isValid());
        EXPECT_EQ(first.get(), second.get());
        EXPECT_EQ(first.get(), found.get());
        EXPECT_EQ(getCachedVulkanBoundShaderStateCount(), 1u);
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);

    EXPECT_EQ(getCachedVulkanBoundShaderStateCount(), 0u);
}

TEST_F(VulkanBoundShaderStateTest, DifferentStagesCreateDifferentStates)
{
    const pipeline::BoundShaderStateRef full =
        createVulkanBoundShaderState(nullptr, m_vertexShader.get(), m_pixelShader.get(), nullptr);
    const pipeline::BoundShaderStateRef depthOnly =
        createVulkanBoundShaderState(nullptr, m_vertexShader.get(), nullptr, nullptr);

    ASSERT_TRUE(full.isValid());
    ASSERT_TRUE(depthOnly.isValid());
    EXPECT_NE(full.get(), depthOnly.get());
    EXPECT_EQ(getCachedVulkanBoundShaderStateCount(), 2u);
}

TEST_F(VulkanBoundShaderStateTest, MissingStateIsNotFound)
{
    const pipeline::BoundShaderStateRef found =
        findCachedVulkanBoundShaderState(nullptr, m_vertexShader.get(), m_pixelShader.get());

    EXPECT_FALSE(found.isValid());
}

}   // namespace gp::rhi::vulkan::tests
