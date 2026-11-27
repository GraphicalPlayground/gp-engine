// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "shaders/VulkanShaderCode.hpp"
#include "shaders/VulkanShaderSerialization.hpp"

namespace gp::rhi::vulkan::tests
{

/// @brief SPIR-V execution model of a vertex shader.
inline constexpr gp::UInt32 kSpirvExecutionModelVertex = 0u;

/// @brief SPIR-V execution model of a compute shader.
inline constexpr gp::UInt32 kSpirvExecutionModelGLCompute = 5u;

/// @brief Builds an instruction's first word.
/// @param[in] length Instruction word count.
/// @param[in] opcode Opcode.
/// @return Encoded word.
[[nodiscard]] inline constexpr gp::UInt32 makeSpirvOp(const gp::UInt32 length, const gp::UInt32 opcode) noexcept
{
    return (length << 16u) | opcode;
}

/// @brief Writes `value` as 8 lowercase hex digits.
/// @param[in] value Value to format.
/// @param[out] outDigits Receives exactly 8 characters.
inline void writeTestHex8(const gp::UInt32 value, char* const outDigits) noexcept
{
    constexpr char kDigits[] = "0123456789abcdef";
    for (gp::UInt32 index = 0u; index < 8u; ++index)
    {
        outDigits[index] = kDigits[(value >> ((7u - index) * 4u)) & 0xFu];
    }
}

/// @brief Builds a complete, valid `void main() {}` module whose entry point carries the backend's mangled name.
/// @details The backend calls every entry point `main_<spirv byte size>_<crc>`, so the name is written after the
///          module is assembled: the 22 characters and terminator always take 6 words, so the size is known.
/// @param[in] executionModel `kSpirvExecutionModelVertex` or `kSpirvExecutionModelGLCompute`.
/// @param[in] spirvCRC CRC stored in the shader header, which the entry point name repeats.
/// @return SPIR-V words.
[[nodiscard]] inline gp::Vector<gp::UInt32> makeTestSpirv(const gp::UInt32 executionModel, const gp::UInt32 spirvCRC)
{
    constexpr gp::USize kNameWordOffset = 13u;
    constexpr gp::USize kNameWordCount = 6u;

    gp::Vector<gp::UInt32> words{};
    words.append({ 0x07230203u, 0x00010000u, 0u, 5u, 0u });
    words.append({ makeSpirvOp(2u, 17u), 1u });       // OpCapability Shader
    words.append({ makeSpirvOp(3u, 14u), 0u, 1u });   // OpMemoryModel Logical GLSL450
    words.append({ makeSpirvOp(9u, 15u), executionModel, 1u, 0u, 0u, 0u, 0u, 0u, 0u });   // OpEntryPoint %1 "..."
    if (executionModel == kSpirvExecutionModelGLCompute)
    {
        words.append({ makeSpirvOp(6u, 16u), 1u, 17u, 1u, 1u, 1u });   // OpExecutionMode %1 LocalSize 1 1 1
    }
    words.append({ makeSpirvOp(2u, 19u), 2u });                        // %2 = OpTypeVoid
    words.append({ makeSpirvOp(3u, 33u), 3u, 2u });                    // %3 = OpTypeFunction %2
    words.append({ makeSpirvOp(5u, 54u), 2u, 1u, 0u, 3u });            // %1 = OpFunction %2 None %3
    words.append({ makeSpirvOp(2u, 248u), 4u });                       // %4 = OpLabel
    words.append({ makeSpirvOp(1u, 253u) });                           // OpReturn
    words.append({ makeSpirvOp(1u, 56u) });                            // OpFunctionEnd

    char name[kNameWordCount * 4u] = {};
    name[0] = 'm';
    name[1] = 'a';
    name[2] = 'i';
    name[3] = 'n';
    name[4] = '_';
    writeTestHex8(static_cast<gp::UInt32>(words.size() * sizeof(gp::UInt32)), name + 5);
    name[13] = '_';
    writeTestHex8(spirvCRC, name + 14);
    for (gp::USize word = 0u; word < kNameWordCount; ++word)
    {
        gp::UInt32 packed = 0u;
        for (gp::USize byte = 0u; byte < 4u; ++byte)
        {
            packed |= static_cast<gp::UInt32>(static_cast<gp::UInt8>(name[word * 4u + byte])) << (byte * 8u);
        }
        words[kNameWordOffset + word] = packed;
    }
    return words;
}

/// @brief Serializes a shader blob around a generated module.
/// @param[in] executionModel SPIR-V execution model of the module.
/// @param[in] spirvCRC CRC stored in the header.
/// @param[in] debugName Header debug name; changing it changes the blob and therefore the shader key.
/// @return Blob bytes.
[[nodiscard]] inline gp::Vector<gp::UInt8>
    makeTestShaderBlob(const gp::UInt32 executionModel, const gp::UInt32 spirvCRC, const char* const debugName)
{
    VulkanShaderCode code{};
    code.header.spirvCRC = spirvCRC;
    code.header.debugName = gp::String(debugName);
    code.spirv = makeTestSpirv(executionModel, spirvCRC);
    return serializeVulkanShaderCode(code);
}

}   // namespace gp::rhi::vulkan::tests
