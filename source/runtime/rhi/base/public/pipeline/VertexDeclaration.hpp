// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/Resource.hpp"
#include "CoreMinimal.hpp"
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "pipeline/VertexDeclarationDesc.hpp"
#include "RHIBase.hpp"

namespace gp::rhi::pipeline
{

/// @brief Immutable GPU vertex input layout resource mapping vertex buffer streams to shader attributes.
class GP_RHI_API VertexDeclaration : public resource::Resource
{
public:
    /// @brief Constructs a vertex declaration tagged with `ResourceType::VertexDeclaration`.
    VertexDeclaration() noexcept;

    VertexDeclaration(const VertexDeclaration&) = delete;
    VertexDeclaration& operator=(const VertexDeclaration&) = delete;
    VertexDeclaration(VertexDeclaration&&) = delete;
    VertexDeclaration& operator=(VertexDeclaration&&) = delete;

public:
    /// @brief Populates `outElements` with the vertex attribute list used to create this layout.
    /// @param[out] outElements Receives the creation element list when supported by the concrete backend.
    /// @return True if `outElements` was populated; false if the backend does not retain creation parameters.
    [[nodiscard]] virtual bool getInitializer(VertexDeclarationElementList& outElements) const noexcept;

    /// @brief Returns a stable 32-bit hash used to key precached graphics pipeline state objects.
    /// @return Backend-computed hash, or `0` if precache hashing is not implemented.
    [[nodiscard]] virtual gp::UInt32 getPrecachePSOHash() const noexcept;

protected:
    /// @copydoc resource::Resource::~Resource()
    virtual ~VertexDeclaration() noexcept override;
};

}   // namespace gp::rhi::pipeline
