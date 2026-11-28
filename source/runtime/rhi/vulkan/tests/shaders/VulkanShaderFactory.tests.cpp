// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "shaders/VulkanShaderFactory.hpp"
#include "../resources/VulkanResourceTestFakes.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceDeletionQueue.hpp"
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "device/VulkanDevice.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/ShaderForward.hpp"   // IWYU pragma: keep
#include "shaders/VertexShader.hpp"
#include "shaders/VulkanBaseShader.hpp"
#include "shaders/VulkanShader.hpp"
#include "shaders/VulkanShaderCode.hpp"
#include "shaders/VulkanShaderModule.hpp"
#include "shaders/VulkanShaderRHI.hpp"
#include "shaders/VulkanShaderSerialization.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <cstring>
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Builds an instruction's first word.
/// @param[in] length Instruction word count.
/// @param[in] opcode Opcode.
/// @return Encoded word.
[[nodiscard]] constexpr gp::UInt32 makeOp(const gp::UInt32 length, const gp::UInt32 opcode) noexcept
{
    return (length << 16u) | opcode;
}

/// @brief Returns a complete, valid compute module: `void main() {}` with a 1x1x1 workgroup.
/// @return SPIR-V words.
[[nodiscard]] gp::Vector<gp::UInt32> makeEmptyComputeSpirv()
{
    gp::Vector<gp::UInt32> words{};
    words.append({ kSpirvMagicNumber, 0x00010000u, 0u, 5u, 0u });
    words.append({ makeOp(2u, 17u), 1u });                        // OpCapability Shader
    words.append({ makeOp(3u, 14u), 0u, 1u });                    // OpMemoryModel Logical GLSL450
    words.append({ makeOp(5u, 15u), 5u, 1u, 0x6E69616Du, 0u });   // OpEntryPoint GLCompute %1 "main"
    words.append({ makeOp(6u, 16u), 1u, 17u, 1u, 1u, 1u });       // OpExecutionMode %1 LocalSize 1 1 1
    words.append({ makeOp(2u, 19u), 2u });                        // %2 = OpTypeVoid
    words.append({ makeOp(3u, 33u), 3u, 2u });                    // %3 = OpTypeFunction %2
    words.append({ makeOp(5u, 54u), 2u, 1u, 0u, 3u });            // %1 = OpFunction %2 None %3
    words.append({ makeOp(2u, 248u), 4u });                       // %4 = OpLabel
    words.append({ makeOp(1u, 253u) });                           // OpReturn
    words.append({ makeOp(1u, 56u) });                            // OpFunctionEnd
    return words;
}

/// @brief Serializes a shader blob around the empty compute module.
/// @param[in] spirvCRC CRC stored in the header, which names the entry point.
/// @param[in] debugName Header debug name; changing it changes the blob and therefore the shader key.
/// @return Blob bytes.
[[nodiscard]] gp::Vector<gp::UInt8> makeShaderBlob(const gp::UInt32 spirvCRC, const char* const debugName)
{
    VulkanShaderCode code{};
    code.header.spirvCRC = spirvCRC;
    code.header.debugName = gp::String(debugName);
    code.spirv = makeEmptyComputeSpirv();
    return serializeVulkanShaderCode(code);
}

/// @brief Offline fixture: a device without a native handle, so shaders never create modules.
class VulkanShaderFactoryTest : public ::testing::Test
{
protected:
    virtual void TearDown() override
    {
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
    }

    /// @brief Device without native handles.
    VulkanDevice m_offlineDevice{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_API_VERSION_1_3, false };

    /// @brief Factory under test; declared after the device so it is destroyed first.
    VulkanShaderFactory m_factory{};
};

/// @brief Live fixture creating shader modules on a real device.
class VulkanShaderModuleLiveTest : public VulkanResourceLiveTest
{};

}   // namespace

TEST(VulkanShaderFactoryKeyTest, KeyPacksSizeAndCrc)
{
    const gp::UInt8 bytes[5] = { 1u, 2u, 3u, 4u, 5u };
    const gp::UInt64 key = VulkanShaderFactory::computeShaderKey(gp::ConstVectorView<gp::UInt8>(bytes, 5u));

    EXPECT_EQ(key & 0xFFFFFFFFull, 5u);
    EXPECT_NE(key >> 32u, 0u);
    EXPECT_EQ(key, VulkanShaderFactory::computeShaderKey(gp::ConstVectorView<gp::UInt8>(bytes, 5u)));
    EXPECT_NE(key, VulkanShaderFactory::computeShaderKey(gp::ConstVectorView<gp::UInt8>(bytes, 4u)));
}

TEST_F(VulkanShaderFactoryTest, IdenticalBlobsShareOneShader)
{
    const gp::Vector<gp::UInt8> blob = makeShaderBlob(0x12345678u, "Shared");
    const gp::ConstVectorView<gp::UInt8> code(blob);
    {
        const gp::RefCountedPtr<VulkanComputeShader> first =
            m_factory.createShader<VulkanComputeShader>(m_offlineDevice, nullptr, code);
        const gp::RefCountedPtr<VulkanComputeShader> second =
            m_factory.createShader<VulkanComputeShader>(m_offlineDevice, nullptr, code);

        ASSERT_TRUE(first.isValid());
        EXPECT_EQ(first.get(), second.get());
        EXPECT_EQ(m_factory.getNumShaders(shader::ShaderFrequency::Compute), 1u);
        EXPECT_EQ(first->getShaderKey(), VulkanShaderFactory::computeShaderKey(code));
        EXPECT_EQ(first->getVulkanFrequency(), shader::ShaderFrequency::Compute);
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);

    EXPECT_EQ(m_factory.getNumShaders(shader::ShaderFrequency::Compute), 0u);
}

TEST_F(VulkanShaderFactoryTest, DifferentBlobsCreateDifferentShaders)
{
    const gp::Vector<gp::UInt8> blobA = makeShaderBlob(1u, "A");
    const gp::Vector<gp::UInt8> blobB = makeShaderBlob(2u, "B");
    {
        const gp::RefCountedPtr<VulkanComputeShader> shaderA = m_factory.createShader<VulkanComputeShader>(
            m_offlineDevice, nullptr, gp::ConstVectorView<gp::UInt8>(blobA)
        );
        const gp::RefCountedPtr<VulkanComputeShader> shaderB = m_factory.createShader<VulkanComputeShader>(
            m_offlineDevice, nullptr, gp::ConstVectorView<gp::UInt8>(blobB)
        );

        ASSERT_TRUE(shaderA.isValid());
        ASSERT_TRUE(shaderB.isValid());
        EXPECT_NE(shaderA.get(), shaderB.get());
        EXPECT_EQ(m_factory.getNumShaders(shader::ShaderFrequency::Compute), 2u);
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);

    EXPECT_EQ(m_factory.getNumShaders(shader::ShaderFrequency::Compute), 0u);
}

TEST_F(VulkanShaderFactoryTest, FrequenciesAreRegisteredSeparately)
{
    const gp::Vector<gp::UInt8> blob = makeShaderBlob(3u, "Both");
    const gp::ConstVectorView<gp::UInt8> code(blob);
    {
        const gp::RefCountedPtr<VulkanComputeShader> compute =
            m_factory.createShader<VulkanComputeShader>(m_offlineDevice, nullptr, code);
        const gp::RefCountedPtr<VulkanVertexShader> vertex =
            m_factory.createShader<VulkanVertexShader>(m_offlineDevice, nullptr, code);

        ASSERT_TRUE(compute.isValid());
        ASSERT_TRUE(vertex.isValid());
        EXPECT_EQ(m_factory.getNumShaders(shader::ShaderFrequency::Compute), 1u);
        EXPECT_EQ(m_factory.getNumShaders(shader::ShaderFrequency::Vertex), 1u);
        EXPECT_FALSE(m_factory.lookupShader<VulkanPixelShader>(compute->getShaderKey()).isValid());
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
}

TEST_F(VulkanShaderFactoryTest, LookupFindsLiveShadersOnly)
{
    const gp::Vector<gp::UInt8> blob = makeShaderBlob(4u, "Lookup");
    gp::UInt64 shaderKey = 0u;
    {
        const gp::RefCountedPtr<VulkanVertexShader> created =
            m_factory.createShader<VulkanVertexShader>(m_offlineDevice, nullptr, gp::ConstVectorView<gp::UInt8>(blob));
        ASSERT_TRUE(created.isValid());
        shaderKey = created->getShaderKey();

        const gp::RefCountedPtr<VulkanVertexShader> found = m_factory.lookupShader<VulkanVertexShader>(shaderKey);
        EXPECT_EQ(found.get(), created.get());
        EXPECT_FALSE(m_factory.lookupShader<VulkanVertexShader>(0u).isValid());

        gp::Array<gp::UInt64, VulkanShaderFactory::kNumGraphicsStages> keys{};
        keys[0] = shaderKey;
        keys[1] = shaderKey;
        gp::Array<VulkanShader*, VulkanShaderFactory::kNumGraphicsStages> shaders{};
        m_factory.lookupGfxShaders(keys, shaders);
        EXPECT_EQ(shaders[0], static_cast<VulkanShader*>(created.get()));
        EXPECT_EQ(shaders[1], nullptr);
        EXPECT_EQ(shaders[2], nullptr);
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);

    EXPECT_FALSE(m_factory.lookupShader<VulkanVertexShader>(shaderKey).isValid());
}

TEST_F(VulkanShaderFactoryTest, MalformedBlobCreatesNothing)
{
    const gp::UInt8 garbage[8] = { 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u };
    const gp::RefCountedPtr<VulkanPixelShader> created = m_factory.createShader<VulkanPixelShader>(
        m_offlineDevice, nullptr, gp::ConstVectorView<gp::UInt8>(garbage, 8u)
    );

    EXPECT_FALSE(created.isValid());
    EXPECT_EQ(m_factory.getNumShaders(shader::ShaderFrequency::Pixel), 0u);
}

TEST_F(VulkanShaderFactoryTest, RhiEntryPointSetsTheHashAndDebugName)
{
    const gp::Vector<gp::UInt8> blob = makeShaderBlob(5u, "NamedVertex");
    shader::ShaderHash hash{};
    {
        const shader::VertexShaderRef created =
            createVulkanVertexShader(m_factory, m_offlineDevice, nullptr, gp::ConstVectorView<gp::UInt8>(blob), hash);

        ASSERT_TRUE(created.isValid());
        EXPECT_EQ(created->getHash(), hash);
        EXPECT_EQ(m_factory.getNumShaders(shader::ShaderFrequency::Vertex), 1u);
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
}

TEST_F(VulkanShaderFactoryTest, EntryPointEncodesSpirvSizeAndCrc)
{
    const gp::Vector<gp::UInt8> blob = makeShaderBlob(0x00ABCDEFu, "Entry");
    {
        const gp::RefCountedPtr<VulkanComputeShader> created =
            m_factory.createShader<VulkanComputeShader>(m_offlineDevice, nullptr, gp::ConstVectorView<gp::UInt8>(blob));
        ASSERT_TRUE(created.isValid());

        const gp::USize spirvSize = static_cast<gp::USize>(makeEmptyComputeSpirv().size()) * 4u;
        ASSERT_EQ(spirvSize, 0x8Cu);
        EXPECT_STREQ(created->getEntryPoint(), "main_0000008c_00abcdef");
        EXPECT_LT(std::strlen(created->getEntryPoint()), VulkanShader::kEntryPointNameCapacity);
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
}

TEST_F(VulkanShaderFactoryTest, OfflineShaderCreatesNoModules)
{
    const gp::Vector<gp::UInt8> blob = makeShaderBlob(6u, "Offline");
    {
        const gp::RefCountedPtr<VulkanComputeShader> created =
            m_factory.createShader<VulkanComputeShader>(m_offlineDevice, nullptr, gp::ConstVectorView<gp::UInt8>(blob));
        ASSERT_TRUE(created.isValid());

        const gp::RefCountedPtr<VulkanShaderModule> module = created->getOrCreateHandle(1u);
        EXPECT_TRUE(!module.isValid() || !module->isValid());
        EXPECT_FALSE(created->needsSpirvInputAttachmentPatching(4u));
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
}

TEST_F(VulkanShaderModuleLiveTest, ModuleIsCreatedFromValidSpirv)
{
    const gp::Vector<gp::UInt32> spirv = makeEmptyComputeSpirv();
    const gp::RefCountedPtr<VulkanShaderModule> module(
        new VulkanShaderModule(*m_device, gp::ConstVectorView<gp::UInt32>(spirv), nullptr)
    );

    EXPECT_TRUE(module->isValid());
    EXPECT_NE(module->getHandle(), VK_NULL_HANDLE);
}

TEST_F(VulkanShaderModuleLiveTest, ShaderCachesOneModulePerLayoutHash)
{
    VulkanShaderFactory factory;
    const gp::Vector<gp::UInt8> blob = makeShaderBlob(7u, "Live");
    {
        const gp::RefCountedPtr<VulkanComputeShader> created =
            factory.createShader<VulkanComputeShader>(*m_device, nullptr, gp::ConstVectorView<gp::UInt8>(blob));
        ASSERT_TRUE(created.isValid());

        const gp::RefCountedPtr<VulkanShaderModule> first = created->getOrCreateHandle(100u);
        const gp::RefCountedPtr<VulkanShaderModule> again = created->getOrCreateHandle(100u);
        const gp::RefCountedPtr<VulkanShaderModule> other = created->getOrCreateHandle(200u);

        ASSERT_TRUE(first.isValid());
        EXPECT_TRUE(first->isValid());
        EXPECT_EQ(first.get(), again.get());
        EXPECT_NE(first.get(), other.get());
        EXPECT_EQ(created->getNumShaderModules(), 2u);

        created->purgeShaderModules();
        EXPECT_EQ(created->getNumShaderModules(), 0u);
        // Outstanding references keep purged modules alive.
        EXPECT_TRUE(first->isValid());
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);

    EXPECT_EQ(factory.getNumShaders(shader::ShaderFrequency::Compute), 0u);
}

}   // namespace gp::rhi::vulkan::tests
