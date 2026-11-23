// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "memory/MemoryOrder.hpp"
#include "pipeline/BoundShaderState.hpp"
#include "pipeline/ComputePipelineState.hpp"
#include "pipeline/GraphicsPipelineState.hpp"
#include "pipeline/VertexDeclaration.hpp"
#include "shaders/AmplificationShader.hpp"
#include "shaders/ComputeShader.hpp"
#include "shaders/GeometryShader.hpp"
#include "shaders/MeshShader.hpp"
#include "shaders/PixelShader.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/VertexShader.hpp"
#include "shaders/WorkGraphShader.hpp"

namespace gp::rhi::pipeline
{

BoundShaderStateInput::BoundShaderStateInput(
    VertexDeclaration* const inVertexDeclaration,
    shader::VertexShader* const inVertexShader,
    shader::PixelShader* const inPixelShader,
    shader::GeometryShader* const inGeometryShader
) noexcept
    : m_vertexDeclaration(inVertexDeclaration)
    , m_vertexShader(inVertexShader)
    , m_pixelShader(inPixelShader)
    , m_geometryShader(inGeometryShader)
{}

BoundShaderStateInput::BoundShaderStateInput(
    shader::MeshShader* const inMeshShader,
    shader::AmplificationShader* const inAmplificationShader,
    shader::PixelShader* const inPixelShader
) noexcept
    : m_pixelShader(inPixelShader)
    , m_meshShader(inMeshShader)
    , m_amplificationShader(inAmplificationShader)
{}

void BoundShaderStateInput::addRefResources() const noexcept
{
    if (m_meshShader != nullptr)
    {
        GP_CHECKF(m_vertexDeclaration == nullptr, "Mesh pipeline must not bind a VertexDeclaration.");
        GP_CHECKF(m_vertexShader == nullptr, "Mesh pipeline must not bind a VertexShader.");
        GP_CHECKF(m_workGraphShader == nullptr, "Mesh pipeline must not bind a WorkGraphShader.");
        m_meshShader->addRef();

        if (m_amplificationShader != nullptr)
        {
            m_amplificationShader->addRef();
        }
    }
    else if (m_workGraphShader != nullptr)
    {
        GP_CHECKF(m_vertexDeclaration == nullptr, "Work graph pipeline must not bind a VertexDeclaration.");
        GP_CHECKF(m_vertexShader == nullptr, "Work graph pipeline must not bind a VertexShader.");
        GP_CHECKF(m_meshShader == nullptr, "Work graph pipeline must not bind a MeshShader.");
        m_workGraphShader->addRef();
    }
    else
    {
        GP_CHECKF(m_vertexDeclaration != nullptr, "Vertex pipeline requires a non-null VertexDeclaration.");
        m_vertexDeclaration->addRef();

        GP_CHECKF(m_vertexShader != nullptr, "Vertex pipeline requires a non-null VertexShader.");
        m_vertexShader->addRef();
    }

    if (m_pixelShader != nullptr)
    {
        m_pixelShader->addRef();
    }

    if (m_geometryShader != nullptr)
    {
        m_geometryShader->addRef();
    }
}

void BoundShaderStateInput::releaseResources() const noexcept
{
    if (m_meshShader != nullptr)
    {
        GP_CHECKF(m_vertexDeclaration == nullptr, "Mesh pipeline must not bind a VertexDeclaration.");
        GP_CHECKF(m_vertexShader == nullptr, "Mesh pipeline must not bind a VertexShader.");
        GP_CHECKF(m_workGraphShader == nullptr, "Mesh pipeline must not bind a WorkGraphShader.");
        m_meshShader->release();

        if (m_amplificationShader != nullptr)
        {
            m_amplificationShader->release();
        }
    }
    else if (m_workGraphShader != nullptr)
    {
        GP_CHECKF(m_vertexDeclaration == nullptr, "Work graph pipeline must not bind a VertexDeclaration.");
        GP_CHECKF(m_vertexShader == nullptr, "Work graph pipeline must not bind a VertexShader.");
        GP_CHECKF(m_meshShader == nullptr, "Work graph pipeline must not bind a MeshShader.");
        m_workGraphShader->release();
    }
    else
    {
        GP_CHECKF(m_vertexDeclaration != nullptr, "Vertex pipeline requires a non-null VertexDeclaration.");
        m_vertexDeclaration->release();

        GP_CHECKF(m_vertexShader != nullptr, "Vertex pipeline requires a non-null VertexShader.");
        m_vertexShader->release();
    }

    if (m_pixelShader != nullptr)
    {
        m_pixelShader->release();
    }

    if (m_geometryShader != nullptr)
    {
        m_geometryShader->release();
    }
}

shader::Shader* BoundShaderStateInput::getShader(const shader::ShaderFrequency frequency) const noexcept
{
    switch (frequency)
    {
    case shader::ShaderFrequency::Vertex:
        return m_vertexShader;

    case shader::ShaderFrequency::Mesh:
        return m_meshShader;

    case shader::ShaderFrequency::Amplification:
        return m_amplificationShader;

    case shader::ShaderFrequency::Pixel:
        return m_pixelShader;

    case shader::ShaderFrequency::Geometry:
        return m_geometryShader;

    case shader::ShaderFrequency::WorkGraphRoot:
    case shader::ShaderFrequency::WorkGraphComputeNode:
        return m_workGraphShader;

    default:
        return nullptr;
    }
}

BoundShaderState::BoundShaderState() noexcept
    : resource::Resource(resource::ResourceType::BoundShaderState)
{}

BoundShaderState::~BoundShaderState() noexcept = default;

GraphicsPipelineState::GraphicsPipelineState() noexcept
    : resource::Resource(resource::ResourceType::GraphicsPipelineState)
{}

GraphicsPipelineState::~GraphicsPipelineState() noexcept = default;

void PipelineStateStats::updateStats(const gp::UInt64 currentFrame) noexcept
{
    gp::UInt64 expectedFirst = firstFrameUsed.load(gp::memory::MemoryOrder::Relaxed);
    while ((expectedFirst == kInvalidFrame || currentFrame < expectedFirst) &&
           !firstFrameUsed.compareExchangeWeak(
               expectedFirst, currentFrame, gp::memory::MemoryOrder::Relaxed, gp::memory::MemoryOrder::Relaxed
           ))
    {
    }

    gp::UInt64 expectedLast = lastFrameUsed.load(gp::memory::MemoryOrder::Relaxed);
    while (currentFrame > expectedLast &&
           !lastFrameUsed.compareExchangeWeak(
               expectedLast, currentFrame, gp::memory::MemoryOrder::Relaxed, gp::memory::MemoryOrder::Relaxed
           ))
    {
    }

    totalBindCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
}

void PipelineStateStats::updateStats(PipelineStateStats* const stats, const gp::UInt64 currentFrame) noexcept
{
    if (stats != nullptr)
    {
        stats->updateStats(currentFrame);
    }
}

ComputePipelineState::ComputePipelineState(shader::ComputeShader* const computeShader) noexcept
    : resource::Resource(resource::ResourceType::ComputePipelineState)
    , m_computeShader(computeShader)
{
    GP_CHECKF(computeShader != nullptr, "ComputePipelineState requires a non-null ComputeShader.");
}

ComputePipelineState::~ComputePipelineState() noexcept = default;

void ComputePipelineState::markUsed() noexcept
{
    m_isUsed.store(true, gp::memory::MemoryOrder::Release);
}

bool ComputePipelineState::isUsed() const noexcept
{
    return m_isUsed.load(gp::memory::MemoryOrder::Acquire);
}

}   // namespace gp::rhi::pipeline
