// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/GenericStagingBuffer.hpp"
#include "memory/Memory.hpp"

namespace gp::rhi
{

GenericStagingBuffer::~GenericStagingBuffer() noexcept = default;

void* GenericStagingBuffer::lock(const gp::UInt32 offset, const gp::UInt32 numBytes) noexcept
{
    GP_CHECKF(!m_isLocked, "GenericStagingBuffer::lock() called while already locked.");
    const gp::USize requiredEnd = static_cast<gp::USize>(offset) + static_cast<gp::USize>(numBytes);
    if (numBytes == 0u || requiredEnd > m_shadowData.size())
    {
        return nullptr;
    }

    m_isLocked = true;
    return m_shadowData.data() + offset;
}

void GenericStagingBuffer::unlock() noexcept
{
    GP_CHECKF(m_isLocked, "GenericStagingBuffer::unlock() called while not locked.");
    m_isLocked = false;
}

gp::UInt64 GenericStagingBuffer::getGPUSizeBytes() const noexcept
{
    return static_cast<gp::UInt64>(m_shadowData.size());
}

void GenericStagingBuffer::writeShadowData(const void* const srcData, const gp::UInt32 numBytes) noexcept
{
    m_shadowData.resize(static_cast<gp::USize>(numBytes));
    if (numBytes == 0u)
    {
        return;
    }

    if (srcData != nullptr)
    {
        gp::memory::copyMemory(m_shadowData.data(), srcData, static_cast<gp::USize>(numBytes));
    }
    else
    {
        gp::memory::setMemory(m_shadowData.data(), 0, static_cast<gp::USize>(numBytes));
    }
}

}   // namespace gp::rhi
