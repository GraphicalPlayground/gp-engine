// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "RHIBase.hpp"
#include "shaders/Shader.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/ShaderForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::shader
{

/// @brief Shared base class for hardware ray tracing shader stages.
/// @details Stores ray payload type bits, maximum ray payload byte size, and local shader binding table data size so
///          ray tracing pipeline builders can validate shader compatibility and size shader binding records.
class GP_RHI_API RayTracingShader : public Shader
{
public:
    RayTracingShader() = delete;

    /// @brief Constructs a ray tracing shader tagged with `frequency`.
    /// @param[in] frequency Ray tracing `ShaderFrequency` stage (`RayGen`, `RayMiss`, `RayHitGroup`, or `RayCallable`).
    explicit RayTracingShader(ShaderFrequency frequency) noexcept;

    RayTracingShader(const RayTracingShader&) = delete;
    RayTracingShader& operator=(const RayTracingShader&) = delete;
    RayTracingShader(RayTracingShader&&) = delete;
    RayTracingShader& operator=(RayTracingShader&&) = delete;

protected:
    /// @copydoc Shader::~Shader()
    virtual ~RayTracingShader() noexcept override;

public:
    /// @brief Sets the bitmask identifying the ray payload layout accepted by this shader.
    /// @param[in] payloadType Engine-defined ray payload type bitmask.
    GP_FORCEINLINE_HINT void setRayTracingPayloadType(const gp::UInt32 payloadType) noexcept
    {
        m_rayTracingPayloadType = payloadType;
    }

    /// @brief Returns the ray payload type bitmask associated with this shader.
    /// @return Ray payload type bitmask.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getRayTracingPayloadType() const noexcept
    {
        return m_rayTracingPayloadType;
    }

    /// @brief Sets the maximum size in bytes of the ray payload struct used by this shader.
    /// @param[in] payloadSize Maximum ray payload struct size in bytes.
    GP_FORCEINLINE_HINT void setRayTracingPayloadSize(const gp::UInt32 payloadSize) noexcept
    {
        m_rayTracingPayloadSize = payloadSize;
    }

    /// @brief Returns the maximum ray payload struct size in bytes.
    /// @return Ray payload size in bytes.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getRayTracingPayloadSize() const noexcept
    {
        return m_rayTracingPayloadSize;
    }

    /// @brief Sets the size in bytes of local shader binding data required by this shader.
    /// @param[in] localBindingDataSize Local shader record payload size in bytes.
    GP_FORCEINLINE_HINT void setLocalBindingDataSize(const gp::UInt32 localBindingDataSize) noexcept
    {
        m_localBindingDataSize = localBindingDataSize;
    }

    /// @brief Returns the size in bytes of local shader binding data required by this shader.
    /// @return Local shader record payload size in bytes.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getLocalBindingDataSize() const noexcept
    {
        return m_localBindingDataSize;
    }

private:
    /// @brief Bitmask identifying the ray payload type accepted by this shader.
    gp::UInt32 m_rayTracingPayloadType{ 0u };

    /// @brief Maximum byte size of the ray payload structure used by this shader.
    gp::UInt32 m_rayTracingPayloadSize{ 0u };

    /// @brief Byte size of local shader binding data required per shader binding table record.
    gp::UInt32 m_localBindingDataSize{ 0u };
};

}   // namespace gp::rhi::shader
