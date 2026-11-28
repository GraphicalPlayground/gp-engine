// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanBufferLocking.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "resources/Buffer.hpp"
#include "resources/VulkanBuffer.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Downcasts an RHI buffer to the Vulkan implementation.
/// @param[in] buffer Non-null buffer created by the Vulkan backend.
/// @return Vulkan buffer.
[[nodiscard]] VulkanBuffer& toVulkanBuffer(resource::Buffer* const buffer) noexcept
{
    GP_CHECK(buffer != nullptr);
    return *static_cast<VulkanBuffer*>(buffer);
}

}   // namespace

void* lockVulkanBufferBottomOfPipe(
    command::CommandListBase& cmdList,
    resource::Buffer* const buffer,
    const gp::UInt32 offset,
    const gp::UInt32 numBytes,
    const resource::ResourceLockMode lockMode
) noexcept
{
    return toVulkanBuffer(buffer).lock(cmdList, lockMode, numBytes, offset);
}

void unlockVulkanBufferBottomOfPipe(command::CommandListBase& cmdList, resource::Buffer* const buffer) noexcept
{
    toVulkanBuffer(buffer).unlock(cmdList);
}

void* lockVulkanBuffer(
    command::CommandListBase& cmdList,
    resource::Buffer* const buffer,
    const gp::UInt32 offset,
    const gp::UInt32 numBytes,
    const resource::ResourceLockMode lockMode
) noexcept
{
    return toVulkanBuffer(buffer).lock(cmdList, lockMode, numBytes, offset);
}

void unlockVulkanBuffer(command::CommandListBase& cmdList, resource::Buffer* const buffer) noexcept
{
    toVulkanBuffer(buffer).unlock(cmdList);
}

}   // namespace gp::rhi::vulkan

