// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/ResourceForward.hpp"
#include "CoreMinimal.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "pipeline/PipelineForward.hpp"        // IWYU pragma: keep
#include "resources/UniformBufferLayout.hpp"
#include "resources/UniformBufferLayoutDesc.hpp"
#include "shaders/ShaderEnums.hpp"
#include <cstddef>

namespace gp::rhi::vulkan::tests
{

/// @brief Shader parameter struct with constants followed by one sampler member.
struct alignas(16) TestUniformBufferContents final
{
    gp::Float32 constants[4]{};
    pipeline::SamplerState* sampler{ nullptr };
    gp::UInt64 padding{ 0u };
};

/// @brief Byte offset of the sampler member in `TestUniformBufferContents`.
inline constexpr gp::UInt16 kTestSamplerOffset = static_cast<gp::UInt16>(offsetof(TestUniformBufferContents, sampler));

/// @brief Builds a layout over `TestUniformBufferContents`.
/// @param[in] withConstants True to give the layout a constant buffer.
/// @param[in] withSampler True to declare the sampler member as a resource.
/// @return New layout.
[[nodiscard]] inline resource::UniformBufferLayoutRef
    createTestUniformBufferLayout(const bool withConstants, const bool withSampler) noexcept
{
    resource::UniformBufferLayoutDesc desc(
        "TestUniformBuffer", withConstants ? static_cast<gp::UInt32>(sizeof(TestUniformBufferContents)) : 0u
    );
    if (withSampler)
    {
        desc.resources.pushBack(
            resource::UniformBufferResource{ kTestSamplerOffset, shader::UniformBufferBaseType::Sampler }
        );
    }
    desc.computeHash();
    return resource::UniformBufferLayoutRef(new resource::UniformBufferLayout(desc));
}

}   // namespace gp::rhi::vulkan::tests
