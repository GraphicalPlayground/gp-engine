// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/Resource.hpp"
#include "core/ResourceForward.hpp"
#include "CoreMinimal.hpp"
#include "presentation/PresentationForward.hpp"   // IWYU pragma: keep
#include "RHIBase.hpp"
#include "templates/Function.hpp"
#include "templates/Interval.hpp"

namespace gp::rhi::presentation
{

/// @brief Swapchain and backbuffer presentation surface bound to a native OS window.
/// @details Backend subclasses own the platform swapchain, backbuffer textures, and frame-pacing synchronization
///          primitives. All virtual methods provide safe no-op or null defaults so backends only override the
///          platform capabilities they support.
class GP_RHI_API Viewport : public resource::Resource
{
public:
    /// @brief Constructs a viewport resource tagged with `ResourceType::Viewport`.
    Viewport() noexcept;

    Viewport(const Viewport&) = delete;
    Viewport& operator=(const Viewport&) = delete;
    Viewport(Viewport&&) = delete;
    Viewport& operator=(Viewport&&) = delete;

public:
    /// @brief Returns the backend-native swapchain object handle.
    /// @return Opaque pointer to the native swapchain (such as `IDXGISwapChain*` or `VkSwapchainKHR`), or `nullptr`.
    [[nodiscard]] virtual void* getNativeSwapChain() const noexcept;

    /// @brief Returns the backend-native texture handle for the current swapchain backbuffer.
    /// @return Opaque pointer to the native backbuffer texture, or `nullptr`.
    [[nodiscard]] virtual void* getNativeBackBufferTexture() const noexcept;

    /// @brief Returns the backend-native render target view handle for the current swapchain backbuffer.
    /// @return Opaque pointer to the native backbuffer render target view, or `nullptr`.
    [[nodiscard]] virtual void* getNativeBackBufferRT() const noexcept;

    /// @brief Returns the native OS window handle associated with this viewport.
    /// @param[out] outAddParam Optional output receiving a platform-specific auxiliary handle (such as an X11 display).
    /// @return Opaque pointer to the native window handle, or `nullptr`.
    [[nodiscard]] virtual void* getNativeWindow(void** outAddParam = nullptr) const noexcept;

    /// @brief Attaches or clears a `CustomPresent` hook on this viewport.
    /// @param[in] customPresent Custom presentation hook to bind, or `nullptr` to restore default presentation.
    virtual void setCustomPresent(CustomPresent* customPresent) noexcept;

    /// @brief Creates a platform-native presentation layer covering the specified draw-layer ranges.
    /// @param[in] newNativeLayer Integer identifier for the new native layer.
    /// @param[in] nativeViewHandle Opaque handle to the OS view or compositor surface backing the layer.
    /// @param[in] layerRanges Inclusive draw-layer index intervals routed to this native layer.
    virtual void createNativeLayer(
        gp::Int32 newNativeLayer, void* nativeViewHandle, gp::ConstVectorView<gp::Interval<gp::Int32>> layerRanges
    ) noexcept;

    /// @brief Destroys a previously created platform-native presentation layer.
    /// @param[in] oldNativeLayer Integer identifier of the native layer to remove.
    virtual void deleteNativeLayer(gp::Int32 oldNativeLayer) noexcept;

    /// @brief Selects which native presentation layer receives draws that do not match an explicit range.
    /// @param[in] nativeLayer Integer identifier of the default native layer.
    virtual void setDefaultNativeLayer(gp::Int32 nativeLayer) noexcept;

    /// @brief Sets the sub-rectangle of the swapchain surface cropped for display output.
    /// @param[in] newCropWindow Crop rectangle in viewport pixel coordinates.
    virtual void setWindowCropSize(const resource::IntRect& newCropWindow) noexcept;

    /// @brief Returns the `CustomPresent` hook currently bound to this viewport.
    /// @return Non-owning pointer to the active `CustomPresent`, or `nullptr` when none is bound.
    [[nodiscard]] virtual CustomPresent* getCustomPresent() const noexcept;

    /// @brief Returns an optional SDR backbuffer texture paired with an HDR swapchain backbuffer.
    /// @param[in] backBuffer Primary swapchain backbuffer texture for the current frame.
    /// @return Pointer to the companion SDR backbuffer texture, or `nullptr` if unavailable.
    [[nodiscard]] virtual resource::Texture* getOptionalSDRBackBuffer(resource::Texture* backBuffer) const noexcept;

    /// @brief Blocks the calling thread until the previous frame's GPU completion event finishes.
    /// @details Used by frame pacing to bound CPU-to-GPU queue depth. Default implementation is a no-op.
    virtual void waitForFrameEventCompletion() noexcept;

    /// @brief Signals a GPU completion event for the current frame so a subsequent frame can synchronize against it.
    /// @details Default implementation is a no-op.
    virtual void issueFrameEvent() noexcept;

    /// @brief Registers begin and end callbacks invoked around swapchain presentation for latency measurement.
    /// @param[in] beginCallback Callback invoked right before submitting the present call.
    /// @param[in] endCallback Callback invoked right after the present call returns.
    void setPresentLatencyCallbacks(gp::Function<void()>&& beginCallback, gp::Function<void()>&& endCallback) noexcept;

protected:
    /// @copydoc resource::Resource::~Resource()
    virtual ~Viewport() noexcept override;

protected:
    /// @brief Optional latency marker callback invoked immediately before swapchain present.
    gp::Function<void()> m_presentBeginLatencyCallback{};

    /// @brief Optional latency marker callback invoked immediately after swapchain present.
    gp::Function<void()> m_presentEndLatencyCallback{};
};

}   // namespace gp::rhi::presentation
