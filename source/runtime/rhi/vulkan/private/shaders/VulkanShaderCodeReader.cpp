// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "shaders/VulkanShaderCodeReader.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <cstring>

namespace gp::rhi::vulkan
{

VulkanShaderCodeReader::VulkanShaderCodeReader(const gp::ConstVectorView<gp::UInt8> bytes) noexcept
    : m_data(bytes.data())
    , m_size(static_cast<gp::USize>(bytes.size()))
{}

bool VulkanShaderCodeReader::canRead(const gp::USize sizeInBytes) noexcept
{
    if (m_hasFailed || (sizeInBytes > (m_size - m_offset)))
    {
        m_hasFailed = true;
        return false;
    }
    return true;
}

bool VulkanShaderCodeReader::readUInt8(gp::UInt8& outValue) noexcept
{
    if (!canRead(1u))
    {
        return false;
    }
    outValue = m_data[m_offset];
    ++m_offset;
    return true;
}

bool VulkanShaderCodeReader::readUInt32(gp::UInt32& outValue) noexcept
{
    if (!canRead(4u))
    {
        return false;
    }

    // Assemble byte by byte so the format stays little-endian regardless of the host.
    gp::UInt32 value = 0u;
    for (gp::USize i = 0u; i < 4u; ++i)
    {
        value |= static_cast<gp::UInt32>(m_data[m_offset + i]) << (8u * i);
    }
    m_offset += 4u;
    outValue = value;
    return true;
}

bool VulkanShaderCodeReader::readUInt64(gp::UInt64& outValue) noexcept
{
    if (!canRead(8u))
    {
        return false;
    }

    gp::UInt64 value = 0u;
    for (gp::USize i = 0u; i < 8u; ++i)
    {
        value |= static_cast<gp::UInt64>(m_data[m_offset + i]) << (8u * i);
    }
    m_offset += 8u;
    outValue = value;
    return true;
}

bool VulkanShaderCodeReader::readBytes(void* const outDestination, const gp::USize sizeInBytes) noexcept
{
    if (!canRead(sizeInBytes))
    {
        return false;
    }
    if (sizeInBytes != 0u)
    {
        std::memcpy(outDestination, m_data + m_offset, sizeInBytes);
    }
    m_offset += sizeInBytes;
    return true;
}

bool VulkanShaderCodeReader::readCount(gp::UInt32& outCount, const gp::USize minBytesPerElement) noexcept
{
    GP_ASSERT(minBytesPerElement > 0u);

    gp::UInt32 count = 0u;
    if (!readUInt32(count))
    {
        return false;
    }

    // Reject counts the remaining bytes cannot possibly hold before any container is sized from them.
    if (static_cast<gp::USize>(count) > (getRemaining() / minBytesPerElement))
    {
        m_hasFailed = true;
        return false;
    }
    outCount = count;
    return true;
}

bool VulkanShaderCodeReader::readString(gp::String& outString) noexcept
{
    gp::UInt32 length = 0u;
    if (!readCount(length, 1u))
    {
        return false;
    }

    const char* const characters = reinterpret_cast<const char*>(m_data + m_offset);
    outString = gp::String(characters, static_cast<gp::String::SizeType>(length));
    m_offset += length;
    return true;
}

bool VulkanShaderCodeReader::readView(const gp::USize sizeInBytes, gp::ConstVectorView<gp::UInt8>& outView) noexcept
{
    if (!canRead(sizeInBytes))
    {
        return false;
    }
    outView = gp::ConstVectorView<gp::UInt8>(m_data + m_offset, sizeInBytes);
    m_offset += sizeInBytes;
    return true;
}

}   // namespace gp::rhi::vulkan
