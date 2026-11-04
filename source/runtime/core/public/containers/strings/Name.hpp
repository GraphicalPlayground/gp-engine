// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Memory.hpp"
#include "containers/ContainerForward.hpp"
#include "containers/strings/CharTraits.hpp"
#include "containers/strings/FixedString.hpp"
#include "containers/strings/NameEnums.hpp"
#include "containers/strings/NameTypes.hpp"
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "containers/strings/StringEnums.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "memory/MemoryBase.hpp"   // IWYU pragma: keep
#include "threading/atomics/Atomic.hpp"
#include "threading/atomics/AtomicFlag.hpp"
#include <charconv>
#include <compare>
#include <format>
#include <ostream>
#include <string_view>

namespace gp::container
{

/// @brief 64-shard, cache-line-isolated global string intern pool backing `gp::Name` (ADR-0046).
/// @details Partitions interned base strings across 64 independent shards selected by the upper 6 bits of the 64-bit
///          FNV-1a case-insensitive hash. Each shard bump-allocates `NameEntryHeader` + null-terminated UTF-8 strings
///          inside permanent `16 KiB` slabs so `resolveView()` and `resolveCString()` are $O(1)$, lock-free, and never
///          invalidated by hash-table growth.
class GP_CORE_API NamePool final
{
public:
    /// @brief Number of independent cache-line-isolated shards (`2^6 = 64`).
    static constexpr gp::USize kShardCount = 64u;

    /// @brief Byte size of each permanent bump-allocated string slab (`16 KiB`).
    static constexpr gp::USize kSlabSizeBytes = 16384u;

    /// @brief Maximum number of `16 KiB` slabs per shard (`512` slabs = `8 MiB` per shard, `512 MiB` total pool).
    static constexpr gp::USize kMaxSlabsPerShard = 512u;

    /// @brief Initial open-addressed probe table capacity per shard (allocated lazily on first insert).
    static constexpr gp::USize kInitialTableCapacity = 64u;

public:
    /// @brief Returns the process-wide singleton `NamePool` instance.
    /// @return Reference to the global `NamePool`.
    [[nodiscard]] static NamePool& instance() noexcept;

    /// @brief Splits an input identifier into its base string and optional Unreal-style `_N` instance suffix.
    /// @details A trailing `_N` is treated as a split numeric suffix if and only if:
    ///          1. There is at least one character before `'_'` (e.g., `"_0"` is kept as a literal base string).
    ///          2. The characters following `'_'` form a canonical non-negative decimal `gp::Int32` with no leading
    ///             zeros (`"_0"` or `"_[1-9][0-9]*"`, so `"Agent_007"` is preserved literally).
    /// @param[in] input Full input string view (e.g., `"Enemy_42"`).
    /// @param[out] outBase Receives the base string view (e.g., `"Enemy"`).
    /// @param[out] outInternalNumber Receives `kNameNoNumber` (`0`) if unnumbered, or `parsedNumber + 1` if numbered.
    /// @return True if a numeric suffix was split, false otherwise.
    static constexpr bool splitNameWithCheck(
        const BasicStringView<char> input, BasicStringView<char>& outBase, gp::Int32& outInternalNumber
    ) noexcept
    {
        outBase = input;
        outInternalNumber = kNameNoNumber;

        const gp::USize len = input.size();
        if (len < 3u)
        {
            return false;
        }

        // Scan backwards over trailing ASCII digits (at most 10 digits for Int32::max = 2147483647).
        gp::USize digitEnd = len;
        gp::USize cursor = len;
        while (cursor > 0u && CharTraits<char>::isDigit(input[cursor - 1u]))
        {
            --cursor;
        }

        const gp::USize digitCount = digitEnd - cursor;
        if (digitCount == 0u || digitCount > 10u || cursor < 2u || input[cursor - 1u] != '_')
        {
            return false;
        }

        // Reject leading zeros on multi-digit suffixes (e.g., "Agent_007" stays literal).
        if (digitCount > 1u && input[cursor] == '0')
        {
            return false;
        }

        gp::UInt64 parsed = 0u;
        for (gp::USize i = cursor; i < digitEnd; ++i)
        {
            parsed = (parsed * 10u) + static_cast<gp::UInt64>(input[i] - '0');
        }

        // Internal number is `parsed + 1`, which must fit in positive `gp::Int32` (`<= 2147483647`).
        constexpr gp::UInt64 kMaxExternalNumber = 2147483646ULL;
        if (parsed > kMaxExternalNumber)
        {
            return false;
        }

        outBase = input.substr(0u, cursor - 1u);
        outInternalNumber = static_cast<gp::Int32>(parsed + 1u);
        return true;
    }

    /// @brief Checks whether a base string view represents the canonical `"None"` / empty identifier.
    /// @param[in] baseStr Base string view to inspect.
    /// @return True if `baseStr` is empty or case-insensitively equal to `"None"`.
    [[nodiscard]] static constexpr bool isCanonicalNone(const BasicStringView<char> baseStr) noexcept
    {
        return baseStr.isEmpty() || baseStr.equalsIgnoreCase("None");
    }

public:
    /// @briefDeleted copy and move constructors (process-wide singleton).
    NamePool(const NamePool&) = delete;
    NamePool& operator=(const NamePool&) = delete;
    NamePool(NamePool&&) = delete;
    NamePool& operator=(NamePool&&) = delete;

    /// @brief Destructor. Releases all allocated shard probe tables and string slabs.
    ~NamePool() noexcept;

    /// @brief Finds or interns a base string in the pool according to `findMode`.
    /// @param[in] baseStr Unnumbered base string view (truncated to `kMaxNameLength` if longer).
    /// @param[in] findMode `NameFindMode::Add` to intern if missing, `NameFindMode::Find` for read-only lookup.
    /// @return Non-zero `NameEntryId` on match/insert, or `kNameNoneId` (`0`) if `"None"`, empty, or not found.
    [[nodiscard]] NameEntryId
        findOrAdd(BasicStringView<char> baseStr, NameFindMode findMode = NameFindMode::Add) noexcept;

    /// @brief Resolves a `NameEntryId` to its immutable `NameEntryHeader` in $O(1)$ lock-free time.
    /// @param[in] id Packed `NameEntryId` returned by `findOrAdd`.
    /// @return Pointer to the `NameEntryHeader`, or `nullptr` if `id == kNameNoneId` or invalid.
    [[nodiscard]] const NameEntryHeader* resolveEntry(NameEntryId id) const noexcept;

    /// @brief Resolves a `NameEntryId` to its permanent UTF-8 `StringView` in $O(1)$ lock-free time.
    /// @param[in] id Packed `NameEntryId`.
    /// @return `StringView` of the interned string, or `"None"` if `id == kNameNoneId`.
    [[nodiscard]] BasicStringView<char> resolveView(NameEntryId id) const noexcept;

    /// @brief Resolves a `NameEntryId` to its permanent null-terminated C-string in $O(1)$ lock-free time.
    /// @param[in] id Packed `NameEntryId`.
    /// @return Null-terminated `const char*` of the interned string, or `"None"` if `id == kNameNoneId`.
    [[nodiscard]] const char* resolveCString(NameEntryId id) const noexcept;

    /// @brief Resolves a `NameEntryId` to its precomputed 64-bit case-insensitive FNV-1a hash.
    /// @param[in] id Packed `NameEntryId`.
    /// @return 64-bit hash stored in `NameEntryHeader`, or `0` if `id == kNameNoneId`.
    [[nodiscard]] gp::UInt64 resolveHash(NameEntryId id) const noexcept;

    /// @brief Returns the total number of unique base strings currently interned across all 64 shards.
    /// @return Total interned base string count.
    [[nodiscard]] gp::USize entryCount() const noexcept;

    /// @brief Returns the total number of `16 KiB` slabs currently allocated across all 64 shards.
    /// @return Total allocated slab count.
    [[nodiscard]] gp::USize slabCount() const noexcept;

    /// @brief Returns the total number of slab bytes currently allocated across all 64 shards.
    /// @return Total slab memory footprint in bytes.
    [[nodiscard]] gp::USize allocatedSlabBytes() const noexcept;

private:
    static constexpr gp::USize kCacheLineAlignment = 64u;

    struct alignas(kCacheLineAlignment) Shard
    {
        gp::thread::AtomicFlag lock{};
        NameEntryId* slots{ nullptr };
        gp::USize slotCapacity{ 0u };
        gp::thread::Atomic<gp::USize> entryCount{ 0u };
        gp::USize activeSlabIndex{ 0u };
        gp::USize activeSlabOffset{ 0u };
        gp::thread::Atomic<gp::USize> allocatedSlabCount{ 0u };
        gp::thread::Atomic<gp::UInt8*> slabs[kMaxSlabsPerShard]{};
    };

    NamePool() noexcept = default;

    [[nodiscard]] static constexpr gp::UInt32 shardIndexFromHash(const gp::UInt64 hash) noexcept
    {
        return static_cast<gp::UInt32>((hash >> 58u) & 0x3Fu);
    }

    [[nodiscard]] static constexpr NameEntryId
        packEntryId(const gp::UInt32 shardIdx, const gp::UInt32 slabIdx, const gp::UInt32 byteOffset) noexcept
    {
        return kNameValidIdBit | ((shardIdx & 0x3Fu) << 25u) | ((slabIdx & 0x3FFFu) << 11u) |
               ((byteOffset >> 3u) & 0x7FFu);
    }

    static constexpr void unpackEntryId(
        const NameEntryId id, gp::UInt32& outShardIdx, gp::UInt32& outSlabIdx, gp::UInt32& outByteOffset
    ) noexcept
    {
        outShardIdx = (id >> 25u) & 0x3Fu;
        outSlabIdx = (id >> 11u) & 0x3FFFu;
        outByteOffset = (id & 0x7FFu) << 3u;
    }

    [[nodiscard]] const NameEntryHeader* resolveShardEntry(const Shard& shard, NameEntryId id) const noexcept;
    void growShardTable(Shard& shard, gp::USize newCapacity) noexcept;
    [[nodiscard]] NameEntryId allocateEntryInShard(
        Shard& shard, gp::UInt32 shardIdx, BasicStringView<char> baseStr, gp::UInt64 caseInsensitiveHash
    ) noexcept;

private:
    Shard m_shards[kShardCount]{};
};

/// @brief 8-byte, trivially copyable, $O(1)$-comparable interned string identifier (ADR-0046).
/// @details Combines a 32-bit `NameEntryId` handle into `gp::NamePool` with a 32-bit Unreal-style split numeric
///          instance suffix (`m_number`). Equality (`==`, `!=`) and fast table ordering (`<=>`) execute in a single
///          64-bit register comparison without dereferencing character memory.
class GP_CORE_API Name
{
public:
    /// @brief Default-constructs an empty `"None"` identifier (`isNone() == true`) with zero pool lookups.
    [[nodiscard]] constexpr Name() noexcept = default;

    /// @brief Constructs a `Name` from a UTF-8 `StringView`, automatically splitting any canonical `_N` suffix.
    /// @param[in] str Identifier string view (e.g., `"Pelvis"`, `"Enemy_42"`, or `"None"`).
    /// @param[in] findMode `NameFindMode::Add` to intern if new, `NameFindMode::Find` for non-inserting lookup.
    [[nodiscard]] explicit(false) Name(BasicStringView<char> str, NameFindMode findMode = NameFindMode::Add) noexcept;

    /// @brief Constructs a `Name` from a null-terminated UTF-8 C-string, splitting any canonical `_N` suffix.
    /// @param[in] str Null-terminated C-string (`nullptr` or `""` produces `Name()`).
    /// @param[in] findMode `NameFindMode::Add` to intern if new, `NameFindMode::Find` for non-inserting lookup.
    [[nodiscard]] explicit(false) Name(const char* str, NameFindMode findMode = NameFindMode::Add) noexcept;

    /// @brief Constructs a `Name` from an explicit base string and non-negative instance number (`"Base_number"`).
    /// @param[in] baseStr Unsplit base string (e.g., `"Enemy"`).
    /// @param[in] number External instance suffix (`>= 0` produces `"Base_number"`, `< 0` produces unnumbered
    /// `"Base"`).
    /// @param[in] findMode `NameFindMode::Add` to intern if new, `NameFindMode::Find` for non-inserting lookup.
    [[nodiscard]] Name(
        BasicStringView<char> baseStr, gp::Int32 number, NameFindMode findMode = NameFindMode::Add
    ) noexcept;

    /// @brief Constructs a `Name` directly from a pre-resolved `NameEntryId` and raw internal number.
    /// @param[in] comparisonId `NameEntryId` from `NamePool`.
    /// @param[in] rawInternalNumber Raw internal number (`kNameNoNumber` = `0`, or `externalNumber + 1`).
    /// @return Constructed `Name`.
    [[nodiscard]] static constexpr Name
        fromComparisonId(const NameEntryId comparisonId, const gp::Int32 rawInternalNumber = kNameNoNumber) noexcept
    {
        return Name(comparisonId, rawInternalNumber, InternalTag{});
    }

public:
    /// @brief Checks whether this name is `"None"` (`comparisonId() == kNameNoneId && !hasNumber()`).
    /// @return True if this name is `"None"`, false otherwise.
    [[nodiscard]] constexpr bool isNone() const noexcept
    {
        return m_comparisonId == kNameNoneId && m_number == kNameNoNumber;
    }

    /// @brief Alias for `isNone()`.
    /// @return True if this name is `"None"`, false otherwise.
    [[nodiscard]] constexpr bool isEmpty() const noexcept
    {
        return isNone();
    }

    /// @brief Checks whether this name is non-empty (`!isNone()`).
    /// @return True if valid (not `"None"`), false otherwise.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return !isNone();
    }

    /// @brief Contextual boolean conversion returning `!isNone()`.
    /// @return True if valid, false if `"None"`.
    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return !isNone();
    }

    /// @brief Returns the 32-bit `NameEntryId` handle in `NamePool`.
    /// @return `NameEntryId` (`kNameNoneId` if `"None"`).
    [[nodiscard]] constexpr NameEntryId comparisonId() const noexcept
    {
        return m_comparisonId;
    }

    /// @brief Returns the raw internal number (`0` = no suffix, `> 0` = `externalNumber + 1`).
    /// @return Raw internal number field.
    [[nodiscard]] constexpr gp::Int32 rawNumber() const noexcept
    {
        return m_number;
    }

    /// @brief Checks whether this name carries an `_N` numeric instance suffix.
    /// @return True if `m_number > kNameNoNumber`, false otherwise.
    [[nodiscard]] constexpr bool hasNumber() const noexcept
    {
        return m_number > kNameNoNumber;
    }

    /// @brief Returns the external numeric suffix (`0` for `"Enemy_0"`, `42` for `"Enemy_42"`, `0` if unnumbered).
    /// @return Non-negative suffix integer.
    [[nodiscard]] constexpr gp::Int32 getNumber() const noexcept
    {
        return m_number > kNameNoNumber ? (m_number - 1) : 0;
    }

    /// @brief Sets the external numeric suffix (`>= 0` sets `_N`, `< 0` clears the suffix).
    /// @param[in] number External instance suffix (`>= 0`), or negative to clear.
    constexpr void setNumber(const gp::Int32 number) noexcept
    {
        m_number = (number >= 0) ? (number + 1) : kNameNoNumber;
    }

    /// @brief Clears any `_N` numeric suffix on this name in-place.
    constexpr void clearNumber() noexcept
    {
        m_number = kNameNoNumber;
    }

    /// @brief Returns a copy of this `Name` with the specified external instance number (`O(1)`, zero pool lookup).
    /// @param[in] number External instance suffix (`>= 0`), or negative to clear.
    /// @return New `Name` sharing the same `comparisonId()`.
    [[nodiscard]] constexpr Name withNumber(const gp::Int32 number) const noexcept
    {
        return Name(m_comparisonId, (number >= 0) ? (number + 1) : kNameNoNumber, InternalTag{});
    }

    /// @brief Returns a copy of this `Name` with its numeric suffix removed (`O(1)`, zero pool lookup).
    /// @return Unnumbered base `Name`.
    [[nodiscard]] constexpr Name withoutNumber() const noexcept
    {
        return Name(m_comparisonId, kNameNoNumber, InternalTag{});
    }

    /// @brief Returns the unnumbered base string as a permanent `StringView` in $O(1)$ lock-free time.
    /// @note Does not include any `_N` numeric suffix; use `toString()` or `appendTo()` when the suffix is required.
    /// @return `StringView` pointing directly into the `NamePool` slab (`"None"` if `m_comparisonId == kNameNoneId`).
    [[nodiscard]] BasicStringView<char> baseView() const noexcept
    {
        return NamePool::instance().resolveView(m_comparisonId);
    }

    /// @brief Returns the unnumbered base string as a permanent null-terminated C-string in $O(1)$ lock-free time.
    /// @return Null-terminated `const char*` (`"None"` if `m_comparisonId == kNameNoneId`).
    [[nodiscard]] const char* baseCString() const noexcept
    {
        return NamePool::instance().resolveCString(m_comparisonId);
    }

    /// @brief Returns the precomputed 64-bit case-insensitive FNV-1a `NameHash` of the base string.
    /// @return `NameHash` of the base entry (`0` if `m_comparisonId == kNameNoneId`).
    [[nodiscard]] NameHash baseNameHash() const noexcept
    {
        return NameHash(NamePool::instance().resolveHash(m_comparisonId));
    }

    /// @brief Formats the full identifier (including `_N` suffix if `hasNumber()`) into a `gp::String`.
    /// @note Fits inside `gp::String`'s 22-byte inline SSO buffer with zero heap allocations for typical identifiers!
    /// @return Formatted `gp::String` (e.g., `"None"`, `"Pelvis"`, or `"Enemy_42"`).
    [[nodiscard]] String toString() const noexcept;

    /// @brief Formats the full identifier (including `_N` suffix if `hasNumber()`) into a stack-allocated
    /// `BasicFixedString<char, N>`.
    /// @tparam N Maximum character capacity of the fixed string.
    /// @return Stack-allocated `BasicFixedString<char, N>`.
    template <gp::USize N>
    [[nodiscard]] BasicFixedString<char, N> toFixedString() const noexcept
    {
        BasicFixedString<char, N> result(baseView());
        if (hasNumber())
        {
            char numBuf[16]{};
            numBuf[0] = '_';
            const gp::Int32 externalNum = getNumber();
            const std::to_chars_result res = std::to_chars(numBuf + 1, numBuf + sizeof(numBuf), externalNum, 10);
            if (res.ec == std::errc{})
            {
                result.append(BasicStringView<char>(numBuf, static_cast<gp::USize>(res.ptr - numBuf)));
            }
        }
        return result;
    }

    /// @brief Appends the full identifier (including `_N` suffix if `hasNumber()`) to `outStr`.
    /// @param[in,out] outStr Destination string to append into.
    void appendTo(String& outStr) const noexcept;

    /// @brief Computes a fast 64-bit hash combining `m_comparisonId` and `m_number` in $O(1)$ time.
    /// @return 64-bit hash value suitable for hash maps (`gp::Map`, `gp::Set`).
    [[nodiscard]] constexpr gp::UInt64 hash() const noexcept
    {
        const gp::UInt64 combined = (static_cast<gp::UInt64>(m_comparisonId) << 32u) |
                                    static_cast<gp::UInt64>(static_cast<gp::UInt32>(m_number));
        // MurmurHash3 64-bit finalizer (fmix64)
        gp::UInt64 h = combined ^ (combined >> 33u);
        h *= 0xFF51AFD7ED558CCDULL;
        h ^= h >> 33u;
        h *= 0xC4CEB9FE1A85EC53ULL;
        h ^= h >> 33u;
        return h;
    }

public:
    /// @brief $O(1)$ single-register equality comparison.
    /// @param[in] other Right-hand `Name` to compare against.
    /// @return True if both names share the same `comparisonId` and `rawNumber`.
    [[nodiscard]] constexpr bool operator==(const Name& other) const noexcept
    {
        return m_comparisonId == other.m_comparisonId && m_number == other.m_number;
    }

    /// @brief $O(1)$ fast index-based three-way comparison (orders by `comparisonId`, then `rawNumber`).
    /// @note Use `compareLexicographical()` when alphabetical ordering is required for UI display.
    /// @param[in] other Right-hand `Name` to compare against.
    /// @return `std::strong_ordering` of the packed `(m_comparisonId, m_number)` pair.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const Name& other) const noexcept
    {
        if (const auto cmp = m_comparisonId <=> other.m_comparisonId; cmp != 0)
        {
            return cmp;
        }
        return m_number <=> other.m_number;
    }

    /// @brief Compares two `Name` instances alphabetically (comparing base strings first, then numeric suffixes).
    /// @param[in] other Right-hand `Name` to compare against.
    /// @param[in] searchCase Case-sensitivity mode (defaults to `SearchCase::IgnoreCase`).
    /// @return Negative if `*this < other`, `0` if equal, positive if `*this > other`.
    [[nodiscard]] gp::Int32
        compareLexicographical(const Name& other, SearchCase searchCase = SearchCase::IgnoreCase) const noexcept;

    /// @brief Checks whether this `Name` matches the string `str` without inserting `str` into `NamePool`.
    /// @param[in] str Candidate string view (may include an `_N` suffix).
    /// @param[in] searchCase Case-sensitivity mode (defaults to `SearchCase::IgnoreCase`).
    /// @return True if this `Name` matches `str`.
    [[nodiscard]] bool
        equalsString(BasicStringView<char> str, SearchCase searchCase = SearchCase::IgnoreCase) const noexcept;

private:
    struct InternalTag
    {};

    constexpr Name(const NameEntryId comparisonId, const gp::Int32 rawNumber, InternalTag) noexcept
        : m_comparisonId(comparisonId)
        , m_number(rawNumber)
    {}

private:
    NameEntryId m_comparisonId{ kNameNoneId };
    gp::Int32 m_number{ kNameNoNumber };
};

static_assert(sizeof(Name) == 8u, "gp::Name must be exactly 8 bytes.");
static_assert(alignof(Name) == 4u, "gp::Name must be 4-byte aligned.");
static_assert(gp::concepts::IsRelocatable<Name>, "gp::Name must satisfy gp::concepts::IsRelocatable.");

/// @brief Stream insertion operator for `gp::container::Name`.
/// @param[in,out] os Output stream.
/// @param[in] name Name to write.
/// @return Reference to `os`.
inline std::ostream& operator<<(std::ostream& os, const Name& name)
{
    const String str = name.toString();
    return os.write(str.data(), static_cast<std::streamsize>(str.size()));
}

}   // namespace gp::container

namespace gp
{

using NamePool = container::NamePool;
using Name = container::Name;

}   // namespace gp

namespace std
{

/// @brief `std::formatter` specialization for `gp::container::Name`.
template <>
struct formatter<gp::container::Name, char> : formatter<string_view, char>
{
    /// @brief Formats `name` into `ctx`.
    /// @tparam InFormatContext Format context type.
    /// @param[in] name The `gp::Name` to format.
    /// @param[in,out] ctx The format context.
    /// @return Iterator past the end of the formatted output.
    template <typename InFormatContext>
    auto format(const gp::container::Name& name, InFormatContext& ctx) const
    {
        const gp::String str = name.toString();
        return formatter<string_view, char>::format(string_view(str.data(), str.size()), ctx);
    }
};

}   // namespace std
