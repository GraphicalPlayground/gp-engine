// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "containers/strings/Name.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "core/Resource.hpp"
#include "core/ResourceEnums.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIBase.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/TransitionDesc.hpp"
#include "validation/ValidationForward.hpp"

namespace gp::rhi::resource
{

/// @brief Shared base class for GPU buffers, textures, and other RHI resources that support views and state tracking.
/// @details Extends `Resource` with a debug `gp::Name` and a `sync::TrackedAccess` state pair updated by command
///          execution contexts during barrier replay. Ownership transfer helpers (`takeOwnership` and
///          `releaseOwnership`) carry tracked barrier state when a concrete resource replaces or aliases its
///          underlying GPU allocation.
class GP_RHI_API ViewableResource : public Resource
{
    friend class ::gp::rhi::command::CommandListBase;
    friend class ::gp::rhi::command::ComputeCommandList;
    friend class ::gp::rhi::command::IComputeContext;

public:
    ViewableResource(const ViewableResource&) = delete;
    ViewableResource& operator=(const ViewableResource&) = delete;
    ViewableResource(ViewableResource&&) = delete;
    ViewableResource& operator=(ViewableResource&&) = delete;

public:
    /// @brief Reads the last recorded GPU access mask without command-list synchronization.
    /// @note Callers outside the executing RHI context may observe stale state while transitions are still in flight.
    /// @return Tracked `sync::ResourceAccess` mask.
    [[nodiscard]] GP_FORCEINLINE_HINT sync::ResourceAccess getTrackedAccessUnsafe() const noexcept
    {
        return m_trackedAccess.access;
    }

    /// @brief Reads the last recorded pipeline stage mask without command-list synchronization.
    /// @details Resources with `PipelineStage::None` recorded default to `PipelineStage::Graphics`.
    /// @note Callers outside the executing RHI context may observe stale state while transitions are still in flight.
    /// @return Active `sync::PipelineStage` mask.
    [[nodiscard]] GP_FORCEINLINE_HINT sync::PipelineStage getTrackedPipelinesUnsafe() const noexcept
    {
        return (m_trackedAccess.pipelines != sync::PipelineStage::None) ? m_trackedAccess.pipelines
                                                                        : sync::PipelineStage::Graphics;
    }

    /// @brief Returns the debug name assigned to this resource.
    /// @return Interned `gp::Name`, or `"None"` when unnamed or when constructed with `GP_RHI_ENABLE_RESOURCE_INFO`
    ///         disabled and not later updated via `setName()`.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::Name getName() const noexcept
    {
        return m_name;
    }

#if GP_ENABLE_RHI_VALIDATION
    /// @brief Returns the barrier/subresource validation tracker attached to this resource (`nullptr` if none).
    /// @return Pointer to `validation::Resource`, or `nullptr`.
    [[nodiscard]] virtual validation::Resource* getValidationTrackerResource() noexcept
    {
        return nullptr;
    }
#endif

protected:
    /// @brief Constructs a viewable resource with its runtime type tag, initial access state, and debug metadata.
    /// @details Interns `debugName` and records `ownerName` only when `GP_RHI_ENABLE_RESOURCE_INFO` is enabled.
    /// @param[in] resourceType Concrete runtime resource type tag.
    /// @param[in] initialAccess Initial GPU resource access state.
    /// @param[in] debugName Optional debug label (`nullptr` or empty leaves `m_name` as `"None"`).
    /// @param[in] ownerName Optional diagnostic owner name forwarded to `Resource::setOwnerName()`.
    ViewableResource(
        ResourceType resourceType,
        sync::ResourceAccess initialAccess,
        gp::StringView debugName = gp::StringView{},
        gp::Name ownerName = gp::Name{}
    ) noexcept;

    /// @copydoc Resource::~Resource()
    virtual ~ViewableResource() noexcept override;

    /// @brief Updates tracked access and pipeline state from an executing command context.
    /// @details Virtual so indirection wrappers such as `TextureReference` can forward updates to their target.
    /// @param[in] trackedAccess New access mask and pipeline stage state.
    virtual void setTrackedAccessFromContext(sync::TrackedAccess trackedAccess) noexcept;

    /// @brief Copies tracked barrier state from `other` when adopting its backing GPU allocation.
    /// @param[in,out] other Source viewable resource whose GPU allocation is being adopted.
    void takeOwnership(ViewableResource& other) noexcept;

    /// @brief Resets tracked barrier state to `ResourceAccess::Unknown` when releasing backing GPU ownership.
    void releaseOwnership() noexcept;

    /// @brief Replaces the debug name stored on this resource.
    /// @param[in] inName New debug name.
    GP_FORCEINLINE_HINT void setName(const gp::Name inName) noexcept
    {
        m_name = inName;
    }

protected:
    /// @brief Debug identifier for profiling, validation messages, and resource dumps.
    gp::Name m_name{};

private:
    /// @brief Returns the tracked access mask for trusted command contexts.
    /// @return Current `sync::ResourceAccess`.
    [[nodiscard]] GP_FORCEINLINE_HINT sync::ResourceAccess getTrackedAccessForContext() const noexcept
    {
        return m_trackedAccess.access;
    }

    /// @brief Returns the tracked pipeline mask for trusted command contexts, defaulting `None` to `Graphics`.
    /// @return Active `sync::PipelineStage` mask.
    [[nodiscard]] GP_FORCEINLINE_HINT sync::PipelineStage getTrackedPipelinesForContext() const noexcept
    {
        return (m_trackedAccess.pipelines != sync::PipelineStage::None) ? m_trackedAccess.pipelines
                                                                        : sync::PipelineStage::Graphics;
    }

private:
    sync::TrackedAccess m_trackedAccess{};
};

}   // namespace gp::rhi::resource
