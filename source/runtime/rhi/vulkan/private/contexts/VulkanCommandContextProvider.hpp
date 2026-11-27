// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"           // IWYU pragma: keep
#include "contexts/VulkanContextForward.hpp"     // IWYU pragma: keep
#include "core/ResourceForward.hpp"              // IWYU pragma: keep
#include "CoreMinimal.hpp"                       // IWYU pragma: keep
#include "resources/IVulkanCommandContextProvider.hpp"
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "sync/IVulkanSyncContextProvider.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Resolves RHI command lists to the Vulkan contexts recording them.
/// @details Resources, queries and fences are written against narrow provider interfaces so they never see the
///          context types. This class is the production implementation of both seams: the lowest level context of
///          every command list is a `VulkanCommandListContext`, and uploads go to one upload context acquired lazily
///          from the pool and handed back by `finalizeUploads()`.
/// @note Not thread-safe: the upload context is shared by every caller of `getUploadRecorder()`.
class GP_RHI_VULKAN_API VulkanCommandContextProvider final
    : public IVulkanCommandContextProvider
    , public IVulkanSyncContextProvider
{
public:
    /// @brief Creates a provider drawing upload contexts from `uploadPool`.
    /// @param[in] uploadPool Upload context pool; null disables `getUploadRecorder()`.
    explicit VulkanCommandContextProvider(VulkanUploadContextPool* uploadPool) noexcept;

    /// @brief Returns any acquired upload context to the pool.
    virtual ~VulkanCommandContextProvider() noexcept override;

    VulkanCommandContextProvider(const VulkanCommandContextProvider&) = delete;
    VulkanCommandContextProvider& operator=(const VulkanCommandContextProvider&) = delete;
    VulkanCommandContextProvider(VulkanCommandContextProvider&&) = delete;
    VulkanCommandContextProvider& operator=(VulkanCommandContextProvider&&) = delete;

public:
    /// @brief Returns the Vulkan context recording `cmdList`.
    /// @param[in] cmdList Command list whose lowest level context is a `VulkanCommandListContext`.
    /// @return Vulkan context.
    [[nodiscard]] static VulkanCommandListContext& getVulkanContext(command::CommandListBase& cmdList) noexcept;

    /// @copydoc IVulkanCommandContextProvider::getCommandListRecorder()
    [[nodiscard]] virtual IVulkanTransferCommandRecorder*
        getCommandListRecorder(command::CommandListBase& cmdList) noexcept override;

    /// @copydoc IVulkanCommandContextProvider::getUploadRecorder()
    /// @note Records into the shared upload context; `cmdList` is not used.
    [[nodiscard]] virtual IVulkanTransferCommandRecorder*
        getUploadRecorder(command::CommandListBase& cmdList) noexcept override;

    /// @copydoc IVulkanCommandContextProvider::getContextRecorder()
    [[nodiscard]] virtual IVulkanTransferCommandRecorder*
        getContextRecorder(command::IComputeContext* context) noexcept override;

    /// @copydoc IVulkanCommandContextProvider::getTrackedPipelines()
    /// @note `resource` must be a viewable resource (buffer or texture), the only kind callers track.
    [[nodiscard]] virtual sync::PipelineStage
        getTrackedPipelines(command::IComputeContext* context, const resource::Resource* resource) noexcept override;

    /// @copydoc IVulkanCommandContextProvider::submitAndBlockUntilGPUIdle()
    /// @note Uploads are flushed first, so the graphics work sees them.
    virtual void submitAndBlockUntilGPUIdle(command::CommandListImmediate& cmdList) noexcept override;

    /// @copydoc IVulkanCommandContextProvider::needsExtraTransitions()
    /// @note Always true: Vulkan images need explicit layout transitions around backend-side copies.
    [[nodiscard]] virtual bool needsExtraTransitions(const command::CommandListBase& cmdList) const noexcept override;

    /// @copydoc IVulkanSyncContextProvider::addPendingSyncPoint()
    virtual void
        addPendingSyncPoint(command::CommandListBase& cmdList, const VulkanSyncPointRef& syncPoint) noexcept override;

    /// @copydoc IVulkanSyncContextProvider::addSubmissionEvent()
    virtual void
        addSubmissionEvent(command::CommandListBase& cmdList, const VulkanSyncPointRef& syncPoint) noexcept override;

    /// @copydoc IVulkanSyncContextProvider::signalSyncPoint()
    virtual void
        signalSyncPoint(command::CommandListBase& cmdList, const VulkanSyncPointRef& syncPoint) noexcept override;

    /// @copydoc IVulkanSyncContextProvider::getActiveCommandBuffer()
    [[nodiscard]] virtual VkCommandBuffer getActiveCommandBuffer(command::CommandListBase& cmdList) noexcept override;

    /// @brief Finalizes the shared upload context into `outCommandList` and returns it to the pool.
    /// @param[out] outCommandList Receives the upload payloads; untouched when nothing was recorded.
    /// @return True when an upload context was active.
    bool finalizeUploads(VulkanPlatformCommandList& outCommandList) noexcept;

    /// @brief Returns the upload context currently acquired.
    /// @return Upload context, or `nullptr` when none is active.
    [[nodiscard]] VulkanUploadContext* getActiveUploadContext() const noexcept
    {
        return m_activeUploadContext;
    }

private:
    /// @brief Pool upload contexts come from; null when uploads are disabled.
    VulkanUploadContextPool* m_uploadPool{ nullptr };

    /// @brief Upload context collecting uploads until `finalizeUploads()`.
    VulkanUploadContext* m_activeUploadContext{ nullptr };
};

}   // namespace gp::rhi::vulkan
