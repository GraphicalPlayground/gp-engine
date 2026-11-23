// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "core/ResourceEnums.hpp"
#include "pipeline/BlendState.hpp"
#include "pipeline/BlendStateDesc.hpp"
#include "pipeline/DepthStencilState.hpp"
#include "pipeline/DepthStencilStateDesc.hpp"
#include "pipeline/RasterizerState.hpp"
#include "pipeline/RasterizerStateDesc.hpp"
#include "pipeline/SamplerState.hpp"
#include "pipeline/VertexDeclaration.hpp"
#include "views/DescriptorHandle.hpp"

namespace gp::rhi::pipeline
{

SamplerState::SamplerState() noexcept
    : resource::Resource(resource::ResourceType::SamplerState)
{}

SamplerState::~SamplerState() noexcept = default;

bool SamplerState::isImmutable() const noexcept
{
    return false;
}

view::DescriptorHandle SamplerState::getBindlessHandle() const noexcept
{
    return view::DescriptorHandle{};
}

RasterizerState::RasterizerState() noexcept
    : resource::Resource(resource::ResourceType::RasterizerState)
{}

RasterizerState::~RasterizerState() noexcept = default;

bool RasterizerState::getInitializer(RasterizerStateDesc& outDesc) const noexcept
{
    static_cast<void>(outDesc);
    return false;
}

DepthStencilState::DepthStencilState() noexcept
    : resource::Resource(resource::ResourceType::DepthStencilState)
{}

DepthStencilState::~DepthStencilState() noexcept = default;

bool DepthStencilState::getInitializer(DepthStencilStateDesc& outDesc) const noexcept
{
    static_cast<void>(outDesc);
    return false;
}

BlendState::BlendState() noexcept
    : resource::Resource(resource::ResourceType::BlendState)
{}

BlendState::~BlendState() noexcept = default;

bool BlendState::getInitializer(BlendStateDesc& outDesc) const noexcept
{
    static_cast<void>(outDesc);
    return false;
}

VertexDeclaration::VertexDeclaration() noexcept
    : resource::Resource(resource::ResourceType::VertexDeclaration)
{}

VertexDeclaration::~VertexDeclaration() noexcept = default;

bool VertexDeclaration::getInitializer(VertexDeclarationElementList& outElements) const noexcept
{
    static_cast<void>(outElements);
    return false;
}

gp::UInt32 VertexDeclaration::getPrecachePSOHash() const noexcept
{
    return 0u;
}

}   // namespace gp::rhi::pipeline
