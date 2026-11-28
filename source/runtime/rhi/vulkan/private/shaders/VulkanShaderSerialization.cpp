// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "shaders/VulkanShaderSerialization.hpp"
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                 // IWYU pragma: keep
#include "logging/LogMacros.hpp"
#include "shaders/ShaderTypes.hpp"
#include "shaders/VulkanShaderCode.hpp"
#include "shaders/VulkanShaderCodeReader.hpp"
#include "shaders/VulkanShaderCodeWriter.hpp"
#include "shaders/VulkanShaderHeader.hpp"
#include "shaders/VulkanShaderHeaderEnums.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Reads a count-prefixed array of 32-bit words.
/// @param[in,out] reader Source reader.
/// @param[out] outValues Receives the words.
/// @return True on success.
bool readUInt32Array(VulkanShaderCodeReader& reader, gp::Vector<gp::UInt32>& outValues) noexcept
{
    gp::UInt32 count = 0u;
    if (!reader.readCount(count, 4u))
    {
        return false;
    }

    outValues.clear();
    outValues.reserve(count);
    for (gp::UInt32 i = 0u; i < count; ++i)
    {
        gp::UInt32 value = 0u;
        if (!reader.readUInt32(value))
        {
            return false;
        }
        outValues.pushBack(value);
    }
    return true;
}

/// @brief Writes a count-prefixed array of 32-bit words.
/// @param[in,out] writer Destination writer.
/// @param[in] values Words to write.
void writeUInt32Array(VulkanShaderCodeWriter& writer, const gp::Vector<gp::UInt32>& values) noexcept
{
    writer.writeUInt32(static_cast<gp::UInt32>(values.size()));
    for (const gp::UInt32 value: values)
    {
        writer.writeUInt32(value);
    }
}

/// @brief Reads a SPIR-V module stored as a byte size followed by its words.
/// @param[in,out] reader Source reader.
/// @param[out] outWords Receives the module.
/// @return True when the size is a non-zero multiple of four and the first word is the SPIR-V magic number.
bool readSpirvModule(VulkanShaderCodeReader& reader, gp::Vector<gp::UInt32>& outWords) noexcept
{
    gp::UInt32 sizeInBytes = 0u;
    if (!reader.readUInt32(sizeInBytes))
    {
        return false;
    }
    if ((sizeInBytes == 0u) || ((sizeInBytes % 4u) != 0u) || (sizeInBytes > reader.getRemaining()))
    {
        reader.markFailed();
        return false;
    }

    const gp::UInt32 wordCount = sizeInBytes / 4u;
    outWords.clear();
    outWords.reserve(wordCount);
    for (gp::UInt32 i = 0u; i < wordCount; ++i)
    {
        gp::UInt32 word = 0u;
        if (!reader.readUInt32(word))
        {
            return false;
        }
        outWords.pushBack(word);
    }

    if (outWords[0] != kSpirvMagicNumber)
    {
        reader.markFailed();
        return false;
    }
    return true;
}

/// @brief Writes a SPIR-V module as a byte size followed by its words.
/// @param[in,out] writer Destination writer.
/// @param[in] words Module to write.
void writeSpirvModule(VulkanShaderCodeWriter& writer, const gp::Vector<gp::UInt32>& words) noexcept
{
    writer.writeUInt32(static_cast<gp::UInt32>(words.size() * 4u));
    for (const gp::UInt32 word: words)
    {
        writer.writeUInt32(word);
    }
}

/// @brief Reads a ray hit group entry point storage value and validates its range.
/// @param[in,out] reader Source reader.
/// @param[out] outValue Receives the value.
/// @return True on success.
bool readHitGroupEntrypoint(VulkanShaderCodeReader& reader, VulkanRayHitGroupEntrypoint& outValue) noexcept
{
    gp::UInt8 raw = 0u;
    if (!reader.readUInt8(raw))
    {
        return false;
    }
    if (raw > static_cast<gp::UInt8>(VulkanRayHitGroupEntrypoint::SeparateBlob))
    {
        reader.markFailed();
        return false;
    }
    outValue = static_cast<VulkanRayHitGroupEntrypoint>(raw);
    return true;
}

}   // namespace

bool readVulkanShaderHeader(VulkanShaderCodeReader& reader, VulkanShaderHeader& outHeader) noexcept
{
    outHeader = VulkanShaderHeader{};

    // Bindings: descriptor type and debug name. The name is always serialized so the byte layout does not depend on
    // the build configuration; it is only kept when binding debug names are enabled.
    gp::UInt32 bindingCount = 0u;
    if (!reader.readCount(bindingCount, 8u))
    {
        return false;
    }
    outHeader.bindings.reserve(bindingCount);
    for (gp::UInt32 i = 0u; i < bindingCount; ++i)
    {
        VulkanShaderBindingInfo binding{};
        gp::String debugName{};
        if (!reader.readUInt32(binding.descriptorType) || !reader.readString(debugName))
        {
            return false;
        }
#if GP_VULKAN_ENABLE_BINDING_DEBUG_NAMES
        binding.debugName = debugName;
#endif
        outHeader.bindings.pushBack(binding);
    }

    gp::UInt32 uniformBufferCount = 0u;
    if (!reader.readCount(uniformBufferCount, 7u))
    {
        return false;
    }
    if (uniformBufferCount > bindingCount)
    {
        reader.markFailed();
        return false;
    }
    outHeader.uniformBufferInfos.reserve(uniformBufferCount);
    for (gp::UInt32 i = 0u; i < uniformBufferCount; ++i)
    {
        VulkanShaderUniformBufferInfo info{};
        if (!reader.readUInt32(info.layoutHash) || !reader.readUInt8(info.staticSlot) ||
            !reader.readUInt8(info.hasResources) || !reader.readUInt8(info.bindlessCBIndex))
        {
            return false;
        }
        outHeader.uniformBufferInfos.pushBack(info);
    }

    gp::UInt32 inputAttachmentCount = 0u;
    if (!reader.readCount(inputAttachmentCount, 2u))
    {
        return false;
    }
    outHeader.inputAttachmentInfos.reserve(inputAttachmentCount);
    for (gp::UInt32 i = 0u; i < inputAttachmentCount; ++i)
    {
        gp::UInt8 bindingIndex = 0u;
        gp::UInt8 type = 0u;
        if (!reader.readUInt8(bindingIndex) || !reader.readUInt8(type))
        {
            return false;
        }
        if ((bindingIndex >= bindingCount) || (type >= static_cast<gp::UInt8>(VulkanShaderAttachmentType::Count)))
        {
            reader.markFailed();
            return false;
        }
        outHeader.inputAttachmentInfos.pushBack(
            VulkanShaderInputAttachmentInfo{ bindingIndex, static_cast<VulkanShaderAttachmentType>(type) }
        );
    }

    gp::UInt32 globalSamplerCount = 0u;
    if (!reader.readCount(globalSamplerCount, 2u))
    {
        return false;
    }
    outHeader.globalSamplerInfos.reserve(globalSamplerCount);
    for (gp::UInt32 i = 0u; i < globalSamplerCount; ++i)
    {
        gp::UInt8 bindingIndex = 0u;
        gp::UInt8 type = 0u;
        if (!reader.readUInt8(bindingIndex) || !reader.readUInt8(type))
        {
            return false;
        }
        if ((bindingIndex >= bindingCount) || (type >= static_cast<gp::UInt8>(VulkanGlobalSamplerType::Count)))
        {
            reader.markFailed();
            return false;
        }
        outHeader.globalSamplerInfos.pushBack(
            VulkanShaderGlobalSamplerInfo{ bindingIndex, static_cast<VulkanGlobalSamplerType>(type) }
        );
    }

    if (!reader.readUInt32(outHeader.numBoundUniformBuffers) || !reader.readUInt32(outHeader.packedGlobalsSize) ||
        !reader.readUInt32(outHeader.inputAttachmentsMask) || !reader.readUInt32(outHeader.inOutMask) ||
        !reader.readUInt32(outHeader.rayTracingPayloadType) || !reader.readUInt32(outHeader.rayTracingPayloadSize))
    {
        return false;
    }
    if (outHeader.numBoundUniformBuffers > uniformBufferCount)
    {
        reader.markFailed();
        return false;
    }

    gp::UInt32 constantCount = 0u;
    if (!reader.readCount(constantCount, 8u))
    {
        return false;
    }
    outHeader.specializationConstantData.values.reserve(constantCount);
    for (gp::UInt32 i = 0u; i < constantCount; ++i)
    {
        VulkanSpecializationConstantValue value{};
        if (!reader.readUInt32(value.constantId) || !reader.readUInt32(value.defaultValue))
        {
            return false;
        }
        outHeader.specializationConstantData.values.pushBack(value);
    }

    gp::UInt32 exclusionCount = 0u;
    if (!reader.readCount(exclusionCount, 8u))
    {
        return false;
    }
    outHeader.specializationConstantData.exclusions.reserve(exclusionCount);
    for (gp::UInt32 i = 0u; i < exclusionCount; ++i)
    {
        shader::ShaderSpecializationExclusion exclusion{};
        if (!reader.readUInt32(exclusion.bindPoint) || !readUInt32Array(reader, exclusion.specializationHashes))
        {
            return false;
        }
        outHeader.specializationConstantData.exclusions.pushBack(exclusion);
    }

    gp::UInt8 usageFlags = 0u;
    if (!reader.readUInt64(outHeader.codeHash.low) || !reader.readUInt64(outHeader.codeHash.high) ||
        !reader.readUInt32(outHeader.spirvCRC) || !reader.readUInt8(outHeader.waveSize) ||
        !reader.readUInt8(usageFlags) || !readHitGroupEntrypoint(reader, outHeader.rayGroupAnyHit) ||
        !readHitGroupEntrypoint(reader, outHeader.rayGroupIntersection) || !reader.readString(outHeader.debugName))
    {
        return false;
    }
    outHeader.usageFlags = static_cast<VulkanShaderUsageFlags>(usageFlags);
    return true;
}

void writeVulkanShaderHeader(VulkanShaderCodeWriter& writer, const VulkanShaderHeader& header) noexcept
{
    writer.writeUInt32(static_cast<gp::UInt32>(header.bindings.size()));
    for (const VulkanShaderBindingInfo& binding: header.bindings)
    {
        writer.writeUInt32(binding.descriptorType);
#if GP_VULKAN_ENABLE_BINDING_DEBUG_NAMES
        writer.writeString(binding.debugName);
#else
        writer.writeString(gp::String{});
#endif
    }

    writer.writeUInt32(static_cast<gp::UInt32>(header.uniformBufferInfos.size()));
    for (const VulkanShaderUniformBufferInfo& info: header.uniformBufferInfos)
    {
        writer.writeUInt32(info.layoutHash);
        writer.writeUInt8(info.staticSlot);
        writer.writeUInt8(info.hasResources);
        writer.writeUInt8(info.bindlessCBIndex);
    }

    writer.writeUInt32(static_cast<gp::UInt32>(header.inputAttachmentInfos.size()));
    for (const VulkanShaderInputAttachmentInfo& info: header.inputAttachmentInfos)
    {
        writer.writeUInt8(info.bindingIndex);
        writer.writeUInt8(static_cast<gp::UInt8>(info.type));
    }

    writer.writeUInt32(static_cast<gp::UInt32>(header.globalSamplerInfos.size()));
    for (const VulkanShaderGlobalSamplerInfo& info: header.globalSamplerInfos)
    {
        writer.writeUInt8(info.bindingIndex);
        writer.writeUInt8(static_cast<gp::UInt8>(info.type));
    }

    writer.writeUInt32(header.numBoundUniformBuffers);
    writer.writeUInt32(header.packedGlobalsSize);
    writer.writeUInt32(header.inputAttachmentsMask);
    writer.writeUInt32(header.inOutMask);
    writer.writeUInt32(header.rayTracingPayloadType);
    writer.writeUInt32(header.rayTracingPayloadSize);

    writer.writeUInt32(static_cast<gp::UInt32>(header.specializationConstantData.values.size()));
    for (const VulkanSpecializationConstantValue& value: header.specializationConstantData.values)
    {
        writer.writeUInt32(value.constantId);
        writer.writeUInt32(value.defaultValue);
    }

    writer.writeUInt32(static_cast<gp::UInt32>(header.specializationConstantData.exclusions.size()));
    for (const shader::ShaderSpecializationExclusion& exclusion: header.specializationConstantData.exclusions)
    {
        writer.writeUInt32(exclusion.bindPoint);
        writeUInt32Array(writer, exclusion.specializationHashes);
    }

    writer.writeUInt64(header.codeHash.low);
    writer.writeUInt64(header.codeHash.high);
    writer.writeUInt32(header.spirvCRC);
    writer.writeUInt8(header.waveSize);
    writer.writeUInt8(static_cast<gp::UInt8>(header.usageFlags));
    writer.writeUInt8(static_cast<gp::UInt8>(header.rayGroupAnyHit));
    writer.writeUInt8(static_cast<gp::UInt8>(header.rayGroupIntersection));
    writer.writeString(header.debugName);
}

bool readShaderResourceTable(VulkanShaderCodeReader& reader, shader::ShaderResourceTable& outTable) noexcept
{
    outTable = shader::ShaderResourceTable{};
    return reader.readUInt32(outTable.resourceTableBits) && readUInt32Array(reader, outTable.shaderResourceViewMap) &&
           readUInt32Array(reader, outTable.samplerMap) && readUInt32Array(reader, outTable.unorderedAccessViewMap) &&
           readUInt32Array(reader, outTable.resourceTableLayoutHashes) &&
           readUInt32Array(reader, outTable.textureMap) && readUInt32Array(reader, outTable.resourceCollectionMap) &&
           readUInt32Array(reader, outTable.descriptorRangeMap) && readUInt32Array(reader, outTable.uniformBlockMap);
}

void writeShaderResourceTable(VulkanShaderCodeWriter& writer, const shader::ShaderResourceTable& table) noexcept
{
    writer.writeUInt32(table.resourceTableBits);
    writeUInt32Array(writer, table.shaderResourceViewMap);
    writeUInt32Array(writer, table.samplerMap);
    writeUInt32Array(writer, table.unorderedAccessViewMap);
    writeUInt32Array(writer, table.resourceTableLayoutHashes);
    writeUInt32Array(writer, table.textureMap);
    writeUInt32Array(writer, table.resourceCollectionMap);
    writeUInt32Array(writer, table.descriptorRangeMap);
    writeUInt32Array(writer, table.uniformBlockMap);
}

bool parseVulkanShaderCode(const gp::ConstVectorView<gp::UInt8> bytes, VulkanShaderCode& outCode) noexcept
{
    VulkanShaderCodeReader reader(bytes);

    gp::UInt32 magic = 0u;
    gp::UInt32 version = 0u;
    if (!reader.readUInt32(magic) || !reader.readUInt32(version) || (magic != kVulkanShaderCodeMagic))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Shader code is not a Vulkan shader blob ({} bytes).", bytes.size());
        return false;
    }
    if (version != kVulkanShaderCodeVersion)
    {
        GP_LOG_ERROR(
            LogVulkanRHI,
            "Shader code version {} does not match the expected version {}.",
            version,
            kVulkanShaderCodeVersion
        );
        return false;
    }

    if (!readVulkanShaderHeader(reader, outCode.header))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Shader code header is malformed (offset {}).", reader.getOffset());
        return false;
    }
    if (!readShaderResourceTable(reader, outCode.resourceTable))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Shader code resource table is malformed (offset {}).", reader.getOffset());
        return false;
    }
    if (!readSpirvModule(reader, outCode.spirv))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Shader code SPIR-V is malformed (offset {}).", reader.getOffset());
        return false;
    }

    outCode.anyHitSpirv.clear();
    outCode.intersectionSpirv.clear();
    if ((outCode.header.rayGroupAnyHit == VulkanRayHitGroupEntrypoint::SeparateBlob) &&
        !readSpirvModule(reader, outCode.anyHitSpirv))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Shader code any-hit SPIR-V is malformed (offset {}).", reader.getOffset());
        return false;
    }
    if ((outCode.header.rayGroupIntersection == VulkanRayHitGroupEntrypoint::SeparateBlob) &&
        !readSpirvModule(reader, outCode.intersectionSpirv))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Shader code intersection SPIR-V is malformed (offset {}).", reader.getOffset());
        return false;
    }

    if (!reader.isAtEnd())
    {
        GP_LOG_ERROR(LogVulkanRHI, "Shader code has {} unexpected trailing bytes.", reader.getRemaining());
        return false;
    }
    return true;
}

gp::Vector<gp::UInt8> serializeVulkanShaderCode(const VulkanShaderCode& code) noexcept
{
    VulkanShaderCodeWriter writer;
    writer.writeUInt32(kVulkanShaderCodeMagic);
    writer.writeUInt32(kVulkanShaderCodeVersion);
    writeVulkanShaderHeader(writer, code.header);
    writeShaderResourceTable(writer, code.resourceTable);
    writeSpirvModule(writer, code.spirv);
    if (code.header.rayGroupAnyHit == VulkanRayHitGroupEntrypoint::SeparateBlob)
    {
        writeSpirvModule(writer, code.anyHitSpirv);
    }
    if (code.header.rayGroupIntersection == VulkanRayHitGroupEntrypoint::SeparateBlob)
    {
        writeSpirvModule(writer, code.intersectionSpirv);
    }
    return writer.releaseBytes();
}

}   // namespace gp::rhi::vulkan
