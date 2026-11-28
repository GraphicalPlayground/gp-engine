// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanTextureStats.hpp"
#include "CoreMinimal.hpp"
#include "memory/MemoryOrder.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/VulkanTextureEnums.hpp"
#include "resources/VulkanTextureUsage.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Process-wide texture memory counters.
struct VulkanTextureStatCounters final
{
    gp::thread::Atomic<gp::Int64> categoryBytes[kVulkanTextureStatCategoryCount]{};
    gp::thread::Atomic<gp::Int64> categoryCounts[kVulkanTextureStatCategoryCount]{};
    gp::thread::Atomic<gp::Int64> totalTextureBytes{ 0 };
};

VulkanTextureStatCounters g_textureStatCounters{};

constexpr gp::memory::MemoryOrder kStatOrder = gp::memory::MemoryOrder::Relaxed;

}   // namespace

VulkanTextureStatCategory getVulkanTextureStatCategory(const resource::TextureDesc& desc) noexcept
{
    using resource::TextureCreateFlags;

    if (hasAnyTextureFlags(desc.flags, TextureCreateFlags::CPUReadback))
    {
        return VulkanTextureStatCategory::CPUReadback;
    }
    else if (
        hasAnyTextureFlags(
            desc.flags,
            TextureCreateFlags::RenderTargetable | TextureCreateFlags::DepthStencilTargetable |
                TextureCreateFlags::ResolveTargetable | TextureCreateFlags::UAV
        )
    )
    {
        return VulkanTextureStatCategory::RenderTarget;
    }
    else if (desc.isTextureCube())
    {
        return VulkanTextureStatCategory::TextureCube;
    }
    else if (desc.isTexture3D())
    {
        return VulkanTextureStatCategory::Texture3D;
    }
    return VulkanTextureStatCategory::Texture2D;
}

void updateVulkanTextureStats(
    const VulkanTextureStatCategory category, const gp::UInt64 textureSize, const bool isAllocating
) noexcept
{
    const gp::Int64 delta = isAllocating ? static_cast<gp::Int64>(textureSize) : -static_cast<gp::Int64>(textureSize);
    const gp::Int64 countDelta = isAllocating ? 1 : -1;
    const gp::USize categoryIndex = static_cast<gp::USize>(category);

    static_cast<void>(g_textureStatCounters.categoryBytes[categoryIndex].fetchAdd(delta, kStatOrder));
    static_cast<void>(g_textureStatCounters.categoryCounts[categoryIndex].fetchAdd(countDelta, kStatOrder));
    static_cast<void>(g_textureStatCounters.totalTextureBytes.fetchAdd(delta, kStatOrder));
}

VulkanTextureMemoryStats getVulkanTextureMemoryStats() noexcept
{
    VulkanTextureMemoryStats stats{};
    for (gp::USize index = 0u; index < kVulkanTextureStatCategoryCount; ++index)
    {
        stats.categoryBytes[index] = g_textureStatCounters.categoryBytes[index].load(kStatOrder);
        stats.categoryCounts[index] = g_textureStatCounters.categoryCounts[index].load(kStatOrder);
    }
    stats.totalTextureBytes = g_textureStatCounters.totalTextureBytes.load(kStatOrder);
    return stats;
}

}   // namespace gp::rhi::vulkan
