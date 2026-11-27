// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief CPU shadow of the packed globals uniform buffer of one shader stage.
/// @details Loose shader parameters (the ones outside any uniform buffer) are packed by the shader compiler into a
///          single buffer at binding 0 of the stage's set. Setting a parameter only writes this shadow copy and marks
///          it dirty; the copy is uploaded to transient memory once, right before the next draw or dispatch, no
///          matter how many parameters changed.
/// @note Not thread-safe; owned by one descriptor state on the recording thread.
class GP_RHI_VULKAN_API VulkanPackedUniformBufferState
{
public:
    /// @brief Creates an empty state for a stage without packed globals.
    VulkanPackedUniformBufferState() noexcept = default;

    /// @brief Sizes the shadow copy and zeroes it.
    /// @param[in] packedGlobalsSize Size of the stage's packed globals buffer in bytes; zero disables the state.
    void initialize(gp::UInt32 packedGlobalsSize) noexcept;

    /// @brief Marks the whole shadow copy for upload, keeping its contents.
    /// @details Called when a pipeline is bound again: its last upload lived in transient memory that may have been
    ///          recycled since.
    void reset() noexcept;

    /// @brief Writes one parameter into the shadow copy.
    /// @details Only marks the state dirty when the bytes actually change, so redundant parameter sets cost a compare.
    /// @param[in] byteOffset Offset of the parameter in the packed buffer.
    /// @param[in] numBytes Size of the parameter.
    /// @param[in] data Parameter bytes; must hold `numBytes` bytes.
    void setParameter(gp::UInt32 byteOffset, gp::UInt32 numBytes, const void* data) noexcept;

    /// @brief Clears the dirty flag after the shadow copy was uploaded.
    void clearDirty() noexcept
    {
        m_isDirty = false;
    }

    /// @brief Reports whether the shadow copy changed since the last upload.
    /// @return True when an upload is pending.
    [[nodiscard]] bool isDirty() const noexcept
    {
        return m_isDirty;
    }

    /// @brief Reports whether the stage has packed globals at all.
    /// @return True when the size is non-zero.
    [[nodiscard]] bool hasData() const noexcept
    {
        return !m_data.isEmpty();
    }

    /// @brief Returns the shadow copy.
    /// @return Pointer to `getSize()` bytes, or null when the stage has no packed globals.
    [[nodiscard]] const gp::UInt8* getData() const noexcept
    {
        return m_data.isEmpty() ? nullptr : m_data.data();
    }

    /// @brief Returns the size of the packed globals buffer.
    /// @return Size in bytes.
    [[nodiscard]] gp::UInt32 getSize() const noexcept
    {
        return static_cast<gp::UInt32>(m_data.size());
    }

private:
    /// @brief Shadow copy uploaded on the next flush.
    gp::Vector<gp::UInt8> m_data{};

    /// @brief True when `m_data` changed since the last upload.
    bool m_isDirty{ false };
};

}   // namespace gp::rhi::vulkan
