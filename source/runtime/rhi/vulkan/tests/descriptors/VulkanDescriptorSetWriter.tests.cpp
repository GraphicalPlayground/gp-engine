// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "descriptors/VulkanDescriptorSetWriter.hpp"
#include "containers/arrays/Vector.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceDeletionQueue.hpp"
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetKey.hpp"
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "descriptors/VulkanDescriptorSetWriteContainer.hpp"
#include "device/VulkanDevice.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "pipeline/SamplerState.hpp"           // IWYU pragma: keep
#include "states/VulkanSamplerState.hpp"
#include "views/VulkanViewTypes.hpp"
#include "VulkanConfiguration.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <cstdint>
#include <gtest/gtest.h>
#include <initializer_list>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Returns a fake non-dispatchable handle; never passed to the driver.
/// @tparam InHandle Vulkan handle type.
/// @param[in] value Non-zero handle value.
/// @return Handle.
template <typename InHandle>
[[nodiscard]] InHandle makeFakeHandle(const std::uintptr_t value) noexcept
{
    return reinterpret_cast<InHandle>(value);
}

/// @brief Writer bound to a container sized for a list of binding types.
class VulkanDescriptorSetWriterTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        VkSamplerCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        m_defaultSampler = pipeline::SamplerStateRef(new VulkanSamplerState(m_device, createInfo, nullptr));
        m_otherSampler = pipeline::SamplerStateRef(new VulkanSamplerState(m_device, createInfo, nullptr));

        m_defaultImageView.view = makeFakeHandle<VkImageView>(0xD000u);
        m_defaultImageView.viewId = 1u;
    }

    virtual void TearDown() override
    {
        m_defaultSampler = pipeline::SamplerStateRef();
        m_otherSampler = pipeline::SamplerStateRef();
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
    }

    /// @brief Sizes the container for `types` and sets the writer up over it.
    /// @param[in] types Binding types.
    /// @param[in] useCache Whether to pass hashable infos (descriptor set cache path).
    /// @return Number of dynamic offsets.
    gp::UInt32 setup(const std::initializer_list<VkDescriptorType> types, const bool useCache)
    {
        m_types.clear();
        m_types.append(types);
        const gp::USize count = static_cast<gp::USize>(m_types.size());
        m_container.descriptorWrites.resize(count, VkWriteDescriptorSet{});
        m_container.descriptorImageInfo.resize(count, VkDescriptorImageInfo{});
        m_container.descriptorBufferInfo.resize(count, VkDescriptorBufferInfo{});
        m_container.bindingToDynamicOffsetMap.resize(count, gp::UInt8{ 0u });
        m_container.accelerationStructures.resize(count, VkAccelerationStructureKHR{ VK_NULL_HANDLE });
        m_container.accelerationStructureWrites.resize(count, VkWriteDescriptorSetAccelerationStructureKHR{});
        m_container.hashableDescriptorInfo.resize(count + 1u, VulkanHashableDescriptorInfo{});
        m_dynamicOffsets.resize(count, 0u);

        const gp::UInt32 numDynamicOffsets = m_writer.setupDescriptorWrites(
            gp::ConstVectorView<VkDescriptorType>(m_types),
            useCache ? m_container.hashableDescriptorInfo.data() : nullptr,
            m_container.descriptorWrites.data(),
            m_container.descriptorImageInfo.data(),
            m_container.descriptorBufferInfo.data(),
            m_container.bindingToDynamicOffsetMap.data(),
            m_container.accelerationStructureWrites.data(),
            m_container.accelerationStructures.data(),
            getDefaultSampler(),
            m_defaultImageView
        );
        m_writer.setDynamicOffsets(m_dynamicOffsets.data());
        return numDynamicOffsets;
    }

    /// @brief Returns the default sampler.
    /// @return Sampler state.
    [[nodiscard]] const VulkanSamplerState& getDefaultSampler() const noexcept
    {
        return *static_cast<const VulkanSamplerState*>(m_defaultSampler.get());
    }

    /// @brief Returns a second sampler with a different id.
    /// @return Sampler state.
    [[nodiscard]] const VulkanSamplerState& getOtherSampler() const noexcept
    {
        return *static_cast<const VulkanSamplerState*>(m_otherSampler.get());
    }

    /// @brief Device without native handles; samplers still receive ids.
    VulkanDevice m_device{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_API_VERSION_1_3, false };

    /// @brief Sampler bound by default.
    pipeline::SamplerStateRef m_defaultSampler{};

    /// @brief Another sampler.
    pipeline::SamplerStateRef m_otherSampler{};

    /// @brief Image view bound by default.
    VulkanTextureView m_defaultImageView{};

    /// @brief Binding types of the current setup.
    gp::Vector<VkDescriptorType> m_types{};

    /// @brief Storage behind the writer.
    VulkanDescriptorSetWriteContainer m_container{};

    /// @brief Dynamic offsets written by dynamic uniform buffer writes.
    gp::Vector<gp::UInt32> m_dynamicOffsets{};

    /// @brief Writer under test.
    VulkanDescriptorSetWriter m_writer{};
};

}   // namespace

TEST(VulkanDescriptorSetKeyTest, EmptyKeysAreEqual)
{
    VulkanDescriptorSetKey first;
    VulkanDescriptorSetKey second;
    second.generateFromData(nullptr, 0u);

    EXPECT_EQ(first.getHash(), 0u);
    EXPECT_EQ(first.getSize(), 0u);
    EXPECT_TRUE(first == second);
}

TEST(VulkanDescriptorSetKeyTest, KeysCompareTheirBytes)
{
    const gp::UInt32 dataA[3] = { 1u, 2u, 3u };
    const gp::UInt32 dataB[3] = { 1u, 2u, 3u };
    const gp::UInt32 dataC[3] = { 1u, 2u, 4u };
    VulkanDescriptorSetKey keyA;
    VulkanDescriptorSetKey keyB;
    VulkanDescriptorSetKey keyC;
    VulkanDescriptorSetKey shorter;
    keyA.generateFromData(dataA, sizeof(dataA));
    keyB.generateFromData(dataB, sizeof(dataB));
    keyC.generateFromData(dataC, sizeof(dataC));
    shorter.generateFromData(dataA, sizeof(gp::UInt32) * 2u);

    EXPECT_EQ(keyA.getSize(), sizeof(dataA));
    EXPECT_NE(keyA.getHash(), 0u);
    EXPECT_TRUE(keyA == keyB);
    EXPECT_FALSE(keyA == keyC);
    EXPECT_FALSE(keyA == shorter);
}

TEST(VulkanDescriptorSetKeyTest, KeyOwnsACopyOfItsBytes)
{
    gp::UInt32 data[2] = { 5u, 6u };
    VulkanDescriptorSetKey key;
    key.generateFromData(data, sizeof(data));
    VulkanDescriptorSetKey snapshot;
    snapshot.generateFromData(data, sizeof(data));

    data[1] = 7u;

    EXPECT_TRUE(key == snapshot);
}

TEST_F(VulkanDescriptorSetWriterTest, SetupWiresEveryBindingToItsInfo)
{
    const gp::UInt32 numDynamicOffsets = setup(
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          VK_DESCRIPTOR_TYPE_SAMPLER,
          VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
          VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC },
        false
    );

    EXPECT_EQ(numDynamicOffsets, 2u);
    EXPECT_EQ(m_writer.getNumWrites(), 8u);
    EXPECT_EQ(m_writer.getWriteDescriptors(), m_container.descriptorWrites.data());

    const gp::Vector<VkWriteDescriptorSet>& writes = m_container.descriptorWrites;
    for (gp::UInt32 index = 0u; index < 8u; ++index)
    {
        EXPECT_EQ(writes[index].sType, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
        EXPECT_EQ(writes[index].dstBinding, index);
        EXPECT_EQ(writes[index].descriptorCount, 1u);
        EXPECT_EQ(writes[index].descriptorType, m_types[index]);
    }

    // Buffers consume buffer infos in binding order; images and samplers consume image infos.
    EXPECT_EQ(writes[0].pBufferInfo, &m_container.descriptorBufferInfo[0]);
    EXPECT_EQ(writes[1].pBufferInfo, &m_container.descriptorBufferInfo[1]);
    EXPECT_EQ(writes[4].pBufferInfo, &m_container.descriptorBufferInfo[2]);
    EXPECT_EQ(writes[7].pBufferInfo, &m_container.descriptorBufferInfo[3]);
    EXPECT_EQ(writes[2].pImageInfo, &m_container.descriptorImageInfo[0]);
    EXPECT_EQ(writes[3].pImageInfo, &m_container.descriptorImageInfo[1]);
    EXPECT_EQ(writes[5].pImageInfo, nullptr);
    EXPECT_EQ(writes[5].pBufferInfo, nullptr);

    // Image bindings start on the default sampler and view.
    EXPECT_EQ(m_container.descriptorImageInfo[1].imageView, m_defaultImageView.view);
    EXPECT_EQ(m_container.descriptorImageInfo[1].imageLayout, VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_EQ(m_container.descriptorImageInfo[1].sampler, getDefaultSampler().getHandle());

    // Dynamic uniform buffers are numbered in binding order.
    EXPECT_EQ(m_container.bindingToDynamicOffsetMap[0], 0u);
    EXPECT_EQ(m_container.bindingToDynamicOffsetMap[7], 1u);

    // The acceleration structure extension is chained to its write.
    const VkWriteDescriptorSetAccelerationStructureKHR& asWrite = m_container.accelerationStructureWrites[0];
    EXPECT_EQ(writes[6].pNext, &asWrite);
    EXPECT_EQ(asWrite.sType, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR);
    EXPECT_EQ(asWrite.accelerationStructureCount, 1u);
    EXPECT_EQ(asWrite.pAccelerationStructures, &m_container.accelerationStructures[0]);
}

TEST_F(VulkanDescriptorSetWriterTest, BufferWritesReportChanges)
{
    setup({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, false);
    const VkBuffer buffer = makeFakeHandle<VkBuffer>(0xB000u);

    EXPECT_TRUE(m_writer.writeUniformBuffer(0u, buffer, 1u, 0u, 256u));
    EXPECT_FALSE(m_writer.writeUniformBuffer(0u, buffer, 1u, 0u, 256u));
    EXPECT_TRUE(m_writer.writeUniformBuffer(0u, buffer, 1u, 64u, 256u));
    EXPECT_TRUE(m_writer.writeUniformBuffer(0u, buffer, 1u, 64u, 128u));

    const VkDescriptorBufferInfo& info = m_container.descriptorBufferInfo[0];
    EXPECT_EQ(info.buffer, buffer);
    EXPECT_EQ(info.offset, 64u);
    EXPECT_EQ(info.range, 128u);
}

TEST_F(VulkanDescriptorSetWriterTest, DynamicOffsetsAreWrittenWithoutChangingTheDescriptor)
{
    setup({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC }, false);
    const VkBuffer buffer = makeFakeHandle<VkBuffer>(0xB100u);

    EXPECT_TRUE(m_writer.writeDynamicUniformBuffer(1u, buffer, 1u, 0u, 256u, 512u));
    EXPECT_EQ(m_dynamicOffsets[0], 512u);

    EXPECT_FALSE(m_writer.writeDynamicUniformBuffer(1u, buffer, 1u, 0u, 256u, 1024u));
    EXPECT_EQ(m_dynamicOffsets[0], 1024u);
}

TEST_F(VulkanDescriptorSetWriterTest, StorageBufferWriteUsesTheStructuredView)
{
    setup({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER }, false);
    VulkanStructuredBufferView view{};
    view.buffer = makeFakeHandle<VkBuffer>(0xB200u);
    view.handleId = 9u;
    view.offset = 16u;
    view.size = 48u;

    EXPECT_TRUE(m_writer.writeStorageBuffer(0u, view));
    EXPECT_FALSE(m_writer.writeStorageBuffer(0u, view));
    EXPECT_EQ(m_container.descriptorBufferInfo[0].offset, 16u);
    EXPECT_EQ(m_container.descriptorBufferInfo[0].range, 48u);
}

TEST_F(VulkanDescriptorSetWriterTest, ImageWritesReportChanges)
{
    setup(
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT },
        false
    );
    VulkanTextureView view{};
    view.view = makeFakeHandle<VkImageView>(0xE000u);
    view.viewId = 2u;

    EXPECT_TRUE(m_writer.writeImage(0u, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    EXPECT_FALSE(m_writer.writeImage(0u, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    EXPECT_TRUE(m_writer.writeImage(0u, view, VK_IMAGE_LAYOUT_GENERAL));
    EXPECT_TRUE(m_writer.writeStorageImage(1u, view, VK_IMAGE_LAYOUT_GENERAL));
    EXPECT_TRUE(m_writer.writeInputAttachment(2u, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));

    EXPECT_EQ(m_container.descriptorImageInfo[0].imageView, view.view);
    EXPECT_EQ(m_container.descriptorImageInfo[0].imageLayout, VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_EQ(m_container.descriptorImageInfo[2].imageLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

TEST_F(VulkanDescriptorSetWriterTest, TexelBufferWritesPointAtTheView)
{
    setup({ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER }, false);
    VulkanTypedBufferView view{};
    view.view = makeFakeHandle<VkBufferView>(0xF000u);
    view.viewId = 3u;

    EXPECT_TRUE(m_writer.writeUniformTexelBuffer(0u, view));
    EXPECT_TRUE(m_writer.writeUniformTexelBuffer(0u, view));
    EXPECT_TRUE(m_writer.writeStorageTexelBuffer(1u, view));

    EXPECT_EQ(m_container.descriptorWrites[0].pTexelBufferView, &view.view);
    EXPECT_EQ(m_container.descriptorWrites[1].pTexelBufferView, &view.view);
}

TEST_F(VulkanDescriptorSetWriterTest, AccelerationStructureWriteFindsItsExtension)
{
    setup({ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR }, false);
    const VkAccelerationStructureKHR handle = makeFakeHandle<VkAccelerationStructureKHR>(0xA000u);

    EXPECT_TRUE(m_writer.writeAccelerationStructure(0u, handle));
    EXPECT_FALSE(m_writer.writeAccelerationStructure(0u, handle));
    EXPECT_EQ(m_container.accelerationStructures[0], handle);
}

TEST_F(VulkanDescriptorSetWriterTest, SamplerWriteReplacesTheDefault)
{
    setup({ VK_DESCRIPTOR_TYPE_SAMPLER }, true);

    EXPECT_FALSE(m_writer.writeSampler(0u, getDefaultSampler()));
    EXPECT_TRUE(m_writer.writeSampler(0u, getOtherSampler()));
    EXPECT_EQ(m_container.hashableDescriptorInfo[0].image.samplerId, getOtherSampler().getSamplerId());
}

TEST_F(VulkanDescriptorSetWriterTest, SetDescriptorSetTargetsEveryWrite)
{
    setup({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER }, false);
    const VkDescriptorSet set = makeFakeHandle<VkDescriptorSet>(0x5000u);

    m_writer.setDescriptorSet(set);

    EXPECT_EQ(m_container.descriptorWrites[0].dstSet, set);
    EXPECT_EQ(m_container.descriptorWrites[1].dstSet, set);
}

TEST_F(VulkanDescriptorSetWriterTest, CachePathKeyFollowsResourceIds)
{
    setup({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE }, true);
    const VkBuffer buffer = makeFakeHandle<VkBuffer>(0xB300u);
    VulkanTextureView view{};
    view.view = makeFakeHandle<VkImageView>(0xE100u);
    view.viewId = 42u;

    ASSERT_TRUE(m_writer.writeUniformBuffer(0u, buffer, 7u, 0u, 64u));
    ASSERT_TRUE(m_writer.writeImage(1u, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    const VulkanDescriptorSetKey before = m_writer.getKey();
    EXPECT_EQ(before.getSize(), sizeof(VulkanHashableDescriptorInfo) * 3u);

    // Same ids: nothing changed, even though the native handle differs.
    EXPECT_FALSE(m_writer.writeUniformBuffer(0u, makeFakeHandle<VkBuffer>(0xB400u), 7u, 0u, 64u));
    EXPECT_TRUE(m_writer.getKey() == before);

    // A new id changes the key.
    EXPECT_TRUE(m_writer.writeUniformBuffer(0u, buffer, 8u, 0u, 64u));
    EXPECT_FALSE(m_writer.getKey() == before);
    EXPECT_EQ(m_container.hashableDescriptorInfo[0].buffer.id, 8u);
    EXPECT_EQ(m_container.hashableDescriptorInfo[1].image.imageViewId, 42u);
}

TEST_F(VulkanDescriptorSetWriterTest, CachePathKeyIncludesTheLayoutEntry)
{
    setup({ VK_DESCRIPTOR_TYPE_SAMPLER }, true);
    const VulkanDescriptorSetKey before = m_writer.getKey();

    // The trailing entry identifies the layout; a different layout is a different set.
    m_container.hashableDescriptorInfo[1].layout.layoutId = 99u;
    ASSERT_TRUE(m_writer.writeSampler(0u, getOtherSampler()));
    ASSERT_TRUE(m_writer.writeSampler(0u, getDefaultSampler()));

    EXPECT_FALSE(m_writer.getKey() == before);
}

TEST_F(VulkanDescriptorSetWriterTest, VolatileTexelBufferForbidsCaching)
{
    setup({ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER }, true);
    VulkanTypedBufferView stable{};
    stable.view = makeFakeHandle<VkBufferView>(0xF100u);
    stable.viewId = 5u;
    VulkanTypedBufferView transient{};
    transient.view = makeFakeHandle<VkBufferView>(0xF200u);

    EXPECT_TRUE(m_writer.writeStorageTexelBuffer(0u, stable));
    EXPECT_FALSE(m_writer.writeStorageTexelBuffer(0u, stable));
    EXPECT_FALSE(m_writer.hasVolatileResources());

    EXPECT_TRUE(m_writer.writeStorageTexelBuffer(0u, transient));
    EXPECT_TRUE(m_writer.hasVolatileResources());

    m_writer.reset();
    EXPECT_FALSE(m_writer.hasVolatileResources());
}

#if GP_VULKAN_VALIDATE_DESCRIPTORS_WRITTEN

TEST_F(VulkanDescriptorSetWriterTest, ValidationReportsUnwrittenBindings)
{
    setup({ VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER }, false);
    VulkanTextureView view{};
    view.view = makeFakeHandle<VkImageView>(0xE200u);
    view.viewId = 2u;
    VulkanStructuredBufferView buffer{};
    buffer.buffer = makeFakeHandle<VkBuffer>(0xB500u);
    buffer.handleId = 1u;

    // Samplers start written: the default sampler is a valid resource.
    EXPECT_FALSE(m_writer.checkAllWritten());
    static_cast<void>(m_writer.writeImage(1u, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    EXPECT_FALSE(m_writer.checkAllWritten());
    static_cast<void>(m_writer.writeStorageBuffer(2u, buffer));
    EXPECT_TRUE(m_writer.checkAllWritten());

    // A reset starts the next draw from the defaults again.
    m_writer.reset();
    EXPECT_FALSE(m_writer.checkAllWritten());
}

TEST_F(VulkanDescriptorSetWriterTest, ExcludedBindingsAreNotRequired)
{
    setup({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER }, false);
    const gp::UInt32 excluded[2] = { 0u, 1u };

    m_writer.setExcludedResources(gp::ConstVectorView<gp::UInt32>(excluded, 2u));

    EXPECT_TRUE(m_writer.checkAllWritten());
}

TEST_F(VulkanDescriptorSetWriterTest, ValidationSpansMoreThanOneMaskWord)
{
    gp::Vector<VkDescriptorType> types{};
    for (gp::UInt32 i = 0u; i < 40u; ++i)
    {
        types.pushBack(VK_DESCRIPTOR_TYPE_SAMPLER);
    }
    m_types = types;
    m_container.descriptorWrites.resize(40u, VkWriteDescriptorSet{});
    m_container.descriptorImageInfo.resize(40u, VkDescriptorImageInfo{});
    m_writer.setupDescriptorWrites(
        gp::ConstVectorView<VkDescriptorType>(m_types),
        nullptr,
        m_container.descriptorWrites.data(),
        m_container.descriptorImageInfo.data(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        getDefaultSampler(),
        m_defaultImageView
    );

    EXPECT_TRUE(m_writer.checkAllWritten());
}

#endif

}   // namespace gp::rhi::vulkan::tests
