// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/strings/Name.hpp"
#include "containers/strings/CharTraits.hpp"
#include "containers/strings/NameEnums.hpp"
#include "containers/strings/NameTypes.hpp"
#include "containers/strings/String.hpp"
#include "containers/strings/StringEnums.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "memory/allocators/HeapAllocator.hpp"
#include "memory/Memory.hpp"
#include "memory/MemoryBase.hpp"
#include "memory/MemoryOrder.hpp"
#include "memory/utils/MemoryOps.hpp"   // IWYU pragma: keep
#include "platforms/base/system/Thread.hpp"
#include "threading/atomics/AtomicFlag.hpp"
#include <charconv>

namespace gp::container
{

namespace
{

/// @brief TTAS (Test-and-Test-and-Set) spinlock guard for a single `NamePool` shard (ADR-0025, ADR-0047).
class ScopedShardLock final
{
public:
    explicit ScopedShardLock(gp::thread::AtomicFlag& flag) noexcept
        : m_flag(flag)
    {
        while (m_flag.testAndSet(gp::memory::MemoryOrder::Acquire))
        {
            while (m_flag.test(gp::memory::MemoryOrder::Relaxed))
            {
                gp::platform::Thread::spinWait();
            }
        }
    }

    ScopedShardLock(const ScopedShardLock&) = delete;
    ScopedShardLock& operator=(const ScopedShardLock&) = delete;
    ScopedShardLock(ScopedShardLock&&) = delete;
    ScopedShardLock& operator=(ScopedShardLock&&) = delete;

    ~ScopedShardLock() noexcept
    {
        m_flag.clear(gp::memory::MemoryOrder::Release);
    }

private:
    gp::thread::AtomicFlag& m_flag;
};

/// @brief Canonical static `"None"` C-string returned when resolving `kNameNoneId`.
inline constexpr const char kNoneLiteral[] = "None";

}   // namespace

/// @copydoc gp::container::NamePool::instance
NamePool& NamePool::instance() noexcept
{
    static NamePool s_pool{};
    return s_pool;
}

/// @copydoc gp::container::NamePool::~NamePool
NamePool::~NamePool() noexcept
{
    gp::memory::DefaultAllocator allocator{};
    for (gp::USize i = 0u; i < kShardCount; ++i)
    {
        Shard& shard = m_shards[i];
        if (shard.slots != nullptr)
        {
            allocator.deallocate(
                shard.slots, shard.slotCapacity * sizeof(NameEntryId), static_cast<gp::UInt32>(alignof(NameEntryId))
            );
            shard.slots = nullptr;
            shard.slotCapacity = 0u;
        }

        const gp::USize slabCount = shard.allocatedSlabCount.load(gp::memory::MemoryOrder::Relaxed);
        for (gp::USize s = 0u; s < slabCount; ++s)
        {
            gp::UInt8* const slabPtr = shard.slabs[s].load(gp::memory::MemoryOrder::Relaxed);
            if (slabPtr != nullptr)
            {
                allocator.deallocate(slabPtr, kSlabSizeBytes, static_cast<gp::UInt32>(alignof(NameEntryHeader)));
                shard.slabs[s].store(nullptr, gp::memory::MemoryOrder::Relaxed);
            }
        }
    }
}

/// @copydoc gp::container::NamePool::findOrAdd
NameEntryId NamePool::findOrAdd(const BasicStringView<char> baseStr, const NameFindMode findMode) noexcept
{
    if (isCanonicalNone(baseStr))
    {
        return kNameNoneId;
    }

    const BasicStringView<char> clampedStr =
        (baseStr.size() > kMaxNameLength) ? baseStr.substr(0u, kMaxNameLength) : baseStr;

    const gp::UInt64 hash = CharTraits<char>::hashIgnoreCase(clampedStr.data(), clampedStr.size());
    const gp::UInt32 shardIdx = shardIndexFromHash(hash);
    Shard& shard = m_shards[shardIdx];

    ScopedShardLock lock(shard.lock);

    if (shard.slotCapacity > 0u)
    {
        const gp::USize mask = shard.slotCapacity - 1u;
        gp::USize slotIdx = static_cast<gp::USize>(hash) & mask;

        for (;;)
        {
            const NameEntryId existingId = shard.slots[slotIdx];
            if (existingId == kNameNoneId)
            {
                break;
            }

            const NameEntryHeader* const header = resolveShardEntry(shard, existingId);
            if (header != nullptr && header->caseInsensitiveHash == hash &&
                static_cast<gp::USize>(header->length) == clampedStr.size() &&
                CharTraits<char>::equalsIgnoreCase(header->cString(), clampedStr.data(), clampedStr.size()))
            {
                return existingId;
            }

            slotIdx = (slotIdx + 1u) & mask;
        }
    }

    if (findMode == NameFindMode::Find)
    {
        return kNameNoneId;
    }

    const gp::USize currentCount = shard.entryCount.load(gp::memory::MemoryOrder::Relaxed);
    if (shard.slotCapacity == 0u || ((currentCount + 1u) * 10u >= shard.slotCapacity * 7u))
    {
        const gp::USize nextCap = (shard.slotCapacity == 0u) ? kInitialTableCapacity : (shard.slotCapacity * 2u);
        growShardTable(shard, nextCap);
    }

    const NameEntryId newId = allocateEntryInShard(shard, shardIdx, clampedStr, hash);
    GP_ASSERT(newId != kNameNoneId, "NamePool::findOrAdd failed to allocate entry in shard.");

    const gp::USize mask = shard.slotCapacity - 1u;
    gp::USize insertSlot = static_cast<gp::USize>(hash) & mask;
    while (shard.slots[insertSlot] != kNameNoneId)
    {
        insertSlot = (insertSlot + 1u) & mask;
    }

    shard.slots[insertSlot] = newId;
    shard.entryCount.store(currentCount + 1u, gp::memory::MemoryOrder::Release);
    return newId;
}

/// @copydoc gp::container::NamePool::resolveEntry
const NameEntryHeader* NamePool::resolveEntry(const NameEntryId id) const noexcept
{
    if ((id & kNameValidIdBit) == 0u)
    {
        return nullptr;
    }

    gp::UInt32 shardIdx = 0u;
    gp::UInt32 slabIdx = 0u;
    gp::UInt32 byteOffset = 0u;
    unpackEntryId(id, shardIdx, slabIdx, byteOffset);

    if (shardIdx >= kShardCount || slabIdx >= kMaxSlabsPerShard || byteOffset >= kSlabSizeBytes)
    {
        return nullptr;
    }

    const gp::UInt8* const slabPtr = m_shards[shardIdx].slabs[slabIdx].load(gp::memory::MemoryOrder::Acquire);
    if (slabPtr == nullptr)
    {
        return nullptr;
    }

    return reinterpret_cast<const NameEntryHeader*>(slabPtr + byteOffset);
}

/// @copydoc gp::container::NamePool::resolveView
BasicStringView<char> NamePool::resolveView(const NameEntryId id) const noexcept
{
    const NameEntryHeader* const header = resolveEntry(id);
    if (header == nullptr)
    {
        return BasicStringView<char>(kNoneLiteral, 4u);
    }
    return header->view();
}

/// @copydoc gp::container::NamePool::resolveCString
const char* NamePool::resolveCString(const NameEntryId id) const noexcept
{
    const NameEntryHeader* const header = resolveEntry(id);
    if (header == nullptr)
    {
        return kNoneLiteral;
    }
    return header->cString();
}

/// @copydoc gp::container::NamePool::resolveHash
gp::UInt64 NamePool::resolveHash(const NameEntryId id) const noexcept
{
    const NameEntryHeader* const header = resolveEntry(id);
    if (header == nullptr)
    {
        return 0u;
    }
    return header->caseInsensitiveHash;
}

/// @copydoc gp::container::NamePool::entryCount
gp::USize NamePool::entryCount() const noexcept
{
    gp::USize total = 0u;
    for (gp::USize i = 0u; i < kShardCount; ++i)
    {
        total += m_shards[i].entryCount.load(gp::memory::MemoryOrder::Relaxed);
    }
    return total;
}

/// @copydoc gp::container::NamePool::slabCount
gp::USize NamePool::slabCount() const noexcept
{
    gp::USize totalSlabs = 0u;
    for (gp::USize i = 0u; i < kShardCount; ++i)
    {
        totalSlabs += m_shards[i].allocatedSlabCount.load(gp::memory::MemoryOrder::Relaxed);
    }
    return totalSlabs;
}

/// @copydoc gp::container::NamePool::allocatedSlabBytes
gp::USize NamePool::allocatedSlabBytes() const noexcept
{
    return slabCount() * kSlabSizeBytes;
}

const NameEntryHeader* NamePool::resolveShardEntry(const Shard& shard, const NameEntryId id) const noexcept
{
    gp::UInt32 shardIdx = 0u;
    gp::UInt32 slabIdx = 0u;
    gp::UInt32 byteOffset = 0u;
    unpackEntryId(id, shardIdx, slabIdx, byteOffset);

    const gp::UInt8* const slabPtr = shard.slabs[slabIdx].load(gp::memory::MemoryOrder::Relaxed);
    if (slabPtr == nullptr)
    {
        return nullptr;
    }
    return reinterpret_cast<const NameEntryHeader*>(slabPtr + byteOffset);
}

void NamePool::growShardTable(Shard& shard, const gp::USize newCapacity) noexcept
{
    gp::memory::DefaultAllocator allocator{};
    NameEntryId* const newSlots = static_cast<NameEntryId*>(
        allocator.allocate(newCapacity * sizeof(NameEntryId), static_cast<gp::UInt32>(alignof(NameEntryId)))
    );
    GP_ASSERT(newSlots != nullptr, "NamePool::growShardTable failed to allocate hash slots.");

    gp::memory::zeroMemory(newSlots, newCapacity * sizeof(NameEntryId));

    const gp::USize newMask = newCapacity - 1u;
    if (shard.slots != nullptr)
    {
        for (gp::USize i = 0u; i < shard.slotCapacity; ++i)
        {
            const NameEntryId existingId = shard.slots[i];
            if (existingId != kNameNoneId)
            {
                const NameEntryHeader* const header = resolveShardEntry(shard, existingId);
                gp::USize destSlot = static_cast<gp::USize>(header->caseInsensitiveHash) & newMask;
                while (newSlots[destSlot] != kNameNoneId)
                {
                    destSlot = (destSlot + 1u) & newMask;
                }
                newSlots[destSlot] = existingId;
            }
        }

        allocator.deallocate(
            shard.slots, shard.slotCapacity * sizeof(NameEntryId), static_cast<gp::UInt32>(alignof(NameEntryId))
        );
    }

    shard.slots = newSlots;
    shard.slotCapacity = newCapacity;
}

NameEntryId NamePool::allocateEntryInShard(
    Shard& shard, const gp::UInt32 shardIdx, const BasicStringView<char> baseStr, const gp::UInt64 caseInsensitiveHash
) noexcept
{
    const gp::USize requiredBytes = gp::memory::align(sizeof(NameEntryHeader) + baseStr.size() + 1u, 8u);
    GP_ASSERT(requiredBytes <= kSlabSizeBytes, "NamePool entry exceeds slab size.");

    gp::memory::DefaultAllocator allocator{};
    const gp::USize currentSlabCount = shard.allocatedSlabCount.load(gp::memory::MemoryOrder::Relaxed);

    if (currentSlabCount == 0u || (shard.activeSlabOffset + requiredBytes > kSlabSizeBytes))
    {
        const gp::USize newSlabIdx = currentSlabCount;
        GP_ASSERT(newSlabIdx < kMaxSlabsPerShard, "NamePool shard exhausted maximum slab count.");

        gp::UInt8* const newSlab = static_cast<gp::UInt8*>(
            allocator.allocate(kSlabSizeBytes, static_cast<gp::UInt32>(alignof(NameEntryHeader)))
        );
        GP_ASSERT(newSlab != nullptr, "NamePool failed to allocate 16 KiB string slab.");

        shard.slabs[newSlabIdx].store(newSlab, gp::memory::MemoryOrder::Release);
        shard.allocatedSlabCount.store(newSlabIdx + 1u, gp::memory::MemoryOrder::Release);
        shard.activeSlabIndex = newSlabIdx;
        shard.activeSlabOffset = 0u;
    }

    const gp::USize entryOffset = shard.activeSlabOffset;
    gp::UInt8* const activeSlab = shard.slabs[shard.activeSlabIndex].load(gp::memory::MemoryOrder::Relaxed);
    NameEntryHeader* const header = reinterpret_cast<NameEntryHeader*>(activeSlab + entryOffset);

    header->caseInsensitiveHash = caseInsensitiveHash;
    header->length = static_cast<gp::UInt16>(baseStr.size());
    header->flags = 0u;
    header->reserved = 0u;

    char* const dstChars = reinterpret_cast<char*>(header + 1);
    CharTraits<char>::copy(dstChars, baseStr.data(), baseStr.size());
    dstChars[baseStr.size()] = '\0';

    shard.activeSlabOffset += requiredBytes;

    return packEntryId(shardIdx, static_cast<gp::UInt32>(shard.activeSlabIndex), static_cast<gp::UInt32>(entryOffset));
}

/// @copydoc gp::container::Name::Name(BasicStringView<char>, NameFindMode)
Name::Name(const BasicStringView<char> str, const NameFindMode findMode) noexcept
{
    if (str.isEmpty())
    {
        m_comparisonId = kNameNoneId;
        m_number = kNameNoNumber;
        return;
    }

    BasicStringView<char> baseStr{};
    gp::Int32 internalNumber = kNameNoNumber;
    (void)NamePool::splitNameWithCheck(str, baseStr, internalNumber);

    if (NamePool::isCanonicalNone(baseStr))
    {
        m_comparisonId = kNameNoneId;
        m_number = internalNumber;
        return;
    }

    const NameEntryId resolvedId = NamePool::instance().findOrAdd(baseStr, findMode);
    if (resolvedId == kNameNoneId)
    {
        m_comparisonId = kNameNoneId;
        m_number = kNameNoNumber;
        return;
    }

    m_comparisonId = resolvedId;
    m_number = internalNumber;
}

/// @copydoc gp::container::Name::Name(const char*, NameFindMode)
Name::Name(const char* const str, const NameFindMode findMode) noexcept
    : Name(BasicStringView<char>(str), findMode)
{}

/// @copydoc gp::container::Name::Name(BasicStringView<char>, gp::Int32, NameFindMode)
Name::Name(const BasicStringView<char> baseStr, const gp::Int32 number, const NameFindMode findMode) noexcept
{
    const gp::Int32 internalNumber = (number >= 0) ? (number + 1) : kNameNoNumber;
    if (NamePool::isCanonicalNone(baseStr))
    {
        m_comparisonId = kNameNoneId;
        m_number = internalNumber;
        return;
    }

    const NameEntryId resolvedId = NamePool::instance().findOrAdd(baseStr, findMode);
    if (resolvedId == kNameNoneId)
    {
        m_comparisonId = kNameNoneId;
        m_number = kNameNoNumber;
        return;
    }

    m_comparisonId = resolvedId;
    m_number = internalNumber;
}

/// @copydoc gp::container::Name::toString
String Name::toString() const noexcept
{
    String result{};
    appendTo(result);
    return result;
}

/// @copydoc gp::container::Name::appendTo
void Name::appendTo(String& outStr) const noexcept
{
    const BasicStringView<char> base = baseView();
    if (!hasNumber())
    {
        outStr.append(base);
        return;
    }

    char numBuf[16]{};
    numBuf[0] = '_';
    const gp::Int32 externalNum = getNumber();
    const std::to_chars_result res = std::to_chars(numBuf + 1, numBuf + sizeof(numBuf), externalNum, 10);
    const gp::USize numLen = (res.ec == std::errc{}) ? static_cast<gp::USize>(res.ptr - numBuf) : 0u;

    outStr.reserve(outStr.size() + base.size() + numLen);
    outStr.append(base);
    if (numLen > 0u)
    {
        outStr.append(numBuf, numLen);
    }
}

/// @copydoc gp::container::Name::compareLexicographical
gp::Int32 Name::compareLexicographical(const Name& other, const SearchCase searchCase) const noexcept
{
    if (*this == other && searchCase == SearchCase::IgnoreCase)
    {
        return 0;
    }

    const BasicStringView<char> lhsBase = baseView();
    const BasicStringView<char> rhsBase = other.baseView();
    const gp::Int32 baseCmp = lhsBase.compare(rhsBase, searchCase);
    if (baseCmp != 0)
    {
        return baseCmp;
    }

    if (m_number < other.m_number)
    {
        return -1;
    }
    if (m_number > other.m_number)
    {
        return 1;
    }
    return 0;
}

/// @copydoc gp::container::Name::equalsString
bool Name::equalsString(const BasicStringView<char> str, const SearchCase searchCase) const noexcept
{
    if (str.isEmpty())
    {
        return isNone();
    }

    BasicStringView<char> rhsBase{};
    gp::Int32 rhsInternalNumber = kNameNoNumber;
    (void)NamePool::splitNameWithCheck(str, rhsBase, rhsInternalNumber);

    if (m_number != rhsInternalNumber)
    {
        return false;
    }

    if (m_comparisonId == kNameNoneId)
    {
        return NamePool::isCanonicalNone(rhsBase);
    }

    return baseView().equals(rhsBase, searchCase);
}

}   // namespace gp::container
