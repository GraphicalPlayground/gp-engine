// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                       // IWYU pragma: keep
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Non-owning bundle of the device-level services a Vulkan resource needs.
/// @details The device does not own its memory, staging and deletion managers, so resources receive them through
///          this bundle instead. The owner (the dynamic RHI, or a test fixture) keeps the bundle alive for longer than
///          every resource created from it.
struct GP_RHI_VULKAN_API VulkanResourceServices final
{
    /// @brief Logical device.
    VulkanDevice* device{ nullptr };

    /// @brief GPU memory manager serving buffer allocations.
    VulkanMemoryManager* memoryManager{ nullptr };

    /// @brief Host-visible staging buffer pool.
    VulkanStagingManager* stagingManager{ nullptr };

    /// @brief Deferred deletion queue for native handles; `nullptr` destroys handles immediately.
    VulkanDeferredDeletionQueue* deferredDeletionQueue{ nullptr };

    /// @brief Command context seam; required for any operation that records GPU commands.
    IVulkanCommandContextProvider* contextProvider{ nullptr };

    /// @brief Reports whether the device, memory and staging managers are all bound.
    /// @return True when resources can be allocated.
    [[nodiscard]] bool isValid() const noexcept;

    /// @brief Reports whether every device-local heap is also host visible (integrated GPUs).
    /// @return True on unified memory architectures.
    [[nodiscard]] bool hasUnifiedMemory() const noexcept;

    /// @brief Releases a `VkBuffer` handle once the GPU is done with it.
    /// @details Goes through the deferred deletion queue when one is bound, otherwise destroys the handle now.
    /// @param[in] buffer Handle to release; ignored when null.
    void enqueueBufferDeletion(VkBuffer buffer) const noexcept;

    /// @brief Releases a `VkImage` handle once the GPU is done with it.
    /// @details Goes through the deferred deletion queue when one is bound, otherwise destroys the handle now.
    /// @param[in] image Handle to release; ignored when null.
    void enqueueImageDeletion(VkImage image) const noexcept;

    /// @brief Releases a `VkImageView` handle once the GPU is done with it.
    /// @details Goes through the deferred deletion queue when one is bound, otherwise destroys the handle now.
    /// @param[in] imageView Handle to release; ignored when null.
    void enqueueImageViewDeletion(VkImageView imageView) const noexcept;

    /// @brief Releases a `VkBufferView` handle once the GPU is done with it.
    /// @details Goes through the deferred deletion queue when one is bound, otherwise destroys the handle now.
    /// @param[in] bufferView Handle to release; ignored when null.
    void enqueueBufferViewDeletion(VkBufferView bufferView) const noexcept;

    /// @brief Releases a `VkAccelerationStructureKHR` handle once the GPU is done with it.
    /// @details Goes through the deferred deletion queue when one is bound, otherwise destroys the handle now.
    /// @param[in] accelerationStructure Handle to release; ignored when null.
    void enqueueAccelerationStructureDeletion(VkAccelerationStructureKHR accelerationStructure) const noexcept;
};

}   // namespace gp::rhi::vulkan
