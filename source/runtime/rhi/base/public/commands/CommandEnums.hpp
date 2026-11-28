// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/ExclusiveDepthStencil.hpp"
#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::rhi::command
{

/// @brief Action taken on a render target attachment when a render pass begins.
enum class RenderTargetLoadAction : gp::UInt8
{
    /// @brief Existing contents are discarded and left undefined.
    NoAction = 0,
    /// @brief Preserves existing attachment contents on pass entry.
    Load,
    /// @brief Clears to the fast-clear value stored on the resource.
    Clear
};

/// @brief Total number of `RenderTargetLoadAction` enumerators and minimum bit width to pack it.
inline constexpr gp::UInt32 kRenderTargetLoadActionCount = static_cast<gp::UInt32>(RenderTargetLoadAction::Clear) + 1u;
inline constexpr gp::UInt32 kRenderTargetLoadActionNumBits = 2u;
static_assert((1u << kRenderTargetLoadActionNumBits) >= kRenderTargetLoadActionCount);

/// @brief Action taken on a render target attachment when a render pass ends.
enum class RenderTargetStoreAction : gp::UInt8
{
    /// @brief Discards rendered contents without writing back to memory.
    NoAction = 0,
    /// @brief Writes rendered contents back to memory.
    Store,
    /// @brief Resolves multisampled contents with a box filter before storing.
    MultisampleResolve
};

/// @brief Total number of `RenderTargetStoreAction` enumerators and minimum bit width to pack it.
inline constexpr gp::UInt32 kRenderTargetStoreActionCount =
    static_cast<gp::UInt32>(RenderTargetStoreAction::MultisampleResolve) + 1u;
inline constexpr gp::UInt32 kRenderTargetStoreActionNumBits = 2u;
static_assert((1u << kRenderTargetStoreActionNumBits) >= kRenderTargetStoreActionCount);

/// @brief Combined load and store action packed as `(Load << 2) | Store` for a single attachment.
enum class RenderTargetActions : gp::UInt8
{
    DontLoadDontStore = (static_cast<gp::UInt8>(RenderTargetLoadAction::NoAction) << 2u) |
                        static_cast<gp::UInt8>(RenderTargetStoreAction::NoAction),
    DontLoadStore = (static_cast<gp::UInt8>(RenderTargetLoadAction::NoAction) << 2u) |
                    static_cast<gp::UInt8>(RenderTargetStoreAction::Store),
    ClearStore = (static_cast<gp::UInt8>(RenderTargetLoadAction::Clear) << 2u) |
                 static_cast<gp::UInt8>(RenderTargetStoreAction::Store),
    LoadStore = (static_cast<gp::UInt8>(RenderTargetLoadAction::Load) << 2u) |
                static_cast<gp::UInt8>(RenderTargetStoreAction::Store),
    ClearDontStore = (static_cast<gp::UInt8>(RenderTargetLoadAction::Clear) << 2u) |
                     static_cast<gp::UInt8>(RenderTargetStoreAction::NoAction),
    LoadDontStore = (static_cast<gp::UInt8>(RenderTargetLoadAction::Load) << 2u) |
                    static_cast<gp::UInt8>(RenderTargetStoreAction::NoAction),
    ClearResolve = (static_cast<gp::UInt8>(RenderTargetLoadAction::Clear) << 2u) |
                   static_cast<gp::UInt8>(RenderTargetStoreAction::MultisampleResolve),
    LoadResolve = (static_cast<gp::UInt8>(RenderTargetLoadAction::Load) << 2u) |
                  static_cast<gp::UInt8>(RenderTargetStoreAction::MultisampleResolve)
};

/// @brief Packs a load action and store action into a single `RenderTargetActions` value.
/// @param[in] loadAction Pass-entry load policy.
/// @param[in] storeAction Pass-exit store policy.
/// @return Combined `RenderTargetActions` enumerator.
[[nodiscard]] inline constexpr RenderTargetActions
    makeRenderTargetActions(const RenderTargetLoadAction loadAction, const RenderTargetStoreAction storeAction) noexcept
{
    return static_cast<RenderTargetActions>(
        (static_cast<gp::UInt8>(loadAction) << kRenderTargetStoreActionNumBits) | static_cast<gp::UInt8>(storeAction)
    );
}

/// @brief Extracts the `RenderTargetLoadAction` component from a packed `RenderTargetActions`.
/// @param[in] action Packed attachment action.
/// @return Unpacked `RenderTargetLoadAction`.
[[nodiscard]] inline constexpr RenderTargetLoadAction getLoadAction(const RenderTargetActions action) noexcept
{
    return static_cast<RenderTargetLoadAction>(static_cast<gp::UInt8>(action) >> kRenderTargetStoreActionNumBits);
}

/// @brief Extracts the `RenderTargetStoreAction` component from a packed `RenderTargetActions`.
/// @param[in] action Packed attachment action.
/// @return Unpacked `RenderTargetStoreAction`.
[[nodiscard]] inline constexpr RenderTargetStoreAction getStoreAction(const RenderTargetActions action) noexcept
{
    return static_cast<RenderTargetStoreAction>(
        static_cast<gp::UInt8>(action) & ((1u << kRenderTargetStoreActionNumBits) - 1u)
    );
}

/// @brief Combined depth and stencil load/store actions packed as `(DepthAction << 4) | StencilAction`.
enum class DepthStencilTargetActions : gp::UInt8
{
    DontLoadDontStore = (static_cast<gp::UInt8>(RenderTargetActions::DontLoadDontStore) << 4u) |
                        static_cast<gp::UInt8>(RenderTargetActions::DontLoadDontStore),
    DontLoadStoreDepthStencil = (static_cast<gp::UInt8>(RenderTargetActions::DontLoadStore) << 4u) |
                                static_cast<gp::UInt8>(RenderTargetActions::DontLoadStore),
    DontLoadStoreStencilDontStoreDepth = (static_cast<gp::UInt8>(RenderTargetActions::DontLoadDontStore) << 4u) |
                                         static_cast<gp::UInt8>(RenderTargetActions::DontLoadStore),
    ClearDepthStencilStoreDepthStencil = (static_cast<gp::UInt8>(RenderTargetActions::ClearStore) << 4u) |
                                         static_cast<gp::UInt8>(RenderTargetActions::ClearStore),
    LoadDepthStencilStoreDepthStencil = (static_cast<gp::UInt8>(RenderTargetActions::LoadStore) << 4u) |
                                        static_cast<gp::UInt8>(RenderTargetActions::LoadStore),
    LoadDepthNotStencilStoreDepthNotStencil = (static_cast<gp::UInt8>(RenderTargetActions::LoadStore) << 4u) |
                                              static_cast<gp::UInt8>(RenderTargetActions::DontLoadDontStore),
    LoadDepthNotStencilDontStore = (static_cast<gp::UInt8>(RenderTargetActions::LoadDontStore) << 4u) |
                                   static_cast<gp::UInt8>(RenderTargetActions::DontLoadDontStore),
    LoadDepthStencilStoreDepthNotStencil = (static_cast<gp::UInt8>(RenderTargetActions::LoadStore) << 4u) |
                                           static_cast<gp::UInt8>(RenderTargetActions::LoadDontStore),
    LoadDepthStencilStoreStencilNotDepth = (static_cast<gp::UInt8>(RenderTargetActions::LoadDontStore) << 4u) |
                                           static_cast<gp::UInt8>(RenderTargetActions::LoadStore),
    ClearDepthStencilStoreDepthNotStencil = (static_cast<gp::UInt8>(RenderTargetActions::ClearStore) << 4u) |
                                            static_cast<gp::UInt8>(RenderTargetActions::ClearDontStore),
    ClearDepthStencilStoreStencilNotDepth = (static_cast<gp::UInt8>(RenderTargetActions::ClearDontStore) << 4u) |
                                            static_cast<gp::UInt8>(RenderTargetActions::ClearStore),
    ClearDepthStencilDontStoreDepthStencil = (static_cast<gp::UInt8>(RenderTargetActions::ClearDontStore) << 4u) |
                                             static_cast<gp::UInt8>(RenderTargetActions::ClearDontStore),
    LoadDepthStencilDontStoreDepthStencil = (static_cast<gp::UInt8>(RenderTargetActions::LoadDontStore) << 4u) |
                                            static_cast<gp::UInt8>(RenderTargetActions::LoadDontStore),
    ClearDepthNotStencilStoreDepthNotStencil = (static_cast<gp::UInt8>(RenderTargetActions::ClearStore) << 4u) |
                                               static_cast<gp::UInt8>(RenderTargetActions::DontLoadDontStore),
    ClearDepthNotStencilDontStore = (static_cast<gp::UInt8>(RenderTargetActions::ClearDontStore) << 4u) |
                                    static_cast<gp::UInt8>(RenderTargetActions::DontLoadDontStore),
    ClearDepthStoreDepthLoadStencilStoreStencil = (static_cast<gp::UInt8>(RenderTargetActions::ClearStore) << 4u) |
                                                  static_cast<gp::UInt8>(RenderTargetActions::LoadStore),
    LoadDepthClearStencilStoreDepthStencil = (static_cast<gp::UInt8>(RenderTargetActions::LoadStore) << 4u) |
                                             static_cast<gp::UInt8>(RenderTargetActions::ClearStore),
    ClearStencilDontLoadDepthStoreStencilNotDepth =
        (static_cast<gp::UInt8>(RenderTargetActions::DontLoadDontStore) << 4u) |
        static_cast<gp::UInt8>(RenderTargetActions::ClearStore),
    LoadDepthNotStencilResolveDepth = (static_cast<gp::UInt8>(RenderTargetActions::LoadResolve) << 4u) |
                                      static_cast<gp::UInt8>(RenderTargetActions::DontLoadDontStore),
    ClearDepthNotStencilResolveDepth = (static_cast<gp::UInt8>(RenderTargetActions::ClearResolve) << 4u) |
                                       static_cast<gp::UInt8>(RenderTargetActions::DontLoadDontStore),
    LoadDepthStencilResolveDepth = (static_cast<gp::UInt8>(RenderTargetActions::LoadResolve) << 4u) |
                                   static_cast<gp::UInt8>(RenderTargetActions::LoadDontStore),
    ClearDepthStencilResolveDepth = (static_cast<gp::UInt8>(RenderTargetActions::ClearResolve) << 4u) |
                                    static_cast<gp::UInt8>(RenderTargetActions::ClearDontStore),
    ClearDepthStencilResolveStencilNotDepth = (static_cast<gp::UInt8>(RenderTargetActions::ClearDontStore) << 4u) |
                                              static_cast<gp::UInt8>(RenderTargetActions::ClearResolve)
};

/// @brief Packs separate depth and stencil `RenderTargetActions` into a `DepthStencilTargetActions`.
/// @param[in] depthAction Load/store policy for the depth plane.
/// @param[in] stencilAction Load/store policy for the stencil plane.
/// @return Combined `DepthStencilTargetActions` value.
[[nodiscard]] inline constexpr DepthStencilTargetActions makeDepthStencilTargetActions(
    const RenderTargetActions depthAction, const RenderTargetActions stencilAction
) noexcept
{
    return static_cast<DepthStencilTargetActions>(
        (static_cast<gp::UInt8>(depthAction) << 4u) | static_cast<gp::UInt8>(stencilAction)
    );
}

/// @brief Extracts the depth-plane `RenderTargetActions` from `action`.
/// @param[in] action Combined depth-stencil action.
/// @return Depth-plane load/store action.
[[nodiscard]] inline constexpr RenderTargetActions getDepthActions(const DepthStencilTargetActions action) noexcept
{
    return static_cast<RenderTargetActions>(static_cast<gp::UInt8>(action) >> 4u);
}

/// @brief Extracts the stencil-plane `RenderTargetActions` from `action`.
/// @param[in] action Combined depth-stencil action.
/// @return Stencil-plane load/store action.
[[nodiscard]] inline constexpr RenderTargetActions getStencilActions(const DepthStencilTargetActions action) noexcept
{
    return static_cast<RenderTargetActions>(static_cast<gp::UInt8>(action) & 0x0Fu);
}

/// @brief Flush level requested on `CommandListImmediate::immediateFlush()`.
enum class ImmediateFlushType : gp::UInt8
{
    /// @brief Waits only for outstanding parallel command recording tasks without dispatching.
    WaitForOutstandingTasksOnly = 0,
    /// @brief Submits recorded command lists for execution without blocking for completion.
    DispatchToRHIThread = 1,
    /// @brief Submits recorded command lists and waits until execution completes.
    FlushRHIThread = 2,
    /// @brief Submits recorded command lists, waits for completion, and drains deferred resource deletes.
    FlushRHIThreadFlushResources = 3
};

/// @brief Bitmask flags controlling `CommandListExecutor::submit()` behavior.
enum class SubmitFlags : gp::UInt32
{
    None = 0u,
    /// @brief Closes the current batch and submits finalized command lists to the backend.
    SubmitToGPU = 1u << 0,
    /// @brief Gathers and destroys deferred resources and runs `DynamicRHI::processDeleteQueue()`.
    DeleteResources = 1u << 1,
    /// @brief Blocks until submitted command lists complete translation and submission.
    FlushRHIThread = 1u << 2,
    /// @brief Signals end-of-frame to `DynamicRHI::endFrame()`.
    EndFrame = 1u << 3,
    /// @brief Requests GPU profiling capture on the next frame.
    ProfileNextFrame = 1u << 4,
    /// @brief Switches executor bypass mode on once the pipeline drains.
    EnableBypass = 1u << 5,
    /// @brief Switches executor bypass mode off once the pipeline drains.
    DisableBypass = 1u << 6
};

/// @brief Preset color and depth-stencil load/store combinations for a render pass.
enum class SimpleRenderTargetMode : gp::UInt8
{
    ExistingColorAndDepth = 0,
    UninitializedColorAndDepth,
    UninitializedColorExistingDepth,
    UninitializedColorClearDepth,
    ClearColorExistingDepth,
    ClearColorAndDepth,
    /// @brief Loads existing color and depth, then discards depth writes at pass end.
    ExistingContentsNoDepthStore,
    ExistingColorAndClearDepth,
    ExistingColorAndDepthAndClearStencil
};

/// @brief Targets affected by a depth-stencil clear operation.
enum class ClearDepthStencil : gp::UInt8
{
    Depth = 0,
    Stencil,
    DepthStencil
};

/// @brief Scheduling priority for async compute queue submissions.
enum class AsyncComputePriority : gp::UInt8
{
    Default = 0,
    High
};

/// @brief Driver hint for how much GPU capacity async compute may claim against graphics.
enum class AsyncComputeBudget : gp::UInt8
{
    Least = 0,
    GfxHeavy,
    Balanced,
    ComputeHeavy,
    All
};

/// @brief Driver hint for balancing occupancy between graphics and synchronous compute work.
enum class SyncComputeBudget : gp::UInt8
{
    Default = 0,
    Balanced,
    /// @brief Favors compute occupancy; may serialize concurrent async compute queues.
    ComputeHeavy
};

/// @brief Decodes a `SimpleRenderTargetMode` preset and depth-stencil usage into explicit load/store actions.
/// @details Disables depth or stencil load actions when the corresponding plane is unused and drops store actions when
///          the plane is read-only to avoid redundant memory bandwidth on tile-based GPUs.
/// @param[in] mode Preset render target mode to decode.
/// @param[out] outColorLoadAction Decoded color load action.
/// @param[out] outColorStoreAction Decoded color store action.
/// @param[out] outDepthLoadAction Decoded depth load action.
/// @param[out] outDepthStoreAction Decoded depth store action.
/// @param[out] outStencilLoadAction Decoded stencil load action.
/// @param[out] outStencilStoreAction Decoded stencil store action.
/// @param[in] depthStencilUsage Active depth and stencil read/write access mode for the pass.
inline constexpr void decodeRenderTargetMode(
    const SimpleRenderTargetMode mode,
    RenderTargetLoadAction& outColorLoadAction,
    RenderTargetStoreAction& outColorStoreAction,
    RenderTargetLoadAction& outDepthLoadAction,
    RenderTargetStoreAction& outDepthStoreAction,
    RenderTargetLoadAction& outStencilLoadAction,
    RenderTargetStoreAction& outStencilStoreAction,
    const resource::ExclusiveDepthStencil depthStencilUsage
) noexcept
{
    outColorLoadAction = RenderTargetLoadAction::NoAction;
    outColorStoreAction = RenderTargetStoreAction::Store;
    outDepthLoadAction = RenderTargetLoadAction::NoAction;
    outDepthStoreAction = RenderTargetStoreAction::Store;
    outStencilLoadAction = RenderTargetLoadAction::NoAction;
    outStencilStoreAction = RenderTargetStoreAction::Store;

    switch (mode)
    {
    case SimpleRenderTargetMode::ExistingColorAndDepth:
        outColorLoadAction = RenderTargetLoadAction::Load;
        outDepthLoadAction = RenderTargetLoadAction::Load;
        outStencilLoadAction = RenderTargetLoadAction::Load;
        break;
    case SimpleRenderTargetMode::UninitializedColorAndDepth:
        outColorLoadAction = RenderTargetLoadAction::NoAction;
        outDepthLoadAction = RenderTargetLoadAction::NoAction;
        outStencilLoadAction = RenderTargetLoadAction::NoAction;
        break;
    case SimpleRenderTargetMode::UninitializedColorExistingDepth:
        outColorLoadAction = RenderTargetLoadAction::NoAction;
        outDepthLoadAction = RenderTargetLoadAction::Load;
        outStencilLoadAction = RenderTargetLoadAction::Load;
        break;
    case SimpleRenderTargetMode::UninitializedColorClearDepth:
        outColorLoadAction = RenderTargetLoadAction::NoAction;
        outDepthLoadAction = RenderTargetLoadAction::Clear;
        outStencilLoadAction = RenderTargetLoadAction::Clear;
        break;
    case SimpleRenderTargetMode::ClearColorExistingDepth:
        outColorLoadAction = RenderTargetLoadAction::Clear;
        outDepthLoadAction = RenderTargetLoadAction::Load;
        outStencilLoadAction = RenderTargetLoadAction::Load;
        break;
    case SimpleRenderTargetMode::ClearColorAndDepth:
        outColorLoadAction = RenderTargetLoadAction::Clear;
        outDepthLoadAction = RenderTargetLoadAction::Clear;
        outStencilLoadAction = RenderTargetLoadAction::Clear;
        break;
    case SimpleRenderTargetMode::ExistingContentsNoDepthStore:
        outColorLoadAction = RenderTargetLoadAction::Load;
        outDepthLoadAction = RenderTargetLoadAction::Load;
        outStencilLoadAction = RenderTargetLoadAction::Load;
        outDepthStoreAction = RenderTargetStoreAction::NoAction;
        break;
    case SimpleRenderTargetMode::ExistingColorAndClearDepth:
        outColorLoadAction = RenderTargetLoadAction::Load;
        outDepthLoadAction = RenderTargetLoadAction::Clear;
        outStencilLoadAction = RenderTargetLoadAction::Clear;
        break;
    case SimpleRenderTargetMode::ExistingColorAndDepthAndClearStencil:
        outColorLoadAction = RenderTargetLoadAction::Load;
        outDepthLoadAction = RenderTargetLoadAction::Load;
        outStencilLoadAction = RenderTargetLoadAction::Clear;
        break;
    default:
        if !consteval
        {
            GP_CHECK_NO_ENTRY();
        }
        break;
    }

    if (!depthStencilUsage.isUsingDepth())
    {
        outDepthLoadAction = RenderTargetLoadAction::NoAction;
    }

    if (!depthStencilUsage.isDepthWrite())
    {
        outDepthStoreAction = RenderTargetStoreAction::NoAction;
    }

    if (!depthStencilUsage.isUsingStencil())
    {
        outStencilLoadAction = RenderTargetLoadAction::NoAction;
    }

    if (!depthStencilUsage.isStencilWrite())
    {
        outStencilStoreAction = RenderTargetStoreAction::NoAction;
    }
}

}   // namespace gp::rhi::command

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::command::SubmitFlags);
