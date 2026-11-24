// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi
{

/// @brief Defines the RHI feature level tiers supported by the engine.
enum class FeatureLevel : gp::Int32;

/// @brief Graphics backend API implemented by the active RHI.
enum class InterfaceType : gp::UInt8;

/// @brief High-level rendering pipeline architecture selected for a target feature level.
enum class ShadingPath : gp::UInt8;

/// @brief Runtime availability of an RHI hardware or driver feature.
enum class FeatureSupport : gp::UInt8;

/// @brief Hardware support level for static shader binding layouts.
enum class StaticShaderBindingLayoutSupport : gp::UInt8;

/// @brief PCI vendor IDs for known GPU manufacturers and software rasterizers.
enum class GpuVendorId : gp::UInt32;

/// @brief Global RHI hardware capability, limit, and memory telemetry table.
struct RHIGlobals;

/// @brief Arguments passed to `DynamicRHI::finalizeContexts()`.
struct FinalizeContextsArgs;

/// @brief Finalized platform command lists returned by `DynamicRHI::finalizeContexts()`.
struct FinalizeContextsResult;

/// @brief Arguments passed to `DynamicRHI::submitCommandLists()`.
struct SubmitCommandListsArgs;

/// @brief Frame boundary metadata passed to `DynamicRHI::endFrame()`.
struct EndFrameArgs;

/// @brief Display output resolution and refresh rate entry.
struct ScreenResolution;

/// @brief Configuration descriptor controlling RHI module discovery, feature level selection, and bootstrap.
struct RHIBootstrapDesc;

/// @brief Result produced by `selectDynamicRHIModule()` identifying the chosen backend module and feature level.
struct RHISelectionResult;

/// @brief Backend interface that owns GPU device creation, resource factories, and deferred teardown.
class DynamicRHI;

/// @brief Intermediate `DynamicRHI` base providing fallback PSO creation for backends without native PSOs.
class DynamicRHIPSOFallback;

/// @brief Default `RenderQueryPool` backed by `DynamicRHI::createRenderQuery()` and a free-list vector.
class DefaultRenderQueryPool;

/// @brief Generic CPU-backed `StagingBuffer` fallback for backends without custom staging readback objects.
class GenericStagingBuffer;

/// @brief RAII scope acquiring and releasing backend thread ownership on the active `DynamicRHI`.
class ScopedRHIThreadOwnership;

/// @brief Module interface implemented by loadable RHI backend plugins (`rhi/null`, `rhi/vulkan`, `rhi/d3d12`, etc.).
class IDynamicRHIModule;

}   // namespace gp::rhi
