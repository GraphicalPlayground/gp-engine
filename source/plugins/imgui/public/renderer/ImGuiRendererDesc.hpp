// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::imgui
{

/// @brief Plain-old-data configuration for `ImGuiRHIRenderer::initialize()`.
/// @details Initial capacities only size the first allocation. The renderer grows its dynamic vertex/index buffers on
///          demand (with `growthSlack` extra elements) so a frame never fails to draw. Sizing them for the expected
///          peak avoids buffer re-creation spikes in the first frames.
struct ImGuiRendererDesc
{
    /// @brief Number of `ImDrawVert` elements reserved by the first dynamic vertex buffer allocation.
    gp::UInt32 initialVertexCapacity{ 16384u };

    /// @brief Number of `ImDrawIdx` elements reserved by the first dynamic index buffer allocation.
    gp::UInt32 initialIndexCapacity{ 32768u };

    /// @brief Extra elements added on top of the required count whenever a geometry buffer must grow.
    gp::UInt32 growthSlack{ 8192u };
};

/// @brief Per-frame counters produced by `ImGuiRHIRenderer` for profiling overlays.
struct ImGuiRendererStats
{
    /// @brief Number of `drawIndexedPrimitive()` calls recorded last frame.
    gp::UInt32 drawCalls{ 0u };

    /// @brief Total vertices uploaded last frame.
    gp::UInt32 vertexCount{ 0u };

    /// @brief Total indices uploaded last frame.
    gp::UInt32 indexCount{ 0u };

    /// @brief Number of ImGui-managed textures currently resident on the GPU (font atlas pages, etc.).
    gp::UInt32 residentTextures{ 0u };

    /// @brief Number of texture create/update/destroy requests honored last frame.
    gp::UInt32 textureRequests{ 0u };

    /// @brief Current dynamic vertex buffer capacity in elements.
    gp::UInt32 vertexCapacity{ 0u };

    /// @brief Current dynamic index buffer capacity in elements.
    gp::UInt32 indexCapacity{ 0u };
};

}   // namespace gp::imgui
