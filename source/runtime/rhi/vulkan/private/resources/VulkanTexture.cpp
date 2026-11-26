// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanTexture.hpp"
#include "allocators/PoolAllocationData.hpp"   // IWYU pragma: keep
#include "barriers/VulkanPipelineBarrier.hpp"
#include "commands/CommandListBase.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/Memory.hpp"
#include "memory/MemoryBase.hpp"
#include "memory/VulkanDeviceMemory.hpp"
#include "memory/VulkanMemoryEnums.hpp"
#include "memory/VulkanMemoryManager.hpp"
#include "resources/IVulkanCommandContextProvider.hpp"
#include "resources/IVulkanTransferCommandRecorder.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/TextureUtils.hpp"
#include "resources/VulkanTextureStats.hpp"
#include "resources/VulkanTextureUsage.hpp"
#include "RHIDefinitions.hpp"
#include "sync/SyncEnums.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "translation/VulkanPixelFormat.hpp"
#include "translation/VulkanResourceEnums.hpp"
#include "views/ViewEnums.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"
#include <utility>

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Images at least this large get a dedicated allocation instead of a pool range.
constexpr VkDeviceSize kDedicatedImageAllocationThreshold = 12ull * 1024ull * 1024ull;

/// @brief Alignment of every mip inside a CPU readback buffer.
constexpr gp::UInt32 kCpuReadbackMipAlignment = 16u;

/// @brief Reports whether the texture is a render, depth or resolve target.
/// @param[in] flags Texture creation flags.
/// @return True for any targetable texture.
[[nodiscard]] bool isTargetableTexture(const resource::TextureCreateFlags flags) noexcept
{
    using resource::TextureCreateFlags;
    return hasAnyTextureFlags(
        flags,
        TextureCreateFlags::RenderTargetable | TextureCreateFlags::DepthStencilTargetable |
            TextureCreateFlags::ResolveTargetable | TextureCreateFlags::DepthStencilResolveTarget
    );
}

/// @brief Reports whether the device exposes lazily allocated (tile) memory.
/// @param[in] services Texture services.
/// @return True when memoryless attachments are possible.
[[nodiscard]] bool supportsMemorylessImages(const VulkanResourceServices& services) noexcept
{
    return services.memoryManager->getDeviceMemoryManager().supportsMemoryType(VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT);
}

}   // namespace

VulkanTexture::VulkanTexture(
    const VulkanResourceServices& services,
    command::CommandListBase& cmdList,
    const resource::TextureCreateDesc& createDesc
) noexcept
    : resource::Texture(createDesc)
    , m_services(services)
{
    GP_CHECKF(m_services.isValid(), "VulkanTexture requires a device, a memory manager and a staging manager.");

    const resource::TextureDesc& desc = getDesc();
    m_statCategory = getVulkanTextureStatCategory(desc);

    if (hasAnyTextureFlags(desc.flags, resource::TextureCreateFlags::CPUReadback))
    {
        createCpuReadbackBuffer();
        return;
    }

    if (!createImage())
    {
        return;
    }
    createViews();

    // The layout the image must be in to match its initial RHI access. When nothing specific is needed the image still
    // leaves UNDEFINED, so every later transition has a defined source layout.
    const bool hasInitialData = (createDesc.initAction != resource::TextureInitAction::Default);
    const sync::ResourceAccess initialAccess = (createDesc.initialState != sync::ResourceAccess::Unknown)
                                                   ? createDesc.initialState
                                                   : resource::getDefaultResourceState(desc.flags, hasInitialData);
    const bool isDepthStencilTarget =
        hasAnyTextureFlags(desc.flags, resource::TextureCreateFlags::DepthStencilTargetable);
    m_defaultLayout = getVulkanInitialImageLayout(initialAccess, isDepthStencilTarget, supportsSampling(), false);
    if (m_defaultLayout == VK_IMAGE_LAYOUT_UNDEFINED)
    {
        m_defaultLayout = VK_IMAGE_LAYOUT_GENERAL;
    }

    // Initializers upload the data and do the final transition themselves.
    if (!hasInitialData)
    {
        const bool canClear = (m_imageUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0u;
        const bool shouldClear = canClear && isTargetableTexture(desc.flags) && hasClearValue();
        setInitialImageState(cmdList, shouldClear);
    }
}

VulkanTexture::VulkanTexture(
    const VulkanResourceServices& services, const resource::TextureCreateDesc& createDesc, const VkImage externalImage
) noexcept
    : resource::Texture(createDesc)
    , m_services(services)
{
    GP_CHECKF(m_services.isValid(), "VulkanTexture requires a device, a memory manager and a staging manager.");
    GP_CHECKF(externalImage != VK_NULL_HANDLE, "Wrapping a null external image.");

    const resource::TextureDesc& desc = getDesc();
    m_statCategory = getVulkanTextureStatCategory(desc);

    if (!generateVulkanImageCreateInfo(
            *m_services.device,
            desc,
            false,
            supportsMemorylessImages(m_services),
            m_imageCreateInfo,
            m_viewFormat,
            m_storageFormat
        ))
    {
        GP_LOG_ERROR(LogVulkanRHI, "External texture '{}' does not fit the device limits.", createDesc.debugName);
        return;
    }

    m_image = externalImage;
    m_imageUsageFlags = m_imageCreateInfo.imageCreateInfo.usage;
    m_tiling = m_imageCreateInfo.imageCreateInfo.tiling;
    m_fullAspectMask = getVkImageAspectFlags(m_viewFormat, true, true);
    m_partialAspectMask = getVkImageAspectFlags(m_viewFormat, false, true);
    m_ownerType = VulkanImageOwnerType::ExternalOwner;
    createViews();
}

VulkanTexture::~VulkanTexture() noexcept
{
    releaseStorage();
}

void* VulkanTexture::getNativeResource() const noexcept
{
    if (m_cpuReadbackBuffer.isValid())
    {
        return reinterpret_cast<void*>(m_cpuReadbackBuffer->buffer);
    }
    return reinterpret_cast<void*>(m_image);
}

void* VulkanTexture::getNativeShaderResourceView() const noexcept
{
    return reinterpret_cast<void*>(m_partialView);
}

void* VulkanTexture::getTextureBaseRHI() noexcept
{
    return static_cast<void*>(this);
}

VkImageLayout VulkanTexture::getTrackedLayout() const noexcept
{
    const bool isDepthStencilTarget =
        hasAnyTextureFlags(getDesc().flags, resource::TextureCreateFlags::DepthStencilTargetable);
    const VkImageLayout trackedLayout =
        getVulkanInitialImageLayout(getTrackedAccessUnsafe(), isDepthStencilTarget, supportsSampling(), false);
    if (trackedLayout != VK_IMAGE_LAYOUT_UNDEFINED)
    {
        return trackedLayout;
    }
    return (m_defaultLayout != VK_IMAGE_LAYOUT_UNDEFINED) ? m_defaultLayout : VK_IMAGE_LAYOUT_GENERAL;
}

VkImageViewType VulkanTexture::getViewType() const noexcept
{
    return textureDimensionToVkImageViewType(getDesc().dimension);
}

gp::UInt32 VulkanTexture::getNumberOfArrayLevels() const noexcept
{
    return getVulkanTextureLayerCount(getDesc());
}

const VulkanView* VulkanTexture::findOrAddInternalView(const VulkanTextureViewDesc& viewDesc) noexcept
{
    if (m_image == VK_NULL_HANDLE)
    {
        return nullptr;
    }

    const gp::UInt64 hash = viewDesc.getHash();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_internalViewsMutex);
    for (const InternalView& entry: m_internalViews)
    {
        // The hash covers every field, so a match only needs the field comparison to settle collisions.
        if ((entry.hash == hash) && (entry.desc == viewDesc))
        {
            return entry.view.get();
        }
    }

    // Attachment views never need a bindless slot.
    gp::UniquePtr<VulkanView> view = gp::makeUnique<VulkanView>(m_services, view::DescriptorType::Invalid);
    view->createTextureView(m_image, viewDesc);
    const VulkanView* const result = view.get();
    m_internalViews.emplaceBack(InternalView{ hash, viewDesc, std::move(view) });
    return result;
}

gp::USize VulkanTexture::getNumInternalViews() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_internalViewsMutex);
    return m_internalViews.size();
}

void VulkanTexture::aliasTextureResources(VulkanTexture& source) noexcept
{
    GP_CHECKF(&source != this, "A texture cannot alias itself.");
    GP_CHECKF(!source.m_cpuReadbackBuffer.isValid(), "CPU readback textures cannot be aliased.");

    releaseStorage();

    // Keep the source alive for as long as its image is borrowed.
    m_aliasedSource = gp::RefCountedPtr<VulkanTexture>(&source);

    m_image = source.m_image;
    m_imageCreateInfo.imageCreateInfo = source.m_imageCreateInfo.imageCreateInfo;
    m_imageCreateInfo.imageCreateInfo.pNext = nullptr;
    m_memoryRequirements = source.m_memoryRequirements;
    m_allocation.reference(source.m_allocation);
    m_viewFormat = source.m_viewFormat;
    m_storageFormat = source.m_storageFormat;
    m_imageUsageFlags = source.m_imageUsageFlags;
    m_fullAspectMask = source.m_fullAspectMask;
    m_partialAspectMask = source.m_partialAspectMask;
    m_tiling = source.m_tiling;
    m_defaultLayout = source.m_defaultLayout;
    m_ownerType = VulkanImageOwnerType::Aliased;

    createViews();
}

void VulkanTexture::registerPoolOwner(command::CommandListBase& cmdList) noexcept
{
    if (m_ownerType != VulkanImageOwnerType::LocalOwner || m_allocation.isPoolAllocated == 0u)
    {
        return;
    }

    cmdList.enqueueLambda([texture = gp::RefCountedPtr<VulkanTexture>(this)](command::CommandListBase&) noexcept
    {
        texture->m_services.memoryManager->setPoolAllocationOwner(texture->m_allocation, texture.get());
    });
}

bool VulkanTexture::onAllocationMoved(const allocator::PoolMoveRequest& request) noexcept
{
    GP_CHECK(m_allocation.isPoolAllocated != 0u);

    const resource::TextureDesc& desc = getDesc();
    const bool isMemoryless = hasAnyTextureFlags(desc.flags, resource::TextureCreateFlags::Memoryless);
    const bool isExternal = hasAnyTextureFlags(desc.flags, resource::TextureCreateFlags::External);

    // Only plain, optimally tiled, single-sampled images can be copied without losing anything.
    if (m_ownerType != VulkanImageOwnerType::LocalOwner || m_cpuReadbackBuffer.isValid() || isExternal ||
        isMemoryless || isTargetableTexture(desc.flags) || desc.numSamples > 1u || m_tiling != VK_IMAGE_TILING_OPTIMAL)
    {
        return false;
    }

    if (m_services.contextProvider == nullptr)
    {
        return false;
    }

    // Transfers are illegal inside a render pass.
    command::IComputeContext* const graphicsContext = request.contexts[sync::PipelineStage::Graphics];
    IVulkanTransferCommandRecorder* const recorder = m_services.contextProvider->getContextRecorder(graphicsContext);
    if (recorder == nullptr || !recorder->isOutsideRenderPass())
    {
        return false;
    }

    // Async compute may still read the old image.
    const sync::PipelineStage trackedPipelines = m_services.contextProvider->getTrackedPipelines(graphicsContext, this);
    if ((trackedPipelines & sync::PipelineStage::AsyncCompute) != sync::PipelineStage::None)
    {
        return false;
    }

    VulkanMemoryManager& memoryManager = *m_services.memoryManager;
    const VkDevice deviceHandle = m_services.device->getHandle();

    VkImage newImage = VK_NULL_HANDLE;
    if (vkCreateImage(deviceHandle, &m_imageCreateInfo.imageCreateInfo, GP_VULKAN_CPU_ALLOCATOR, &newImage) !=
        VK_SUCCESS)
    {
        return false;
    }

    VulkanAllocation destAllocation{};
    memoryManager.initPoolAllocation(destAllocation, m_allocation, *request.newData);
    GP_CHECK(destAllocation.size >= m_allocation.size);
    if (destAllocation.bindImage(memoryManager, newImage) != VK_SUCCESS)
    {
        vkDestroyImage(deviceHandle, newImage, GP_VULKAN_CPU_ALLOCATOR);
        return false;
    }

    const VkImageSubresourceRange fullRange = VulkanPipelineBarrier::makeSubresourceRange(m_fullAspectMask);
    const VkImageLayout currentLayout = getTrackedLayout();
    const gp::UInt32 layerCount = getVulkanTextureLayerCount(desc);
    const gp::UInt32 mipCount = gp::math::max<gp::UInt32>(1u, desc.numMips);

    VkImageCopy regions[kMaxTextureMipCount]{};
    for (gp::UInt32 mipIndex = 0u; mipIndex < mipCount; ++mipIndex)
    {
        const resource::IntVector mipExtent = resource::calculateMipExtent(desc, mipIndex);

        VkImageCopy& region = regions[mipIndex];
        region.srcSubresource.aspectMask = m_fullAspectMask;
        region.srcSubresource.mipLevel = mipIndex;
        region.srcSubresource.baseArrayLayer = 0u;
        region.srcSubresource.layerCount = layerCount;
        region.dstSubresource = region.srcSubresource;
        region.extent.width = static_cast<gp::UInt32>(mipExtent.x);
        region.extent.height = static_cast<gp::UInt32>(mipExtent.y);
        region.extent.depth = static_cast<gp::UInt32>(mipExtent.z);
    }

    recorder->imageLayoutTransition(m_image, currentLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, fullRange);
    recorder->imageLayoutTransition(
        newImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, fullRange
    );
    recorder->copyImage(
        m_image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        newImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        gp::ConstVectorView<VkImageCopy>(regions, mipCount)
    );
    recorder->imageLayoutTransition(newImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, currentLayout, fullRange);

    // The old image may still be referenced by in-flight work; its memory range now belongs to the pool again.
    destroyViews();
    m_services.enqueueImageDeletion(m_image);
    m_image = newImage;

    m_allocation.offset = destAllocation.offset;
    m_allocation.vulkanHandle = destAllocation.vulkanHandle;
    m_allocation.handleId = destAllocation.handleId;

    createViews();
    updateLinkedViews(request.contexts);
    return true;
}

void VulkanTexture::createCpuReadbackBuffer() noexcept
{
    const resource::TextureDesc& desc = getDesc();
    GP_CHECKF(desc.isTexture2D(), "CPU readback textures must be 2D.");
    GP_CHECKF(desc.numSamples <= 1u, "CPU readback textures cannot be multisampled.");
    GP_CHECKF(desc.numMips <= kMaxTextureMipCount, "Too many mips for a CPU readback texture.");

    m_cpuReadbackBuffer = gp::makeUnique<VulkanCpuReadbackBuffer>();
    VulkanCpuReadbackBuffer& readback = *m_cpuReadbackBuffer;

    // Every mip is packed tightly, starting on a 16-byte boundary.
    gp::UInt64 totalSize = 0u;
    const gp::UInt32 mipCount = gp::math::max<gp::UInt32>(1u, desc.numMips);
    for (gp::UInt32 mipIndex = 0u; mipIndex < mipCount; ++mipIndex)
    {
        const gp::UInt64 mipSize = resource::calculateTextureMipSize(desc, mipIndex);
        readback.mipOffsets[mipIndex] = static_cast<gp::UInt32>(totalSize);
        readback.mipSizes[mipIndex] = static_cast<gp::UInt32>(mipSize);
        totalSize += gp::memory::align(mipSize, static_cast<gp::UInt64>(kCpuReadbackMipAlignment));
    }

    m_viewFormat = resolveVulkanTextureFormat(*m_services.device, desc.format, false);
    m_storageFormat = m_viewFormat;
    m_fullAspectMask = getVkImageAspectFlags(m_viewFormat, true, true);
    m_partialAspectMask = getVkImageAspectFlags(m_viewFormat, false, true);
    m_tiling = VK_IMAGE_TILING_LINEAR;
    m_ownerType = VulkanImageOwnerType::LocalOwner;

    if (totalSize == 0u)
    {
        GP_LOG_ERROR(LogVulkanRHI, "CPU readback texture has an empty mip chain.");
        return;
    }

    readback.buffer = createVulkanBuffer(*m_services.device, totalSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    if (readback.buffer == VK_NULL_HANDLE)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Failed to create a CPU readback buffer of {} bytes.", totalSize);
        return;
    }

    VulkanMemoryManager& memoryManager = *m_services.memoryManager;
    const VulkanAllocationFlags allocFlags =
        VulkanAllocationFlags::HostVisible | VulkanAllocationFlags::HostCached | VulkanAllocationFlags::AutoBind;
    if (!memoryManager.allocateBufferMemory(
            m_allocation, readback.buffer, allocFlags, nullptr, kCpuReadbackMipAlignment
        ))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Out of memory allocating a CPU readback buffer of {} bytes.", totalSize);
        m_services.enqueueBufferDeletion(readback.buffer);
        readback.buffer = VK_NULL_HANDLE;
        return;
    }

    m_memoryRequirements.size = totalSize;
    m_memoryRequirements.alignment = kCpuReadbackMipAlignment;

    // Start from known content: reads before the first copy return zeros.
    void* const mappedData = m_allocation.getMappedPointer(memoryManager);
    if (mappedData != nullptr)
    {
        gp::memory::zeroMemory(mappedData, static_cast<gp::USize>(totalSize));
        m_allocation.flushMappedMemory(memoryManager);
    }

    updateVulkanTextureStats(m_statCategory, totalSize, true);
}

bool VulkanTexture::createImage() noexcept
{
    const resource::TextureDesc& desc = getDesc();
    VulkanMemoryManager& memoryManager = *m_services.memoryManager;
    const VkDevice deviceHandle = m_services.device->getHandle();
    const bool supportsMemoryless = supportsMemorylessImages(m_services);

    if (!generateVulkanImageCreateInfo(
            *m_services.device, desc, false, supportsMemoryless, m_imageCreateInfo, m_viewFormat, m_storageFormat
        ))
    {
        return false;
    }

    m_imageUsageFlags = m_imageCreateInfo.imageCreateInfo.usage;
    m_tiling = m_imageCreateInfo.imageCreateInfo.tiling;

    if (vkCreateImage(deviceHandle, &m_imageCreateInfo.imageCreateInfo, GP_VULKAN_CPU_ALLOCATOR, &m_image) !=
        VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkCreateImage failed for a {}x{} texture.", desc.extent.x, desc.extent.y);
        m_image = VK_NULL_HANDLE;
        return false;
    }

    m_fullAspectMask = getVkImageAspectFlags(m_viewFormat, true, true);
    m_partialAspectMask = getVkImageAspectFlags(m_viewFormat, false, true);

    const bool isMemoryless =
        supportsMemoryless && hasAnyTextureFlags(desc.flags, resource::TextureCreateFlags::Memoryless);
    const bool isRenderTarget = isTargetableTexture(desc.flags);
    const bool isUAV = hasAnyTextureFlags(desc.flags, resource::TextureCreateFlags::UAV);

    VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (isMemoryless)
    {
        memoryFlags |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    }
    if (m_tiling != VK_IMAGE_TILING_OPTIMAL)
    {
        memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }

    const VulkanAllocationMetaType metaType =
        (isRenderTarget || isUAV) ? VulkanAllocationMetaType::ImageRenderTarget : VulkanAllocationMetaType::ImageOther;

    vkGetImageMemoryRequirements(deviceHandle, m_image, &m_memoryRequirements);

    // Render targets get their own allocation (drivers may place them in special heaps), so do large images that
    // would fragment the pools. Tile memory is never dedicated.
    const bool isDedicated =
        !isMemoryless && (isRenderTarget || m_memoryRequirements.size >= kDedicatedImageAllocationThreshold);
    const bool isAllocated =
        isDedicated ? memoryManager.allocateDedicatedImageMemory(m_allocation, m_image, memoryFlags, metaType)
                    : memoryManager.allocateImageMemory(m_allocation, m_memoryRequirements, memoryFlags, metaType);
    if (!isAllocated)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Out of GPU memory allocating a texture of {} bytes.", m_memoryRequirements.size);
        vkDestroyImage(deviceHandle, m_image, GP_VULKAN_CPU_ALLOCATOR);
        m_image = VK_NULL_HANDLE;
        return false;
    }

    if (m_allocation.bindImage(memoryManager, m_image) != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkBindImageMemory failed for a texture of {} bytes.", m_memoryRequirements.size);
        memoryManager.freeVulkanAllocation(m_allocation, VulkanFreeFlags::DontDefer);
        vkDestroyImage(deviceHandle, m_image, GP_VULKAN_CPU_ALLOCATOR);
        m_image = VK_NULL_HANDLE;
        return false;
    }

    m_ownerType = VulkanImageOwnerType::LocalOwner;
    updateVulkanTextureStats(m_statCategory, m_memoryRequirements.size, true);
    return true;
}

VkImageView VulkanTexture::createView(const VkImageAspectFlags aspectMask) const noexcept
{
    const resource::TextureDesc& desc = getDesc();
    VulkanDevice& device = *m_services.device;

    VkImageViewCreateInfo viewInfo = zeroVulkanStruct<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
    viewInfo.image = m_image;
    viewInfo.viewType = textureDimensionToVkImageViewType(desc.dimension);
    viewInfo.format = m_viewFormat;
    viewInfo.subresourceRange = VulkanPipelineBarrier::makeSubresourceRange(
        aspectMask, 0u, gp::math::max<gp::UInt32>(1u, desc.numMips), 0u, getVulkanTextureLayerCount(desc)
    );

    // Swizzles only matter when sampling; attachments and storage views require the identity mapping.
    if (supportsSampling())
    {
        viewInfo.components = device.getFormatComponentMapping(desc.format);
    }
    else
    {
        viewInfo.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_IDENTITY,
                                                  VK_COMPONENT_SWIZZLE_IDENTITY,
                                                  VK_COMPONENT_SWIZZLE_IDENTITY,
                                                  VK_COMPONENT_SWIZZLE_IDENTITY };
    }

    // Storage usage is inherited from the image, but sRGB (or any format without storage support) cannot be written
    // as a storage image. Restrict the view usage so the view stays valid.
    VkImageViewUsageCreateInfo usageInfo =
        zeroVulkanStruct<VkImageViewUsageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO);
    if ((m_imageUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT) != 0u)
    {
        const VkFormatProperties formatProperties = device.getFormatProperties(m_viewFormat);
        const VkFormatFeatureFlags features = (m_tiling == VK_IMAGE_TILING_OPTIMAL)
                                                  ? formatProperties.optimalTilingFeatures
                                                  : formatProperties.linearTilingFeatures;
        const bool supportsStorage = (features & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0u;
        if (!supportsStorage || (m_viewFormat != m_storageFormat))
        {
            usageInfo.usage = m_imageUsageFlags & ~static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_STORAGE_BIT);
            viewInfo.pNext = &usageInfo;
        }
    }

    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(device.getHandle(), &viewInfo, GP_VULKAN_CPU_ALLOCATOR, &view) != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkCreateImageView failed (format {}).", static_cast<gp::Int32>(m_viewFormat));
        return VK_NULL_HANDLE;
    }
    return view;
}

void VulkanTexture::createViews() noexcept
{
    if (m_image == VK_NULL_HANDLE)
    {
        return;
    }

    m_defaultView = createView(m_fullAspectMask);
    m_partialView = (m_partialAspectMask != m_fullAspectMask) ? createView(m_partialAspectMask) : m_defaultView;
}

void VulkanTexture::destroyViews() noexcept
{
    {
        // Each view hands its native object to the deferred deletion queue.
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_internalViewsMutex);
        m_internalViews.clear();
    }

    if (m_partialView != m_defaultView)
    {
        m_services.enqueueImageViewDeletion(m_partialView);
    }
    m_services.enqueueImageViewDeletion(m_defaultView);
    m_partialView = VK_NULL_HANDLE;
    m_defaultView = VK_NULL_HANDLE;
}

void VulkanTexture::releaseStorage() noexcept
{
    destroyViews();

    if (m_ownerType == VulkanImageOwnerType::LocalOwner)
    {
        if (m_cpuReadbackBuffer.isValid())
        {
            m_services.enqueueBufferDeletion(m_cpuReadbackBuffer->buffer);
        }
        else
        {
            m_services.enqueueImageDeletion(m_image);
        }

        if (m_allocation.hasAllocation())
        {
            updateVulkanTextureStats(m_statCategory, m_memoryRequirements.size, false);
            m_services.memoryManager->freeVulkanAllocation(m_allocation);
        }
    }

    m_allocation = VulkanAllocation{};
    m_cpuReadbackBuffer.reset();
    m_aliasedSource.reset();
    m_image = VK_NULL_HANDLE;
    m_ownerType = VulkanImageOwnerType::None;
}

void VulkanTexture::setInitialImageState(command::CommandListBase& cmdList, const bool shouldClear) noexcept
{
    // The texture is not reference counted yet; the deferred work captures handles and values only.
    const VkImageSubresourceRange fullRange = VulkanPipelineBarrier::makeSubresourceRange(m_fullAspectMask);
    const bool isDepthStencil = isDepthOrStencilAspect();
    const VkImage image = m_image;
    const VkImageLayout finalLayout = m_defaultLayout;
    IVulkanCommandContextProvider* const provider = m_services.contextProvider;
    GP_CHECKF(provider != nullptr, "Setting the initial texture layout requires a command context provider.");

    VkClearColorValue clearColor{};
    VkClearDepthStencilValue clearDepthStencil{};
    if (shouldClear)
    {
        if (isDepthStencil)
        {
            getDepthStencilClearValue(clearDepthStencil.depth, clearDepthStencil.stencil);
        }
        else
        {
            const gp::math::LinearColor color = getClearColor();
            clearColor.float32[0] = color.r;
            clearColor.float32[1] = color.g;
            clearColor.float32[2] = color.b;
            clearColor.float32[3] = color.a;
        }
    }

    cmdList.enqueueLambda(
        [provider, image, fullRange, finalLayout, shouldClear, isDepthStencil, clearColor, clearDepthStencil](
            command::CommandListBase& executingCmdList
        ) noexcept
    {
        IVulkanTransferCommandRecorder* const recorder = provider->getUploadRecorder(executingCmdList);
        GP_CHECK(recorder != nullptr);

        VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (shouldClear)
        {
            recorder->imageLayoutTransition(image, currentLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, fullRange);
            currentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

            if (isDepthStencil)
            {
                recorder->clearDepthStencilImage(image, currentLayout, clearDepthStencil, fullRange);
            }
            else
            {
                recorder->clearColorImage(image, currentLayout, clearColor, fullRange);
            }
        }

        if (currentLayout != finalLayout)
        {
            recorder->imageLayoutTransition(image, currentLayout, finalLayout, fullRange);
        }
    }
    );
}

}   // namespace gp::rhi::vulkan
