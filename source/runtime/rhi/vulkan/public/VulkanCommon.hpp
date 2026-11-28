// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Fundamental.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/Memory.hpp"
#include "RHIVulkan.hpp"   // IWYU pragma: export
#include "shaders/ShaderEnums.hpp"
#include <cstddef>
#include <type_traits>

/// @brief Controls whether the Vulkan backend advertises geometry shader support.
#ifndef GP_VULKAN_SUPPORTS_GEOMETRY_SHADERS
    #define GP_VULKAN_SUPPORTS_GEOMETRY_SHADERS GP_PLATFORM_SUPPORTS_GEOMETRY_SHADERS
#endif

/// @brief Enables embedding binding-table debug names in generated shader metadata for descriptor layout debugging.
#ifndef GP_VULKAN_ENABLE_BINDING_DEBUG_NAMES
    #define GP_VULKAN_ENABLE_BINDING_DEBUG_NAMES 0
#endif

namespace gp::rhi::vulkan
{

/// @brief Compact shader-stage index space shared across graphics, compute, and ray tracing pipelines.
/// @details Values for mutually exclusive pipeline families overlap intentionally (for example `Vertex == 0`,
///          `Compute == 0`, and `RayGen == 0`) so fixed-size stage arrays remain compact.
namespace shader_stage
{

/// @brief Pipeline-local shader stage index.
enum class Stage : gp::Int32
{
    Vertex = 0,
    Pixel = 1,
    Geometry = 2,
    Mesh = 3,
    Task = 4,

    NumGraphicsStages = 5,

    RayGen = 0,
    RayMiss = 1,
    RayHitGroup = 2,
    RayCallable = 3,

    NumRayTracingStages = 4,

    Compute = 0,

    NumComputeStages = 1,

    /// @brief Even upper bound simplifying bindless alignment calculations across all stage families.
    MaxNumStages = 6,

    Invalid = -1
};

/// @brief Maximum number of graphics shader stages active simultaneously (`Task+Mesh+Pixel` or
/// `Vertex+Geometry+Pixel`).
inline constexpr gp::UInt32 kMaxActiveGraphicStages = 3u;

static_assert(
    static_cast<gp::Int32>(Stage::MaxNumStages) >=
        gp::math::max<gp::Int32>(
            static_cast<gp::Int32>(Stage::NumComputeStages),
            gp::math::max<gp::Int32>(
                static_cast<gp::Int32>(Stage::NumGraphicsStages), static_cast<gp::Int32>(Stage::NumRayTracingStages)
            )
        ),
    "Stage::MaxNumStages must accommodate every pipeline stage family."
);

/// @brief Maps a `shader::ShaderFrequency` enumerator to its compact pipeline-local `Stage` index.
/// @param[in] frequency Target shader frequency.
/// @return Corresponding `Stage`, or `Stage::Invalid` when `frequency` is unsupported.
[[nodiscard]] inline Stage getStageForFrequency(const shader::ShaderFrequency frequency) noexcept
{
    switch (frequency)
    {
    case shader::ShaderFrequency::Vertex:
        return Stage::Vertex;
    case shader::ShaderFrequency::Mesh:
        return Stage::Mesh;
    case shader::ShaderFrequency::Amplification:
        return Stage::Task;
    case shader::ShaderFrequency::Pixel:
        return Stage::Pixel;
    case shader::ShaderFrequency::Geometry:
        return Stage::Geometry;
    case shader::ShaderFrequency::RayGen:
        return Stage::RayGen;
    case shader::ShaderFrequency::RayMiss:
        return Stage::RayMiss;
    case shader::ShaderFrequency::RayHitGroup:
        return Stage::RayHitGroup;
    case shader::ShaderFrequency::RayCallable:
        return Stage::RayCallable;
    case shader::ShaderFrequency::Compute:
        return Stage::Compute;
    default:
        GP_CHECKF(false, "Invalid shader frequency %d", static_cast<gp::Int32>(frequency));
        break;
    }

    return Stage::Invalid;
}

/// @brief Maps a graphics `Stage` index back to its `shader::ShaderFrequency`.
/// @param[in] stage Graphics shader stage (`Vertex`, `Pixel`, `Geometry`, `Mesh`, or `Task`).
/// @return Matching `shader::ShaderFrequency`, or `static_cast<shader::ShaderFrequency>(kShaderFrequencyCount)` on
///         invalid input.
[[nodiscard]] inline shader::ShaderFrequency getFrequencyForGfxStage(const Stage stage) noexcept
{
    switch (stage)
    {
    case Stage::Vertex:
        return shader::ShaderFrequency::Vertex;
    case Stage::Pixel:
        return shader::ShaderFrequency::Pixel;
    case Stage::Geometry:
        return shader::ShaderFrequency::Geometry;
    case Stage::Mesh:
        return shader::ShaderFrequency::Mesh;
    case Stage::Task:
        return shader::ShaderFrequency::Amplification;
    default:
        GP_CHECKF(false, "Invalid graphics shader stage: %d", static_cast<gp::Int32>(stage));
        break;
    }

    return static_cast<shader::ShaderFrequency>(shader::kShaderFrequencyCount);
}

}   // namespace shader_stage

/// @brief Bindless descriptor set indices and uniform buffer slot partitioning constants for Vulkan pipelines.
namespace bindless
{

/// @brief Maximum uniform buffers allocated per shader stage in bindless layouts.
inline constexpr gp::UInt32 kMaxUniformBuffersPerStage = 16u;

/// @brief Total uniform buffers available across all stages in a bindless pipeline (guaranteed >= 32 in Vulkan 1.4).
inline constexpr gp::UInt32 kMaxUniformBuffersTotal = 32u;

/// @brief Fixed descriptor set slot assignments for bindless pipelines.
enum class DescriptorSet : gp::UInt32
{
    BindlessSamplerSet = 0u,
    BindlessResourceSet = 1u,

    BindlessStorageBufferSet = BindlessResourceSet,
    BindlessUniformBufferSet = BindlessResourceSet,
    BindlessStorageImageSet = BindlessResourceSet,
    BindlessSampledImageSet = BindlessResourceSet,
    BindlessStorageTexelBufferSet = BindlessResourceSet,
    BindlessUniformTexelBufferSet = BindlessResourceSet,
    BindlessAccelerationStructureSet = BindlessResourceSet,

    /// @brief Number of descriptor sets reserved for bindless samplers and resources.
    NumBindlessSets = 2u,

    /// @brief Descriptor set index reserved for single-use uniform buffers (such as globals).
    BindlessSingleUseUniformBufferSet = NumBindlessSets,

    /// @brief Total descriptor sets bound by a bindless pipeline layout.
    MaxNumSets = NumBindlessSets + 1u
};

/// @brief Returns the starting uniform buffer descriptor offset for `frequency` in a bindless pipeline.
/// @details Stages that execute alone or bind the most uniform buffers start at `0`. The second concurrent stage starts
///          at `kMaxUniformBuffersPerStage`, while the third stage returns `-1` to signal allocation backward from the
///          end of the table.
/// @param[in] frequency Target shader frequency.
/// @return Non-negative base offset, or `-1` for reverse-allocated third stages (`Geometry`, `Amplification`).
[[nodiscard]] inline gp::Int32 getOffsetForFrequency(const shader::ShaderFrequency frequency) noexcept
{
    switch (frequency)
    {
    case shader::ShaderFrequency::Pixel:
        return 0;

    case shader::ShaderFrequency::Vertex:
        return static_cast<gp::Int32>(kMaxUniformBuffersPerStage);
    case shader::ShaderFrequency::Geometry:
        return -1;

    case shader::ShaderFrequency::Mesh:
        return static_cast<gp::Int32>(kMaxUniformBuffersPerStage);
    case shader::ShaderFrequency::Amplification:
        return -1;

    case shader::ShaderFrequency::Compute:
        return 0;

    case shader::ShaderFrequency::RayGen:
    case shader::ShaderFrequency::RayMiss:
    case shader::ShaderFrequency::RayHitGroup:
    case shader::ShaderFrequency::RayCallable:
        return 0;

    default:
        GP_CHECKF(false, "Invalid shader frequency %d", static_cast<gp::Int32>(frequency));
        break;
    }

    return 0;
}

}   // namespace bindless

/// @brief Zero-initializes a Vulkan API struct in place and writes its `sType` tag.
/// @details Keeps `sType` at offset `0` and zeroes every remaining byte (including struct padding) without requiring
///          callers to manually clear `pNext` or individual fields.
/// @tparam InStructType Standard-layout Vulkan struct with `sType` at byte offset `0`.
/// @tparam InStructureTypeEnum `VkStructureType` or compatible 32-bit integer tag.
/// @param[out] outStruct Destination Vulkan struct to initialize.
/// @param[in] structureType `VkStructureType` enumerator written into `outStruct.sType`.
template <typename InStructType, typename InStructureTypeEnum>
inline void zeroVulkanStruct(InStructType& outStruct, const InStructureTypeEnum structureType) noexcept
{
    static_assert(
        !gp::concepts::IsPointer<InStructType>, "zeroVulkanStruct() expects a struct reference, not a pointer."
    );
    static_assert(offsetof(InStructType, sType) == 0u, "Vulkan struct must place sType at offset 0.");
    static_assert(sizeof(outStruct.sType) == sizeof(gp::Int32), "Vulkan sType must be 32 bits wide.");

    outStruct.sType = static_cast<decltype(outStruct.sType)>(structureType);
    gp::memory::zeroMemory(
        reinterpret_cast<gp::UInt8*>(&outStruct) + sizeof(outStruct.sType),
        sizeof(InStructType) - sizeof(outStruct.sType)
    );
}

/// @brief Constructs and returns a zero-initialized Vulkan API struct tagged with `structureType`.
/// @tparam InStructType Standard-layout Vulkan struct with `sType` at byte offset `0`.
/// @tparam InStructureTypeEnum `VkStructureType` or compatible 32-bit integer tag.
/// @param[in] structureType `VkStructureType` enumerator written into `sType`.
/// @return Fully zero-initialized `InStructType` with `sType` set.
template <typename InStructType, typename InStructureTypeEnum>
[[nodiscard]] inline InStructType zeroVulkanStruct(const InStructureTypeEnum structureType) noexcept
{
    InStructType result;
    zeroVulkanStruct(result, structureType);
    return result;
}

/// @brief Prepends `added` to the head of `existing.pNext` inside a Vulkan structure extension chain.
/// @tparam InExistingChainType Head Vulkan struct containing a `pNext` pointer field.
/// @tparam InNewStructType Extension Vulkan struct containing a `pNext` pointer field.
/// @param[in,out] existing Existing head struct whose `pNext` will point to `added`.
/// @param[in,out] added Extension struct inserted immediately after `existing`.
template <typename InExistingChainType, typename InNewStructType>
inline void addToPNext(InExistingChainType& existing, InNewStructType& added) noexcept
{
    using AddedNextPtr = decltype(added.pNext);
    using ExistingNextPtr = decltype(existing.pNext);

    if constexpr (gp::concepts::IsConst<std::remove_pointer_t<AddedNextPtr>>)
    {
        added.pNext = static_cast<const void*>(existing.pNext);
    }
    else
    {
        added.pNext = const_cast<void*>(static_cast<const void*>(existing.pNext));
    }

    if constexpr (gp::concepts::IsConst<std::remove_pointer_t<ExistingNextPtr>>)
    {
        existing.pNext = static_cast<const void*>(&added);
    }
    else
    {
        existing.pNext = static_cast<void*>(&added);
    }
}

}   // namespace gp::rhi::vulkan
