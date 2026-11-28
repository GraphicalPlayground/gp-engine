// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"    // IWYU pragma: keep
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                 // IWYU pragma: keep
#include "resources/UniformBufferEnums.hpp"
#include "shaders/AmplificationShader.hpp"
#include "shaders/ComputeShader.hpp"
#include "shaders/GeometryShader.hpp"
#include "shaders/MeshShader.hpp"
#include "shaders/PixelShader.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/VertexShader.hpp"
#include "shaders/VulkanShader.hpp"
#include "shaders/VulkanShaderCode.hpp"
#include "shaders/VulkanShaderForward.hpp"   // IWYU pragma: keep
#include "shaders/VulkanShaderHeaderEnums.hpp"
#include "templates/Enums.hpp"
#include "VulkanForward.hpp"   // IWYU pragma: keep
#include <utility>

namespace gp::rhi::vulkan
{

/// @brief Joins an RHI shader stage type with the Vulkan backend shader state.
/// @details The RHI half (`InRHIShaderType`) carries the reference count, the resource table and the static slots the
///          renderer reads. The backend half (`VulkanShader`) carries the descriptor header, the SPIR-V and the module
///          cache. Both are filled from one decoded `VulkanShaderCode` in the constructor.
/// @tparam InRHIShaderType RHI shader stage class, for example `shader::VertexShader`.
/// @tparam InFrequency Stage of `InRHIShaderType`.
template <typename InRHIShaderType, shader::ShaderFrequency InFrequency>
class VulkanBaseShader final
    : public InRHIShaderType
    , public VulkanShader
{
public:
    /// @brief Stage of this shader type.
    static constexpr shader::ShaderFrequency kStaticFrequency = InFrequency;

    static_assert(InRHIShaderType::kStaticFrequency == InFrequency, "Shader frequency does not match the RHI type.");

    /// @brief Builds the shader from decoded code.
    /// @details The resource table is moved into the RHI half. Static slots come straight from the header: the
    ///          compiler resolved them offline, so no uniform buffer layout registry is consulted.
    /// @param[in] device Logical device that creates the modules.
    /// @param[in] deferredDeletionQueue Queue used to release module handles; may be null.
    /// @param[in] code Decoded shader blob; consumed.
    /// @param[in] shaderKey Deduplication key, or zero.
    /// @param[in] factory Factory that registered the shader; may be null.
    VulkanBaseShader(
        VulkanDevice& device,
        VulkanDeferredDeletionQueue* const deferredDeletionQueue,
        VulkanShaderCode&& code,
        const gp::UInt64 shaderKey,
        VulkanShaderFactory* const factory
    ) noexcept
        : InRHIShaderType()
        , VulkanShader(
              device,
              deferredDeletionQueue,
              InFrequency,
              std::move(code.header),
              std::move(code.spirv),
              shaderKey,
              factory
          )
    {
        const VulkanShaderHeader& header = getCodeHeader();

        gp::Vector<resource::UniformBufferStaticSlot> staticSlots{};
        staticSlots.reserve(header.uniformBufferInfos.size());
        for (const VulkanShaderUniformBufferInfo& info: header.uniformBufferInfos)
        {
            staticSlots.pushBack(info.staticSlot);
        }

        this->setShaderResourceTable(std::move(code.resourceTable));
        this->setStaticSlots(std::move(staticSlots));
        this->setSpecializationConstants(!header.specializationConstantData.values.isEmpty());
        this->setSpecializationExclusions(header.specializationConstantData.exclusions);
        this->setShaderBundleUsage(
            (header.usageFlags & VulkanShaderUsageFlags::ShaderBundle) != VulkanShaderUsageFlags::None
        );
        if constexpr (InFrequency == shader::ShaderFrequency::Compute)
        {
            this->setNoDerivativeOps(
                (header.usageFlags & VulkanShaderUsageFlags::NoDerivativeOps) != VulkanShaderUsageFlags::None
            );
        }
        if (!header.debugName.isEmpty())
        {
            this->setDebugShaderName(header.debugName.view());
        }
    }

protected:
    /// @brief Destroys both halves; the backend half unregisters from its factory first.
    virtual ~VulkanBaseShader() noexcept override = default;
};

/// @brief Vulkan vertex shader.
using VulkanVertexShader = VulkanBaseShader<shader::VertexShader, shader::ShaderFrequency::Vertex>;

/// @brief Vulkan pixel (fragment) shader.
using VulkanPixelShader = VulkanBaseShader<shader::PixelShader, shader::ShaderFrequency::Pixel>;

/// @brief Vulkan geometry shader.
using VulkanGeometryShader = VulkanBaseShader<shader::GeometryShader, shader::ShaderFrequency::Geometry>;

/// @brief Vulkan compute shader.
using VulkanComputeShader = VulkanBaseShader<shader::ComputeShader, shader::ShaderFrequency::Compute>;

/// @brief Vulkan mesh shader.
using VulkanMeshShader = VulkanBaseShader<shader::MeshShader, shader::ShaderFrequency::Mesh>;

/// @brief Vulkan task (amplification) shader.
using VulkanTaskShader = VulkanBaseShader<shader::AmplificationShader, shader::ShaderFrequency::Amplification>;

}   // namespace gp::rhi::vulkan
