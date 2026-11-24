// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "resources/UniformBuffer.hpp"
#include "resources/UniformBufferLayout.hpp"
#include "RHINull.hpp"

namespace gp::rhi::null
{

/// @brief Concrete no-op GPU uniform buffer resource allocated by `NullDynamicRHI`.
class GP_RHI_NULL_API NullUniformBuffer final : public resource::UniformBuffer
{
public:
    /// @brief Constructs a null uniform buffer bound to `layout`.
    /// @param[in] layout Immutable uniform buffer layout.
    explicit NullUniformBuffer(const resource::UniformBufferLayout* const layout) noexcept
        : UniformBuffer(layout)
    {}

protected:
    /// @copydoc resource::UniformBuffer::~UniformBuffer()
    virtual ~NullUniformBuffer() noexcept override = default;
};

}   // namespace gp::rhi::null
