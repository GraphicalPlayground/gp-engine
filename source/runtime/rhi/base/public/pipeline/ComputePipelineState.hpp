// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/Resource.hpp"
#include "CoreMinimal.hpp"
#include "memory/MemoryOrder.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "pipeline/PipelineForward.hpp"        // IWYU pragma: keep
#include "RHIBase.hpp"
#include "shaders/ComputeShader.hpp"
#include "shaders/ShaderForward.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::rhi::pipeline
{

/// @brief Lock-free usage counters tracking when and how often a compiled pipeline state is bound.
/// @details Attached to cached pipeline states and compute shaders to drive pipeline cache eviction policies and
///          precache telemetry without taking locks on command recording threads.
struct GP_RHI_API PipelineStateStats
{
    /// @brief Sentinel frame index indicating that the pipeline has never been bound.
    static constexpr gp::UInt64 kInvalidFrame = ~gp::UInt64{ 0u };

    /// @brief Earliest engine frame index on which this pipeline was bound, or `kInvalidFrame` before first use.
    gp::thread::Atomic<gp::UInt64> firstFrameUsed{ kInvalidFrame };

    /// @brief Most recent engine frame index on which this pipeline was bound.
    gp::thread::Atomic<gp::UInt64> lastFrameUsed{ 0u };

    /// @brief Cumulative number of times this pipeline has been bound across all threads.
    gp::thread::Atomic<gp::UInt64> totalBindCount{ 0u };

    /// @brief Records a pipeline bind during `currentFrame` using lock-free atomic updates.
    /// @param[in] currentFrame Monotonic engine frame index.
    void updateStats(gp::UInt64 currentFrame) noexcept;

    /// @brief Null-safe helper updating `stats` when non-null.
    /// @param[in,out] stats Target pipeline stats record, or `nullptr` (ignored).
    /// @param[in] currentFrame Monotonic engine frame index.
    static void updateStats(PipelineStateStats* stats, gp::UInt64 currentFrame) noexcept;

    /// @brief Reports whether this pipeline has been bound at least once.
    /// @return True when `totalBindCount` is non-zero.
    [[nodiscard]] GP_FORCEINLINE_HINT bool hasBeenUsed() const noexcept
    {
        return totalBindCount.load(gp::memory::MemoryOrder::Relaxed) > 0u;
    }
};

/// @brief Compiled compute pipeline state object wrapping a `shader::ComputeShader`.
/// @details Tagged with `ResourceType::ComputePipelineState` so deferred deletion increments
///          `Resource::getNumDeletedPipelineStates()`. Retains a strong `shader::ComputeShaderRef` so the underlying
///          compute shader stays alive until the pipeline state itself finishes deferred destruction.
class GP_RHI_API ComputePipelineState : public resource::Resource
{
public:
    ComputePipelineState() = delete;

    /// @brief Constructs a compute pipeline state retaining `computeShader`.
    /// @param[in] computeShader Non-null compute shader bound to this pipeline state.
    explicit ComputePipelineState(shader::ComputeShader* computeShader) noexcept;

    ComputePipelineState(const ComputePipelineState&) = delete;
    ComputePipelineState& operator=(const ComputePipelineState&) = delete;
    ComputePipelineState(ComputePipelineState&&) = delete;
    ComputePipelineState& operator=(ComputePipelineState&&) = delete;

protected:
    /// @copydoc resource::Resource::~Resource()
    virtual ~ComputePipelineState() noexcept override;

public:
    /// @brief Marks whether backend compilation of this compute pipeline state succeeded.
    /// @details Named separately from `Resource::isValid()` so callers can distinguish lifetime validity from backend
    ///          compilation readiness.
    /// @param[in] isValid True if the compiled pipeline can be bound for dispatch; false if compilation failed.
    GP_FORCEINLINE_HINT void setPipelineValid(const bool isValid) noexcept
    {
        m_isPipelineValid.store(isValid, gp::memory::MemoryOrder::Release);
    }

    /// @brief Reports whether backend compilation of this compute pipeline state succeeded.
    /// @return True when the pipeline state is valid for GPU dispatch; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isPipelineValid() const noexcept
    {
        return m_isPipelineValid.load(gp::memory::MemoryOrder::Acquire);
    }

    /// @brief Records that this compute pipeline state has been bound by at least one command list.
    virtual void markUsed() noexcept;

    /// @brief Reports whether `markUsed()` has been called on this compute pipeline state.
    /// @return True if this pipeline has been bound; false otherwise.
    [[nodiscard]] virtual bool isUsed() const noexcept;

    /// @brief Returns the compute shader bound to this pipeline state.
    /// @return Non-owning pointer to `shader::ComputeShader`.
    [[nodiscard]] GP_FORCEINLINE_HINT shader::ComputeShader* getComputeShader() const noexcept
    {
        return m_computeShader.get();
    }

    /// @brief Returns the retained smart pointer handle to the bound compute shader.
    /// @return Immutable reference to the internal `shader::ComputeShaderRef`.
    [[nodiscard]] GP_FORCEINLINE_HINT const shader::ComputeShaderRef& getComputeShaderRef() const noexcept
    {
        return m_computeShader;
    }

protected:
    /// @brief Strong reference keeping the compute shader alive for the lifetime of this pipeline state.
    shader::ComputeShaderRef m_computeShader{};

private:
    /// @brief Indicates whether backend compilation succeeded for this pipeline state.
    gp::thread::Atomic<bool> m_isPipelineValid{ true };

    /// @brief Tracks whether this pipeline state has been bound to a command list.
    gp::thread::Atomic<bool> m_isUsed{ false };
};

}   // namespace gp::rhi::pipeline
