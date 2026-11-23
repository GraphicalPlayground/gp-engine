// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "sync/Transition.hpp"
#include "memory/GlobalMemory.hpp"
#include "memory/Memory.hpp"
#include "memory/MemoryBase.hpp"
#include "sync/TransitionDesc.hpp"
#include <new>   // IWYU pragma: keep

namespace gp::rhi::sync
{

namespace
{

TransitionBackendHooks g_transitionBackendHooks{};

[[nodiscard]] constexpr bool isPowerOfTwo(const gp::USize value) noexcept
{
    return (value != 0u) && ((value & (value - 1u)) == 0u);
}

}   // namespace

void Transition::cleanup() const noexcept
{
    Transition* const mutableTransition = const_cast<Transition*>(this);

    if (g_transitionBackendHooks.onRelease != nullptr)
    {
        g_transitionBackendHooks.onRelease(mutableTransition);
    }

    mutableTransition->~Transition();

    gp::memory::Malloc* const allocator = gp::memory::getGlobalMalloc();
    GP_CHECKF(allocator != nullptr, "Global engine allocator must be valid when freeing an RHI Transition.");
    allocator->deallocate(mutableTransition);
}

gp::USize Transition::getPrivateDataSizeInBytes() noexcept
{
    return g_transitionBackendHooks.privateDataSizeInBytes;
}

gp::USize Transition::getPrivateDataAlignInBytes() noexcept
{
    return g_transitionBackendHooks.privateDataAlignInBytes;
}

gp::USize Transition::getTotalAllocationSize() noexcept
{
    const gp::USize privateSize = g_transitionBackendHooks.privateDataSizeInBytes;
    const gp::USize privateAlign = g_transitionBackendHooks.privateDataAlignInBytes;
    const gp::USize effectiveAlign = (privateAlign > 1u) ? privateAlign : 1u;

    return gp::memory::align(sizeof(Transition), effectiveAlign) + privateSize;
}

gp::USize Transition::getAlignment() noexcept
{
    const gp::USize headerAlign = alignof(Transition);
    const gp::USize privateAlign = g_transitionBackendHooks.privateDataAlignInBytes;
    return (privateAlign > headerAlign) ? privateAlign : headerAlign;
}

void TransitionController::configureBackend(const TransitionBackendHooks& hooks) noexcept
{
    if (hooks.privateDataSizeInBytes > 0u)
    {
        GP_CHECKF(
            isPowerOfTwo(hooks.privateDataAlignInBytes),
            "Transition private data alignment must be a non-zero power of two when privateDataSizeInBytes > 0."
        );
    }
    else
    {
        GP_CHECKF(
            hooks.privateDataAlignInBytes == 0u || isPowerOfTwo(hooks.privateDataAlignInBytes),
            "Transition private data alignment must be zero or a power of two."
        );
    }

    g_transitionBackendHooks = hooks;
}

void TransitionController::resetBackend() noexcept
{
    g_transitionBackendHooks = TransitionBackendHooks{};
}

Transition* TransitionController::create(const TransitionCreateInfo& createInfo) noexcept
{
    GP_CHECKF(
        (createInfo.srcPipelines != PipelineStage::None) || (createInfo.dstPipelines != PipelineStage::None),
        "TransitionController::create() requires at least one source or destination pipeline stage."
    );

    gp::memory::Malloc* const allocator = gp::memory::getGlobalMalloc();
    GP_CHECKF(allocator != nullptr, "Global engine allocator must be valid when allocating an RHI Transition.");

    const gp::USize totalSize = Transition::getTotalAllocationSize();
    const gp::UInt32 alignment = static_cast<gp::UInt32>(Transition::getAlignment());

    void* const rawMemory = allocator->tryAllocate(totalSize, alignment);
    if (rawMemory == nullptr) [[unlikely]]
    {
        return nullptr;
    }

    // Zero trailing backend private-data bytes before backend initialization runs.
    if (g_transitionBackendHooks.privateDataSizeInBytes > 0u)
    {
        gp::memory::zeroMemory(rawMemory, totalSize);
    }

    Transition* const transition =
        ::new (rawMemory) Transition(createInfo.srcPipelines, createInfo.dstPipelines, createInfo.flags);

    if (g_transitionBackendHooks.onCreate != nullptr)
    {
        g_transitionBackendHooks.onCreate(transition, createInfo);
    }

    return transition;
}

void TransitionController::markBegin(const Transition* const transition, const PipelineStage pipeline) noexcept
{
    GP_CHECKF(transition != nullptr, "TransitionController::markBegin() called with a null Transition pointer.");
    transition->markBegin(pipeline);
}

void TransitionController::markEnd(const Transition* const transition, const PipelineStage pipeline) noexcept
{
    GP_CHECKF(transition != nullptr, "TransitionController::markEnd() called with a null Transition pointer.");
    transition->markEnd(pipeline);
}

gp::USize TransitionController::getPrivateDataSizeInBytes() noexcept
{
    return Transition::getPrivateDataSizeInBytes();
}

gp::USize TransitionController::getPrivateDataAlignInBytes() noexcept
{
    return Transition::getPrivateDataAlignInBytes();
}

gp::USize TransitionController::getTotalAllocationSize() noexcept
{
    return Transition::getTotalAllocationSize();
}

gp::USize TransitionController::getAlignment() noexcept
{
    return Transition::getAlignment();
}

}   // namespace gp::rhi::sync
