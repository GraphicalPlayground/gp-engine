// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::rhi::view
{

/// @brief Scope of bindless descriptor indexing enabled for shaders.
/// @details Values are ordered from least to most permissive so comparison operators apply.
enum class BindlessConfiguration : gp::UInt8
{
    Disabled = 0,
    RayTracing,
    /// @brief Ray tracing, materials, and shaders that explicitly opt in.
    Minimal,
    All
};

/// @brief Categories of hardware descriptor heaps managed by modern backends.
enum class DescriptorHeapType : gp::UInt8
{
    Standard = 0,
    Sampler,
    RenderTarget,
    DepthStencil,
    /// @brief Sentinel count of valid descriptor heap categories.
    Count,
    Invalid = 0xFFu
};

/// @brief Total number of valid `DescriptorHeapType` categories (`Standard` through `DepthStencil`).
inline constexpr gp::UInt8 kDescriptorHeapTypeCount = static_cast<gp::UInt8>(DescriptorHeapType::Count);

/// @brief Concrete view type stored in a descriptor handle.
enum class DescriptorType : gp::UInt8
{
    BufferSRV = 0,
    BufferUAV,
    TypedBufferSRV,
    TypedBufferUAV,
    TextureSRV,
    TextureUAV,
    CBV,
    Sampler,
    AccelerationStructure,
    Invalid
};

/// @brief Bitmask over `DescriptorType` values for descriptor range and heap filtering.
enum class DescriptorTypeMask : gp::UInt16
{
    None = 0u,
    BufferSRV = 1u << 0,
    BufferUAV = 1u << 1,
    TypedBufferSRV = 1u << 2,
    TypedBufferUAV = 1u << 3,
    TextureSRV = 1u << 4,
    TextureUAV = 1u << 5,
    CBV = 1u << 6,
    Sampler = 1u << 7,
    AccelerationStructure = 1u << 8,

    SRV = (1u << 0) | (1u << 2) | (1u << 4),
    UAV = (1u << 1) | (1u << 3) | (1u << 5),
    Standard = ((1u << 0) | (1u << 2) | (1u << 4)) | ((1u << 1) | (1u << 3) | (1u << 5)) | (1u << 6) | (1u << 8)
};

/// @brief High-level category of an RHI resource view descriptor.
enum class ViewType : gp::UInt8
{
    BufferSRV = 0,
    BufferUAV = 1,
    TextureSRV = 2,
    TextureUAV = 3
};

/// @brief Sub-kind describing how a buffer view interprets raw buffer memory.
enum class BufferViewType : gp::UInt8
{
    Unknown = 0,
    Typed = 1,
    Structured = 2,
    AccelerationStructure = 3,
    Raw = 4
};

/// @brief Texture dimensionality targeted by a texture view descriptor.
enum class TextureViewDimension : gp::UInt8
{
    Unknown = 0,
    Texture2D = 1,
    Texture2DArray = 2,
    TextureCube = 3,
    TextureCubeArray = 4,
    Texture3D = 5
};

/// @brief Minimum bit width required to store any `TextureViewDimension` enumerator in a bitfield.
inline constexpr gp::UInt32 kTextureViewDimensionNumBits = 3u;
static_assert(
    (1u << kTextureViewDimensionNumBits) > static_cast<gp::UInt32>(TextureViewDimension::Texture3D),
    "kTextureViewDimensionNumBits must be wide enough to represent all TextureViewDimension enumerators."
);

}   // namespace gp::rhi::view

GP_ENABLE_ENUM_COMPARISON_OPERATIONS(gp::rhi::view::BindlessConfiguration);
GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::view::DescriptorTypeMask);

namespace gp::rhi::view
{

/// @brief Checks whether bindless descriptor indexing is disabled.
/// @param[in] configuration Active bindless configuration tier.
/// @return True when bindless is disabled.
[[nodiscard]] inline constexpr bool isBindlessDisabled(const BindlessConfiguration configuration) noexcept
{
    return configuration == BindlessConfiguration::Disabled;
}

/// @brief Checks whether bindless is restricted strictly to ray tracing shaders.
/// @param[in] configuration Active bindless configuration tier.
/// @return True when only ray tracing shaders use bindless indexing.
[[nodiscard]] inline constexpr bool
    isBindlessEnabledForOnlyRayTracing(const BindlessConfiguration configuration) noexcept
{
    return configuration == BindlessConfiguration::RayTracing;
}

/// @brief Checks whether ray tracing shaders can use bindless descriptor indexing.
/// @param[in] configuration Active bindless configuration tier.
/// @return True when bindless is enabled for ray tracing or broader scopes.
[[nodiscard]] inline constexpr bool isBindlessEnabledForRayTracing(const BindlessConfiguration configuration) noexcept
{
    return configuration >= BindlessConfiguration::RayTracing;
}

/// @brief Checks whether graphics shaders can use bindless descriptor indexing.
/// @param[in] configuration Active bindless configuration tier.
/// @return True when bindless is enabled for minimal or full shader scopes.
[[nodiscard]] inline constexpr bool isBindlessEnabledForAnyGraphics(const BindlessConfiguration configuration) noexcept
{
    return configuration >= BindlessConfiguration::Minimal;
}

/// @brief Checks whether all shader stages can use bindless descriptor indexing.
/// @param[in] configuration Active bindless configuration tier.
/// @return True when bindless is fully enabled.
[[nodiscard]] inline constexpr bool isBindlessFullyEnabled(const BindlessConfiguration configuration) noexcept
{
    return configuration == BindlessConfiguration::All;
}

/// @brief Converts a single `DescriptorType` into its corresponding `DescriptorTypeMask` bit.
/// @param[in] type Descriptor type to convert.
/// @return Single-bit mask for `type`, or `DescriptorTypeMask::None` for `DescriptorType::Invalid`.
[[nodiscard]] inline constexpr DescriptorTypeMask descriptorTypeMaskFromType(const DescriptorType type) noexcept
{
    if (type == DescriptorType::Invalid)
    {
        return DescriptorTypeMask::None;
    }

    return static_cast<DescriptorTypeMask>(1u << static_cast<gp::UInt16>(type));
}

/// @brief Diagnostic snapshot reporting utilization of a backend bindless descriptor heap or allocator pool.
struct BindlessDescriptorAllocationInfo
{
    /// @brief Bitmask of `DescriptorType` values served by this descriptor pool.
    DescriptorTypeMask types{ DescriptorTypeMask::None };

    /// @brief Number of descriptors currently allocated.
    gp::UInt32 count{ 0u };

    /// @brief Current backing heap capacity in descriptors.
    gp::UInt32 capacity{ 0u };

    /// @brief Maximum hardware or configured capacity in descriptors.
    gp::UInt32 maxCapacity{ 0u };

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Allocation snapshot to compare against.
    /// @return True when types, count, capacity, and maxCapacity all match.
    [[nodiscard]] constexpr bool operator==(const BindlessDescriptorAllocationInfo& rhs) const noexcept = default;
};

/// @brief Maps a `ViewType` category to its corresponding bindless `DescriptorType`.
/// @param[in] viewType View category to convert.
/// @return Matching `DescriptorType` enumerator.
[[nodiscard]] inline constexpr DescriptorType descriptorTypeFromViewType(const ViewType viewType) noexcept
{
    switch (viewType)
    {
    case ViewType::BufferUAV:
        return DescriptorType::BufferUAV;

    case ViewType::TextureSRV:
        return DescriptorType::TextureSRV;

    case ViewType::TextureUAV:
        return DescriptorType::TextureUAV;

    case ViewType::BufferSRV:
    default:
        return DescriptorType::BufferSRV;
    }
}

}   // namespace gp::rhi::view
