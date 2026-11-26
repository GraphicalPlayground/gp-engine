// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "shaders/VulkanShaderCodeWriter.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <utility>

namespace gp::rhi::vulkan
{

void VulkanShaderCodeWriter::writeUInt8(const gp::UInt8 value) noexcept
{
    m_bytes.pushBack(value);
}

void VulkanShaderCodeWriter::writeUInt32(const gp::UInt32 value) noexcept
{
    for (gp::UInt32 i = 0u; i < 4u; ++i)
    {
        m_bytes.pushBack(static_cast<gp::UInt8>((value >> (8u * i)) & 0xFFu));
    }
}

void VulkanShaderCodeWriter::writeUInt64(const gp::UInt64 value) noexcept
{
    for (gp::UInt32 i = 0u; i < 8u; ++i)
    {
        m_bytes.pushBack(static_cast<gp::UInt8>((value >> (8u * i)) & 0xFFu));
    }
}

void VulkanShaderCodeWriter::writeBytes(const void* const source, const gp::USize sizeInBytes) noexcept
{
    if (sizeInBytes == 0u)
    {
        return;
    }
    m_bytes.append(static_cast<const gp::UInt8*>(source), static_cast<gp::Vector<gp::UInt8>::SizeType>(sizeInBytes));
}

void VulkanShaderCodeWriter::writeString(const gp::String& value) noexcept
{
    writeUInt32(static_cast<gp::UInt32>(value.size()));
    writeBytes(value.data(), static_cast<gp::USize>(value.size()));
}

gp::Vector<gp::UInt8> VulkanShaderCodeWriter::releaseBytes() noexcept
{
    gp::Vector<gp::UInt8> bytes(std::move(m_bytes));
    m_bytes.clear();
    return bytes;
}

}   // namespace gp::rhi::vulkan
