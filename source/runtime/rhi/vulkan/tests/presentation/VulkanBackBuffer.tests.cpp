// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "presentation/VulkanBackBuffer.hpp"
#include "../resources/VulkanResourceTestFakes.hpp"
#include "CoreMinimal.hpp"
#include "maths/color/PixelFormatEnums.hpp"   // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"
#include "platform/VulkanPlatform.hpp"
#include "presentation/VulkanPresentationDesc.hpp"
#include "presentation/VulkanPresentationForward.hpp"
#include "presentation/VulkanViewport.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/VulkanTexture.hpp"
#include "resources/VulkanTextureEnums.hpp"
#include "RHIForward.hpp"
#include "sync/SyncEnums.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Side length of the test textures.
constexpr gp::Int32 kBackBufferSize = 64;

/// @brief Live fixture providing a window-less viewport and a source texture to borrow from.
class VulkanBackBufferLiveTest : public VulkanResourceLiveTest
{
protected:
    /// @brief Builds the description shared by the back buffer and its source.
    /// @param[in] debugName Texture name.
    /// @return Render targetable, sampled 2D description in the present state.
    [[nodiscard]] static resource::TextureCreateDesc makeDesc(const char* const debugName) noexcept
    {
        resource::TextureCreateDesc desc = resource::TextureCreateDesc::create2D(
            debugName, kBackBufferSize, kBackBufferSize, PixelFormat::B8G8R8A8_UNorm
        );
        desc.setFlags(resource::TextureCreateFlags::RenderTargetable | resource::TextureCreateFlags::ShaderResource);
        desc.setInitialState(sync::ResourceAccess::Present);
        return desc;
    }

    /// @brief Creates a viewport that never gets a swapchain (no window).
    /// @return Viewport reference.
    [[nodiscard]] VulkanViewportRef createWindowlessViewport() noexcept
    {
        VulkanViewportServices services{};
        services.resourceServices = m_services;
        return VulkanViewportRef(new VulkanViewport(
            services,
            VulkanPlatformWindowContext{},
            static_cast<gp::UInt32>(kBackBufferSize),
            static_cast<gp::UInt32>(kBackBufferSize),
            false,
            PixelFormat::B8G8R8A8_UNorm
        ));
    }

    /// @brief Creates a texture owning its image, used as the borrowed source.
    /// @return Texture reference.
    [[nodiscard]] gp::RefCountedPtr<VulkanTexture> createSource() noexcept
    {
        return gp::RefCountedPtr<VulkanTexture>(new VulkanTexture(m_services, cmdList(), makeDesc("Source")));
    }
};

}   // namespace

TEST_F(VulkanBackBufferLiveTest, StartsWithoutImage)
{
    const VulkanViewportRef viewport = createWindowlessViewport();
    const VulkanBackBufferRef backBuffer(new VulkanBackBuffer(m_services, *viewport, makeDesc("BackBuffer")));

    EXPECT_FALSE(backBuffer->hasAcquiredImage());
    EXPECT_EQ(backBuffer->getImage(), static_cast<VkImage>(VK_NULL_HANDLE));
    EXPECT_EQ(backBuffer->getDefaultView(), static_cast<VkImageView>(VK_NULL_HANDLE));
    EXPECT_EQ(backBuffer->getViewport(), viewport.get());
    EXPECT_EQ(backBuffer->getDesc().extent.x, kBackBufferSize);
    EXPECT_EQ(backBuffer->getDesc().extent.y, kBackBufferSize);
}

TEST_F(VulkanBackBufferLiveTest, BindBorrowsImageAndViewsOfSource)
{
    const VulkanViewportRef viewport = createWindowlessViewport();
    const VulkanBackBufferRef backBuffer(new VulkanBackBuffer(m_services, *viewport, makeDesc("BackBuffer")));
    const gp::RefCountedPtr<VulkanTexture> source = createSource();
    ASSERT_TRUE(source->isValid());

    backBuffer->bindAcquiredImage(*source);

    EXPECT_TRUE(backBuffer->hasAcquiredImage());
    EXPECT_EQ(backBuffer->getImage(), source->getImage());
    EXPECT_EQ(backBuffer->getDefaultView(), source->getDefaultView());
    EXPECT_EQ(backBuffer->getViewFormat(), source->getViewFormat());
    EXPECT_EQ(backBuffer->getFullAspectMask(), source->getFullAspectMask());
    EXPECT_EQ(backBuffer->getOwnerType(), VulkanImageOwnerType::Borrowed);
    EXPECT_EQ(backBuffer->getNativeResource(), source->getNativeResource());
}

TEST_F(VulkanBackBufferLiveTest, ReleaseLeavesSourceIntact)
{
    const VulkanViewportRef viewport = createWindowlessViewport();
    const VulkanBackBufferRef backBuffer(new VulkanBackBuffer(m_services, *viewport, makeDesc("BackBuffer")));
    const gp::RefCountedPtr<VulkanTexture> source = createSource();
    ASSERT_TRUE(source->isValid());
    const VkImage sourceImage = source->getImage();
    const VkImageView sourceView = source->getDefaultView();

    backBuffer->bindAcquiredImage(*source);
    backBuffer->releaseAcquiredImage();

    EXPECT_FALSE(backBuffer->hasAcquiredImage());
    EXPECT_EQ(backBuffer->getDefaultView(), static_cast<VkImageView>(VK_NULL_HANDLE));
    EXPECT_EQ(source->getImage(), sourceImage);
    EXPECT_EQ(source->getDefaultView(), sourceView);
    EXPECT_EQ(source->getOwnerType(), VulkanImageOwnerType::LocalOwner);
}

TEST_F(VulkanBackBufferLiveTest, RebindSwitchesToNewSource)
{
    const VulkanViewportRef viewport = createWindowlessViewport();
    const VulkanBackBufferRef backBuffer(new VulkanBackBuffer(m_services, *viewport, makeDesc("BackBuffer")));
    const gp::RefCountedPtr<VulkanTexture> first = createSource();
    const gp::RefCountedPtr<VulkanTexture> second = createSource();
    ASSERT_TRUE(first->isValid());
    ASSERT_TRUE(second->isValid());

    backBuffer->bindAcquiredImage(*first);
    backBuffer->bindAcquiredImage(*second);

    EXPECT_EQ(backBuffer->getImage(), second->getImage());
    EXPECT_EQ(backBuffer->getDefaultView(), second->getDefaultView());
    EXPECT_TRUE(first->isValid());
}

TEST_F(VulkanBackBufferLiveTest, DetachedBackBufferIgnoresImageRequests)
{
    const VulkanViewportRef viewport = createWindowlessViewport();
    const VulkanBackBufferRef backBuffer(new VulkanBackBuffer(m_services, *viewport, makeDesc("BackBuffer")));

    backBuffer->detachViewport();
    backBuffer->onGetBackBufferImage(cmdList());

    EXPECT_EQ(backBuffer->getViewport(), nullptr);
    EXPECT_FALSE(backBuffer->hasAcquiredImage());
    EXPECT_EQ(viewport->getRecreateCount(), 0u);
}

TEST_F(VulkanBackBufferLiveTest, ImageRequestWithoutWindowFallsBackWithoutImage)
{
    const VulkanViewportRef viewport = createWindowlessViewport();
    const VulkanBackBufferRef backBuffer(new VulkanBackBuffer(m_services, *viewport, makeDesc("BackBuffer")));

    // No surface can be created: the viewport tries one recreation, gives up for the frame and binds its fallback
    // (none here, since `initialize()` never ran).
    backBuffer->onGetBackBufferImage(cmdList());

    EXPECT_FALSE(backBuffer->hasAcquiredImage());
    EXPECT_EQ(viewport->getSwapChain(), nullptr);
    EXPECT_EQ(viewport->getAcquiredImageIndex(), -1);
    EXPECT_EQ(viewport->getRecreateCount(), 1u);
}

}   // namespace gp::rhi::vulkan::tests
