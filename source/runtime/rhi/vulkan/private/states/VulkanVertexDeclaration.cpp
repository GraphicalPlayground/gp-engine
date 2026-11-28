// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "states/VulkanVertexDeclaration.hpp"
#include "containers/arrays/Vector.hpp"        // IWYU pragma: keep
#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "states/VulkanVertexDeclarationKey.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/UniqueLock.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief One cached declaration and its key.
struct VertexDeclarationCacheEntry final
{
    /// @brief Element list and hashes.
    VulkanVertexDeclarationKey key;

    /// @brief Cached declaration.
    pipeline::VertexDeclarationRef declaration{};
};

/// @brief Process-wide declaration cache.
struct VertexDeclarationCache final
{
    /// @brief Guards `entries`.
    gp::thread::Mutex mutex{};

    /// @brief Cached declarations in creation order.
    gp::Vector<VertexDeclarationCacheEntry> entries{};
};

/// @brief Returns the process-wide declaration cache.
/// @details Intentionally never destroyed: releasing references during static destruction would reach the RHI
///          resource deletion queue after it is gone. The owner empties the cache at shutdown instead.
/// @return Cache shared by every caller.
[[nodiscard]] VertexDeclarationCache& getVertexDeclarationCache() noexcept
{
    static VertexDeclarationCache* const s_cache = new VertexDeclarationCache();
    return *s_cache;
}

}   // namespace

VulkanVertexDeclaration::VulkanVertexDeclaration(
    const pipeline::VertexDeclarationElementList& elements, const gp::UInt32 hash, const gp::UInt32 hashNoStrides
) noexcept
    : m_elements(elements)
    , m_hash(hash)
    , m_hashNoStrides(hashNoStrides)
{}

VulkanVertexDeclaration::~VulkanVertexDeclaration() noexcept = default;

pipeline::VertexDeclarationRef
    VulkanVertexDeclaration::findOrCreate(const pipeline::VertexDeclarationElementList& elements) noexcept
{
    const VulkanVertexDeclarationKey key(elements);

    VertexDeclarationCache& cache = getVertexDeclarationCache();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(cache.mutex);
    for (const VertexDeclarationCacheEntry& entry: cache.entries)
    {
        // The hash rejects quickly; the byte comparison settles CRC collisions.
        if ((entry.key.hash == key.hash) && (entry.key == key))
        {
            return entry.declaration;
        }
    }

    const pipeline::VertexDeclarationRef declaration(
        new VulkanVertexDeclaration(elements, key.hash, key.hashNoStrides)
    );
    cache.entries.emplaceBack(VertexDeclarationCacheEntry{ key, declaration });
    return declaration;
}

void VulkanVertexDeclaration::emptyCache() noexcept
{
    VertexDeclarationCache& cache = getVertexDeclarationCache();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(cache.mutex);
    cache.entries.clear();
}

gp::USize VulkanVertexDeclaration::getCacheSize() noexcept
{
    VertexDeclarationCache& cache = getVertexDeclarationCache();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(cache.mutex);
    return cache.entries.size();
}

bool VulkanVertexDeclaration::getInitializer(pipeline::VertexDeclarationElementList& outElements) const noexcept
{
    outElements = m_elements;
    return true;
}

gp::UInt32 VulkanVertexDeclaration::getPrecachePSOHash() const noexcept
{
    return m_hashNoStrides;
}

}   // namespace gp::rhi::vulkan
