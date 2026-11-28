// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanUniformBuffer.hpp"
#include "commands/CommandListBase.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/Resource.hpp"
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "memory/VulkanMemoryManager.hpp"
#include "memory/VulkanTempBlockAllocator.hpp"
#include "memory/VulkanUniformBufferMemory.hpp"
#include "resources/UniformBufferLayout.hpp"
#include "resources/VulkanUniformBufferData.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

VulkanUniformBuffer::VulkanUniformBuffer(
    const VulkanResourceServices& services,
    const resource::UniformBufferLayout* const layout,
    const void* const contents,
    const resource::UniformBufferUsage usage,
    const resource::UniformBufferValidation validation
) noexcept
    : resource::UniformBuffer(layout)
    , m_services(services)
    , m_usage(usage)
{
    GP_CHECK(layout != nullptr);
    GP_CHECK(m_services.isValid());

    // A uniform buffer always has either a constant block or at least one resource slot.
    const gp::USize numResources = layout->getResources().size();
    GP_CHECK((numResources > 0u) || (layout->getConstantBufferSize() > 0u));

    if ((validation == resource::UniformBufferValidation::ValidateResources) && (contents != nullptr))
    {
        static_cast<void>(validateVulkanUniformBufferResources(*layout, contents));
    }

    if (numResources > 0u)
    {
        m_resourceTable.resize(numResources);
        if (contents != nullptr)
        {
            updateResourceTable(*layout, contents, numResources);
        }
    }

    if (layout->getConstantBufferSize() > 0u)
    {
        allocateMemory(contents);
    }
}

VulkanUniformBuffer::~VulkanUniformBuffer() noexcept
{
    if (m_services.memoryManager != nullptr)
    {
        freeVulkanUniformBufferMemory(
            *m_services.memoryManager, m_services.deferredDeletionQueue, m_services.tempBlockAllocator, m_allocation
        );
    }
    m_mappedConstants = nullptr;
}

void VulkanUniformBuffer::allocateMemory(const void* const initialData) noexcept
{
    const gp::UInt32 dataSize = getLayout().getConstantBufferSize();
    VulkanMemoryManager& memoryManager = *m_services.memoryManager;

    // Release the previous range; recorded commands keep reading it until the GPU is done.
    freeVulkanUniformBufferMemory(
        memoryManager, m_services.deferredDeletionQueue, m_services.tempBlockAllocator, m_allocation
    );
    m_mappedConstants = nullptr;

    const bool isVolatile =
        (m_usage == resource::UniformBufferUsage::SingleDraw) || (m_usage == resource::UniformBufferUsage::SingleFrame);
    if (isVolatile && (m_services.tempBlockAllocator != nullptr))
    {
        const gp::UInt32 alignment = getVulkanUniformBufferAlignment(*m_services.device);
        m_mappedConstants = m_services.tempBlockAllocator->allocate(dataSize, alignment, nullptr, m_allocation);
    }
    else if (allocateVulkanUniformBufferMemory(memoryManager, m_allocation, dataSize))
    {
        m_mappedConstants = m_allocation.getMappedPointer(memoryManager);
    }

    if ((m_mappedConstants != nullptr) && (initialData != nullptr))
    {
        // Bindless indices are patched once a bindless descriptor manager provides valid handles.
        updateVulkanUniformBufferConstants(m_mappedConstants, initialData, getLayout(), false);
        m_allocation.flushMappedMemory(memoryManager);
    }
}

void VulkanUniformBuffer::updateResourceTable(
    const resource::UniformBufferLayout& layout, const void* const contents, const gp::USize numResources
) noexcept
{
    GP_CHECK(m_resourceTable.size() == numResources);

    const gp::ConstVectorView<resource::UniformBufferResource> members = layout.getResources();
    for (gp::USize index = 0u; index < numResources; ++index)
    {
        m_resourceTable[index] = getVulkanUniformBufferResource(contents, members[index]);
    }
}

void VulkanUniformBuffer::updateResourceTable(const gp::ConstVectorView<resource::Resource*> resources) noexcept
{
    GP_CHECK(resources.size() <= m_resourceTable.size());

    for (gp::USize index = 0u; index < resources.size(); ++index)
    {
        m_resourceTable[index] = resources[index];
    }
}

VkDeviceAddress VulkanUniformBuffer::getDeviceAddress() const noexcept
{
    const VulkanDevice& device = *m_services.device;
    if ((device.getOptionalExtensions().hasBufferDeviceAddress == 0u) ||
        (dynamic_api::vkGetBufferDeviceAddressKHR == nullptr) || (getBufferHandle() == VK_NULL_HANDLE))
    {
        return 0u;
    }

    VkBufferDeviceAddressInfo addressInfo =
        zeroVulkanStruct<VkBufferDeviceAddressInfo>(VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO);
    addressInfo.buffer = getBufferHandle();
    const VkDeviceAddress bufferAddress = dynamic_api::vkGetBufferDeviceAddressKHR(device.getHandle(), &addressInfo);
    return bufferAddress + getOffset();
}

view::DescriptorHandle VulkanUniformBuffer::getBindlessHandle() const noexcept
{
    return m_bindlessHandle;
}

resource::UniformBufferRef createVulkanUniformBuffer(
    const VulkanResourceServices& services,
    const void* const contents,
    const resource::UniformBufferLayout* const layout,
    const resource::UniformBufferUsage usage,
    const resource::UniformBufferValidation validation
) noexcept
{
    if (!services.isValid() || (layout == nullptr))
    {
        return resource::UniformBufferRef{};
    }

    return resource::UniformBufferRef(new VulkanUniformBuffer(services, layout, contents, usage, validation));
}

void updateVulkanUniformBuffer(
    command::CommandListBase& cmdList, resource::UniformBuffer* const uniformBuffer, const void* const contents
) noexcept
{
    GP_CHECK(uniformBuffer != nullptr);
    VulkanUniformBuffer* const vulkanUniformBuffer = static_cast<VulkanUniformBuffer*>(uniformBuffer);

    const resource::UniformBufferLayout& layout = vulkanUniformBuffer->getLayout();
    const gp::UInt32 constantBufferSize = layout.getConstantBufferSize();
    const bool isBypass = cmdList.bypass();

    // Read the resource slots now: the caller may reuse `contents` as soon as this function returns.
    const gp::ConstVectorView<resource::UniformBufferResource> members = layout.getResources();
    const gp::USize numResources = members.size();
    resource::Resource** resources = nullptr;
    if (numResources > 0u)
    {
        resources = static_cast<resource::Resource**>(cmdList.alloc(
            sizeof(resource::Resource*) * numResources, static_cast<gp::UInt32>(alignof(resource::Resource*))
        ));
        for (gp::USize index = 0u; index < numResources; ++index)
        {
            resources[index] = getVulkanUniformBufferResource(contents, members[index]);
        }
    }

    // Keep the resources alive while the update waits in the command list.
    if (!isBypass)
    {
        for (gp::USize index = 0u; index < numResources; ++index)
        {
            if (resources[index] != nullptr)
            {
                static_cast<void>(resources[index]->addRef());
            }
        }
    }

    const void* const tempContents = (isBypass || (constantBufferSize == 0u) || (contents == nullptr))
                                         ? contents
                                         : cmdList.allocCopy(contents, constantBufferSize, 16u);

    cmdList.enqueueLambda([uniformBufferRef = gp::RefCountedPtr<VulkanUniformBuffer>(vulkanUniformBuffer),
                           tempContents,
                           constantBufferSize,
                           isBypass,
                           resources,
                           numResources](command::CommandListBase&) noexcept
    {
        if (constantBufferSize > 0u)
        {
            uniformBufferRef->allocateMemory(tempContents);
        }

        uniformBufferRef->updateResourceTable(gp::ConstVectorView<resource::Resource*>(resources, numResources));
        if (!isBypass)
        {
            for (gp::USize index = 0u; index < numResources; ++index)
            {
                if (resources[index] != nullptr)
                {
                    static_cast<void>(resources[index]->release());
                }
            }
        }
    });
}

}   // namespace gp::rhi::vulkan
