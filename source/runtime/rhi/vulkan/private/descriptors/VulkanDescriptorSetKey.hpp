// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"                  // IWYU pragma: keep
#include "CoreMinimal.hpp"                               // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Byte key identifying the contents of a descriptor set, for descriptor set caching.
/// @details The key keeps a copy of the hashed bytes and compares them on equality. CRC-32 collides far too often for
///          a cache that may hold tens of thousands of sets, and binding a set with the wrong resources is a silent
///          rendering bug, so the hash only short-circuits mismatches.
class GP_RHI_VULKAN_API VulkanDescriptorSetKey
{
public:
    /// @brief Creates an empty key.
    VulkanDescriptorSetKey() noexcept = default;

    /// @brief Replaces the key with a copy of `size` bytes at `data`.
    /// @param[in] data First byte; may be null when `size` is zero.
    /// @param[in] size Byte count.
    void generateFromData(const void* data, gp::USize size) noexcept;

    /// @brief Returns the CRC-32 of the key bytes.
    /// @return Hash; zero for an empty key.
    [[nodiscard]] gp::UInt32 getHash() const noexcept
    {
        return m_hash;
    }

    /// @brief Returns the number of key bytes.
    /// @return Byte count.
    [[nodiscard]] gp::USize getSize() const noexcept
    {
        return static_cast<gp::USize>(m_data.size());
    }

    /// @brief Compares hashes, then bytes.
    /// @param[in] rhs Key to compare against.
    /// @return True when both keys hold the same bytes.
    [[nodiscard]] bool operator==(const VulkanDescriptorSetKey& rhs) const noexcept;

private:
    /// @brief Copied key bytes.
    gp::Vector<gp::UInt8> m_data{};

    /// @brief CRC-32 of `m_data`.
    gp::UInt32 m_hash{ 0u };
};

}   // namespace gp::rhi::vulkan
