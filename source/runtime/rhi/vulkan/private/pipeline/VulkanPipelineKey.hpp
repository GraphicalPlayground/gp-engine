// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"         // IWYU pragma: keep
#include "CoreMinimal.hpp"                      // IWYU pragma: keep
#include "pipeline/VulkanPipelineForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include <type_traits>

namespace gp::rhi::vulkan
{

/// @brief Byte serialization of a pipeline description plus its 64-bit XXH64 hash.
/// @details The graphics pipeline cache is keyed by this value. Keeping the bytes, and comparing them on equality,
///          makes a hash collision harmless: two descriptions only share a `VkPipeline` when every serialized field
///          matches. The hash only short-circuits mismatches and picks the bucket.
class GP_RHI_VULKAN_API VulkanPipelineKey
{
public:
    /// @brief Creates an empty key (hash 0).
    VulkanPipelineKey() noexcept = default;

    /// @brief Returns the 64-bit hash of the key bytes.
    /// @return XXH64 of `getData()`, or 0 for an empty key.
    [[nodiscard]] gp::UInt64 getHash() const noexcept
    {
        return m_hash;
    }

    /// @brief Returns the serialized bytes.
    /// @return Byte vector.
    [[nodiscard]] const gp::Vector<gp::UInt8>& getData() const noexcept
    {
        return m_data;
    }

    /// @brief Reports whether the key holds no bytes.
    /// @return True for an empty key.
    [[nodiscard]] bool isEmpty() const noexcept
    {
        return m_data.isEmpty();
    }

    /// @brief Compares hashes, then sizes, then bytes.
    /// @param[in] rhs Key to compare against.
    /// @return True when both keys hold the same bytes.
    [[nodiscard]] bool operator==(const VulkanPipelineKey& rhs) const noexcept;

private:
    friend class VulkanPipelineKeyWriter;

    /// @brief Serialized description.
    gp::Vector<gp::UInt8> m_data{};

    /// @brief XXH64 of `m_data`.
    gp::UInt64 m_hash{ 0u };
};

/// @brief Appends typed values to a `VulkanPipelineKey` and seals it with its hash.
/// @details Values are written as their raw bytes, in call order, with no padding: callers write scalar fields one by
///          one, never whole structs, so the key never depends on compiler padding. Arrays are written as an `Int32`
///          element count followed by the elements, so two arrays of different length cannot alias.
class GP_RHI_VULKAN_API VulkanPipelineKeyWriter
{
public:
    /// @brief Starts writing into `outKey`, clearing it.
    /// @param[out] outKey Key that receives the bytes.
    explicit VulkanPipelineKeyWriter(VulkanPipelineKey& outKey) noexcept;

    VulkanPipelineKeyWriter(const VulkanPipelineKeyWriter&) = delete;
    VulkanPipelineKeyWriter& operator=(const VulkanPipelineKeyWriter&) = delete;
    VulkanPipelineKeyWriter(VulkanPipelineKeyWriter&&) = delete;
    VulkanPipelineKeyWriter& operator=(VulkanPipelineKeyWriter&&) = delete;

    /// @brief Seals the key: computes its hash.
    ~VulkanPipelineKeyWriter() noexcept;

    /// @brief Appends raw bytes.
    /// @param[in] data First byte; may be null when `size` is zero.
    /// @param[in] size Byte count.
    void writeBytes(const void* data, gp::USize size) noexcept;

    /// @brief Appends the raw bytes of one scalar value.
    /// @tparam InValueType Arithmetic or enumeration type.
    /// @param[in] value Value to append.
    template <typename InValueType>
    requires(std::is_arithmetic_v<InValueType> || std::is_enum_v<InValueType>)
    void write(const InValueType value) noexcept
    {
        writeBytes(&value, sizeof(InValueType));
    }

    /// @brief Appends an array element count.
    /// @param[in] count Element count.
    void writeCount(gp::USize count) noexcept;

private:
    /// @brief Key being written.
    VulkanPipelineKey& m_key;
};

}   // namespace gp::rhi::vulkan
