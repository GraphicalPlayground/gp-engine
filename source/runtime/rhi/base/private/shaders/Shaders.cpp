// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "memory/MemoryOrder.hpp"
#include "pipeline/ComputePipelineState.hpp"
#include "shaders/AmplificationShader.hpp"
#include "shaders/ComputeShader.hpp"
#include "shaders/GeometryShader.hpp"
#include "shaders/GraphicsShader.hpp"
#include "shaders/MeshShader.hpp"
#include "shaders/PixelShader.hpp"
#include "shaders/RayCallableShader.hpp"
#include "shaders/RayGenShader.hpp"
#include "shaders/RayHitGroupShader.hpp"
#include "shaders/RayMissShader.hpp"
#include "shaders/RayTracingShader.hpp"
#include "shaders/Shader.hpp"
#include "shaders/ShaderData.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/VertexShader.hpp"
#include "shaders/WorkGraphComputeNodeShader.hpp"
#include "shaders/WorkGraphRootShader.hpp"
#include "shaders/WorkGraphShader.hpp"

namespace gp::rhi::shader
{

ShaderData::ShaderData() noexcept = default;

ShaderData::~ShaderData() noexcept = default;

Shader::Shader(const resource::ResourceType resourceType, const ShaderFrequency frequency) noexcept
    : resource::Resource(resourceType)
    , ShaderData()
    , m_frequency(frequency)
{}

Shader::~Shader() noexcept
{
    GP_CHECKF(
        m_inUseByPSOCompilation.load(gp::memory::MemoryOrder::Acquire) == 0,
        "Shader is being destroyed while still referenced by an in-flight PSO compilation."
    );
}

bool Shader::isExcludedFromSpecialization(
    const gp::UInt32 bindPoint, const gp::UInt32 specializationHash
) const noexcept
{
    if (m_hasSpecializationConstants == 0u)
    {
        return false;
    }

    for (const ShaderSpecializationExclusion& entry: m_specializationExclusions)
    {
        if (entry.bindPoint == bindPoint)
        {
            for (const gp::UInt32 excludedHash: entry.specializationHashes)
            {
                if (excludedHash == specializationHash)
                {
                    return true;
                }
            }
            return false;
        }
    }

    return false;
}

void Shader::setInUseByPSOCompilation(const bool inUse) noexcept
{
    if (inUse)
    {
        m_inUseByPSOCompilation.fetchAdd(1, gp::memory::MemoryOrder::AcqRel);
    }
    else
    {
        const gp::Int32 previousCount = m_inUseByPSOCompilation.fetchSub(1, gp::memory::MemoryOrder::AcqRel);
        GP_CHECKF(previousCount > 0, "Shader PSO compilation use count underflowed.");
    }
}

GraphicsShader::GraphicsShader(const resource::ResourceType resourceType, const ShaderFrequency frequency) noexcept
    : Shader(resourceType, frequency)
{
    GP_CHECKF(isValidGraphicsFrequency(frequency), "GraphicsShader requires a graphics ShaderFrequency.");
}

GraphicsShader::~GraphicsShader() noexcept = default;

VertexShader::VertexShader() noexcept
    : GraphicsShader(resource::ResourceType::VertexShader, ShaderFrequency::Vertex)
{}

VertexShader::~VertexShader() noexcept = default;

MeshShader::MeshShader() noexcept
    : GraphicsShader(resource::ResourceType::MeshShader, ShaderFrequency::Mesh)
{}

MeshShader::~MeshShader() noexcept = default;

AmplificationShader::AmplificationShader() noexcept
    : GraphicsShader(resource::ResourceType::AmplificationShader, ShaderFrequency::Amplification)
{}

AmplificationShader::~AmplificationShader() noexcept = default;

PixelShader::PixelShader() noexcept
    : GraphicsShader(resource::ResourceType::PixelShader, ShaderFrequency::Pixel)
{}

PixelShader::~PixelShader() noexcept = default;

GeometryShader::GeometryShader() noexcept
    : GraphicsShader(resource::ResourceType::GeometryShader, ShaderFrequency::Geometry)
{}

GeometryShader::~GeometryShader() noexcept = default;

RayTracingShader::RayTracingShader(const ShaderFrequency frequency) noexcept
    : Shader(resource::ResourceType::RayTracingShader, frequency)
{
    GP_CHECKF(isRayTracingShaderFrequency(frequency), "RayTracingShader requires a ray tracing ShaderFrequency.");
}

RayTracingShader::~RayTracingShader() noexcept = default;

RayGenShader::RayGenShader() noexcept
    : RayTracingShader(ShaderFrequency::RayGen)
{}

RayGenShader::~RayGenShader() noexcept = default;

RayMissShader::RayMissShader() noexcept
    : RayTracingShader(ShaderFrequency::RayMiss)
{}

RayMissShader::~RayMissShader() noexcept = default;

RayCallableShader::RayCallableShader() noexcept
    : RayTracingShader(ShaderFrequency::RayCallable)
{}

RayCallableShader::~RayCallableShader() noexcept = default;

RayHitGroupShader::RayHitGroupShader() noexcept
    : RayTracingShader(ShaderFrequency::RayHitGroup)
{}

RayHitGroupShader::~RayHitGroupShader() noexcept = default;

ComputeShader::ComputeShader() noexcept
    : Shader(resource::ResourceType::ComputeShader, ShaderFrequency::Compute)
{}

ComputeShader::~ComputeShader() noexcept = default;

void ComputeShader::updateStats(const gp::UInt64 currentFrame) noexcept
{
    pipeline::PipelineStateStats::updateStats(m_stats, currentFrame);
}

WorkGraphShader::WorkGraphShader(const ShaderFrequency frequency) noexcept
    : Shader(resource::ResourceType::WorkGraphShader, frequency)
{
    GP_CHECKF(isWorkGraphShaderFrequency(frequency), "WorkGraphShader requires a work graph ShaderFrequency.");
}

WorkGraphShader::~WorkGraphShader() noexcept = default;

WorkGraphRootShader::WorkGraphRootShader() noexcept
    : WorkGraphShader(ShaderFrequency::WorkGraphRoot)
{}

WorkGraphRootShader::~WorkGraphRootShader() noexcept = default;

WorkGraphComputeNodeShader::WorkGraphComputeNodeShader() noexcept
    : WorkGraphShader(ShaderFrequency::WorkGraphComputeNode)
{}

WorkGraphComputeNodeShader::~WorkGraphComputeNodeShader() noexcept = default;

}   // namespace gp::rhi::shader
