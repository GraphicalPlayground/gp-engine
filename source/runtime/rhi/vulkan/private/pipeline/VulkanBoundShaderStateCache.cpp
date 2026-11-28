// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanBoundShaderStateCache.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "memory/pointers/PointerTypes.hpp"
#include "pipeline/BoundShaderState.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/UniqueLock.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Process-wide bound shader state cache.
/// @details A flat array scanned with a hash prefilter: the cache holds one entry per live shader combination, which
///          stays small, and a scan of contiguous pointers is cheaper than a node-based map at that size.
struct BoundShaderStateCache final
{
    /// @brief Guards `links`.
    gp::thread::Mutex mutex{};

    /// @brief Cached links, in insertion order.
    gp::Vector<VulkanCachedBoundShaderStateLink*> links{};
};

/// @brief Returns the process-wide cache.
/// @details Intentionally never destroyed: states released during static destruction would otherwise reach a
///          destroyed mutex.
/// @return Cache shared by every caller.
[[nodiscard]] BoundShaderStateCache& getBoundShaderStateCache() noexcept
{
    static BoundShaderStateCache* const s_cache = new BoundShaderStateCache();
    return *s_cache;
}

}   // namespace

VulkanCachedBoundShaderStateLink::VulkanCachedBoundShaderStateLink(
    pipeline::VertexDeclaration* vertexDeclaration,
    shader::VertexShader* vertexShader,
    shader::PixelShader* pixelShader,
    shader::GeometryShader* geometryShader,
    pipeline::BoundShaderState* boundShaderState
) noexcept
    : m_key(vertexDeclaration, vertexShader, pixelShader, geometryShader)
    , m_boundShaderState(boundShaderState)
{}

VulkanCachedBoundShaderStateLink::VulkanCachedBoundShaderStateLink(
    shader::MeshShader* meshShader,
    shader::AmplificationShader* amplificationShader,
    shader::PixelShader* pixelShader,
    pipeline::BoundShaderState* boundShaderState
) noexcept
    : m_key(meshShader, amplificationShader, pixelShader)
    , m_boundShaderState(boundShaderState)
{}

void VulkanCachedBoundShaderStateLink::addToCache() noexcept
{
    BoundShaderStateCache& cache = getBoundShaderStateCache();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(cache.mutex);
    cache.links.pushBack(this);
}

void VulkanCachedBoundShaderStateLink::removeFromCache() noexcept
{
    BoundShaderStateCache& cache = getBoundShaderStateCache();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(cache.mutex);
    for (gp::USize index = 0u; index < cache.links.size(); ++index)
    {
        if (cache.links[index] == this)
        {
            cache.links.removeAtSwap(index);
            return;
        }
    }
}

pipeline::BoundShaderStateRef findCachedVulkanBoundShaderState(
    const pipeline::VertexDeclaration* vertexDeclaration,
    const shader::VertexShader* vertexShader,
    const shader::PixelShader* pixelShader,
    const shader::GeometryShader* geometryShader,
    const shader::MeshShader* meshShader,
    const shader::AmplificationShader* amplificationShader
) noexcept
{
    const VulkanBoundShaderStateLookupKey lookupKey =
        (meshShader != nullptr)
            ? VulkanBoundShaderStateLookupKey(meshShader, amplificationShader, pixelShader)
            : VulkanBoundShaderStateLookupKey(vertexDeclaration, vertexShader, pixelShader, geometryShader);

    BoundShaderStateCache& cache = getBoundShaderStateCache();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(cache.mutex);
    for (VulkanCachedBoundShaderStateLink* link: cache.links)
    {
        const VulkanBoundShaderStateLookupKey& candidate = link->getKey().getLookupKey();
        if (candidate.getHash() != lookupKey.getHash() || !(candidate == lookupKey))
        {
            continue;
        }

        // The state cannot be destroyed while the mutex is held (its destructor removes the link under the same
        // mutex), but it may already be waiting for deferred deletion: only a live state can be handed out.
        pipeline::BoundShaderState* state = link->getBoundShaderState();
        if (state->tryAddRefIfLive())
        {
            return pipeline::BoundShaderStateRef(state, gp::kAdoptRef);
        }
    }
    return pipeline::BoundShaderStateRef{};
}

void emptyCachedVulkanBoundShaderStates() noexcept
{
    BoundShaderStateCache& cache = getBoundShaderStateCache();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(cache.mutex);
    cache.links.clear();
}

gp::USize getCachedVulkanBoundShaderStateCount() noexcept
{
    BoundShaderStateCache& cache = getBoundShaderStateCache();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(cache.mutex);
    return cache.links.size();
}

}   // namespace gp::rhi::vulkan
