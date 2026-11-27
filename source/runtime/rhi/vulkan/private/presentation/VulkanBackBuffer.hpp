// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "CoreMinimal.hpp"                              // IWYU pragma: keep
#include "presentation/VulkanPresentationForward.hpp"   // IWYU pragma: keep
#include "resources/TextureCreateDesc.hpp"
#include "resources/VulkanResourceServices.hpp"
#include "resources/VulkanTexture.hpp"
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief RHI texture standing for "the image currently acquired from the swapchain".
/// @details The renderer holds one texture for the whole life of the viewport, yet every frame renders into a
///          different swapchain image. The back buffer bridges the two: it has no image of its own, and the first time
///          the RHI timeline needs it in a frame (`onGetBackBufferImage()`), it asks its viewport to acquire an image
///          and borrows that image and its views (`VulkanTexture::bindBorrowedImage()`). Presenting releases the
///          binding (`releaseAcquiredImage()`), so the next frame acquires again.
///
///          When no image can be acquired (minimized window, lost surface), the viewport hands out an off-screen
///          "rendering back buffer" of the same size instead, so the frame still renders somewhere valid.
/// @note Bottom of pipe: every method runs on the RHI timeline.
class GP_RHI_VULKAN_API VulkanBackBuffer final : public VulkanTexture
{
public:
    /// @brief Creates a back buffer describing the viewport's images, bound to no image yet.
    /// @param[in] services Device, memory and deferred deletion services.
    /// @param[in] viewport Viewport the images are acquired from; must outlive the binding or call `detachViewport()`.
    /// @param[in] createDesc Description matching the swapchain images (size, format, flags).
    VulkanBackBuffer(
        const VulkanResourceServices& services, VulkanViewport& viewport, const resource::TextureCreateDesc& createDesc
    ) noexcept;

    /// @brief Binds the acquired swapchain image if this frame has none yet.
    /// @param[in,out] executingCmdList Command list being executed; its Vulkan context records the acquire wait.
    void onGetBackBufferImage(command::CommandListBase& executingCmdList) noexcept;

    /// @brief Binds `source` as this frame's image.
    /// @param[in,out] source Swapchain image texture or rendering back buffer.
    void bindAcquiredImage(VulkanTexture& source) noexcept;

    /// @brief Drops the image bound for this frame; the next frame acquires again.
    void releaseAcquiredImage() noexcept;

    /// @brief Forgets the viewport once it is destroyed; later acquires do nothing.
    void detachViewport() noexcept
    {
        m_viewport = nullptr;
    }

    /// @brief Reports whether an image is bound for this frame.
    /// @return True after a successful `onGetBackBufferImage()` until `releaseAcquiredImage()`.
    [[nodiscard]] bool hasAcquiredImage() const noexcept
    {
        return getImage() != VK_NULL_HANDLE;
    }

    /// @brief Returns the viewport the back buffer acquires from.
    /// @return Viewport, or `nullptr` once detached.
    [[nodiscard]] VulkanViewport* getViewport() const noexcept
    {
        return m_viewport;
    }

protected:
    /// @brief Drops the binding; the borrowed image belongs to the swapchain.
    virtual ~VulkanBackBuffer() noexcept override;

private:
    VulkanViewport* m_viewport{ nullptr };
};

}   // namespace gp::rhi::vulkan
