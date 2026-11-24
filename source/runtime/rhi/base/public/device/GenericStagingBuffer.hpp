// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/DeviceForward.hpp"       // IWYU pragma: keep
#include "resources/StagingBuffer.hpp"
#include "RHIBase.hpp"

namespace gp::rhi
{

/// @brief Generic CPU-backed `StagingBuffer` fallback for backends without custom staging readback objects.
class GP_RHI_API GenericStagingBuffer final : public resource::StagingBuffer
{
public:
    /// @brief Constructs an empty generic staging buffer.
    GenericStagingBuffer() noexcept = default;

    /// @copydoc resource::StagingBuffer::lock()
    [[nodiscard]] virtual void* lock(gp::UInt32 offset, gp::UInt32 numBytes) noexcept override;

    /// @copydoc resource::StagingBuffer::unlock()
    virtual void unlock() noexcept override;

    /// @copydoc resource::StagingBuffer::getGPUSizeBytes()
    [[nodiscard]] virtual gp::UInt64 getGPUSizeBytes() const noexcept override;

    /// @brief Stores `numBytes` copied from a source buffer into this generic staging buffer's shadow storage.
    /// @param[in] srcData Source bytes (may be `nullptr` to zero-fill).
    /// @param[in] numBytes Number of bytes to retain.
    void writeShadowData(const void* srcData, gp::UInt32 numBytes) noexcept;

protected:
    /// @copydoc resource::StagingBuffer::~StagingBuffer()
    virtual ~GenericStagingBuffer() noexcept override;

private:
    /// @brief CPU-accessible shadow bytes populated by `writeShadowData()`.
    gp::Vector<gp::UInt8> m_shadowData{};
};

}   // namespace gp::rhi
