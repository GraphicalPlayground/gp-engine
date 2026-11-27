// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Configuration of the graphics pipeline LRU, read once when a cache manager is created.
/// @details The LRU keeps the total driver-side size of live `VkPipeline` objects under a budget. Evicted pipelines
///          keep their description and are recreated on their next use, trading a hitch for memory on devices where
///          pipelines are expensive to keep around.
struct VulkanPipelineLRUSettings
{
    /// @brief The LRU tracks and evicts graphics pipelines (`r.Vulkan.EnablePipelineLRUCache`).
    bool isEnabled{ false };

    /// @brief Total byte budget of live graphics pipelines (`r.Vulkan.PipelineLRUSize`).
    gp::UInt32 maxSizeInBytes{ 512u * 1024u * 1024u };

    /// @brief Largest number of live graphics pipelines (`r.Vulkan.PipelineLRUCapacity`).
    gp::UInt32 capacity{ 8192u };

    /// @brief Frames without use after which `tickLRU` evicts a pipeline; 0 disables aging
    /// (`r.Vulkan.PSOLRUEvictAfterUnusedFrames`).
    gp::UInt32 evictAfterUnusedFrames{ 0u };

    /// @brief Evicting a pipeline also drops its shaders' cached modules
    /// (`r.Vulkan.ReleaseShaderModuleWhenEvictingPSO`).
    bool releaseShaderModulesOnEvict{ false };

    /// @brief Precached pipelines are not compiled at all, only recorded (`r.Vulkan.PipelineLRUCacheEvictImmediately`).
    bool evictPrecachedImmediately{ false };
};

/// @brief Reads the pipeline LRU console variables.
/// @return Current settings.
[[nodiscard]] GP_RHI_VULKAN_API VulkanPipelineLRUSettings getVulkanPipelineLRUSettings() noexcept;

}   // namespace gp::rhi::vulkan
