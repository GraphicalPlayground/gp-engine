// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/ContainerForward.hpp"   // IWYU pragma: keep
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Bounds-checked little-endian reader over a serialized shader blob.
/// @details Shader blobs come from disk or from the network, so every read is validated against the remaining
///          byte count. The first failing read latches the reader into a failed state: every later read returns
///          false and leaves its output untouched, which lets a parser chain reads and check `hasFailed()` once.
///          Element counts are validated with `readCount()` before any container is sized, so a corrupted count
///          can never trigger a multi-gigabyte allocation.
/// @note The reader never owns the bytes; the viewed memory must outlive it.
class GP_RHI_VULKAN_API VulkanShaderCodeReader
{
public:
    /// @brief Creates a reader positioned at the first byte of `bytes`.
    /// @param[in] bytes Serialized blob to read.
    explicit VulkanShaderCodeReader(gp::ConstVectorView<gp::UInt8> bytes) noexcept;

    /// @brief Reads one byte.
    /// @param[out] outValue Receives the byte on success.
    /// @return True on success; false when the blob is exhausted or the reader already failed.
    bool readUInt8(gp::UInt8& outValue) noexcept;

    /// @brief Reads a little-endian 32-bit unsigned integer.
    /// @param[out] outValue Receives the value on success.
    /// @return True on success.
    bool readUInt32(gp::UInt32& outValue) noexcept;

    /// @brief Reads a little-endian 64-bit unsigned integer.
    /// @param[out] outValue Receives the value on success.
    /// @return True on success.
    bool readUInt64(gp::UInt64& outValue) noexcept;

    /// @brief Copies raw bytes out of the blob.
    /// @param[out] outDestination Buffer receiving `sizeInBytes` bytes; may be null only when `sizeInBytes` is zero.
    /// @param[in] sizeInBytes Number of bytes to copy.
    /// @return True on success.
    bool readBytes(void* outDestination, gp::USize sizeInBytes) noexcept;

    /// @brief Reads an element count and validates that the blob can hold that many elements.
    /// @param[out] outCount Receives the count on success.
    /// @param[in] minBytesPerElement Smallest serialized size of one element; must be greater than zero.
    /// @return True when the count fits in the remaining bytes.
    bool readCount(gp::UInt32& outCount, gp::USize minBytesPerElement) noexcept;

    /// @brief Reads a length-prefixed string (32-bit byte length followed by the characters, no terminator).
    /// @param[out] outString Receives the string on success.
    /// @return True on success.
    bool readString(gp::String& outString) noexcept;

    /// @brief Returns a view of the next `sizeInBytes` bytes and advances past them.
    /// @param[in] sizeInBytes Number of bytes to skip.
    /// @param[out] outView Receives a view into the source blob on success.
    /// @return True on success.
    bool readView(gp::USize sizeInBytes, gp::ConstVectorView<gp::UInt8>& outView) noexcept;

    /// @brief Latches the reader into the failed state; used by parsers that reject a semantically invalid value.
    void markFailed() noexcept
    {
        m_hasFailed = true;
    }

    /// @brief Reports whether any read failed.
    /// @return True once a read ran past the end or `markFailed()` was called.
    [[nodiscard]] bool hasFailed() const noexcept
    {
        return m_hasFailed;
    }

    /// @brief Reports whether every byte was consumed.
    /// @return True when the read offset reached the end of the blob.
    [[nodiscard]] bool isAtEnd() const noexcept
    {
        return m_offset == m_size;
    }

    /// @brief Returns the current read offset.
    /// @return Number of bytes consumed so far.
    [[nodiscard]] gp::USize getOffset() const noexcept
    {
        return m_offset;
    }

    /// @brief Returns the number of unread bytes.
    /// @return Bytes left between the offset and the end of the blob.
    [[nodiscard]] gp::USize getRemaining() const noexcept
    {
        return m_size - m_offset;
    }

private:
    /// @brief Validates that `sizeInBytes` bytes are available, latching failure otherwise.
    /// @param[in] sizeInBytes Number of bytes about to be read.
    /// @return True when the read may proceed.
    [[nodiscard]] bool canRead(gp::USize sizeInBytes) noexcept;

private:
    /// @brief First byte of the viewed blob.
    const gp::UInt8* m_data{ nullptr };

    /// @brief Total size of the blob in bytes.
    gp::USize m_size{ 0u };

    /// @brief Current read offset.
    gp::USize m_offset{ 0u };

    /// @brief Sticky failure flag.
    bool m_hasFailed{ false };
};

}   // namespace gp::rhi::vulkan
