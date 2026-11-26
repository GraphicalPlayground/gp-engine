// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"    // IWYU pragma: keep
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "resources/UniformBufferEnums.hpp"
#include "RHIDefinitions.hpp"
#include "shaders/ShaderForward.hpp"   // IWYU pragma: keep
#include "shaders/ShaderTypes.hpp"
#include "shaders/VulkanShaderHeaderEnums.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

/// @brief One descriptor binding declared by a shader; its index in `VulkanShaderHeader::bindings` is the slot.
struct VulkanShaderBindingInfo
{
    /// @brief `VkDescriptorType` of the binding, stored as its raw 32-bit value.
    gp::UInt32 descriptorType{ 0u };

#if GP_VULKAN_ENABLE_BINDING_DEBUG_NAMES
    /// @brief Name of the shader parameter behind this binding.
    gp::String debugName{};
#endif

    /// @brief Memberwise equality comparison.
    /// @param[in] rhs Binding to compare against.
    /// @return True when every serialized field matches.
    [[nodiscard]] bool operator==(const VulkanShaderBindingInfo& rhs) const noexcept = default;
};

/// @brief Metadata of a uniform buffer binding, stored at the same index as its `VulkanShaderBindingInfo`.
struct VulkanShaderUniformBufferInfo
{
    /// @brief Hash of the uniform buffer layout the shader was compiled against.
    gp::UInt32 layoutHash{ 0u };

    /// @brief Global static slot of the uniform buffer, or `kMaxUniformBufferStaticSlots` when it has none.
    /// @note The compiler resolves the slot offline, so the backend does not need a layout registry at load time.
    resource::UniformBufferStaticSlot staticSlot{ kMaxUniformBufferStaticSlots };

    /// @brief Non-zero when the uniform buffer also carries resources (textures, buffers, samplers).
    gp::UInt8 hasResources{ 0u };

    /// @brief Index of the constant buffer in the bindless heap, used by bindless shaders.
    gp::UInt8 bindlessCBIndex{ 0u };

    /// @brief Memberwise equality comparison.
    /// @param[in] rhs Entry to compare against.
    /// @return True when every field matches.
    [[nodiscard]] bool operator==(const VulkanShaderUniformBufferInfo& rhs) const noexcept = default;
};

/// @brief Maps an input attachment binding to the render target it reads.
struct VulkanShaderInputAttachmentInfo
{
    /// @brief Index of the `VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT` binding.
    gp::UInt8 bindingIndex{ 0u };

    /// @brief Render target read by the binding.
    VulkanShaderAttachmentType type{ VulkanShaderAttachmentType::Depth };

    /// @brief Memberwise equality comparison.
    /// @param[in] rhs Entry to compare against.
    /// @return True when every field matches.
    [[nodiscard]] bool operator==(const VulkanShaderInputAttachmentInfo& rhs) const noexcept = default;
};

/// @brief Maps a sampler binding to one of the engine-wide immutable samplers.
struct VulkanShaderGlobalSamplerInfo
{
    /// @brief Index of the sampler binding.
    gp::UInt8 bindingIndex{ 0u };

    /// @brief Global sampler bound at `bindingIndex`.
    VulkanGlobalSamplerType type{ VulkanGlobalSamplerType::Invalid };

    /// @brief Memberwise equality comparison.
    /// @param[in] rhs Entry to compare against.
    /// @return True when every field matches.
    [[nodiscard]] bool operator==(const VulkanShaderGlobalSamplerInfo& rhs) const noexcept = default;
};

/// @brief One specialization constant declared by a shader.
struct VulkanSpecializationConstantValue
{
    /// @brief SPIR-V `SpecId` of the constant.
    gp::UInt32 constantId{ 0u };

    /// @brief Default 32-bit value baked into the shader.
    gp::UInt32 defaultValue{ 0u };

    /// @brief Memberwise equality comparison.
    /// @param[in] rhs Entry to compare against.
    /// @return True when every field matches.
    [[nodiscard]] bool operator==(const VulkanSpecializationConstantValue& rhs) const noexcept = default;
};

/// @brief Specialization constants of a shader and the bind points that must not be specialized.
struct VulkanSpecializationConstantData
{
    /// @brief Declared specialization constants.
    gp::Vector<VulkanSpecializationConstantValue> values{};

    /// @brief Bind points whose specialization hashes must be skipped.
    gp::Vector<shader::ShaderSpecializationExclusion> exclusions{};

    /// @brief Memberwise equality comparison.
    /// @param[in] rhs Data to compare against.
    /// @return True when both lists match element for element.
    [[nodiscard]] bool operator==(const VulkanSpecializationConstantData& rhs) const noexcept = default;
};

/// @brief Descriptor metadata the shader compiler serializes in front of every SPIR-V blob.
/// @details The SPIR-V module alone cannot be bound: the backend must know, per binding slot, which descriptor type
///          the shader expects, which slots hold packed globals or constant-free uniform buffers, which bindings read
///          subpass inputs, and which samplers are global. Reflecting the SPIR-V at load time would cost a parse per
///          shader and could not recover the engine-level facts (static slots, layout hashes), so the compiler writes
///          this header once and the backend reads it back with a bounds-checked reader.
///
///          Layout facts the descriptor layout builder relies on:
///          - `bindings[i]` is binding slot `i` of the stage's descriptor set.
///          - When `packedGlobalsSize != 0`, binding 0 is the packed globals uniform buffer.
///          - Uniform buffer slots below `numBoundUniformBuffers` hold constants and get a descriptor; the remaining
///            uniform buffer slots are resource-only and are skipped.
struct VulkanShaderHeader
{
    /// @brief Every binding of the shader; the array index is the binding slot.
    gp::Vector<VulkanShaderBindingInfo> bindings{};

    /// @brief Uniform buffer metadata; entry `i` describes `bindings[i]` when it is a uniform buffer.
    gp::Vector<VulkanShaderUniformBufferInfo> uniformBufferInfos{};

    /// @brief Input attachment bindings and the render targets they read.
    gp::Vector<VulkanShaderInputAttachmentInfo> inputAttachmentInfos{};

    /// @brief Sampler bindings that use engine-wide immutable samplers.
    gp::Vector<VulkanShaderGlobalSamplerInfo> globalSamplerInfos{};

    /// @brief Number of uniform buffers that carry constants and need a binding.
    gp::UInt32 numBoundUniformBuffers{ 0u };

    /// @brief Byte size of the packed globals uniform buffer, or zero when the shader has none.
    gp::UInt32 packedGlobalsSize{ 0u };

    /// @brief Input attachments in use; bit `N` is `VulkanShaderAttachmentType` value `N`.
    gp::UInt32 inputAttachmentsMask{ 0u };

    /// @brief Bit mask of the stage input and output locations (mostly relevant for vertex shaders).
    gp::UInt32 inOutMask{ 0u };

    /// @brief Ray tracing payload type identifier.
    gp::UInt32 rayTracingPayloadType{ 0u };

    /// @brief Ray tracing payload size in bytes.
    gp::UInt32 rayTracingPayloadSize{ 0u };

    /// @brief Specialization constants declared by the shader.
    VulkanSpecializationConstantData specializationConstantData{};

    /// @brief 128-bit hash of the source shader code.
    shader::ShaderHash codeHash{};

    /// @brief CRC-32 of the SPIR-V words; part of the entry point name `main_<size>_<crc>`.
    gp::UInt32 spirvCRC{ 0u };

    /// @brief Required subgroup size, or zero when the shader does not request one.
    gp::UInt8 waveSize{ 0u };

    /// @brief Resource usage facts recorded by the compiler.
    VulkanShaderUsageFlags usageFlags{ VulkanShaderUsageFlags::None };

    /// @brief Storage of the any-hit entry point (ray hit groups only).
    VulkanRayHitGroupEntrypoint rayGroupAnyHit{ VulkanRayHitGroupEntrypoint::NotPresent };

    /// @brief Storage of the intersection entry point (ray hit groups only).
    VulkanRayHitGroupEntrypoint rayGroupIntersection{ VulkanRayHitGroupEntrypoint::NotPresent };

    /// @brief Human-readable shader name used for object debug names.
    gp::String debugName{};

    /// @brief Memberwise equality comparison.
    /// @param[in] rhs Header to compare against.
    /// @return True when every serialized field matches.
    [[nodiscard]] bool operator==(const VulkanShaderHeader& rhs) const noexcept = default;
};

}   // namespace gp::rhi::vulkan
