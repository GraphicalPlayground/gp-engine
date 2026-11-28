// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Hands finalized payloads to the GPU and waits on their sync points.
/// @details Contexts never touch `VulkanQueue` submission directly. `VulkanContextCommon::flushCommands()` finalizes
///          into payloads and passes them here, which lets the dynamic RHI route them through its submission thread
///          while unit tests and bring-up code use the direct `VulkanImmediatePayloadSubmitter`.
class GP_RHI_VULKAN_API IVulkanPayloadSubmitter
{
public:
    /// @brief Virtual destructor for safe polymorphic deletion.
    virtual ~IVulkanPayloadSubmitter() noexcept = default;

public:
    /// @brief Takes ownership of `payloads` and queues them for GPU submission in order.
    /// @param[in,out] payloads Finalized payloads; the vector is emptied on return.
    virtual void submitPayloads(gp::Vector<VulkanPayload*>& payloads) noexcept = 0;

    /// @brief Blocks until `syncPoint` is signaled or `timeoutNs` elapses.
    /// @param[in] syncPoint Sync point appended to one of the submitted payloads.
    /// @param[in] timeoutNs Maximum wait in nanoseconds.
    /// @return True when `syncPoint` completed before the timeout.
    [[nodiscard]] virtual bool waitForSyncPoint(const VulkanSyncPoint& syncPoint, gp::UInt64 timeoutNs) noexcept = 0;
};

}   // namespace gp::rhi::vulkan
