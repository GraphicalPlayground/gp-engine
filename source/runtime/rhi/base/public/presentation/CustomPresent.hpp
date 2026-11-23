// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "core/Resource.hpp"
#include "CoreMinimal.hpp"
#include "presentation/PresentationForward.hpp"   // IWYU pragma: keep
#include "RHIBase.hpp"

namespace gp::rhi::presentation
{

/// @brief Extension hook that lets external runtimes override swapchain presentation on a `Viewport`.
/// @details Backends invoke this hook around swapchain resize, present, and thread-ownership transitions so
///          subsystems such as XR compositors or video capture bridges can intercept frame submission without
///          modifying backend swapchain classes directly.
class GP_RHI_API CustomPresent : public resource::Resource
{
public:
    /// @brief Constructs a custom present hook tagged with `ResourceType::CustomPresent`.
    CustomPresent() noexcept;

    CustomPresent(const CustomPresent&) = delete;
    CustomPresent& operator=(const CustomPresent&) = delete;
    CustomPresent(CustomPresent&&) = delete;
    CustomPresent& operator=(CustomPresent&&) = delete;

public:
    /// @brief Notifies the hook that the owning viewport is about to resize or recreate its backbuffer.
    /// @note Implementations must release any cached references to the current backbuffer before returning.
    virtual void onBackBufferResize() noexcept = 0;

    /// @brief Reports whether the RHI backend should still invoke the native swapchain present call.
    /// @return True to execute the backend's native present after `present()`; false if this hook handles presenting.
    [[nodiscard]] virtual bool needsNativePresent() noexcept = 0;

    /// @brief Reports whether the backend should advance its swapchain backbuffer index when skipping native present.
    /// @return True to advance the backbuffer index; false by default.
    [[nodiscard]] virtual bool needsAdvanceBackbuffer() noexcept;

    /// @brief Executes custom presentation logic for the current frame.
    /// @details Called from the RHI thread during viewport presentation before the optional native present. The base
    ///          implementation asserts because any hook registered on a viewport must override this method.
    /// @param[in] viewport Viewport being presented.
    /// @param[in,out] cmdContext Active graphics command context for submitting custom present work.
    /// @param[in,out] inOutSyncInterval Requested vertical sync interval; the hook may adjust it before native present.
    /// @return True if the backend should proceed with native present when `needsNativePresent()` is also true; false
    ///         to suppress native present for this frame.
    [[nodiscard]] virtual bool
        present(Viewport* viewport, command::ICommandContext& cmdContext, gp::Int32& inOutSyncInterval) noexcept;

    /// @brief Called immediately after the swapchain present call completes.
    /// @details Runs on the RHI thread even when native present was skipped. Default implementation is a no-op.
    virtual void postPresent() noexcept;

    /// @brief Called when the RHI thread acquires graphics API thread ownership (for example, an OpenGL context).
    /// @details Default implementation is a no-op.
    virtual void onAcquireThreadOwnership() noexcept;

    /// @brief Called when the RHI thread releases graphics API thread ownership.
    /// @details Default implementation is a no-op.
    virtual void onReleaseThreadOwnership() noexcept;

protected:
    /// @copydoc resource::Resource::~Resource()
    virtual ~CustomPresent() noexcept override;
};

}   // namespace gp::rhi::presentation
