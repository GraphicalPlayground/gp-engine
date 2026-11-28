// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "shaders/ShaderEnums.hpp"

namespace gp::rhi::vulkan
{

enum class VulkanShaderAttachmentType : gp::UInt8;
enum class VulkanGlobalSamplerType : gp::UInt8;
enum class VulkanRayHitGroupEntrypoint : gp::UInt8;
enum class VulkanShaderUsageFlags : gp::UInt8;

struct VulkanShaderBindingInfo;
struct VulkanShaderUniformBufferInfo;
struct VulkanShaderInputAttachmentInfo;
struct VulkanShaderGlobalSamplerInfo;
struct VulkanSpecializationConstantValue;
struct VulkanSpecializationConstantData;
struct VulkanShaderHeader;
struct VulkanShaderCode;

class VulkanShaderCodeReader;
class VulkanShaderCodeWriter;
class VulkanShaderModule;
class VulkanShader;
class VulkanShaderFactory;

template <typename InRHIShaderType, shader::ShaderFrequency InFrequency>
class VulkanBaseShader;

}   // namespace gp::rhi::vulkan
