// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

enum class VulkanDynamicUniformBufferMode : gp::UInt8;

union VulkanHashableDescriptorInfo;
struct VulkanUniformBufferGatherInfo;
struct VulkanDescriptorSetLayoutEntry;
struct VulkanDescriptorSetWriteContainer;

class VulkanDescriptorSetKey;
class VulkanDescriptorSetsLayoutInfo;
class VulkanDescriptorSetLayoutCache;
class VulkanDescriptorSetsLayout;
class VulkanPipelineLayout;
class VulkanDescriptorPool;
class VulkanTypedDescriptorPoolSet;
class VulkanDescriptorPoolSetContainer;
class VulkanDescriptorPoolsManager;
class VulkanDescriptorSetWriter;
class VulkanGenericDescriptorPool;
class VulkanCachedDescriptorPool;
class VulkanDescriptorSetCache;

}   // namespace gp::rhi::vulkan
