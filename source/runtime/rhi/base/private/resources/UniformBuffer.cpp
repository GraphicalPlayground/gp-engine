// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/UniformBuffer.hpp"
#include "containers/arrays/Vector.hpp"
#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "memory/Memory.hpp"
#include "memory/MemoryOrder.hpp"
#include "resources/UniformBufferLayout.hpp"
#include "resources/UniformBufferLayoutDesc.hpp"
#include "RHIBase.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "views/DescriptorHandle.hpp"
#include <utility>

namespace gp::rhi::resource
{

#if GP_RHI_INCLUDE_SHADER_DEBUG_DATA
namespace
{

/// @brief Associates a live `UniformBuffer` pointer with its copied CPU-side constant payload.
struct UniformBufferContentEntry
{
    /// @brief Non-owning key identifying the tracked `UniformBuffer`.
    const UniformBuffer* key{ nullptr };

    /// @brief Deep copy of the uniform buffer's constant data bytes.
    gp::Vector<gp::UInt8> bytes{};
};

/// @brief Synchronized storage backing `uniformBufferContentMap`.
struct UniformBufferContentStorage
{
    /// @brief Mutex serializing all reads, writes, removals, and clears.
    gp::thread::Mutex mutex{};

    /// @brief Active uniform buffer content snapshots.
    gp::Vector<UniformBufferContentEntry> entries{};
};

gp::thread::Atomic<bool> g_uniformBufferContentMapEnabled{ false };

/// @brief Returns the process-wide synchronized snapshot storage.
/// @return Mutable reference to the singleton `UniformBufferContentStorage`.
UniformBufferContentStorage& getContentStorage() noexcept
{
    static UniformBufferContentStorage storage{};
    return storage;
}

}   // namespace

namespace uniformBufferContentMap
{

void setEnabled(const bool enabled) noexcept
{
    UniformBufferContentStorage& storage = getContentStorage();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(storage.mutex);
    g_uniformBufferContentMapEnabled.store(enabled, gp::memory::MemoryOrder::Release);
    if (!enabled)
    {
        storage.entries.clear();
    }
}

bool isEnabled() noexcept
{
    return g_uniformBufferContentMapEnabled.load(gp::memory::MemoryOrder::Acquire);
}

void update(const UniformBuffer* const uniformBuffer, const void* const contents) noexcept
{
    if (!isEnabled())
    {
        return;
    }

    GP_CHECKF(uniformBuffer != nullptr, "uniformBufferContentMap::update requires a non-null UniformBuffer.");

    UniformBufferContentStorage& storage = getContentStorage();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(storage.mutex);

    // Re-check under the lock so a concurrent setEnabled(false) never leaves an orphaned snapshot.
    if (!g_uniformBufferContentMapEnabled.load(gp::memory::MemoryOrder::Acquire))
    {
        return;
    }

    UniformBufferContentEntry* target = nullptr;
    for (UniformBufferContentEntry& entry: storage.entries)
    {
        if (entry.key == uniformBuffer)
        {
            target = &entry;
            break;
        }
    }

    if (target == nullptr)
    {
        storage.entries.emplaceBack();
        target = &storage.entries[storage.entries.size() - 1u];
        target->key = uniformBuffer;
    }

    target->bytes.clear();
    const gp::USize byteCount = static_cast<gp::USize>(uniformBuffer->getSize());
    if (contents != nullptr && byteCount > 0u)
    {
        target->bytes.resize(byteCount);
        gp::memory::copyMemory(target->bytes.data(), contents, byteCount);
    }
}

void remove(const UniformBuffer* const uniformBuffer) noexcept
{
    if (!isEnabled())
    {
        return;
    }

    GP_CHECKF(uniformBuffer != nullptr, "uniformBufferContentMap::remove requires a non-null UniformBuffer.");

    UniformBufferContentStorage& storage = getContentStorage();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(storage.mutex);

    for (gp::USize i = 0u; i < storage.entries.size(); ++i)
    {
        if (storage.entries[i].key == uniformBuffer)
        {
            const gp::USize lastIndex = storage.entries.size() - 1u;
            if (i != lastIndex)
            {
                storage.entries[i] = std::move(storage.entries[lastIndex]);
            }
            storage.entries.popBack();
            return;
        }
    }
}

const void* get(const UniformBuffer* const uniformBuffer) noexcept
{
    if (!isEnabled())
    {
        return nullptr;
    }

    GP_CHECKF(uniformBuffer != nullptr, "uniformBufferContentMap::get requires a non-null UniformBuffer.");

    UniformBufferContentStorage& storage = getContentStorage();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(storage.mutex);

    for (const UniformBufferContentEntry& entry: storage.entries)
    {
        if (entry.key == uniformBuffer)
        {
            return entry.bytes.isEmpty() ? nullptr : static_cast<const void*>(entry.bytes.data());
        }
    }

    return nullptr;
}

void clear() noexcept
{
    UniformBufferContentStorage& storage = getContentStorage();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(storage.mutex);
    storage.entries.clear();
}

}   // namespace uniformBufferContentMap
#endif

UniformBufferLayout::UniformBufferLayout(const UniformBufferLayoutDesc& desc) noexcept
    : Resource(ResourceType::UniformBufferLayout)
    , m_name(desc.name)
    , m_resources(desc.resources)
    , m_graphResources(desc.graphResources)
    , m_graphTextures(desc.graphTextures)
    , m_graphBuffers(desc.graphBuffers)
    , m_graphUniformBuffers(desc.graphUniformBuffers)
    , m_uniformBuffers(desc.uniformBuffers)
    , m_hash(desc.hash != 0u ? desc.hash : UniformBufferLayoutDesc::computeHashValue(desc))
    , m_constantBufferSize(desc.constantBufferSize)
    , m_renderTargetsOffset(desc.renderTargetsOffset)
    , m_staticSlot(desc.staticSlot)
    , m_bindingFlags(desc.bindingFlags)
    , m_flags(desc.flags)
{}

UniformBufferLayout::~UniformBufferLayout() noexcept = default;

bool UniformBufferLayout::operator==(const UniformBufferLayout& rhs) const noexcept
{
    return (m_constantBufferSize == rhs.m_constantBufferSize) && (m_staticSlot == rhs.m_staticSlot) &&
           (m_bindingFlags == rhs.m_bindingFlags) && (m_resources == rhs.m_resources);
}

UniformBuffer::UniformBuffer(const UniformBufferLayout* const layout) noexcept
    : Resource(ResourceType::UniformBuffer)
    , m_layout(layout)
    , m_layoutConstantBufferSize(layout != nullptr ? layout->getConstantBufferSize() : 0u)
{
    GP_CHECKF(layout != nullptr, "UniformBuffer requires a non-null UniformBufferLayout.");
}

UniformBuffer::~UniformBuffer() noexcept
{
#if GP_RHI_INCLUDE_SHADER_DEBUG_DATA
    uniformBufferContentMap::remove(this);
#endif
}

view::DescriptorHandle UniformBuffer::getBindlessHandle() const noexcept
{
    return view::DescriptorHandle{};
}

}   // namespace gp::rhi::resource
