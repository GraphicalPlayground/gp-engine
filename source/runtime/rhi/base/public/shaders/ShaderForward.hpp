// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "hashing/HashingForward.hpp"
#include "memory/MemoryForward.hpp"

namespace gp::rhi::shader
{

/// @brief Identifies a target shader compilation platform.
enum class ShaderPlatform : gp::UInt16;

/// @brief Programmable pipeline stage represented by a shader object.
enum class ShaderFrequency : gp::UInt8;

/// @brief Reflected shader resource binding kind extracted from compiled shader metadata.
enum class ShaderCodeResourceBindingType : gp::UInt8;

/// @brief Member types permitted inside a reflected uniform buffer parameter layout.
enum class UniformBufferBaseType : gp::UInt8;

/// @brief Binding models permitted for a uniform buffer layout.
enum class UniformBufferBindingFlags : gp::UInt8;

/// @brief Execution path used to dispatch shader bundles.
enum class ShaderBundleMethod : gp::UInt8;

/// @brief 128-bit content hash identifying compiled shader bytecode.
using ShaderHash = gp::hashing::Hash128;

/// @brief Expected buffer stride at a reflected shader bind point for runtime validation.
struct ShaderCodeValidationStride;

/// @brief Expected resource binding kind at a reflected shader bind point for runtime validation.
struct ShaderCodeValidationType;

/// @brief Expected uniform buffer byte size at a reflected bind point for runtime validation.
struct ShaderCodeValidationUBSize;

/// @brief Records specialization constant hashes excluded for a shader bind point.
struct ShaderSpecializationExclusion;

/// @brief Binding map linking uniform buffer member slots to hardware shader resource registers.
struct ShaderResourceTable;

/// @brief Input descriptor carrying shader bytecode and optional library ownership for shader creation.
struct CreateShaderDesc;

/// @brief Packed 8-byte constant-data parameter span inside a batched shader parameter stream.
struct ShaderParameter;

/// @brief Resource binding entry inside a batched shader parameter stream.
struct ShaderParameterResource;

/// @brief Resource unbind entry clearing a shader SRV or UAV slot.
struct ShaderParameterUnbind;

/// @brief Page-backed linear allocator backing batched shader parameter arrays on a command list.
class BatchedShaderParametersAllocator;

/// @brief Batched shader parameter collection recorded before dispatching a single RHI parameter update.
struct BatchedShaderParameters;

/// @brief Batched list of SRV/UAV slot unbinds recorded before dispatching a single RHI unbind call.
struct BatchedShaderUnbinds;

/// @brief Inline fixed-capacity set of globally bound static uniform buffers.
class UniformBufferStaticBindings;

/// @brief Collection of compiled shaders loaded from a shared binary archive.
class ShaderLibrary;

/// @brief Reflected resource table and static uniform buffer slot metadata shared by shader objects.
class ShaderData;

/// @brief Reference-counted base resource for all compiled programmable shader stages.
class Shader;

/// @brief Shared base class for graphics pipeline shader stages.
class GraphicsShader;

/// @brief Compiled vertex shader stage resource.
class VertexShader;

/// @brief Compiled mesh shader stage resource.
class MeshShader;

/// @brief Compiled amplification (task) shader stage resource.
class AmplificationShader;

/// @brief Compiled pixel (fragment) shader stage resource.
class PixelShader;

/// @brief Compiled geometry shader stage resource.
class GeometryShader;

/// @brief Shared base class for hardware ray tracing shader stages.
class RayTracingShader;

/// @brief Compiled ray generation shader stage resource.
class RayGenShader;

/// @brief Compiled ray miss shader stage resource.
class RayMissShader;

/// @brief Compiled ray callable shader stage resource.
class RayCallableShader;

/// @brief Compiled ray hit group shader stage resource.
class RayHitGroupShader;

/// @brief Compiled compute shader stage resource.
class ComputeShader;

/// @brief Shared base class for GPU work graph shader stages.
class WorkGraphShader;

/// @brief Compiled work graph root shader stage resource.
class WorkGraphRootShader;

/// @brief Compiled work graph compute node shader stage resource.
class WorkGraphComputeNodeShader;

/// @brief Intrusive smart pointer handle to a `ShaderLibrary`.
using ShaderLibraryRef = gp::RefCountedPtr<ShaderLibrary, gp::DefaultRefCountPolicy<ShaderLibrary>>;

/// @brief Intrusive smart pointer handle to a `Shader`.
using ShaderRef = gp::RefCountedPtr<Shader, gp::DefaultRefCountPolicy<Shader>>;

/// @brief Intrusive smart pointer handle to a `GraphicsShader`.
using GraphicsShaderRef = gp::RefCountedPtr<GraphicsShader, gp::DefaultRefCountPolicy<GraphicsShader>>;

/// @brief Intrusive smart pointer handle to a `VertexShader`.
using VertexShaderRef = gp::RefCountedPtr<VertexShader, gp::DefaultRefCountPolicy<VertexShader>>;

/// @brief Intrusive smart pointer handle to a `MeshShader`.
using MeshShaderRef = gp::RefCountedPtr<MeshShader, gp::DefaultRefCountPolicy<MeshShader>>;

/// @brief Intrusive smart pointer handle to an `AmplificationShader`.
using AmplificationShaderRef = gp::RefCountedPtr<AmplificationShader, gp::DefaultRefCountPolicy<AmplificationShader>>;

/// @brief Intrusive smart pointer handle to a `PixelShader`.
using PixelShaderRef = gp::RefCountedPtr<PixelShader, gp::DefaultRefCountPolicy<PixelShader>>;

/// @brief Intrusive smart pointer handle to a `GeometryShader`.
using GeometryShaderRef = gp::RefCountedPtr<GeometryShader, gp::DefaultRefCountPolicy<GeometryShader>>;

/// @brief Intrusive smart pointer handle to a `RayTracingShader`.
using RayTracingShaderRef = gp::RefCountedPtr<RayTracingShader, gp::DefaultRefCountPolicy<RayTracingShader>>;

/// @brief Intrusive smart pointer handle to a `RayGenShader`.
using RayGenShaderRef = gp::RefCountedPtr<RayGenShader, gp::DefaultRefCountPolicy<RayGenShader>>;

/// @brief Intrusive smart pointer handle to a `RayMissShader`.
using RayMissShaderRef = gp::RefCountedPtr<RayMissShader, gp::DefaultRefCountPolicy<RayMissShader>>;

/// @brief Intrusive smart pointer handle to a `RayCallableShader`.
using RayCallableShaderRef = gp::RefCountedPtr<RayCallableShader, gp::DefaultRefCountPolicy<RayCallableShader>>;

/// @brief Intrusive smart pointer handle to a `RayHitGroupShader`.
using RayHitGroupShaderRef = gp::RefCountedPtr<RayHitGroupShader, gp::DefaultRefCountPolicy<RayHitGroupShader>>;

/// @brief Intrusive smart pointer handle to a `ComputeShader`.
using ComputeShaderRef = gp::RefCountedPtr<ComputeShader, gp::DefaultRefCountPolicy<ComputeShader>>;

/// @brief Intrusive smart pointer handle to a `WorkGraphShader`.
using WorkGraphShaderRef = gp::RefCountedPtr<WorkGraphShader, gp::DefaultRefCountPolicy<WorkGraphShader>>;

/// @brief Intrusive smart pointer handle to a `WorkGraphRootShader`.
using WorkGraphRootShaderRef = gp::RefCountedPtr<WorkGraphRootShader, gp::DefaultRefCountPolicy<WorkGraphRootShader>>;

/// @brief Intrusive smart pointer handle to a `WorkGraphComputeNodeShader`.
using WorkGraphComputeNodeShaderRef =
    gp::RefCountedPtr<WorkGraphComputeNodeShader, gp::DefaultRefCountPolicy<WorkGraphComputeNodeShader>>;

}   // namespace gp::rhi::shader
