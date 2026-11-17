// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/Fnv1a.hpp"
#include "hashing/HashingConcepts.hpp"
#include "hashing/HashingEnums.hpp"
#include "hashing/HashingForward.hpp"   // IWYU pragma: keep
#include "hashing/HashingTypes.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/atomics/AtomicFlag.hpp"

namespace gp::hashing
{

/// @brief Fixed-capacity, zero-allocation reverse lookup table mapping numeric digests back to original strings.
/// @details Modeled after Ubisoft Anvil's `StringID` debug registry and Decima's string-hash diagnostics. Stores
///          entries inside an inline open-addressed hash table backed by a contiguous character arena so logs, crash
///          reports, and editor inspectors can resolve `0x...` hashes back to readable names without heap allocation.
/// @tparam InSlotCapacity Power-of-two slot capacity of the open-addressed table.
/// @tparam InArenaBytes Total inline character storage capacity in bytes.
template <gp::USize InSlotCapacity = 1024u, gp::USize InArenaBytes = 32768u>
class BasicHashRegistry final
{
    static_assert(
        InSlotCapacity >= 8u && (InSlotCapacity & (InSlotCapacity - 1u)) == 0u,
        "BasicHashRegistry InSlotCapacity must be a power of two >= 8."
    );
    static_assert(InArenaBytes >= 64u, "BasicHashRegistry InArenaBytes must be >= 64.");

private:
    struct Slot
    {
        gp::UInt64 hash{ 0ULL };
        gp::UInt32 offset{ 0u };
        gp::UInt32 length{ 0u };
        bool occupied{ false };
    };

    static constexpr gp::USize kMask = InSlotCapacity - 1u;

    Slot m_slots[InSlotCapacity]{};
    char m_arena[InArenaBytes]{};
    gp::USize m_count{ 0u };
    gp::USize m_arenaUsed{ 0u };
    gp::USize m_collisionCount{ 0u };
    mutable gp::thread::AtomicFlag m_lock{};

public:
    /// @brief Maximum number of distinct hash entries stored by this registry.
    static constexpr gp::USize kSlotCapacity = InSlotCapacity;

    /// @brief Maximum character arena capacity in bytes.
    static constexpr gp::USize kArenaCapacity = InArenaBytes;

public:
    /// @brief Constructs an empty `BasicHashRegistry`.
    [[nodiscard]] BasicHashRegistry() noexcept = default;

    BasicHashRegistry(const BasicHashRegistry&) = delete;
    BasicHashRegistry& operator=(const BasicHashRegistry&) = delete;
    BasicHashRegistry(BasicHashRegistry&&) = delete;
    BasicHashRegistry& operator=(BasicHashRegistry&&) = delete;

    /// @brief Destructor.
    ~BasicHashRegistry() noexcept = default;

public:
    /// @brief Registers a precomputed 64-bit hash and its original string under a spinlock.
    /// @param[in] hash Precomputed 64-bit digest key.
    /// @param[in] text Original human-readable string (must be non-empty).
    /// @return True if inserted or already registered with identical text; false if @p text is empty, a different
    ///         string collided on @p hash, or slot/arena capacity is full.
    bool registerEntry(const gp::UInt64 hash, const gp::StringView text) noexcept
    {
        if (text.isEmpty())
        {
            return false;
        }

        acquireLock();

        gp::USize idx = static_cast<gp::USize>(hash) & kMask;
        for (gp::USize probe = 0u; probe < InSlotCapacity; ++probe)
        {
            Slot& slot = m_slots[idx];
            if (!slot.occupied)
            {
                if (m_arenaUsed + text.size() + 1u > InArenaBytes)
                {
                    releaseLock();
                    return false;
                }

                const gp::UInt32 writeOffset = static_cast<gp::UInt32>(m_arenaUsed);
                const gp::UInt32 writeLength = static_cast<gp::UInt32>(text.size());
                for (gp::USize c = 0u; c < text.size(); ++c)
                {
                    m_arena[m_arenaUsed + c] = text[c];
                }
                m_arena[m_arenaUsed + text.size()] = '\0';
                m_arenaUsed += text.size() + 1u;

                slot.hash = hash;
                slot.offset = writeOffset;
                slot.length = writeLength;
                slot.occupied = true;
                ++m_count;

                releaseLock();
                return true;
            }

            if (slot.hash == hash)
            {
                const gp::StringView existing(m_arena + slot.offset, static_cast<gp::USize>(slot.length));
                const bool exactMatch = (existing == text);
                if (!exactMatch)
                {
                    ++m_collisionCount;
                }
                releaseLock();
                return exactMatch;
            }

            idx = (idx + 1u) & kMask;
        }

        releaseLock();
        return false;
    }

    /// @brief Hashes @p text with @p InHasher, registers its 64-bit lookup key, and returns the native digest.
    /// @tparam InHasher Concrete hasher satisfying `concepts::IsStreamingHasher` (defaults to `Fnv1a64`).
    /// @param[in] text Input string to hash and record.
    /// @param[in] hashCase ASCII case-sensitivity mode (defaults to `HashCase::CaseSensitive`).
    /// @return Native digest computed by @p InHasher.
    template <concepts::IsStreamingHasher InHasher = Fnv1a64>
    [[nodiscard]] typename InHasher::DigestType
        registerString(const gp::StringView text, const HashCase hashCase = HashCase::CaseSensitive) noexcept
    {
        const typename InHasher::DigestType digest = InHasher::hash(text, hashCase);
        const gp::UInt64 key64 = toRegistryKey(digest);
        (void)registerEntry(key64, text);
        return digest;
    }

    /// @brief Looks up the original string associated with @p hash.
    /// @note The returned `gp::StringView` points into the append-only inline arena and remains valid until `clear()`.
    /// @param[in] hash 64-bit hash key to resolve.
    /// @return Registered string view, or an empty `gp::StringView` if not found.
    [[nodiscard]] gp::StringView lookup(const gp::UInt64 hash) const noexcept
    {
        acquireLock();

        gp::USize idx = static_cast<gp::USize>(hash) & kMask;
        for (gp::USize probe = 0u; probe < InSlotCapacity; ++probe)
        {
            const Slot& slot = m_slots[idx];
            if (!slot.occupied)
            {
                releaseLock();
                return gp::StringView{};
            }
            if (slot.hash == hash)
            {
                const gp::StringView found(m_arena + slot.offset, static_cast<gp::USize>(slot.length));
                releaseLock();
                return found;
            }
            idx = (idx + 1u) & kMask;
        }

        releaseLock();
        return gp::StringView{};
    }

    /// @brief Looks up the original string for any typed digest (`UInt32`, `UInt64`, `Hash128`, or `Hash256`).
    /// @tparam InDigestT Digest type satisfying `concepts::IsHashDigest`.
    /// @param[in] digest Digest value to resolve.
    /// @return Registered string view, or empty if absent.
    template <concepts::IsHashDigest InDigestT>
    [[nodiscard]] gp::StringView lookupDigest(const InDigestT& digest) const noexcept
    {
        return lookup(toRegistryKey(digest));
    }

    /// @brief Checks whether @p hash is currently registered.
    /// @param[in] hash 64-bit hash key to test.
    /// @return True if present, false otherwise.
    [[nodiscard]] bool contains(const gp::UInt64 hash) const noexcept
    {
        return !lookup(hash).isEmpty();
    }

    /// @brief Returns the number of registered entries.
    /// @return Active entry count (`<= kSlotCapacity`).
    [[nodiscard]] gp::USize size() const noexcept
    {
        acquireLock();
        const gp::USize count = m_count;
        releaseLock();
        return count;
    }

    /// @brief Returns the maximum slot capacity (`InSlotCapacity`).
    /// @return Slot capacity.
    [[nodiscard]] constexpr gp::USize capacity() const noexcept
    {
        return InSlotCapacity;
    }

    /// @brief Returns the number of bytes currently consumed in the inline string arena.
    /// @return Used arena bytes.
    [[nodiscard]] gp::USize usedArenaBytes() const noexcept
    {
        acquireLock();
        const gp::USize used = m_arenaUsed;
        releaseLock();
        return used;
    }

    /// @brief Returns the total inline character arena capacity (`InArenaBytes`).
    /// @return Arena capacity in bytes.
    [[nodiscard]] constexpr gp::USize arenaCapacity() const noexcept
    {
        return InArenaBytes;
    }

    /// @brief Returns the number of hash collisions detected across distinct strings.
    /// @return Cumulative collision count.
    [[nodiscard]] gp::USize collisionCount() const noexcept
    {
        acquireLock();
        const gp::USize collisions = m_collisionCount;
        releaseLock();
        return collisions;
    }

    /// @brief Clears all registered entries and resets the inline arena and collision counter.
    void clear() noexcept
    {
        acquireLock();
        for (gp::USize i = 0u; i < InSlotCapacity; ++i)
        {
            m_slots[i] = Slot{};
        }
        m_count = 0u;
        m_arenaUsed = 0u;
        m_collisionCount = 0u;
        releaseLock();
    }

private:
    template <concepts::IsHashDigest InDigestT>
    [[nodiscard]] static constexpr gp::UInt64 toRegistryKey(const InDigestT& digest) noexcept
    {
        if constexpr (
            gp::concepts::IsSameAs<std::remove_cvref_t<InDigestT>, Hash256> ||
            gp::concepts::IsSameAs<std::remove_cvref_t<InDigestT>, Hash128>
        )
        {
            return digest.foldTo64();
        }
        else
        {
            return static_cast<gp::UInt64>(digest);
        }
    }

    void acquireLock() const noexcept
    {
        while (m_lock.testAndSet(gp::MemoryOrder::Acquire))
        {
            gp::platform::Thread::spinWait();
        }
    }

    void releaseLock() const noexcept
    {
        m_lock.clear(gp::MemoryOrder::Release);
    }
};

/// @brief Default process-wide string-hash debug registry type (`1024` slots, `32 KiB` inline arena).
using HashRegistry = BasicHashRegistry<1024u, 32768u>;

/// @brief Returns the process-wide default `HashRegistry` instance for debugging, logging, and tools.
/// @return Reference to the global `HashRegistry`.
[[nodiscard]] GP_CORE_API HashRegistry& globalHashRegistry() noexcept;

}   // namespace gp::hashing
