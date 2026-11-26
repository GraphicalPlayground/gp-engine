// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "views/VulkanTextureViewDesc.hpp"
#include "CoreMinimal.hpp"
#include "hashing/HashCombine.hpp"
#include "hashing/HashMix.hpp"

namespace gp::rhi::vulkan
{

gp::UInt64 VulkanTextureViewDesc::getHash() const noexcept
{
    // Fields are folded one by one instead of hashing the raw bytes: the struct has padding between its members.
    gp::UInt64 hash = gp::hashing::hashInt64(static_cast<gp::UInt64>(viewType));
    hash = gp::hashing::hashCombine(hash, gp::hashing::hashInt64(static_cast<gp::UInt64>(aspectFlags)));
    hash = gp::hashing::hashCombine(hash, gp::hashing::hashInt64(static_cast<gp::UInt64>(pixelFormat)));
    hash = gp::hashing::hashCombine(hash, gp::hashing::hashInt64(static_cast<gp::UInt64>(format)));
    hash = gp::hashing::hashCombine(hash, gp::hashing::hashInt64(static_cast<gp::UInt64>(firstMip)));
    hash = gp::hashing::hashCombine(hash, gp::hashing::hashInt64(static_cast<gp::UInt64>(numMips)));
    hash = gp::hashing::hashCombine(hash, gp::hashing::hashInt64(static_cast<gp::UInt64>(arraySliceIndex)));
    hash = gp::hashing::hashCombine(hash, gp::hashing::hashInt64(static_cast<gp::UInt64>(numArraySlices)));
    hash = gp::hashing::hashCombine(hash, gp::hashing::hashInt64(useIdentitySwizzle ? 1u : 0u));
    hash = gp::hashing::hashCombine(hash, gp::hashing::hashInt64(static_cast<gp::UInt64>(imageUsageFlags)));
    hash = gp::hashing::hashCombine(hash, gp::hashing::hashInt64(reinterpret_cast<gp::UInt64>(samplerYcbcrConversion)));
    return hash;
}

}   // namespace gp::rhi::vulkan
