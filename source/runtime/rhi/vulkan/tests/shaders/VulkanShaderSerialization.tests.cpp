// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "shaders/VulkanShaderSerialization.hpp"
#include "containers/arrays/Vector.hpp"
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "shaders/ShaderTypes.hpp"
#include "shaders/VulkanShaderCode.hpp"
#include "shaders/VulkanShaderCodeReader.hpp"
#include "shaders/VulkanShaderCodeWriter.hpp"
#include "shaders/VulkanShaderHeader.hpp"
#include "shaders/VulkanShaderHeaderEnums.hpp"
#include "shaders/VulkanSpirvPatching.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Byte offset of the first header field (binding count), after the magic and version words.
constexpr gp::USize kBindingCountOffset = 8u;

/// @brief Returns the smallest well-formed SPIR-V module: the 5-word header.
/// @return SPIR-V words.
[[nodiscard]] gp::Vector<gp::UInt32> makeMinimalSpirv()
{
    gp::Vector<gp::UInt32> words{};
    words.pushBack(kSpirvMagicNumber);
    words.pushBack(0x00010000u);
    words.pushBack(0u);
    words.pushBack(1u);
    words.pushBack(0u);
    return words;
}

/// @brief Returns a shader code instance that exercises every header section.
/// @return Shader code.
[[nodiscard]] VulkanShaderCode makeRichShaderCode()
{
    VulkanShaderCode code{};
    VulkanShaderHeader& header = code.header;

    header.bindings.pushBack(VulkanShaderBindingInfo{ static_cast<gp::UInt32>(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) });
    header.bindings.pushBack(VulkanShaderBindingInfo{ static_cast<gp::UInt32>(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) });
    header.bindings.pushBack(VulkanShaderBindingInfo{ static_cast<gp::UInt32>(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) });
    header.bindings.pushBack(VulkanShaderBindingInfo{ static_cast<gp::UInt32>(VK_DESCRIPTOR_TYPE_SAMPLER) });

    VulkanShaderUniformBufferInfo uniformBuffer{};
    uniformBuffer.layoutHash = 0xCAFEBABEu;
    uniformBuffer.staticSlot = 3u;
    uniformBuffer.hasResources = 1u;
    uniformBuffer.bindlessCBIndex = 7u;
    header.uniformBufferInfos.pushBack(uniformBuffer);

    header.inputAttachmentInfos.pushBack(VulkanShaderInputAttachmentInfo{ 2u, VulkanShaderAttachmentType::Color1 });
    header.globalSamplerInfos.pushBack(
        VulkanShaderGlobalSamplerInfo{ 3u, VulkanGlobalSamplerType::TrilinearWrappedSampler }
    );

    header.numBoundUniformBuffers = 1u;
    header.packedGlobalsSize = 64u;
    header.inputAttachmentsMask = 0x4u;
    header.inOutMask = 0x3u;
    header.specializationConstantData.values.pushBack(VulkanSpecializationConstantValue{ 5u, 9u });
    header.spirvCRC = 0x12345678u;
    header.waveSize = 32u;
    header.usageFlags = VulkanShaderUsageFlags::NoDerivativeOps | VulkanShaderUsageFlags::ShaderBundle;
    header.debugName = gp::String("RichShader");

    code.resourceTable.resourceTableBits = 0x5u;
    code.resourceTable.samplerMap.pushBack(11u);
    code.resourceTable.textureMap.pushBack(22u);
    code.spirv = makeMinimalSpirv();
    return code;
}

/// @brief Writes a header stream holding one binding and one input attachment with the given values.
/// @param[in] attachmentBindingIndex Binding index of the input attachment.
/// @param[in] attachmentType Raw attachment type.
/// @return Serialized bytes, without blob magic or version.
[[nodiscard]] gp::Vector<gp::UInt8>
    makeHeaderWithInputAttachment(const gp::UInt8 attachmentBindingIndex, const gp::UInt8 attachmentType)
{
    VulkanShaderCodeWriter writer;
    writer.writeUInt32(1u);
    writer.writeUInt32(static_cast<gp::UInt32>(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT));
    writer.writeString(gp::String());
    writer.writeUInt32(0u);
    writer.writeUInt32(1u);
    writer.writeUInt8(attachmentBindingIndex);
    writer.writeUInt8(attachmentType);
    // Trailing padding so the counts are never rejected for lack of bytes.
    for (gp::UInt32 i = 0u; i < 64u; ++i)
    {
        writer.writeUInt8(0u);
    }
    return writer.releaseBytes();
}

/// @brief Overwrites a little-endian 32-bit value in a byte buffer.
/// @param[in,out] bytes Buffer to patch.
/// @param[in] offset Byte offset of the value.
/// @param[in] value New value.
void patchUInt32(gp::Vector<gp::UInt8>& bytes, const gp::USize offset, const gp::UInt32 value)
{
    for (gp::USize i = 0u; i < 4u; ++i)
    {
        bytes[offset + i] = static_cast<gp::UInt8>((value >> (8u * i)) & 0xFFu);
    }
}

/// @brief Builds an `OpCode` word from an instruction length and opcode.
/// @param[in] length Instruction word count.
/// @param[in] opcode Opcode.
/// @return Encoded first word.
[[nodiscard]] constexpr gp::UInt32 makeOp(const gp::UInt32 length, const gp::UInt32 opcode) noexcept
{
    return (length << 16u) | opcode;
}

/// @brief Returns a fragment module reading a subpass input through `OpImageRead`.
/// @param[in] withIntZero Whether the module declares the signed `int 0` constant.
/// @return SPIR-V words.
[[nodiscard]] gp::Vector<gp::UInt32> makeSubpassReadSpirv(const bool withIntZero)
{
    gp::Vector<gp::UInt32> words = makeMinimalSpirv();
    // %1 = OpTypeInt 32 1
    words.append({ makeOp(4u, 21u), 1u, 32u, 1u });
    if (withIntZero)
    {
        // %2 = OpConstant %1 0
        words.append({ makeOp(4u, 43u), 1u, 2u, 0u });
    }
    // %3 = OpTypeImage %4 SubpassData depth=0 arrayed=0 MS=0 sampled=2 Unknown
    words.append({ makeOp(9u, 25u), 3u, 4u, 6u, 0u, 0u, 0u, 2u, 0u });
    // %5 = OpLoad %3 %6
    words.append({ makeOp(4u, 61u), 3u, 5u, 6u });
    // %8 = OpImageRead %7 %5 %9
    words.append({ makeOp(5u, 98u), 7u, 8u, 5u, 9u });
    return words;
}

}   // namespace

TEST(VulkanShaderCodeWriterTest, ScalarsAreWrittenLittleEndian)
{
    VulkanShaderCodeWriter writer;
    writer.writeUInt8(0xABu);
    writer.writeUInt32(0x04030201u);
    writer.writeUInt64(0x0807060504030201ull);

    const gp::Vector<gp::UInt8>& bytes = writer.getBytes();
    ASSERT_EQ(bytes.size(), 13u);
    EXPECT_EQ(bytes[0], 0xABu);
    for (gp::USize i = 0u; i < 4u; ++i)
    {
        EXPECT_EQ(bytes[1u + i], static_cast<gp::UInt8>(i + 1u));
    }
    for (gp::USize i = 0u; i < 8u; ++i)
    {
        EXPECT_EQ(bytes[5u + i], static_cast<gp::UInt8>(i + 1u));
    }
}

TEST(VulkanShaderCodeWriterTest, ReleaseBytesLeavesTheWriterEmpty)
{
    VulkanShaderCodeWriter writer;
    writer.writeUInt32(1u);

    const gp::Vector<gp::UInt8> released = writer.releaseBytes();

    EXPECT_EQ(released.size(), 4u);
    EXPECT_TRUE(writer.getBytes().isEmpty());
}

TEST(VulkanShaderCodeReaderTest, ReadsBackEveryWrittenValue)
{
    VulkanShaderCodeWriter writer;
    writer.writeUInt8(7u);
    writer.writeUInt32(0xDEADBEEFu);
    writer.writeUInt64(0x0123456789ABCDEFull);
    writer.writeString(gp::String("entry"));
    const gp::UInt8 raw[3] = { 1u, 2u, 3u };
    writer.writeBytes(raw, 3u);

    VulkanShaderCodeReader reader(gp::ConstVectorView<gp::UInt8>(writer.getBytes()));
    gp::UInt8 value8 = 0u;
    gp::UInt32 value32 = 0u;
    gp::UInt64 value64 = 0u;
    gp::String text{};
    gp::UInt8 bytes[3] = {};

    ASSERT_TRUE(reader.readUInt8(value8));
    ASSERT_TRUE(reader.readUInt32(value32));
    ASSERT_TRUE(reader.readUInt64(value64));
    ASSERT_TRUE(reader.readString(text));
    ASSERT_TRUE(reader.readBytes(bytes, 3u));

    EXPECT_EQ(value8, 7u);
    EXPECT_EQ(value32, 0xDEADBEEFu);
    EXPECT_EQ(value64, 0x0123456789ABCDEFull);
    EXPECT_EQ(text, gp::String("entry"));
    EXPECT_EQ(bytes[2], 3u);
    EXPECT_TRUE(reader.isAtEnd());
    EXPECT_FALSE(reader.hasFailed());
}

TEST(VulkanShaderCodeReaderTest, ReadingPastTheEndFailsAndStaysFailed)
{
    const gp::UInt8 bytes[6] = { 1u, 0u, 0u, 0u, 9u, 9u };
    VulkanShaderCodeReader reader(gp::ConstVectorView<gp::UInt8>(bytes, 6u));
    gp::UInt32 value = 0u;

    EXPECT_TRUE(reader.readUInt32(value));
    EXPECT_FALSE(reader.readUInt32(value));
    EXPECT_TRUE(reader.hasFailed());

    // The failure is sticky: even a read that would fit is refused.
    gp::UInt8 small = 0u;
    EXPECT_FALSE(reader.readUInt8(small));
    EXPECT_EQ(reader.getOffset(), 4u);
}

TEST(VulkanShaderCodeReaderTest, CountsLargerThanTheRemainingBytesAreRejected)
{
    VulkanShaderCodeWriter writer;
    writer.writeUInt32(3u);
    writer.writeUInt32(0u);
    writer.writeUInt32(0u);

    VulkanShaderCodeReader accepting(gp::ConstVectorView<gp::UInt8>(writer.getBytes()));
    gp::UInt32 count = 0u;
    EXPECT_TRUE(accepting.readCount(count, 2u));
    EXPECT_EQ(count, 3u);

    VulkanShaderCodeReader rejecting(gp::ConstVectorView<gp::UInt8>(writer.getBytes()));
    EXPECT_FALSE(rejecting.readCount(count, 4u));
    EXPECT_TRUE(rejecting.hasFailed());
}

TEST(VulkanShaderCodeReaderTest, ReadViewAliasesTheSourceBytes)
{
    const gp::UInt8 bytes[4] = { 10u, 20u, 30u, 40u };
    VulkanShaderCodeReader reader(gp::ConstVectorView<gp::UInt8>(bytes, 4u));
    gp::ConstVectorView<gp::UInt8> view{};

    ASSERT_TRUE(reader.readView(3u, view));

    EXPECT_EQ(view.data(), bytes);
    EXPECT_EQ(view.size(), 3u);
    EXPECT_EQ(reader.getRemaining(), 1u);
}

TEST(VulkanShaderSerializationTest, RichShaderCodeRoundTrips)
{
    const VulkanShaderCode original = makeRichShaderCode();
    const gp::Vector<gp::UInt8> bytes = serializeVulkanShaderCode(original);

    VulkanShaderCode decoded{};
    ASSERT_TRUE(parseVulkanShaderCode(gp::ConstVectorView<gp::UInt8>(bytes), decoded));

    EXPECT_EQ(decoded.header, original.header);
    EXPECT_EQ(decoded.resourceTable, original.resourceTable);
    EXPECT_EQ(decoded.spirv, original.spirv);
    EXPECT_TRUE(decoded.anyHitSpirv.isEmpty());
    EXPECT_TRUE(decoded.intersectionSpirv.isEmpty());
}

TEST(VulkanShaderSerializationTest, SeparateHitGroupBlobsRoundTrip)
{
    VulkanShaderCode original = makeRichShaderCode();
    original.header.rayGroupAnyHit = VulkanRayHitGroupEntrypoint::SeparateBlob;
    original.header.rayGroupIntersection = VulkanRayHitGroupEntrypoint::SeparateBlob;
    original.anyHitSpirv = makeMinimalSpirv();
    original.anyHitSpirv.pushBack(0x11u);
    original.intersectionSpirv = makeMinimalSpirv();
    original.intersectionSpirv.pushBack(0x22u);

    const gp::Vector<gp::UInt8> bytes = serializeVulkanShaderCode(original);
    VulkanShaderCode decoded{};
    ASSERT_TRUE(parseVulkanShaderCode(gp::ConstVectorView<gp::UInt8>(bytes), decoded));

    EXPECT_EQ(decoded.anyHitSpirv, original.anyHitSpirv);
    EXPECT_EQ(decoded.intersectionSpirv, original.intersectionSpirv);
}

TEST(VulkanShaderSerializationTest, EveryTruncationIsRejected)
{
    const gp::Vector<gp::UInt8> bytes = serializeVulkanShaderCode(makeRichShaderCode());

    for (gp::USize size = 0u; size < static_cast<gp::USize>(bytes.size()); ++size)
    {
        VulkanShaderCode decoded{};
        EXPECT_FALSE(parseVulkanShaderCode(gp::ConstVectorView<gp::UInt8>(bytes.data(), size), decoded))
            << "Truncated blob of " << size << " bytes was accepted.";
    }
}

TEST(VulkanShaderSerializationTest, BadMagicIsRejected)
{
    gp::Vector<gp::UInt8> bytes = serializeVulkanShaderCode(makeRichShaderCode());
    patchUInt32(bytes, 0u, 0x0BADF00Du);

    VulkanShaderCode decoded{};
    EXPECT_FALSE(parseVulkanShaderCode(gp::ConstVectorView<gp::UInt8>(bytes), decoded));
}

TEST(VulkanShaderSerializationTest, MismatchedVersionIsRejected)
{
    gp::Vector<gp::UInt8> bytes = serializeVulkanShaderCode(makeRichShaderCode());
    patchUInt32(bytes, 4u, kVulkanShaderCodeVersion + 1u);

    VulkanShaderCode decoded{};
    EXPECT_FALSE(parseVulkanShaderCode(gp::ConstVectorView<gp::UInt8>(bytes), decoded));
}

TEST(VulkanShaderSerializationTest, TrailingBytesAreRejected)
{
    gp::Vector<gp::UInt8> bytes = serializeVulkanShaderCode(makeRichShaderCode());
    bytes.pushBack(0u);

    VulkanShaderCode decoded{};
    EXPECT_FALSE(parseVulkanShaderCode(gp::ConstVectorView<gp::UInt8>(bytes), decoded));
}

TEST(VulkanShaderSerializationTest, OversizedBindingCountIsRejected)
{
    gp::Vector<gp::UInt8> bytes = serializeVulkanShaderCode(makeRichShaderCode());
    patchUInt32(bytes, kBindingCountOffset, 0xFFFFFFFFu);

    VulkanShaderCode decoded{};
    EXPECT_FALSE(parseVulkanShaderCode(gp::ConstVectorView<gp::UInt8>(bytes), decoded));
}

TEST(VulkanShaderSerializationTest, SpirvWithoutMagicIsRejected)
{
    VulkanShaderCode code = makeRichShaderCode();
    code.spirv[0] = 0u;
    const gp::Vector<gp::UInt8> bytes = serializeVulkanShaderCode(code);

    VulkanShaderCode decoded{};
    EXPECT_FALSE(parseVulkanShaderCode(gp::ConstVectorView<gp::UInt8>(bytes), decoded));
}

TEST(VulkanShaderSerializationTest, EmptySpirvIsRejected)
{
    VulkanShaderCode code = makeRichShaderCode();
    code.spirv.clear();
    const gp::Vector<gp::UInt8> bytes = serializeVulkanShaderCode(code);

    VulkanShaderCode decoded{};
    EXPECT_FALSE(parseVulkanShaderCode(gp::ConstVectorView<gp::UInt8>(bytes), decoded));
}

TEST(VulkanShaderSerializationTest, HeaderWithValidInputAttachmentIsAccepted)
{
    const gp::Vector<gp::UInt8> bytes =
        makeHeaderWithInputAttachment(0u, static_cast<gp::UInt8>(VulkanShaderAttachmentType::Depth));
    VulkanShaderCodeReader reader{ gp::ConstVectorView<gp::UInt8>(bytes) };
    VulkanShaderHeader header{};

    // The stream stops after the input attachments, so only that prefix is checked.
    const bool hasRead = readVulkanShaderHeader(reader, header);
    static_cast<void>(hasRead);
    ASSERT_EQ(header.inputAttachmentInfos.size(), 1u);
    EXPECT_EQ(header.inputAttachmentInfos[0].type, VulkanShaderAttachmentType::Depth);
}

TEST(VulkanShaderSerializationTest, OutOfRangeAttachmentTypeIsRejected)
{
    const gp::Vector<gp::UInt8> bytes = makeHeaderWithInputAttachment(0u, 200u);
    VulkanShaderCodeReader reader{ gp::ConstVectorView<gp::UInt8>(bytes) };
    VulkanShaderHeader header{};

    EXPECT_FALSE(readVulkanShaderHeader(reader, header));
    EXPECT_TRUE(reader.hasFailed());
}

TEST(VulkanShaderSerializationTest, AttachmentBindingPastTheBindingsIsRejected)
{
    const gp::Vector<gp::UInt8> bytes =
        makeHeaderWithInputAttachment(1u, static_cast<gp::UInt8>(VulkanShaderAttachmentType::Color0));
    VulkanShaderCodeReader reader{ gp::ConstVectorView<gp::UInt8>(bytes) };
    VulkanShaderHeader header{};

    EXPECT_FALSE(readVulkanShaderHeader(reader, header));
}

TEST(VulkanShaderSerializationTest, MoreUniformBuffersThanBindingsIsRejected)
{
    VulkanShaderCodeWriter writer;
    writer.writeUInt32(0u);
    writer.writeUInt32(1u);
    for (gp::UInt32 i = 0u; i < 32u; ++i)
    {
        writer.writeUInt8(0u);
    }
    VulkanShaderCodeReader reader(gp::ConstVectorView<gp::UInt8>(writer.getBytes()));
    VulkanShaderHeader header{};

    EXPECT_FALSE(readVulkanShaderHeader(reader, header));
}

TEST(VulkanShaderSerializationTest, ResourceTableRoundTrips)
{
    shader::ShaderResourceTable table{};
    table.resourceTableBits = 0xF0u;
    table.shaderResourceViewMap.pushBack(1u);
    table.unorderedAccessViewMap.pushBack(2u);
    table.resourceTableLayoutHashes.pushBack(3u);
    table.resourceCollectionMap.pushBack(4u);
    table.descriptorRangeMap.pushBack(5u);
    table.uniformBlockMap.pushBack(6u);

    VulkanShaderCodeWriter writer;
    writeShaderResourceTable(writer, table);
    VulkanShaderCodeReader reader(gp::ConstVectorView<gp::UInt8>(writer.getBytes()));
    shader::ShaderResourceTable decoded{};

    ASSERT_TRUE(readShaderResourceTable(reader, decoded));
    EXPECT_EQ(decoded, table);
    EXPECT_TRUE(reader.isAtEnd());
}

TEST(VulkanSpirvPatchingTest, SubpassReadIsRewrittenAsMultisampled)
{
    const gp::Vector<gp::UInt32> spirv = makeSubpassReadSpirv(true);
    gp::Vector<gp::UInt32> patched{};

    ASSERT_TRUE(patchSpirvInputAttachments(gp::ConstVectorView<gp::UInt32>(spirv), patched));

    // Two words are added: the `Sample` operand mask and the sample index.
    ASSERT_EQ(patched.size(), spirv.size() + 2u);

    // Header and the integer declarations are copied verbatim.
    for (gp::USize i = 0u; i < 13u; ++i)
    {
        EXPECT_EQ(patched[i], spirv[i]);
    }

    // The image type is flagged multisampled.
    EXPECT_EQ(patched[13], makeOp(9u, 25u));
    EXPECT_EQ(patched[19], 1u);

    // The read gains `Sample %2`.
    const gp::USize readOffset = 26u;
    EXPECT_EQ(patched[readOffset], makeOp(7u, 98u));
    EXPECT_EQ(patched[readOffset + 3u], 5u);
    EXPECT_EQ(patched[readOffset + 5u], 0x40u);
    EXPECT_EQ(patched[readOffset + 6u], 2u);
}

TEST(VulkanSpirvPatchingTest, ModuleWithoutSubpassInputsIsCopiedVerbatim)
{
    gp::Vector<gp::UInt32> spirv = makeMinimalSpirv();
    spirv.append({ makeOp(4u, 21u), 1u, 32u, 0u });
    gp::Vector<gp::UInt32> patched{};

    ASSERT_TRUE(patchSpirvInputAttachments(gp::ConstVectorView<gp::UInt32>(spirv), patched));
    EXPECT_EQ(patched, spirv);
}

TEST(VulkanSpirvPatchingTest, SubpassReadWithoutIntZeroFails)
{
    const gp::Vector<gp::UInt32> spirv = makeSubpassReadSpirv(false);
    gp::Vector<gp::UInt32> patched{};

    EXPECT_FALSE(patchSpirvInputAttachments(gp::ConstVectorView<gp::UInt32>(spirv), patched));
    EXPECT_TRUE(patched.isEmpty());
}

TEST(VulkanSpirvPatchingTest, MalformedInstructionLengthFails)
{
    gp::Vector<gp::UInt32> zeroLength = makeMinimalSpirv();
    zeroLength.pushBack(makeOp(0u, 21u));
    gp::Vector<gp::UInt32> overrun = makeMinimalSpirv();
    overrun.append({ makeOp(9u, 21u), 1u });
    gp::Vector<gp::UInt32> patched{};

    EXPECT_FALSE(patchSpirvInputAttachments(gp::ConstVectorView<gp::UInt32>(zeroLength), patched));
    EXPECT_FALSE(patchSpirvInputAttachments(gp::ConstVectorView<gp::UInt32>(overrun), patched));
}

TEST(VulkanSpirvPatchingTest, TruncatedHeaderFails)
{
    const gp::UInt32 words[3] = { kSpirvMagicNumber, 0x00010000u, 0u };
    gp::Vector<gp::UInt32> patched{};

    EXPECT_FALSE(patchSpirvInputAttachments(gp::ConstVectorView<gp::UInt32>(words, 3u), patched));
}

}   // namespace gp::rhi::vulkan::tests
