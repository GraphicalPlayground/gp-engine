// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/Resource.hpp"
#include "CoreMinimal.hpp"
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "RHIBase.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/ShaderForward.hpp"
#include "shaders/ShaderTypes.hpp"

namespace gp::rhi::pipeline
{

/// @brief Non-owning bundle of graphics shader stages and vertex input layout used to create bound shader states.
/// @details Supports both traditional vertex pipelines (`VertexDeclaration`, `VertexShader`, optional `GeometryShader`,
///          optional `PixelShader`) and mesh-shading pipelines (`MeshShader`, optional `AmplificationShader`, optional
///          `PixelShader`). Call `addRefResources()` and `releaseResources()` when queuing descriptors for asynchronous
///          pipeline state compilation so referenced shaders stay alive until compilation finishes.
class GP_RHI_API BoundShaderStateInput
{
public:
    /// @brief Constructs an empty bound shader state input with all stage pointers set to `nullptr`.
    BoundShaderStateInput() noexcept = default;

    /// @brief Constructs a vertex-pipeline bound shader state input.
    /// @param[in] inVertexDeclaration Non-null vertex input layout resource required by vertex pipelines.
    /// @param[in] inVertexShader Non-null vertex shader stage.
    /// @param[in] inPixelShader Optional pixel shader stage (null for depth-only passes).
    /// @param[in] inGeometryShader Optional geometry shader stage.
    BoundShaderStateInput(
        VertexDeclaration* inVertexDeclaration,
        shader::VertexShader* inVertexShader,
        shader::PixelShader* inPixelShader,
        shader::GeometryShader* inGeometryShader = nullptr
    ) noexcept;

    /// @brief Constructs a mesh-pipeline bound shader state input.
    /// @param[in] inMeshShader Mesh shader stage.
    /// @param[in] inAmplificationShader Optional amplification (task) shader stage.
    /// @param[in] inPixelShader Optional pixel shader stage (null for depth-only passes).
    BoundShaderStateInput(
        shader::MeshShader* inMeshShader,
        shader::AmplificationShader* inAmplificationShader,
        shader::PixelShader* inPixelShader
    ) noexcept;

    BoundShaderStateInput(const BoundShaderStateInput&) noexcept = default;
    BoundShaderStateInput& operator=(const BoundShaderStateInput&) noexcept = default;
    BoundShaderStateInput(BoundShaderStateInput&&) noexcept = default;
    BoundShaderStateInput& operator=(BoundShaderStateInput&&) noexcept = default;
    ~BoundShaderStateInput() noexcept = default;

public:
    /// @brief Increments reference counts on every active shader and vertex declaration referenced by this input.
    /// @details Must be paired with a matching call to `releaseResources()` once asynchronous consumers finish.
    void addRefResources() const noexcept;

    /// @brief Decrements reference counts previously acquired by `addRefResources()`.
    void releaseResources() const noexcept;

    /// @brief Returns the bound vertex declaration, or `nullptr` if unset or using a mesh pipeline.
    /// @return Non-owning pointer to `VertexDeclaration`, or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT VertexDeclaration* getVertexDeclaration() const noexcept
    {
        return m_vertexDeclaration;
    }

    /// @brief Sets the vertex declaration for a vertex-pipeline configuration.
    /// @param[in] inVertexDeclaration Vertex declaration pointer, or `nullptr`.
    GP_FORCEINLINE_HINT void setVertexDeclaration(VertexDeclaration* const inVertexDeclaration) noexcept
    {
        m_vertexDeclaration = inVertexDeclaration;
    }

    /// @brief Returns the bound vertex shader, or `nullptr`.
    /// @return Non-owning pointer to `shader::VertexShader`, or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT shader::VertexShader* getVertexShader() const noexcept
    {
        return m_vertexShader;
    }

    /// @brief Sets the vertex shader stage.
    /// @param[in] inVertexShader Vertex shader pointer, or `nullptr`.
    GP_FORCEINLINE_HINT void setVertexShader(shader::VertexShader* const inVertexShader) noexcept
    {
        m_vertexShader = inVertexShader;
    }

    /// @brief Returns the bound pixel shader, or `nullptr`.
    /// @return Non-owning pointer to `shader::PixelShader`, or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT shader::PixelShader* getPixelShader() const noexcept
    {
        return m_pixelShader;
    }

    /// @brief Sets the pixel shader stage.
    /// @param[in] inPixelShader Pixel shader pointer, or `nullptr`.
    GP_FORCEINLINE_HINT void setPixelShader(shader::PixelShader* const inPixelShader) noexcept
    {
        m_pixelShader = inPixelShader;
    }

    /// @brief Returns the bound mesh shader, or `nullptr`.
    /// @return Non-owning pointer to `shader::MeshShader`, or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT shader::MeshShader* getMeshShader() const noexcept
    {
        return m_meshShader;
    }

    /// @brief Sets the mesh shader stage.
    /// @param[in] inMeshShader Mesh shader pointer, or `nullptr`.
    GP_FORCEINLINE_HINT void setMeshShader(shader::MeshShader* const inMeshShader) noexcept
    {
        m_meshShader = inMeshShader;
    }

    /// @brief Returns the bound amplification shader, or `nullptr`.
    /// @return Non-owning pointer to `shader::AmplificationShader`, or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT shader::AmplificationShader* getAmplificationShader() const noexcept
    {
        return m_amplificationShader;
    }

    /// @brief Sets the amplification shader stage.
    /// @param[in] inAmplificationShader Amplification shader pointer, or `nullptr`.
    GP_FORCEINLINE_HINT void setAmplificationShader(shader::AmplificationShader* const inAmplificationShader) noexcept
    {
        m_amplificationShader = inAmplificationShader;
    }

    /// @brief Returns the bound geometry shader, or `nullptr`.
    /// @return Non-owning pointer to `shader::GeometryShader`, or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT shader::GeometryShader* getGeometryShader() const noexcept
    {
        return m_geometryShader;
    }

    /// @brief Sets the geometry shader stage.
    /// @param[in] inGeometryShader Geometry shader pointer, or `nullptr`.
    GP_FORCEINLINE_HINT void setGeometryShader(shader::GeometryShader* const inGeometryShader) noexcept
    {
        m_geometryShader = inGeometryShader;
    }

    /// @brief Returns the bound work graph shader, or `nullptr`.
    /// @return Non-owning pointer to `shader::WorkGraphShader`, or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT shader::WorkGraphShader* getWorkGraphShader() const noexcept
    {
        return m_workGraphShader;
    }

    /// @brief Sets the work graph shader stage.
    /// @param[in] inWorkGraphShader Work graph shader pointer, or `nullptr`.
    GP_FORCEINLINE_HINT void setWorkGraphShader(shader::WorkGraphShader* const inWorkGraphShader) noexcept
    {
        m_workGraphShader = inWorkGraphShader;
    }

    /// @brief Reports whether this input targets the mesh-shading geometry pipeline.
    /// @return True when `m_meshShader` is non-null.
    [[nodiscard]] GP_FORCEINLINE_HINT bool usesMeshPipeline() const noexcept
    {
        return m_meshShader != nullptr;
    }

    /// @brief Looks up the shader pointer bound to `frequency`, or `nullptr` if that stage is unbound.
    /// @param[in] frequency Pipeline stage to query.
    /// @return Base `shader::Shader*` for the requested stage, or `nullptr`.
    [[nodiscard]] shader::Shader* getShader(shader::ShaderFrequency frequency) const noexcept;

    /// @brief Default memberwise equality comparison across all bound stage slots and specialization constants.
    /// @param[in] rhs Input bundle to compare against.
    /// @return True when every stage pointer, vertex declaration pointer, and specialization array matches.
    [[nodiscard]] bool operator==(const BoundShaderStateInput& rhs) const noexcept = default;

public:
    /// @brief Specialization constant keys applied to the bound vertex shader stage.
    shader::SpecializationContainer vertexShaderSpecializationConstants{};

    /// @brief Specialization constant keys applied to the bound pixel shader stage.
    shader::SpecializationContainer pixelShaderSpecializationConstants{};

    /// @brief Specialization constant keys applied to the bound mesh shader stage.
    shader::SpecializationContainer meshShaderSpecializationConstants{};

    /// @brief Specialization constant keys applied to the bound amplification shader stage.
    shader::SpecializationContainer amplificationShaderSpecializationConstants{};

    /// @brief Specialization constant keys applied to the bound geometry shader stage.
    shader::SpecializationContainer geometryShaderSpecializationConstants{};

private:
    /// @brief Vertex input attribute layout bound for traditional vertex pipelines.
    VertexDeclaration* m_vertexDeclaration{ nullptr };

    /// @brief Vertex shader stage pointer.
    shader::VertexShader* m_vertexShader{ nullptr };

    /// @brief Optional pixel shader stage pointer.
    shader::PixelShader* m_pixelShader{ nullptr };

    /// @brief Mesh shader stage pointer (mutually exclusive with vertex and work graph pipelines).
    shader::MeshShader* m_meshShader{ nullptr };

    /// @brief Optional amplification shader stage pointer paired with `m_meshShader`.
    shader::AmplificationShader* m_amplificationShader{ nullptr };

    /// @brief Optional geometry shader stage pointer.
    shader::GeometryShader* m_geometryShader{ nullptr };

    /// @brief Work graph shader stage pointer (mutually exclusive with vertex and mesh pipelines).
    shader::WorkGraphShader* m_workGraphShader{ nullptr };
};

/// @brief Compiled combination of graphics shader stages and a vertex input layout.
/// @details Backends that link or cache stage programs ahead of full pipeline creation derive from `BoundShaderState`
///          and manage its lifetime through `BoundShaderStateRef` and deferred deletion.
class GP_RHI_API BoundShaderState : public resource::Resource
{
public:
    /// @brief Constructs a bound shader state resource tagged with `ResourceType::BoundShaderState`.
    BoundShaderState() noexcept;

    BoundShaderState(const BoundShaderState&) = delete;
    BoundShaderState& operator=(const BoundShaderState&) = delete;
    BoundShaderState(BoundShaderState&&) = delete;
    BoundShaderState& operator=(BoundShaderState&&) = delete;

protected:
    /// @copydoc resource::Resource::~Resource()
    virtual ~BoundShaderState() noexcept override;
};

}   // namespace gp::rhi::pipeline
