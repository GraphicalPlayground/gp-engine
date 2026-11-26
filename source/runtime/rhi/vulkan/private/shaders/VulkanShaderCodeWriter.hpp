// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"    // IWYU pragma: keep
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                 // IWYU pragma: keep
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Little-endian byte writer producing the blobs `VulkanShaderCodeReader` consumes.
/// @details Used by the offline shader compiler path and by tests to build blobs; the runtime only reads.
class GP_RHI_VULKAN_API VulkanShaderCodeWriter
{
public:
    /// @brief Appends one byte.
    /// @param[in] value Byte to append.
    void writeUInt8(gp::UInt8 value) noexcept;

    /// @brief Appends a little-endian 32-bit unsigned integer.
    /// @param[in] value Value to append.
    void writeUInt32(gp::UInt32 value) noexcept;

    /// @brief Appends a little-endian 64-bit unsigned integer.
    /// @param[in] value Value to append.
    void writeUInt64(gp::UInt64 value) noexcept;

    /// @brief Appends raw bytes.
    /// @param[in] source First byte to copy; may be null only when `sizeInBytes` is zero.
    /// @param[in] sizeInBytes Number of bytes to copy.
    void writeBytes(const void* source, gp::USize sizeInBytes) noexcept;

    /// @brief Appends a length-prefixed string (32-bit byte length followed by the characters).
    /// @param[in] value String to append.
    void writeString(const gp::String& value) noexcept;

    /// @brief Returns the bytes written so far.
    /// @return Serialized blob.
    [[nodiscard]] const gp::Vector<gp::UInt8>& getBytes() const noexcept
    {
        return m_bytes;
    }

    /// @brief Gives up ownership of the written bytes.
    /// @return Serialized blob; the writer is left empty.
    [[nodiscard]] gp::Vector<gp::UInt8> releaseBytes() noexcept;

private:
    /// @brief Serialized blob.
    gp::Vector<gp::UInt8> m_bytes{};
};

}   // namespace gp::rhi::vulkan
