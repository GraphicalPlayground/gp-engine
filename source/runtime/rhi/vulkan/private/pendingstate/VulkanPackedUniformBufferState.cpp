// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pendingstate/VulkanPackedUniformBufferState.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "diagnostics/AssertionMacros.hpp"
#include "memory/Memory.hpp"

namespace gp::rhi::vulkan
{

void VulkanPackedUniformBufferState::initialize(const gp::UInt32 packedGlobalsSize) noexcept
{
    m_data.clear();
    m_data.resize(static_cast<gp::USize>(packedGlobalsSize), gp::UInt8{ 0u });
    m_isDirty = packedGlobalsSize != 0u;
}

void VulkanPackedUniformBufferState::reset() noexcept
{
    m_isDirty = !m_data.isEmpty();
}

void VulkanPackedUniformBufferState::setParameter(
    const gp::UInt32 byteOffset, const gp::UInt32 numBytes, const void* const data
) noexcept
{
    GP_CHECKF(
        static_cast<gp::USize>(byteOffset) + static_cast<gp::USize>(numBytes) <= static_cast<gp::USize>(m_data.size()),
        "Packed parameter [{}, {}) is outside the {}-byte packed globals buffer.",
        byteOffset,
        byteOffset + numBytes,
        m_data.size()
    );
    if ((numBytes == 0u) || (data == nullptr))
    {
        return;
    }

    gp::UInt8* const destination = m_data.data() + byteOffset;
    if (gp::memory::compareMemory(destination, data, static_cast<gp::USize>(numBytes)) != 0)
    {
        gp::memory::copyMemory(destination, data, static_cast<gp::USize>(numBytes));
        m_isDirty = true;
    }
}

}   // namespace gp::rhi::vulkan
