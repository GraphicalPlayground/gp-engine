// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "containers/strings/Name.hpp"
#include "core/GPUMask.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "RHINull.hpp"
#include "sync/GPUFence.hpp"

namespace gp::rhi::null
{

/// @brief Concrete immediately-signaled GPU fence allocated by `NullDynamicRHI`.
/// @details Because `NullDynamicRHI` performs no asynchronous GPU work, `poll()` always returns true and `wait()`
///          returns immediately.
class GP_RHI_NULL_API NullGPUFence final : public sync::GPUFence
{
public:
    /// @brief Constructs a null GPU fence with diagnostic label `name`.
    /// @param[in] name Diagnostic fence name.
    explicit NullGPUFence(const gp::Name name) noexcept
        : GPUFence(name)
    {}

public:
    /// @copydoc sync::GPUFence::clear()
    virtual void clear() noexcept override
    {}

    /// @copydoc sync::GPUFence::poll()
    [[nodiscard]] virtual bool poll() const noexcept override
    {
        return true;
    }

    /// @copydoc sync::GPUFence::wait()
    virtual void wait(command::CommandListImmediate& cmdList, const GPUMask gpuMask) const noexcept override
    {
        static_cast<void>(cmdList);
        static_cast<void>(gpuMask);
    }

protected:
    /// @copydoc sync::GPUFence::~GPUFence()
    virtual ~NullGPUFence() noexcept override = default;
};

}   // namespace gp::rhi::null
