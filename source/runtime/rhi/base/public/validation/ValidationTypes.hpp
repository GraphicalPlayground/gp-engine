// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "core/ResourceForward.hpp"
#include "CoreMinimal.hpp"
#include "RHIBase.hpp"
#include "RHIDefinitions.hpp"
#include "shaders/ShaderEnums.hpp"
#include "sync/SyncEnums.hpp"
#include "validation/ValidationEnums.hpp"     // IWYU pragma: keep
#include "validation/ValidationForward.hpp"   // IWYU pragma: keep
#include "views/ViewForward.hpp"

namespace gp::rhi::validation
{

/// @brief Callback signature invoked whenever the RHI validation layer detects an API contract violation.
using ValidationFailureFn = void (*)(gp::StringView message, void* userData) noexcept;

/// @brief Global toggle enabling RHI validation checks when a `ValidationRHI` wrapper is active.
extern GP_RHI_API bool g_rhiValidationEnabled;

/// @brief Enables extra validation on source buffer copy states (`CopySrc`).
extern GP_RHI_API bool g_rhiValidateBufferSourceCopy;

/// @brief Controls whether `reportValidationFailure()` triggers a debugger break (`false` by default for test safety).
extern GP_RHI_API bool g_rhiValidationBreakOnError;

/// @brief Controls whether identical validation failure strings are deduplicated across frames (`false` counts every
///        occurrence).
extern GP_RHI_API bool g_rhiValidationDeduplicateErrors;

/// @brief Installs or clears an observer callback invoked on every RHI validation failure.
/// @param[in] callback Callback function pointer (`nullptr` to clear).
/// @param[in] userData Opaque context pointer forwarded to `callback`.
GP_RHI_API void setValidationFailureCallback(ValidationFailureFn callback, void* userData = nullptr) noexcept;

/// @brief Reports an RHI validation error, incrementing the failure count, invoking any registered callback, logging
///        to `LogRHI`, and breaking into the debugger only if `g_rhiValidationBreakOnError` is true.
/// @param[in] message Human-readable diagnostic message describing the contract violation.
GP_RHI_API void reportValidationFailure(gp::StringView message) noexcept;

/// @brief Returns the total number of RHI validation failures reported since startup or `clearValidationFailures()`.
/// @return Validation failure count.
[[nodiscard]] GP_RHI_API gp::UInt32 getValidationFailureCount() noexcept;

/// @brief Returns a copy of the most recently reported validation failure message (`""` if none).
/// @return Most recent failure string.
[[nodiscard]] GP_RHI_API gp::String getLastValidationFailureMessage() noexcept;

/// @brief Resets the validation failure counter, last message, and deduplication set.
GP_RHI_API void clearValidationFailures() noexcept;

/// @brief Coordinates of an individual `(mipIndex, arraySlice, planeIndex)` subresource or whole-resource sentinel.
struct SubresourceIndex
{
    static constexpr gp::Int32 kWholeResource = -1;

    gp::Int32 mipIndex{ kWholeResource };
    gp::Int32 arraySlice{ kWholeResource };
    gp::Int32 planeIndex{ kWholeResource };

    constexpr SubresourceIndex() noexcept = default;

    constexpr SubresourceIndex(
        const gp::Int32 inMipIndex, const gp::Int32 inArraySlice, const gp::Int32 inPlaneIndex
    ) noexcept
        : mipIndex(inMipIndex)
        , arraySlice(inArraySlice)
        , planeIndex(inPlaneIndex)
    {}

    [[nodiscard]] constexpr bool isWholeResource() const noexcept
    {
        return (mipIndex == kWholeResource) && (arraySlice == kWholeResource) && (planeIndex == kWholeResource);
    }

    [[nodiscard]] constexpr bool operator==(const SubresourceIndex& rhs) const noexcept = default;
};

/// @brief Pairs a `sync::ResourceAccess` mask with the `sync::PipelineStage` mask holding that state.
struct State
{
    sync::ResourceAccess access{ sync::ResourceAccess::Unknown };
    sync::PipelineStage pipelines{ sync::PipelineStage::None };

    constexpr State() noexcept = default;

    constexpr State(const sync::ResourceAccess inAccess, const sync::PipelineStage inPipelines) noexcept
        : access(inAccess)
        , pipelines(inPipelines)
    {}

    [[nodiscard]] constexpr bool operator==(const State& rhs) const noexcept = default;

    [[nodiscard]] GP_RHI_API gp::String toString() const noexcept;
};

/// @brief Contiguous 3D subresource slice `[mipIndex .. +numMips) x [arraySlice .. +numArraySlices) x [planeIndex ..
///        +numPlanes)`.
struct SubresourceRange
{
    gp::UInt16 mipIndex{ 0u };
    gp::UInt16 numMips{ 0u };
    gp::UInt16 arraySlice{ 0u };
    gp::UInt16 numArraySlices{ 0u };
    gp::UInt8 planeIndex{ 0u };
    gp::UInt8 numPlanes{ 0u };

    constexpr SubresourceRange() noexcept = default;

    constexpr SubresourceRange(
        const gp::UInt16 inMipIndex,
        const gp::UInt16 inNumMips,
        const gp::UInt16 inArraySlice,
        const gp::UInt16 inNumArraySlices,
        const gp::UInt8 inPlaneIndex,
        const gp::UInt8 inNumPlanes
    ) noexcept
        : mipIndex(inMipIndex)
        , numMips(inNumMips)
        , arraySlice(inArraySlice)
        , numArraySlices(inNumArraySlices)
        , planeIndex(inPlaneIndex)
        , numPlanes(inNumPlanes)
    {}

    [[nodiscard]] constexpr bool operator==(const SubresourceRange& rhs) const noexcept = default;
};

/// @brief Identifies a `validation::Resource` and the `SubresourceRange` affected by a barrier or access assertion.
struct ResourceIdentity
{
    Resource* resource{ nullptr };
    SubresourceRange subresourceRange{};

    constexpr ResourceIdentity() noexcept = default;

    [[nodiscard]] constexpr bool operator==(const ResourceIdentity& rhs) const noexcept = default;
};

/// @brief Extends `ResourceIdentity` with view element stride metadata for buffer SRVs/UAVs.
struct GP_RHI_API ViewIdentity : public ResourceIdentity
{
    gp::UInt32 stride{ 0u };

    constexpr ViewIdentity() noexcept = default;

    /// @brief Resolves the tracked `Resource*` and exact `SubresourceRange` covered by `viewDesc` on `targetResource`.
    /// @param[in] targetResource Target buffer or texture.
    /// @param[in] viewDesc View creation descriptor.
    ViewIdentity(resource::ViewableResource* targetResource, const view::ViewDesc& viewDesc) noexcept;

    [[nodiscard]] constexpr bool operator==(const ViewIdentity& rhs) const noexcept = default;
};

/// @brief Heap-allocated synchronization state tracking cross-pipeline split-transition signals and waits.
struct Fence
{
    sync::PipelineStage signalingPipelines{ sync::PipelineStage::None };
    sync::PipelineStage signaledPipelines{ sync::PipelineStage::None };
    sync::PipelineStage waitingPipelines{ sync::PipelineStage::None };
    sync::PipelineStage completedPipelines{ sync::PipelineStage::None };
};

/// @brief Tracks global static uniform buffer bindings on a validation context and verifies slot exclusivity.
struct GP_RHI_API StaticUniformBuffers
{
    resource::UniformBuffer* bindings[kMaxUniformBufferStaticSlots]{};
    bool inRenderPass{ false };

    void reset() noexcept;

    /// @brief Validates that `uniformBuffer`'s layout does not declare a valid global static slot when bound directly
    ///        to a shader parameter slot.
    /// @param[in] uniformBuffer Uniform buffer bound via `setShaderParameters`.
    void validateShaderUniformBuffer(const resource::UniformBuffer* uniformBuffer) const noexcept;
};

/// @brief Tracks uniform buffers bound to slots `[0, kMaxBoundSlots)` of a single programmable shader stage.
struct GP_RHI_API StageBoundUniformBuffers
{
    static constexpr gp::UInt32 kMaxBoundSlots = 32u;

    resource::UniformBuffer* buffers[kMaxBoundSlots]{};
    gp::UInt32 boundMask{ 0u };

    void reset() noexcept;
    void bind(gp::UInt32 slotIndex, resource::UniformBuffer* buffer) noexcept;
};

/// @brief Aggregates bound uniform buffers across every shader stage on a validation context.
struct GP_RHI_API BoundUniformBuffers
{
    StageBoundUniformBuffers stages[shader::kShaderFrequencyCount]{};

    void resetAll() noexcept;
    void resetGraphicsStages() noexcept;
};

}   // namespace gp::rhi::validation
