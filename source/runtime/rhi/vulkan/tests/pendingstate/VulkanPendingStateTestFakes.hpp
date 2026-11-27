// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "descriptors/VulkanDescriptorPoolsManager.hpp"
#include "descriptors/VulkanDescriptorSetsLayout.hpp"
#include "pendingstate/IVulkanPendingStateContext.hpp"
#include "pendingstate/VulkanDescriptorPoolSetAcquirer.hpp"
#include "pendingstate/VulkanPendingStateTypes.hpp"
#include "shaders/VulkanShaderHeader.hpp"
#include "shaders/VulkanShaderHeaderEnums.hpp"
#include "views/VulkanViewTypes.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <cstdint>
#include <initializer_list>

namespace gp::rhi::vulkan::tests
{

/// @brief Returns a fake non-dispatchable handle; never passed to the driver.
/// @tparam InHandle Vulkan handle type.
/// @param[in] value Non-zero handle value.
/// @return Handle.
template <typename InHandle>
[[nodiscard]] inline InHandle makeFakePendingHandle(const std::uintptr_t value) noexcept
{
    return reinterpret_cast<InHandle>(value);
}

/// @brief Builds a shader header from its binding types.
/// @param[in] types Descriptor type of each binding, in binding order.
/// @param[in] numBoundUniformBuffers Number of leading uniform buffers bound as buffers.
/// @param[in] packedGlobalsSize Non-zero when binding 0 is the packed globals buffer.
/// @return Shader header.
[[nodiscard]] inline VulkanShaderHeader makePendingHeader(
    const std::initializer_list<VkDescriptorType> types,
    const gp::UInt32 numBoundUniformBuffers = 0u,
    const gp::UInt32 packedGlobalsSize = 0u
)
{
    VulkanShaderHeader header{};
    for (const VkDescriptorType type: types)
    {
        header.bindings.pushBack(VulkanShaderBindingInfo{ static_cast<gp::UInt32>(type) });
    }
    header.numBoundUniformBuffers = numBoundUniformBuffers;
    header.packedGlobalsSize = packedGlobalsSize;
    return header;
}

/// @brief Returns limits generous enough for every pending state test layout.
/// @return Device limits.
[[nodiscard]] inline VkPhysicalDeviceLimits makePendingLimits() noexcept
{
    VkPhysicalDeviceLimits limits{};
    limits.maxDescriptorSetSamplers = 16u;
    limits.maxDescriptorSetUniformBuffers = 16u;
    limits.maxDescriptorSetUniformBuffersDynamic = 8u;
    limits.maxDescriptorSetStorageBuffers = 16u;
    limits.maxDescriptorSetStorageBuffersDynamic = 4u;
    limits.maxDescriptorSetSampledImages = 16u;
    limits.maxDescriptorSetStorageImages = 16u;
    limits.maxDescriptorSetInputAttachments = 4u;
    return limits;
}

/// @brief Command context services backed by a real pool manager and a host-memory uniform ring.
/// @details Records every call so tests can check what a flush asked for. Uniform allocations are carved from a fixed
///          host buffer starting at `kFirstUniformOffset`, so a non-zero offset proves the state forwards it.
class FakePendingStateContext final : public IVulkanPendingStateContext
{
public:
    /// @brief Offset of the first uniform allocation.
    static constexpr gp::UInt32 kFirstUniformOffset = 256u;

    /// @brief Alignment of every uniform allocation.
    static constexpr gp::UInt32 kUniformAlignment = 256u;

    /// @brief Capacity of the host uniform buffer.
    static constexpr gp::UInt32 kUniformCapacity = 16u * 1024u;

    /// @brief Creates the context over a pool manager.
    /// @param[in] manager Pool manager; must outlive the context.
    explicit FakePendingStateContext(VulkanDescriptorPoolsManager& manager) noexcept
        : m_acquirer(manager)
    {
        m_uniformMemory.resize(static_cast<gp::USize>(kUniformCapacity), gp::UInt8{ 0u });
    }

    /// @copydoc IVulkanPendingStateContext::acquirePoolSetAndDescriptorsIfNeeded()
    [[nodiscard]] virtual bool acquirePoolSetAndDescriptorsIfNeeded(
        const VulkanDescriptorSetsLayout& layout, const bool needDescriptors, VkDescriptorSet* const outSets
    ) noexcept override
    {
        ++numAcquireCalls;
        lastNeedDescriptors = needDescriptors;
        return m_acquirer.acquirePoolSetAndDescriptorsIfNeeded(layout, needDescriptors, currentFrame, outSets);
    }

    /// @copydoc IVulkanPendingStateContext::allocateUniformMemory()
    [[nodiscard]] virtual bool
        allocateUniformMemory(const gp::UInt32 size, VulkanUniformUpload& outUpload) noexcept override
    {
        ++numUniformAllocations;
        lastUniformSize = size;
        if (shouldFailUniformAllocations || ((m_nextUniformOffset + size) > kUniformCapacity))
        {
            return false;
        }
        outUpload.buffer = uniformBuffer;
        outUpload.handleId = uniformBufferId;
        outUpload.offset = m_nextUniformOffset;
        outUpload.mappedData = m_uniformMemory.data() + m_nextUniformOffset;
        lastUniformOffset = m_nextUniformOffset;
        m_nextUniformOffset += ((size + kUniformAlignment - 1u) / kUniformAlignment) * kUniformAlignment;
        return true;
    }

    /// @copydoc IVulkanPendingStateContext::getDescriptorSetCache()
    [[nodiscard]] virtual VulkanDescriptorSetCache* getDescriptorSetCache() noexcept override
    {
        return descriptorSetCache;
    }

    /// @copydoc IVulkanPendingStateContext::getCurrentFrame()
    [[nodiscard]] virtual gp::UInt64 getCurrentFrame() const noexcept override
    {
        return currentFrame;
    }

    /// @copydoc IVulkanPendingStateContext::getInputAttachmentView()
    [[nodiscard]] virtual bool getInputAttachmentView(
        const VulkanShaderAttachmentType type, VulkanTextureView& outView, VkImageLayout& outLayout
    ) const noexcept override
    {
        lastAttachmentType = type;
        if (!hasInputAttachment)
        {
            return false;
        }
        outView = inputAttachmentView;
        outLayout = inputAttachmentLayout;
        return true;
    }

    /// @brief Returns the host bytes of one uniform allocation.
    /// @param[in] offset Allocation offset.
    /// @return Pointer into the host uniform buffer.
    [[nodiscard]] const gp::UInt8* getUniformBytes(const gp::UInt32 offset) const noexcept
    {
        return m_uniformMemory.data() + offset;
    }

    /// @brief Returns the acquirer behind the pool path.
    /// @return Acquirer.
    [[nodiscard]] VulkanDescriptorPoolSetAcquirer& getAcquirer() noexcept
    {
        return m_acquirer;
    }

    /// @brief Native buffer handed out by `allocateUniformMemory()`.
    VkBuffer uniformBuffer{ makeFakePendingHandle<VkBuffer>(0xB000u) };

    /// @brief Id of `uniformBuffer`.
    gp::UInt32 uniformBufferId{ 77u };

    /// @brief True to make every uniform allocation fail.
    bool shouldFailUniformAllocations{ false };

    /// @brief Descriptor set cache returned to the states; null keeps them on the pool path.
    VulkanDescriptorSetCache* descriptorSetCache{ nullptr };

    /// @brief Frame reported to the states.
    gp::UInt64 currentFrame{ 1u };

    /// @brief True when a render pass provides input attachments.
    bool hasInputAttachment{ false };

    /// @brief View returned for every input attachment.
    VulkanTextureView inputAttachmentView{};

    /// @brief Layout returned for every input attachment.
    VkImageLayout inputAttachmentLayout{ VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

    /// @brief Number of `acquirePoolSetAndDescriptorsIfNeeded()` calls.
    gp::UInt32 numAcquireCalls{ 0u };

    /// @brief `needDescriptors` of the last acquisition.
    bool lastNeedDescriptors{ false };

    /// @brief Number of `allocateUniformMemory()` calls.
    gp::UInt32 numUniformAllocations{ 0u };

    /// @brief Size of the last uniform allocation request.
    gp::UInt32 lastUniformSize{ 0u };

    /// @brief Offset of the last successful uniform allocation.
    gp::UInt32 lastUniformOffset{ 0u };

    /// @brief Attachment type of the last input attachment query.
    mutable VulkanShaderAttachmentType lastAttachmentType{ VulkanShaderAttachmentType::Count };

private:
    /// @brief Real pool path.
    VulkanDescriptorPoolSetAcquirer m_acquirer;

    /// @brief Host uniform memory.
    gp::Vector<gp::UInt8> m_uniformMemory{};

    /// @brief Offset of the next uniform allocation.
    gp::UInt32 m_nextUniformOffset{ kFirstUniformOffset };
};

}   // namespace gp::rhi::vulkan::tests
