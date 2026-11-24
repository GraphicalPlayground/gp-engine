// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "resources/Buffer.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "RHINull.hpp"

namespace gp::rhi::null
{

/// @brief Concrete no-op GPU buffer resource allocated by `NullDynamicRHI`.
class GP_RHI_NULL_API NullBuffer final : public resource::Buffer
{
public:
    /// @brief Constructs a null GPU buffer retaining `createDesc`.
    /// @param[in] createDesc Buffer creation descriptor.
    explicit NullBuffer(const resource::BufferCreateDesc& createDesc) noexcept
        : Buffer(createDesc)
    {}

protected:
    /// @copydoc resource::Buffer::~Buffer()
    virtual ~NullBuffer() noexcept override = default;
};

}   // namespace gp::rhi::null
