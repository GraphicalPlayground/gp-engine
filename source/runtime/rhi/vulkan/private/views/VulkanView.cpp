// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "views/VulkanView.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "memory/MemoryOrder.hpp"
#include "memory/VulkanAllocation.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/VulkanBuffer.hpp"
#include "resources/VulkanBufferTypes.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "views/VulkanImageViewCreateInfo.hpp"
#include "views/VulkanTextureViewDesc.hpp"
#include "views/VulkanViewTranslation.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Source of `VulkanTextureView::viewId`; `0` is reserved for "not cacheable".
gp::thread::Atomic<gp::UInt32> g_imageViewIdCounter{ 0u };

/// @brief Source of `VulkanTypedBufferView::viewId`; `0` is reserved for "not cacheable".
gp::thread::Atomic<gp::UInt32> g_bufferViewIdCounter{ 0u };

/// @brief Serializes the lazy `VkBufferView` creation of bindless typed buffer views.
gp::thread::Mutex g_typedBufferViewCreationMutex{};

/// @brief Returns the next non-zero id from `counter`.
/// @param[in,out] counter Id counter to advance.
/// @return New unique id.
[[nodiscard]] gp::UInt32 allocateViewId(gp::thread::Atomic<gp::UInt32>& counter) noexcept
{
    return counter.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed) + 1u;
}

}   // namespace

VulkanView::VulkanView(const VulkanResourceServices& services, const view::DescriptorType descriptorType) noexcept
    : m_services(services)
    , m_descriptorType(descriptorType)
{
    GP_CHECKF(m_services.device != nullptr, "VulkanView needs a device.");

    // Views used strictly as attachments pass `Invalid` and never get a bindless slot. The others would allocate one
    // here from the bindless descriptor manager; until it exists `m_bindlessHandle` stays invalid and descriptor sets
    // are written from the native state.
}

VulkanView::~VulkanView() noexcept
{
    invalidate();

    // A valid bindless slot would be returned to the deferred deletion queue here, once in-flight work is done.
    m_bindlessHandle = view::DescriptorHandle{};
}

void VulkanView::invalidate() noexcept
{
    const bool wasInitialized = isInitialized();

    switch (m_kind)
    {
    case VulkanViewKind::Null:
        break;

    case VulkanViewKind::TypedBuffer:
        m_services.enqueueBufferViewDeletion(m_storage.typedBuffer.view);
        break;

    case VulkanViewKind::Texture:
        m_services.enqueueImageViewDeletion(m_storage.texture.view);
        break;

    case VulkanViewKind::StructuredBuffer:
        // A storage buffer range owns no native object.
        break;

    case VulkanViewKind::AccelerationStructure:
        m_services.enqueueAccelerationStructureDeletion(m_storage.accelerationStructure.handle);
        break;

    default:
        GP_CHECKF(false, "Unhandled VulkanViewKind in VulkanView::invalidate().");
        break;
    }

    m_kind = VulkanViewKind::Null;
    m_storage.invalidated = VulkanInvalidatedViewState{ wasInitialized };
}

const VulkanTextureView& VulkanView::getTextureView() const noexcept
{
    GP_CHECK(m_kind == VulkanViewKind::Texture);
    return m_storage.texture;
}

const VulkanStructuredBufferView& VulkanView::getStructuredBufferView() const noexcept
{
    GP_CHECK(m_kind == VulkanViewKind::StructuredBuffer);
    return m_storage.structuredBuffer;
}

const VulkanAccelerationStructureView& VulkanView::getAccelerationStructureView() const noexcept
{
    GP_CHECK(m_kind == VulkanViewKind::AccelerationStructure);
    return m_storage.accelerationStructure;
}

const VulkanTypedBufferView& VulkanView::getOrCreateTypedBufferView() noexcept
{
    GP_CHECK(m_kind == VulkanViewKind::TypedBuffer);
    VulkanTypedBufferView& typedBufferView = m_storage.typedBuffer;

    // Only bindless views defer the creation; parallel contexts may race to create it, hence the lock.
    if (m_bindlessHandle.isValid())
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(g_typedBufferViewCreationMutex);
        if (typedBufferView.view == VK_NULL_HANDLE)
        {
            createTypedBufferViewHandle(typedBufferView);
        }
    }

    return typedBufferView;
}

void VulkanView::createTextureView(const VkImage image, const VulkanTextureViewDesc& viewDesc) noexcept
{
    initAsTextureView(allocator::ContextArray{}, image, viewDesc);
}

void VulkanView::updateTextureView(
    const allocator::ContextArray& contexts, const VkImage image, const VulkanTextureViewDesc& viewDesc
) noexcept
{
    initAsTextureView(contexts, image, viewDesc);
}

void VulkanView::initAsTypedBufferView(
    const allocator::ContextArray& contexts,
    const VulkanBuffer& buffer,
    const PixelFormat format,
    const gp::UInt32 offset,
    const gp::UInt32 size
) noexcept
{
    // The contexts are only needed to update an in-use bindless descriptor, which is not wired yet.
    static_cast<void>(contexts);

    GP_CHECKF(m_kind == VulkanViewKind::Null, "VulkanView must be invalidated before it is initialized again.");

    const VkBufferViewCreateInfo createInfo =
        translateVulkanTypedBufferViewDesc(*m_services.device, buffer, format, offset, size);

    VulkanTypedBufferView typedBufferView{};
    typedBufferView.buffer = createInfo.buffer;
    typedBufferView.format = createInfo.format;
    typedBufferView.offset = static_cast<gp::UInt32>(createInfo.offset);
    typedBufferView.range = static_cast<gp::UInt32>(createInfo.range);

    // Volatile buffers get a new allocation on every lock, so a descriptor set keyed on their view would never be
    // reused: leave them out of the cache.
    if ((buffer.getDesc().usage & resource::BufferUsageFlags::Volatile) == resource::BufferUsageFlags::None)
    {
        typedBufferView.viewId = allocateViewId(g_bufferViewIdCounter);
    }

    // Bindless descriptors are built from the buffer address, so only descriptor-set views need the handle now.
    if (!m_bindlessHandle.isValid())
    {
        createTypedBufferViewHandle(typedBufferView);
    }

    m_storage.typedBuffer = typedBufferView;
    m_kind = VulkanViewKind::TypedBuffer;
}

void VulkanView::initAsStructuredBufferView(
    const allocator::ContextArray& contexts, const VulkanBuffer& buffer, const gp::UInt32 offset, const gp::UInt32 size
) noexcept
{
    static_cast<void>(contexts);

    GP_CHECKF(m_kind == VulkanViewKind::Null, "VulkanView must be invalidated before it is initialized again.");

    VulkanStructuredBufferView structuredBufferView{};
    structuredBufferView.buffer = buffer.getHandle();
    structuredBufferView.handleId = buffer.getCurrentBufferAlloc().alloc.handleId;
    structuredBufferView.offset = buffer.getOffset() + offset;
    structuredBufferView.size = getVulkanStructuredBufferViewSize(buffer, size);

    m_storage.structuredBuffer = structuredBufferView;
    m_kind = VulkanViewKind::StructuredBuffer;
}

void VulkanView::initAsAccelerationStructureView(
    const allocator::ContextArray& contexts, const VulkanBuffer& buffer, const gp::UInt32 offset, const gp::UInt32 size
) noexcept
{
    static_cast<void>(contexts);

    GP_CHECKF(m_kind == VulkanViewKind::Null, "VulkanView must be invalidated before it is initialized again.");
    GP_CHECKF(
        dynamic_api::vkCreateAccelerationStructureKHR != nullptr,
        "Acceleration structure views need VK_KHR_acceleration_structure."
    );

    VkAccelerationStructureCreateInfoKHR createInfo = zeroVulkanStruct<VkAccelerationStructureCreateInfoKHR>(
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR
    );
    createInfo.buffer = buffer.getHandle();
    createInfo.offset = static_cast<VkDeviceSize>(buffer.getOffset()) + static_cast<VkDeviceSize>(offset);
    createInfo.size = static_cast<VkDeviceSize>(size);
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VulkanAccelerationStructureView accelerationStructureView{};
    if (dynamic_api::vkCreateAccelerationStructureKHR != nullptr)
    {
        const VkResult result = dynamic_api::vkCreateAccelerationStructureKHR(
            m_services.device->getHandle(), &createInfo, GP_VULKAN_CPU_ALLOCATOR, &accelerationStructureView.handle
        );
        if (result != VK_SUCCESS)
        {
            GP_LOG_ERROR(LogVulkanRHI, "vkCreateAccelerationStructureKHR failed ({}).", static_cast<gp::Int32>(result));
            accelerationStructureView.handle = VK_NULL_HANDLE;
        }
    }

    m_storage.accelerationStructure = accelerationStructureView;
    m_kind = VulkanViewKind::AccelerationStructure;
}

void VulkanView::initAsTextureView(
    const allocator::ContextArray& contexts, const VkImage image, const VulkanTextureViewDesc& viewDesc
) noexcept
{
    static_cast<void>(contexts);

    GP_CHECKF(m_kind == VulkanViewKind::Null, "VulkanView must be invalidated before it is initialized again.");
    GP_CHECKF(image != VK_NULL_HANDLE, "Texture views need an image (CPU readback textures have none).");

    const VulkanImageViewCreateInfo createInfo(*m_services.device, image, viewDesc);

    VulkanTextureView textureView{};
    textureView.image = image;
    const VkResult result = vkCreateImageView(
        m_services.device->getHandle(), &createInfo.createInfo, GP_VULKAN_CPU_ALLOCATOR, &textureView.view
    );
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(
            LogVulkanRHI,
            "vkCreateImageView failed ({}, format {}).",
            static_cast<gp::Int32>(result),
            static_cast<gp::Int32>(viewDesc.format)
        );
        textureView.view = VK_NULL_HANDLE;
    }
    textureView.viewId = allocateViewId(g_imageViewIdCounter);

    m_storage.texture = textureView;
    m_kind = VulkanViewKind::Texture;
}

void VulkanView::createTypedBufferViewHandle(VulkanTypedBufferView& typedBufferView) const noexcept
{
    GP_CHECK(typedBufferView.view == VK_NULL_HANDLE);

    VkBufferViewCreateInfo createInfo =
        zeroVulkanStruct<VkBufferViewCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO);
    createInfo.buffer = typedBufferView.buffer;
    createInfo.format = typedBufferView.format;
    createInfo.offset = static_cast<VkDeviceSize>(typedBufferView.offset);
    createInfo.range = static_cast<VkDeviceSize>(typedBufferView.range);

    const VkResult result =
        vkCreateBufferView(m_services.device->getHandle(), &createInfo, GP_VULKAN_CPU_ALLOCATOR, &typedBufferView.view);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(
            LogVulkanRHI,
            "vkCreateBufferView failed ({}, format {}).",
            static_cast<gp::Int32>(result),
            static_cast<gp::Int32>(typedBufferView.format)
        );
        typedBufferView.view = VK_NULL_HANDLE;
    }
}

}   // namespace gp::rhi::vulkan
