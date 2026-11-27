// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "allocators/PoolMoveRequest.hpp"
#include "allocators/PoolResource.hpp"
#include "commands/CommandForward.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "memory/VulkanAllocation.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/VulkanResourceServices.hpp"
#include "resources/VulkanTextureEnums.hpp"
#include "resources/VulkanTextureTypes.hpp"
#include "resources/VulkanViewableResource.hpp"
#include "RHIVulkan.hpp"
#include "threading/sync/Mutex.hpp"
#include "views/VulkanTextureViewDesc.hpp"
#include "views/VulkanView.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Vulkan implementation of a GPU texture: a `VkImage`, its memory and its default views.
/// @details Three storage strategies coexist behind the same RHI type:
///          - Regular textures own a `VkImage` bound to pooled (or dedicated) device memory. The image is created
///            `MUTABLE_FORMAT` so views can reinterpret it (sRGB pairs, UAV formats, aliasable formats); with
///            `VK_KHR_image_format_list` the exact set of view formats is declared up front, which lets drivers keep
///            compression enabled.
///          - `CPUReadback` textures have no image at all. They are a host-cached `VkBuffer` where every mip is packed
///            tightly; copies into them are image-to-buffer copies and the CPU maps the buffer directly.
///          - Aliased textures borrow the image, views and memory of another texture they keep alive.
///
///          A freshly created image is `UNDEFINED`. When its initial RHI access needs another layout (render targets,
///          shader resources, UAVs) or the texture has a fast clear value, the constructor records the transition
///          (and clear) on the upload context, which executes before any graphics work that could read it.
/// @note Constructed on the recording thread; every native command is deferred to the RHI timeline through the
///       command list it receives. Not `final`: a presentation back buffer is a texture whose image changes every
///       frame (see `bindBorrowedImage()`).
class GP_RHI_VULKAN_API VulkanTexture
    : public resource::Texture
    , public allocator::PoolResource
    , public VulkanViewableResource
{
public:
    /// @brief Creates the image (or readback buffer), allocates and binds its memory, and builds the default views.
    /// @details On failure (device limits, out of memory) the texture is left without an image; `isValid()` reports
    ///          it.
    /// @param[in] services Device, memory and context services; copied into the texture.
    /// @param[in,out] cmdList Command list the initial layout transition and pool registration are recorded on.
    /// @param[in] createDesc Creation description.
    VulkanTexture(
        const VulkanResourceServices& services,
        command::CommandListBase& cmdList,
        const resource::TextureCreateDesc& createDesc
    ) noexcept;

    /// @brief Wraps an image owned by someone else (a swapchain image) and builds views for it.
    /// @details The image is neither allocated nor destroyed by the texture; only the views are.
    /// @param[in] services Device, memory and context services.
    /// @param[in] createDesc Description matching the external image.
    /// @param[in] externalImage Image to wrap.
    VulkanTexture(
        const VulkanResourceServices& services, const resource::TextureCreateDesc& createDesc, VkImage externalImage
    ) noexcept;

    /// @copydoc resource::Texture::getNativeResource()
    /// @note Returns the `VkImage`, or the readback `VkBuffer` of a `CPUReadback` texture.
    [[nodiscard]] virtual void* getNativeResource() const noexcept override;

    /// @copydoc resource::Texture::getNativeShaderResourceView()
    /// @note Returns the default `VkImageView`.
    [[nodiscard]] virtual void* getNativeShaderResourceView() const noexcept override;

    /// @copydoc resource::Texture::getTextureBaseRHI()
    [[nodiscard]] virtual void* getTextureBaseRHI() noexcept override;

    /// @brief Reports whether the texture has storage (an image or a readback buffer).
    /// @return False when creation failed.
    [[nodiscard]] bool isValid() const noexcept
    {
        return (m_image != VK_NULL_HANDLE) ||
               (m_cpuReadbackBuffer.isValid() && m_cpuReadbackBuffer->buffer != VK_NULL_HANDLE);
    }

    /// @brief Returns the native image.
    /// @return Image handle, or `VK_NULL_HANDLE` for `CPUReadback` textures.
    [[nodiscard]] VkImage getImage() const noexcept
    {
        return m_image;
    }

    /// @brief Returns the view covering every mip, layer and aspect.
    /// @return Default view, or `VK_NULL_HANDLE`.
    [[nodiscard]] VkImageView getDefaultView() const noexcept
    {
        return m_defaultView;
    }

    /// @brief Returns the view used for sampling: depth only on depth/stencil formats, the default view otherwise.
    /// @return Partial view, or `VK_NULL_HANDLE`.
    [[nodiscard]] VkImageView getPartialView() const noexcept
    {
        return m_partialView;
    }

    /// @brief Returns the format of shader resource views (sRGB when the texture is sRGB).
    /// @return View format.
    [[nodiscard]] VkFormat getViewFormat() const noexcept
    {
        return m_viewFormat;
    }

    /// @brief Returns the format of unordered access views and of the raw texel data (always linear).
    /// @return Storage format.
    [[nodiscard]] VkFormat getStorageFormat() const noexcept
    {
        return m_storageFormat;
    }

    /// @brief Returns every aspect of the format (depth and stencil on combined formats).
    /// @return Aspect mask.
    [[nodiscard]] VkImageAspectFlags getFullAspectMask() const noexcept
    {
        return m_fullAspectMask;
    }

    /// @brief Returns the aspect a single copy or sample addresses (depth only on combined formats).
    /// @return Aspect mask.
    [[nodiscard]] VkImageAspectFlags getPartialAspectMask() const noexcept
    {
        return m_partialAspectMask;
    }

    /// @brief Reports whether the format has a depth or stencil aspect.
    /// @return True for depth/stencil formats.
    [[nodiscard]] bool isDepthOrStencilAspect() const noexcept
    {
        return (m_fullAspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0u;
    }

    /// @brief Returns the usage the image was created with.
    /// @return Image usage flags.
    [[nodiscard]] VkImageUsageFlags getImageUsageFlags() const noexcept
    {
        return m_imageUsageFlags;
    }

    /// @brief Reports whether the image can be sampled.
    /// @return True with `SAMPLED` usage.
    [[nodiscard]] bool supportsSampling() const noexcept
    {
        return (m_imageUsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT) != 0u;
    }

    /// @brief Returns the tiling the image was created with.
    /// @return Image tiling.
    [[nodiscard]] VkImageTiling getTiling() const noexcept
    {
        return m_tiling;
    }

    /// @brief Returns the layout the image is in once the constructor's initial transition has executed.
    /// @return Initial layout, `UNDEFINED` when none was needed.
    [[nodiscard]] VkImageLayout getDefaultLayout() const noexcept
    {
        return m_defaultLayout;
    }

    /// @brief Returns the memory requirements of the image (or the readback buffer size).
    /// @return Size, alignment and memory type bits.
    [[nodiscard]] const VkMemoryRequirements& getMemoryRequirements() const noexcept
    {
        return m_memoryRequirements;
    }

    /// @brief Returns the memory backing the image.
    /// @return Allocation; not owned for aliased and external textures.
    [[nodiscard]] const VulkanAllocation& getAllocation() const noexcept
    {
        return m_allocation;
    }

    /// @brief Returns who owns the image.
    /// @return Owner type.
    [[nodiscard]] VulkanImageOwnerType getOwnerType() const noexcept
    {
        return m_ownerType;
    }

    /// @brief Returns the readback buffer of a `CPUReadback` texture.
    /// @return Readback buffer, or `nullptr` for image-backed textures.
    [[nodiscard]] const VulkanCpuReadbackBuffer* getCpuReadbackBuffer() const noexcept
    {
        return m_cpuReadbackBuffer.get();
    }

    /// @brief Returns the services the texture was created with.
    /// @return Device, memory and context services.
    [[nodiscard]] const VulkanResourceServices& getServices() const noexcept
    {
        return m_services;
    }

    /// @brief Returns the layout matching the tracked RHI access, used around internal copies.
    /// @details Falls back to the default layout when the tracked access maps to no layout, and to `GENERAL` when the
    ///          default layout is `UNDEFINED` (the image content is then irrelevant).
    /// @return Current layout as far as the RHI knows.
    [[nodiscard]] VkImageLayout getTrackedLayout() const noexcept;

    /// @brief Returns the native view type matching the texture dimension.
    /// @return 2D, 2D array, 3D, cube or cube array view type.
    [[nodiscard]] VkImageViewType getViewType() const noexcept;

    /// @brief Returns the number of array layers of the image.
    /// @details Cube faces count as layers, so a cube array of `N` elements has `6 * N` layers; 3D textures have one.
    /// @return Native layer count.
    [[nodiscard]] gp::UInt32 getNumberOfArrayLevels() const noexcept;

    /// @brief Returns an internal image view matching `viewDesc`, creating it on first use.
    /// @details Internal views are the views the backend itself needs on top of the default ones: one mip or one
    ///          slice of a render target bound as a framebuffer attachment, a depth view without swizzle, and so on.
    ///          They are keyed by `VulkanTextureViewDesc` and live until the image changes (rename, alias, release).
    /// @note Thread-safe: render passes may be begun from parallel translation contexts.
    /// @param[in] viewDesc Backend view description.
    /// @return Cached view, or `nullptr` when the texture has no image.
    [[nodiscard]] const VulkanView* findOrAddInternalView(const VulkanTextureViewDesc& viewDesc) noexcept;

    /// @brief Returns the number of cached internal views.
    /// @return Internal view count.
    [[nodiscard]] gp::USize getNumInternalViews() const noexcept;

    /// @brief Makes this texture point at the image, views and memory of `source`.
    /// @details `source` is kept alive through a reference. The previous storage of this texture is released first.
    /// @note Bottom of pipe. Backs `DynamicRHI::aliasTextureResources()`.
    /// @param[in,out] source Texture whose storage is borrowed.
    void aliasTextureResources(VulkanTexture& source) noexcept;

    /// @brief Registers this texture as the owner of its pool range, so defragmentation can move it.
    /// @param[in,out] cmdList Command list the registration is recorded on.
    void registerPoolOwner(command::CommandListBase& cmdList) noexcept;

    /// @brief Copies the image into a new image bound to the range picked by the defragmenter, then switches to it.
    /// @details Declines textures that cannot be moved safely: aliased, external, CPU readback, memoryless, render
    ///          targets, MSAA, linear images, or when no graphics recorder outside a render pass is available.
    /// @note Bottom of pipe. Records on the graphics context of `request.contexts`.
    /// @param[in] request Destination range and active contexts.
    /// @return True when the copy was recorded and the texture now uses the new image.
    [[nodiscard]] bool onAllocationMoved(const allocator::PoolMoveRequest& request) noexcept;

protected:
    /// @brief Creates a texture that describes an image without having one yet.
    /// @details Computes the image description and formats from `createDesc` but binds no image; derived classes
    ///          point the texture at an existing image later with `bindBorrowedImage()`.
    /// @param[in] services Device, memory and context services.
    /// @param[in] createDesc Creation description of the images that will be borrowed.
    VulkanTexture(const VulkanResourceServices& services, const resource::TextureCreateDesc& createDesc) noexcept;

    /// @brief Releases the views, the image and its memory, according to the owner type.
    virtual ~VulkanTexture() noexcept override;

    /// @brief Points this texture at the image and views of `source` without taking ownership of anything.
    /// @details The previous binding is dropped first. `source` is kept alive through a reference; internal views
    ///          requested on this texture are forwarded to (and cached on) `source`, so they live as long as the image.
    /// @note Bottom of pipe. Called every frame by a back buffer once its swapchain image has been acquired.
    /// @param[in,out] source Texture owning (or wrapping) the image to borrow.
    void bindBorrowedImage(VulkanTexture& source) noexcept;

    /// @brief Drops the borrowed image and views; the texture has no image afterwards.
    /// @note Bottom of pipe. Nothing is queued for deletion: the source still owns everything.
    void unbindBorrowedImage() noexcept;

private:
    /// @brief Creates the host-visible readback buffer of a `CPUReadback` texture.
    void createCpuReadbackBuffer() noexcept;

    /// @brief Creates the image, allocates and binds its memory.
    /// @return True on success.
    [[nodiscard]] bool createImage() noexcept;

    /// @brief Creates the default and partial views of the current image.
    void createViews() noexcept;

    /// @brief Releases the default and partial views.
    void destroyViews() noexcept;

    /// @brief Releases whatever storage the texture owns and resets every handle.
    void releaseStorage() noexcept;

    /// @brief Records the initial clear and layout transition on the upload context.
    /// @param[in,out] cmdList Command list the work is deferred on.
    /// @param[in] shouldClear True to clear the image with its clear value.
    void setInitialImageState(command::CommandListBase& cmdList, bool shouldClear) noexcept;

    /// @brief Creates one view of the current image.
    /// @param[in] aspectMask Aspects the view covers.
    /// @return New view, or `VK_NULL_HANDLE`.
    [[nodiscard]] VkImageView createView(VkImageAspectFlags aspectMask) const noexcept;

    /// @brief One cached internal view.
    struct InternalView final
    {
        /// @brief `VulkanTextureViewDesc::getHash()` of `desc`.
        gp::UInt64 hash{ 0u };

        /// @brief Description the view was created from.
        VulkanTextureViewDesc desc{};

        /// @brief Owned view; its native object is released through the deferred deletion queue.
        gp::UniquePtr<VulkanView> view{};
    };

    VulkanResourceServices m_services{};
    mutable gp::thread::Mutex m_internalViewsMutex{};
    gp::Vector<InternalView> m_internalViews{};
    VkImage m_image{ VK_NULL_HANDLE };
    VkImageView m_defaultView{ VK_NULL_HANDLE };
    VkImageView m_partialView{ VK_NULL_HANDLE };
    VulkanImageCreateInfo m_imageCreateInfo{};
    VkMemoryRequirements m_memoryRequirements{};
    VulkanAllocation m_allocation{};
    gp::UniquePtr<VulkanCpuReadbackBuffer> m_cpuReadbackBuffer{};
    gp::RefCountedPtr<VulkanTexture> m_aliasedSource{};
    VkFormat m_viewFormat{ VK_FORMAT_UNDEFINED };
    VkFormat m_storageFormat{ VK_FORMAT_UNDEFINED };
    VkImageUsageFlags m_imageUsageFlags{ 0u };
    VkImageAspectFlags m_fullAspectMask{ 0u };
    VkImageAspectFlags m_partialAspectMask{ 0u };
    VkImageTiling m_tiling{ VK_IMAGE_TILING_OPTIMAL };
    VkImageLayout m_defaultLayout{ VK_IMAGE_LAYOUT_UNDEFINED };
    VulkanImageOwnerType m_ownerType{ VulkanImageOwnerType::None };
    VulkanTextureStatCategory m_statCategory{ VulkanTextureStatCategory::Texture2D };
};

}   // namespace gp::rhi::vulkan
