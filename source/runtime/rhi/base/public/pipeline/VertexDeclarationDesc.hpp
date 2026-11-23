// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/HashCombine.hpp"
#include "hashing/HashMix.hpp"
#include "hashing/HashOf.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "RHIDefinitions.hpp"
#include <type_traits>

namespace gp::rhi::pipeline
{

/// @brief Compact 8-byte, padding-free description of a single vertex input attribute.
struct VertexElement
{
    /// @brief Vertex buffer stream binding slot index.
    gp::UInt8 streamIndex{ 0u };

    /// @brief Byte offset of this attribute from the start of each vertex or instance record.
    gp::UInt8 offset{ 0u };

    /// @brief Data format and component count of the attribute.
    VertexElementType type{ VertexElementType::None };

    /// @brief Shader input attribute location index.
    gp::UInt8 attributeIndex{ 0u };

    /// @brief Byte stride between consecutive vertices or instances in `streamIndex`.
    gp::UInt16 stride{ 0u };

    /// @brief Non-zero when the attribute advances per instance; zero when it advances per vertex.
    gp::UInt16 useInstanceIndex{ 0u };

public:
    /// @brief Constructs a zeroed vertex element.
    constexpr VertexElement() noexcept = default;

    /// @brief Constructs a vertex element describing one shader input attribute.
    /// @param[in] inStreamIndex Vertex stream slot index.
    /// @param[in] inOffset Byte offset inside the stream record.
    /// @param[in] inType Attribute component format.
    /// @param[in] inAttributeIndex Shader input location index.
    /// @param[in] inStride Byte stride of the source vertex stream.
    /// @param[in] inUseInstanceIndex True for per-instance step rate; false for per-vertex step rate.
    constexpr VertexElement(
        const gp::UInt8 inStreamIndex,
        const gp::UInt8 inOffset,
        const VertexElementType inType,
        const gp::UInt8 inAttributeIndex,
        const gp::UInt16 inStride,
        const bool inUseInstanceIndex = false
    ) noexcept
        : streamIndex(inStreamIndex)
        , offset(inOffset)
        , type(inType)
        , attributeIndex(inAttributeIndex)
        , stride(inStride)
        , useInstanceIndex(inUseInstanceIndex ? 1u : 0u)
    {}

    /// @brief Reports whether this attribute advances per instance.
    /// @return True when `useInstanceIndex != 0`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isInstanced() const noexcept
    {
        return useInstanceIndex != 0u;
    }

    /// @brief Compares two vertex elements field by field.
    /// @param[in] rhs Element to compare against.
    /// @return True when all fields match.
    [[nodiscard]] constexpr bool operator==(const VertexElement& rhs) const noexcept = default;

    /// @brief Computes a 64-bit hash over all fields of this vertex element.
    /// @return 64-bit hash value.
    [[nodiscard]] constexpr gp::UInt64 getHash() const noexcept
    {
        gp::UInt64 hash = gp::hashing::hashInt64(streamIndex);
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashInt64(offset));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(type));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashInt64(attributeIndex));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashInt64(stride));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashInt64(useInstanceIndex));
        return hash;
    }

    /// @brief Streams all fields into a `gp::hashing` streaming accumulator.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.append(streamIndex);
        hasher.append(offset);
        hasher.append(type);
        hasher.append(attributeIndex);
        hasher.append(stride);
        hasher.append(useInstanceIndex);
    }
};

static_assert(sizeof(VertexElement) == 8u, "VertexElement must be exactly 8 bytes.");
static_assert(
    std::has_unique_object_representations_v<VertexElement>,
    "VertexElement must have zero compiler padding so byte-level hashing stays deterministic."
);

/// @brief Fixed-capacity inline list of vertex input attributes (up to `kMaxVertexElementCount`, zero heap
/// allocations).
using VertexDeclarationElementList = gp::FixedVector<VertexElement, kMaxVertexElementCount>;

}   // namespace gp::rhi::pipeline
