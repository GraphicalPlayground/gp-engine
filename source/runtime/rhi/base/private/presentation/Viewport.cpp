// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "presentation/Viewport.hpp"
#include "core/ResourceEnums.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "presentation/CustomPresent.hpp"
#include <utility>

namespace gp::rhi::presentation
{

CustomPresent::CustomPresent() noexcept
    : resource::Resource(resource::ResourceType::CustomPresent)
{}

CustomPresent::~CustomPresent() noexcept = default;

bool CustomPresent::needsAdvanceBackbuffer() noexcept
{
    return false;
}

bool CustomPresent::present(
    Viewport* const viewport, command::ICommandContext& cmdContext, gp::Int32& inOutSyncInterval
) noexcept
{
    static_cast<void>(viewport);
    static_cast<void>(cmdContext);
    static_cast<void>(inOutSyncInterval);
    GP_CHECKF(false, "CustomPresent::present() must be overridden by a concrete presentation subclass.");
    return true;
}

void CustomPresent::postPresent() noexcept
{}

void CustomPresent::onAcquireThreadOwnership() noexcept
{}

void CustomPresent::onReleaseThreadOwnership() noexcept
{}

Viewport::Viewport() noexcept
    : resource::Resource(resource::ResourceType::Viewport)
{}

Viewport::~Viewport() noexcept = default;

void* Viewport::getNativeSwapChain() const noexcept
{
    return nullptr;
}

void* Viewport::getNativeBackBufferTexture() const noexcept
{
    return nullptr;
}

void* Viewport::getNativeBackBufferRT() const noexcept
{
    return nullptr;
}

void* Viewport::getNativeWindow(void** const outAddParam) const noexcept
{
    static_cast<void>(outAddParam);
    return nullptr;
}

void Viewport::setCustomPresent(CustomPresent* const customPresent) noexcept
{
    static_cast<void>(customPresent);
}

void Viewport::createNativeLayer(
    const gp::Int32 newNativeLayer,
    void* const nativeViewHandle,
    const gp::ConstVectorView<gp::Interval<gp::Int32>> layerRanges
) noexcept
{
    static_cast<void>(newNativeLayer);
    static_cast<void>(nativeViewHandle);
    static_cast<void>(layerRanges);
}

void Viewport::deleteNativeLayer(const gp::Int32 oldNativeLayer) noexcept
{
    static_cast<void>(oldNativeLayer);
}

void Viewport::setDefaultNativeLayer(const gp::Int32 nativeLayer) noexcept
{
    static_cast<void>(nativeLayer);
}

void Viewport::setWindowCropSize(const resource::IntRect& newCropWindow) noexcept
{
    static_cast<void>(newCropWindow);
}

CustomPresent* Viewport::getCustomPresent() const noexcept
{
    return nullptr;
}

resource::Texture* Viewport::getOptionalSDRBackBuffer(resource::Texture* const backBuffer) const noexcept
{
    static_cast<void>(backBuffer);
    return nullptr;
}

void Viewport::waitForFrameEventCompletion() noexcept
{}

void Viewport::issueFrameEvent() noexcept
{}

void Viewport::setPresentLatencyCallbacks(
    gp::Function<void()>&& beginCallback, gp::Function<void()>&& endCallback
) noexcept
{
    m_presentBeginLatencyCallback = std::move(beginCallback);
    m_presentEndLatencyCallback = std::move(endCallback);
}

}   // namespace gp::rhi::presentation
