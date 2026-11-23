// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/Resource.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIBase.hpp"

namespace gp::rhi::resource
{

/// @brief CPU-readable staging buffer used to transfer data from GPU memory back to the CPU.
/// @details Backends implement `lock()` and `unlock()` to map readback memory after the associated GPU copy fence has
///          signaled. `m_isLocked` tracks whether a CPU mapping is currently active.
class GP_RHI_API StagingBuffer : public Resource
{
public:
    /// @brief Constructs an unlocked staging buffer tagged with `ResourceType::StagingBuffer`.
    StagingBuffer() noexcept;

    StagingBuffer(const StagingBuffer&) = delete;
    StagingBuffer& operator=(const StagingBuffer&) = delete;
    StagingBuffer(StagingBuffer&&) = delete;
    StagingBuffer& operator=(StagingBuffer&&) = delete;

protected:
    /// @copydoc Resource::~Resource()
    virtual ~StagingBuffer() noexcept override;

public:
    /// @brief Maps `[offset, offset + numBytes)` of the staging buffer into CPU-readable memory.
    /// @param[in] offset Byte offset from the start of the staging buffer.
    /// @param[in] numBytes Number of bytes to map for reading.
    /// @return Pointer to the mapped read-only memory range, or `nullptr` if unavailable.
    [[nodiscard]] virtual void* lock(gp::UInt32 offset, gp::UInt32 numBytes) noexcept = 0;

    /// @brief Unmaps the staging buffer range previously mapped by `lock()`.
    virtual void unlock() noexcept = 0;

    /// @brief Returns the backing GPU allocation size in bytes for diagnostics.
    /// @details Base implementation returns `0`; backends may override to report exact staging footprint.
    /// @return Backing allocation size in bytes, or `0` if unreported.
    [[nodiscard]] virtual gp::UInt64 getGPUSizeBytes() const noexcept;

    /// @brief Reports whether this staging buffer is currently locked for CPU readback.
    /// @return True while mapped between `lock()` and `unlock()`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isLocked() const noexcept
    {
        return m_isLocked;
    }

protected:
    /// @brief Tracks whether the staging buffer currently has an active CPU lock.
    bool m_isLocked{ false };
};

}   // namespace gp::rhi::resource
