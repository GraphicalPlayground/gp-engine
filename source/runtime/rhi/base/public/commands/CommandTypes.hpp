// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"   // IWYU pragma: keep
#include "core/ResourceForward.hpp"
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"

namespace gp::rhi::command
{

/// @brief Viewport rectangle and depth clip range matching hardware rasterizer viewport descriptors.
struct ViewportBounds
{
    /// @brief Left edge of the viewport in pixels.
    gp::Float32 topLeftX{ 0.0f };

    /// @brief Top edge of the viewport in pixels.
    gp::Float32 topLeftY{ 0.0f };

    /// @brief Viewport width in pixels.
    gp::Float32 width{ 0.0f };

    /// @brief Viewport height in pixels.
    gp::Float32 height{ 0.0f };

    /// @brief Minimum normalized clip depth (`0.0f` default).
    gp::Float32 minDepth{ 0.0f };

    /// @brief Maximum normalized clip depth (`1.0f` default).
    gp::Float32 maxDepth{ 1.0f };

    /// @brief Constructs a zero-sized viewport with `[0, 1]` depth range.
    constexpr ViewportBounds() noexcept = default;

    /// @brief Constructs viewport bounds from explicit coordinates and depth range.
    /// @param[in] inTopLeftX Left edge in pixels.
    /// @param[in] inTopLeftY Top edge in pixels.
    /// @param[in] inWidth Viewport width in pixels.
    /// @param[in] inHeight Viewport height in pixels.
    /// @param[in] inMinDepth Minimum depth clip bound.
    /// @param[in] inMaxDepth Maximum depth clip bound.
    constexpr ViewportBounds(
        const gp::Float32 inTopLeftX,
        const gp::Float32 inTopLeftY,
        const gp::Float32 inWidth,
        const gp::Float32 inHeight,
        const gp::Float32 inMinDepth = 0.0f,
        const gp::Float32 inMaxDepth = 1.0f
    ) noexcept
        : topLeftX(inTopLeftX)
        , topLeftY(inTopLeftY)
        , width(inWidth)
        , height(inHeight)
        , minDepth(inMinDepth)
        , maxDepth(inMaxDepth)
    {}

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Bounds to compare against.
    /// @return True when all six bounds match.
    [[nodiscard]] constexpr bool operator==(const ViewportBounds& rhs) const noexcept = default;
};

/// @brief Indirect compute dispatch argument block laid out for GPU indirect argument buffers.
struct DispatchIndirectParameters
{
    /// @brief Number of thread groups dispatched along X.
    gp::UInt32 threadGroupCountX{ 0u };

    /// @brief Number of thread groups dispatched along Y.
    gp::UInt32 threadGroupCountY{ 0u };

    /// @brief Number of thread groups dispatched along Z.
    gp::UInt32 threadGroupCountZ{ 0u };

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Parameters to compare against.
    /// @return True when all three group counts match.
    [[nodiscard]] constexpr bool operator==(const DispatchIndirectParameters& rhs) const noexcept = default;
};

/// @brief Indirect non-indexed draw argument block matching hardware `DrawInstancedIndirect` layout.
struct DrawIndirectParameters
{
    /// @brief Number of vertices to draw per instance.
    gp::UInt32 vertexCountPerInstance{ 0u };

    /// @brief Number of instances to draw.
    gp::UInt32 instanceCount{ 0u };

    /// @brief Index of the first vertex to fetch.
    gp::UInt32 startVertexLocation{ 0u };

    /// @brief Starting instance ID.
    gp::UInt32 startInstanceLocation{ 0u };

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Parameters to compare against.
    /// @return True when all four fields match.
    [[nodiscard]] constexpr bool operator==(const DrawIndirectParameters& rhs) const noexcept = default;
};

/// @brief Indirect indexed draw argument block matching hardware `DrawIndexedInstancedIndirect` layout.
struct DrawIndexedIndirectParameters
{
    /// @brief Number of indices read from the index buffer per instance.
    gp::UInt32 indexCountPerInstance{ 0u };

    /// @brief Number of instances to draw.
    gp::UInt32 instanceCount{ 0u };

    /// @brief Offset in indices into the index buffer.
    gp::UInt32 startIndexLocation{ 0u };

    /// @brief Signed offset added to each vertex index before fetching from vertex streams.
    gp::Int32 baseVertexLocation{ 0 };

    /// @brief Starting instance ID.
    gp::UInt32 startInstanceLocation{ 0u };

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Parameters to compare against.
    /// @return True when all five fields match.
    [[nodiscard]] constexpr bool operator==(const DrawIndexedIndirectParameters& rhs) const noexcept = default;
};

/// @brief Byte sub-range `[offset, offset + size)` inside a GPU buffer.
struct BufferRange
{
    /// @brief Target GPU buffer pointer.
    resource::Buffer* buffer{ nullptr };

    /// @brief Byte offset from the start of `buffer`.
    gp::UInt64 offset{ 0u };

    /// @brief Byte length of the range.
    gp::UInt64 size{ 0u };

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Range to compare against.
    /// @return True when buffer, offset, and size match.
    [[nodiscard]] constexpr bool operator==(const BufferRange& rhs) const noexcept = default;
};

/// @brief Parameters defining source/destination texel boxes, slice ranges, and mip ranges for `copyTexture()`.
struct CopyTextureInfo
{
    /// @brief Copy region dimensions in texels (`(0, 0, 0)` copies the full mip extent).
    resource::IntVector size{};

    /// @brief Source texel origin `(x, y, z)`.
    resource::IntVector sourcePosition{};

    /// @brief Destination texel origin `(x, y, z)`.
    resource::IntVector destPosition{};

    /// @brief First source array slice (or cube face index).
    gp::UInt32 sourceSliceIndex{ 0u };

    /// @brief First destination array slice (or cube face index).
    gp::UInt32 destSliceIndex{ 0u };

    /// @brief Number of consecutive array slices to copy.
    gp::UInt32 numSlices{ 1u };

    /// @brief First source mip level.
    gp::UInt32 sourceMipIndex{ 0u };

    /// @brief First destination mip level.
    gp::UInt32 destMipIndex{ 0u };

    /// @brief Number of consecutive mip levels to copy.
    gp::UInt32 numMips{ 1u };

    /// @brief Returns the 2D source texel rectangle `[sourcePosition, sourcePosition + size)`.
    /// @return Source `resource::IntRect`.
    [[nodiscard]] constexpr resource::IntRect getSourceRect() const noexcept
    {
        return resource::IntRect(
            sourcePosition.x, sourcePosition.y, sourcePosition.x + size.x, sourcePosition.y + size.y
        );
    }

    /// @brief Returns the 2D destination texel rectangle `[destPosition, destPosition + size)`.
    /// @return Destination `resource::IntRect`.
    [[nodiscard]] constexpr resource::IntRect getDestRect() const noexcept
    {
        return resource::IntRect(destPosition.x, destPosition.y, destPosition.x + size.x, destPosition.y + size.y);
    }

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Copy info to compare against.
    /// @return True when all fields match.
    [[nodiscard]] constexpr bool operator==(const CopyTextureInfo& rhs) const noexcept = default;
};

/// @brief Parameters defining a GPU-to-GPU buffer region copy.
struct CopyBufferRegionParams
{
    /// @brief Destination GPU buffer.
    resource::Buffer* destBuffer{ nullptr };

    /// @brief Destination byte offset.
    gp::UInt64 dstOffset{ 0u };

    /// @brief Source GPU buffer.
    resource::Buffer* sourceBuffer{ nullptr };

    /// @brief Source byte offset.
    gp::UInt64 srcOffset{ 0u };

    /// @brief Number of bytes to copy.
    gp::UInt64 numBytes{ 0u };

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Copy params to compare against.
    /// @return True when all fields match.
    [[nodiscard]] constexpr bool operator==(const CopyBufferRegionParams& rhs) const noexcept = default;
};

/// @brief Axis-aligned integer rectangle specifying an optional MSAA resolve sub-region.
struct ResolveRect
{
    /// @brief Left coordinate in texels (`-1` when unset).
    gp::Int32 x1{ -1 };

    /// @brief Top coordinate in texels (`-1` when unset).
    gp::Int32 y1{ -1 };

    /// @brief Right coordinate in texels (`-1` when unset).
    gp::Int32 x2{ -1 };

    /// @brief Bottom coordinate in texels (`-1` when unset).
    gp::Int32 y2{ -1 };

    /// @brief Constructs an invalid/unset resolve rect (`-1, -1, -1, -1`).
    constexpr ResolveRect() noexcept = default;

    /// @brief Constructs a resolve rect from explicit corner coordinates.
    /// @param[in] inX1 Left coordinate.
    /// @param[in] inY1 Top coordinate.
    /// @param[in] inX2 Right coordinate.
    /// @param[in] inY2 Bottom coordinate.
    constexpr ResolveRect(
        const gp::Int32 inX1, const gp::Int32 inY1, const gp::Int32 inX2, const gp::Int32 inY2
    ) noexcept
        : x1(inX1)
        , y1(inY1)
        , x2(inX2)
        , y2(inY2)
    {}

    /// @brief Constructs a resolve rect from an `IntRect`.
    /// @param[in] rect Source integer rectangle.
    constexpr explicit ResolveRect(const resource::IntRect& rect) noexcept
        : x1(rect.min.x)
        , y1(rect.min.y)
        , x2(rect.max.x)
        , y2(rect.max.y)
    {}

    /// @brief Reports whether this resolve rect describes a valid non-empty positive region.
    /// @return True when `x1 >= 0 && y1 >= 0 && (x2 - x1) > 0 && (y2 - y1) > 0`.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return (x1 >= 0) && (y1 >= 0) && ((x2 - x1) > 0) && ((y2 - y1) > 0);
    }

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Rect to compare against.
    /// @return True when all four coordinates match.
    [[nodiscard]] constexpr bool operator==(const ResolveRect& rhs) const noexcept = default;
};

}   // namespace gp::rhi::command
