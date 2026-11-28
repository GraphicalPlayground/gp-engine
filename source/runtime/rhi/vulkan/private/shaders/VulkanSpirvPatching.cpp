// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "shaders/VulkanSpirvPatching.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Number of words in the SPIR-V module header.
constexpr gp::UInt32 kSpirvHeaderLength = 5u;

/// @brief `OpTypeInt` opcode.
constexpr gp::UInt32 kOpTypeInt = 21u;

/// @brief `OpTypeImage` opcode.
constexpr gp::UInt32 kOpTypeImage = 25u;

/// @brief `OpConstant` opcode.
constexpr gp::UInt32 kOpConstant = 43u;

/// @brief `OpLoad` opcode.
constexpr gp::UInt32 kOpLoad = 61u;

/// @brief `OpImageRead` opcode.
constexpr gp::UInt32 kOpImageRead = 98u;

/// @brief `Dim` value of subpass input images.
constexpr gp::UInt32 kDimSubpassData = 6u;

/// @brief `Sample` image operand mask bit.
constexpr gp::UInt32 kImageOperandSample = 0x40u;

/// @brief Reports whether `id` is in `ids`.
/// @param[in] ids Tracked result IDs.
/// @param[in] id ID to look up.
/// @return True when present.
[[nodiscard]] bool containsId(const gp::Vector<gp::UInt32>& ids, const gp::UInt32 id) noexcept
{
    for (const gp::UInt32 candidate: ids)
    {
        if (candidate == id)
        {
            return true;
        }
    }
    return false;
}

}   // namespace

bool patchSpirvInputAttachments(
    const gp::ConstVectorView<gp::UInt32> spirv, gp::Vector<gp::UInt32>& outPatched
) noexcept
{
    outPatched.clear();

    const gp::USize length = static_cast<gp::USize>(spirv.size());
    if (length < kSpirvHeaderLength)
    {
        return false;
    }

    const gp::UInt32* const words = spirv.data();
    outPatched.reserve(static_cast<gp::Vector<gp::UInt32>::SizeType>(length + 8u));
    outPatched.append(words, kSpirvHeaderLength);

    gp::UInt32 signedIntType = 0u;
    gp::UInt32 constantZero = 0u;
    gp::Vector<gp::UInt32> subpassImageIds{};

    gp::USize position = kSpirvHeaderLength;
    while (position < length)
    {
        const gp::UInt32 instructionLength = words[position] >> 16u;
        const gp::UInt32 opcode = words[position] & 0x0000FFFFu;
        if ((instructionLength == 0u) || (instructionLength > (length - position)))
        {
            outPatched.clear();
            return false;
        }

        const gp::USize firstPatchedWord = static_cast<gp::USize>(outPatched.size());
        const gp::UInt32* const instruction = words + position;
        bool isRewritten = false;

        if ((opcode == kOpTypeInt) && (instructionLength >= 4u) && (instruction[3] == 1u))
        {
            // OpTypeInt <result> <width> <signedness>: remember the signed integer type.
            signedIntType = instruction[1];
        }
        else if (
            (opcode == kOpConstant) && (instructionLength >= 4u) && (signedIntType != 0u) &&
            (instruction[1] == signedIntType) && (instruction[3] == 0u)
        )
        {
            // OpConstant <type> <result> <value>: the signed `int 0` used as the sample index.
            constantZero = instruction[2];
        }
        else if ((opcode == kOpTypeImage) && (instructionLength >= 9u) && (instruction[3] == kDimSubpassData))
        {
            // OpTypeImage <result> <sampled type> <dim> <depth> <arrayed> <MS> ...: flag the image multisampled.
            outPatched.append(instruction, instructionLength);
            outPatched[firstPatchedWord + 6u] = 1u;
            subpassImageIds.pushBack(instruction[1]);
            isRewritten = true;
        }
        else if ((opcode == kOpLoad) && (instructionLength >= 4u) && containsId(subpassImageIds, instruction[1]))
        {
            // OpLoad <result type> <result> <pointer>: the loaded value is also a subpass image.
            subpassImageIds.pushBack(instruction[2]);
        }
        else if ((opcode == kOpImageRead) && (instructionLength == 5u) && containsId(subpassImageIds, instruction[3]))
        {
            // OpImageRead <result type> <result> <image> <coordinate> -> append `Sample <int 0>`.
            if (constantZero == 0u)
            {
                outPatched.clear();
                return false;
            }
            outPatched.pushBack((7u << 16u) | kOpImageRead);
            outPatched.append(instruction + 1u, 4u);
            outPatched.pushBack(kImageOperandSample);
            outPatched.pushBack(constantZero);
            isRewritten = true;
        }

        if (!isRewritten)
        {
            outPatched.append(instruction, instructionLength);
        }
        position += instructionLength;
    }
    return true;
}

}   // namespace gp::rhi::vulkan
