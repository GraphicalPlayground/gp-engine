// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "maths/base/Bitwise.hpp"
#include "memory/Memory.hpp"
#include <compare>
#include <iterator>
#include <type_traits>

namespace gp
{

using math::getAndClearHighestBit;
using math::getAndClearNextBit;
using math::tryGetAndClearHighestBit;
using math::tryGetAndClearNextBit;

}   // namespace gp

namespace gp::container::detail
{

/// @brief Canonical 64-bit storage word used by `BitArray` and `BitSet`.
/// @details Packing 64 bits per machine word places 512 entity or occupancy flags inside one 64-byte hardware cache
/// line
///          and enables single-instruction `popcnt`, `tzcnt`, and `blsr` sweeps across full machine words.
/// @details Packing 64 bits per machine word places 512 entity or occupancy flags inside one 64-byte hardware cache
/// line
///          and enables single-instruction `popcnt`, `tzcnt`, and `blsr` sweeps across full machine words.
using BitWord = gp::UInt64;

/// @brief Number of bits packed into each `BitWord`.
inline constexpr gp::USize kBitsPerWord = 64u;

/// @brief Bit shift converting a bit index to its containing `BitWord` index (`index >> 6`).
inline constexpr gp::USize kWordIndexShift = 6u;

/// @brief Bitmask extracting a bit's intra-word offset (`index & 63`).
inline constexpr gp::USize kBitInWordMask = 63u;

/// @brief Sentinel index returned when a bit search finds no match.
inline constexpr gp::USize kBitNpos = static_cast<gp::USize>(-1);

/// @brief Returns the minimum number of 64-bit words required to hold `numBits` bits.
/// @param[in] numBits Logical bit count.
/// @return Required number of `BitWord` elements (`0` when `numBits == 0`).
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::USize wordCountForBits(const gp::USize numBits) noexcept
{
    if (numBits == 0u)
    {
        return 0u;
    }
    return ((numBits - 1u) >> kWordIndexShift) + 1u;
}

/// @brief Returns a bitmask covering only the valid low bits in the final word for `numBits` total bits.
/// @param[in] numBits Total logical bit count (`0` produces `0`, exact multiples of 64 produce `~0ULL`).
/// @return Valid-bit mask for the final storage word.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr BitWord lastWordMask(const gp::USize numBits) noexcept
{
    if (numBits == 0u)
    {
        return BitWord{ 0 };
    }
    const gp::USize remainder = numBits & kBitInWordMask;
    if (remainder == 0u)
    {
        return ~BitWord{ 0 };
    }
    return (BitWord{ 1 } << remainder) - BitWord{ 1 };
}

/// @brief Clears unused high padding bits in the final word so word-wide scans stay canonical.
/// @param[in,out] words Backing word buffer.
/// @param[in] numBits Logical bit count.
GP_FORCEINLINE_HINT constexpr void sanitizeTailWord(BitWord* const words, const gp::USize numBits) noexcept
{
    if (numBits == 0u || words == nullptr)
    {
        return;
    }
    const gp::USize remainder = numBits & kBitInWordMask;
    if (remainder != 0u)
    {
        words[(numBits - 1u) >> kWordIndexShift] &= (BitWord{ 1 } << remainder) - BitWord{ 1 };
    }
}

/// @brief Zeroes `wordCount` storage words in both `constexpr` and runtime execution.
/// @param[out] words Destination word buffer.
/// @param[in] wordCount Number of words to zero.
GP_FORCEINLINE_HINT constexpr void zeroWords(BitWord* const words, const gp::USize wordCount) noexcept
{
    if (wordCount == 0u || words == nullptr)
    {
        return;
    }
    if (!std::is_constant_evaluated())
    {
        gp::memory::zeroMemory(words, wordCount * sizeof(BitWord));
        return;
    }
    for (gp::USize i = 0u; i < wordCount; ++i)
    {
        words[i] = BitWord{ 0 };
    }
}

/// @brief Sets `wordCount` complete storage words to all `1` bits (`~0ULL`).
/// @param[out] words Destination word buffer.
/// @param[in] wordCount Number of full 64-bit words to fill with 1 bits.
GP_FORCEINLINE_HINT constexpr void fillFullWordsOnes(BitWord* const words, const gp::USize wordCount) noexcept
{
    if (wordCount == 0u || words == nullptr)
    {
        return;
    }
    if (!std::is_constant_evaluated())
    {
        gp::memory::setMemory(words, 0xFF, wordCount * sizeof(BitWord));
        return;
    }
    for (gp::USize i = 0u; i < wordCount; ++i)
    {
        words[i] = ~BitWord{ 0 };
    }
}

/// @brief Sets all logical bits `[0, numBits)` to 1 and zeroes trailing padding bits in the final word.
/// @param[out] words Destination word buffer.
/// @param[in] numBits Number of logical bits to fill with 1.
GP_FORCEINLINE_HINT constexpr void fillOnesWords(BitWord* const words, const gp::USize numBits) noexcept
{
    if (numBits == 0u || words == nullptr)
    {
        return;
    }
    fillFullWordsOnes(words, wordCountForBits(numBits));
    sanitizeTailWord(words, numBits);
}

/// @brief Copies `wordCount` words from `source` to disjoint `destination`.
/// @param[out] destination Destination word buffer.
/// @param[in] source Source word buffer.
/// @param[in] wordCount Number of words to copy.
GP_FORCEINLINE_HINT constexpr void
    copyWords(BitWord* const destination, const BitWord* const source, const gp::USize wordCount) noexcept
{
    if (wordCount == 0u || destination == nullptr || source == nullptr)
    {
        return;
    }
    if (!std::is_constant_evaluated())
    {
        gp::memory::copyMemory(destination, source, wordCount * sizeof(BitWord));
        return;
    }
    for (gp::USize i = 0u; i < wordCount; ++i)
    {
        destination[i] = source[i];
    }
}

/// @brief Writable proxy reference to a single bit inside a `BitWord`.
/// @details Assignment writes through to the target bit rather than rebinding the proxy, matching `std::bitset` and
///          Unreal's `FBitReference`. Leaving mutating operators `const`-qualified allows C++23 proxy ranges and
///          `std::ranges::swap` to operate on temporary references returned by `operator*()`.
class BitReference
{
public:
    /// @brief Constructs a reference bound to the single bit selected by `mask` in `*word`.
    /// @param[in] word Pointer to the owning 64-bit storage word.
    /// @param[in] mask Single-bit mask `(1ULL << bitOffset)`.
    constexpr BitReference(BitWord* const word, const BitWord mask) noexcept
        : m_word(word)
        , m_mask(mask)
    {}

    /// @brief Default copy constructor (preserves binding to the same bit slot).
    constexpr BitReference(const BitReference&) noexcept = default;

public:
    /// @brief Assigns a boolean value to the referenced bit.
    /// @param[in] value Desired bit state.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr const BitReference& operator=(const bool value) const noexcept
    {
        *m_word = (*m_word & ~m_mask) | (value ? m_mask : BitWord{ 0 });
        return *this;
    }

    /// @brief Copies the boolean value from `other` into the referenced bit.
    /// @param[in] other Source bit reference.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr const BitReference& operator=(const BitReference& other) const noexcept
    {
        return *this = static_cast<bool>(other);
    }

    /// @brief Reads the referenced bit as a `bool`.
    /// @return True if the bit is 1, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr operator bool() const noexcept
    {
        return (*m_word & m_mask) != BitWord{ 0 };
    }

    /// @brief Returns the logical negation of the referenced bit.
    /// @return True if the bit is 0, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool operator~() const noexcept
    {
        return (*m_word & m_mask) == BitWord{ 0 };
    }

    /// @brief Toggles the referenced bit in place.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr const BitReference& flip() const noexcept
    {
        *m_word ^= m_mask;
        return *this;
    }

    /// @brief Bitwise AND assignment with `value`.
    /// @param[in] value Right-hand boolean operand.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr const BitReference& operator&=(const bool value) const noexcept
    {
        if (!value)
        {
            *m_word &= ~m_mask;
        }
        return *this;
    }

    /// @brief Bitwise OR assignment with `value`.
    /// @param[in] value Right-hand boolean operand.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr const BitReference& operator|=(const bool value) const noexcept
    {
        if (value)
        {
            *m_word |= m_mask;
        }
        return *this;
    }

    /// @brief Bitwise XOR assignment with `value`.
    /// @param[in] value Right-hand boolean operand.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr const BitReference& operator^=(const bool value) const noexcept
    {
        if (value)
        {
            *m_word ^= m_mask;
        }
        return *this;
    }

    /// @brief Exchanges the bit values referenced by `*this` and `other`.
    /// @param[in] other Target bit reference.
    constexpr void swap(const BitReference& other) const noexcept
    {
        const bool tmp = static_cast<bool>(*this);
        *this = static_cast<bool>(other);
        other = tmp;
    }

    /// @brief Exchanges the bit values referenced by `lhs` and `rhs`.
    /// @param[in] lhs First bit reference.
    /// @param[in] rhs Second bit reference.
    friend constexpr void swap(const BitReference& lhs, const BitReference& rhs) noexcept
    {
        lhs.swap(rhs);
    }

private:
    BitWord* m_word{ nullptr };
    BitWord m_mask{ 0ULL };
};

/// @brief Random-access iterator stepping bit by bit over `[0, size)` in a `BitArray` or `BitSet`.
/// @tparam InIsConst True for read-only `bool` yields, false for writable `BitReference` yields.
template <bool InIsConst>
class BitIterator
{
public:
    using iterator_category = std::random_access_iterator_tag;
    using iterator_concept = std::random_access_iterator_tag;
    using value_type = bool;
    using difference_type = gp::ISize;
    using pointer = void;
    using reference = std::conditional_t<InIsConst, bool, BitReference>;
    using WordPointer = std::conditional_t<InIsConst, const BitWord*, BitWord*>;

public:
    /// @brief Constructs an unassociated iterator.
    constexpr BitIterator() noexcept = default;

    /// @brief Constructs an iterator addressing `bitIndex` inside `words`.
    /// @param[in] words Backing word buffer pointer.
    /// @param[in] bitIndex Zero-based bit cursor.
    constexpr BitIterator(const WordPointer words, const gp::USize bitIndex) noexcept
        : m_words(words)
        , m_bitIndex(bitIndex)
    {}

    /// @brief Implicit conversion from mutable `BitIterator<false>` to `BitIterator<true>`.
    /// @tparam InOtherConst Source constness (`false` only).
    /// @param[in] other Mutable bit iterator.
    template <bool InOtherConst>
    requires(InIsConst && !InOtherConst) constexpr BitIterator(const BitIterator<InOtherConst>& other) noexcept
        : m_words(other.m_words)
        , m_bitIndex(other.m_bitIndex)
    {}

public:
    /// @brief Dereferences the bit at the current cursor.
    /// @return `BitReference` (mutable) or `bool` (const).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr reference operator*() const noexcept
    {
        GP_ASSERT(m_words != nullptr && "Cannot dereference an unassociated BitIterator.");
        const gp::USize wordIdx = m_bitIndex >> kWordIndexShift;
        const BitWord mask = BitWord{ 1 } << (m_bitIndex & kBitInWordMask);
        if constexpr (InIsConst)
        {
            return (m_words[wordIdx] & mask) != BitWord{ 0 };
        }
        else
        {
            return BitReference(m_words + wordIdx, mask);
        }
    }

    /// @brief Random-access indexing relative to the current cursor.
    /// @param[in] offset Signed bit offset.
    /// @return `BitReference` (mutable) or `bool` (const).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr reference operator[](const difference_type offset) const noexcept
    {
        return *(*this + offset);
    }

    /// @brief Pre-increments the cursor by one bit.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr BitIterator& operator++() noexcept
    {
        ++m_bitIndex;
        return *this;
    }

    /// @brief Post-increments the cursor by one bit.
    /// @return Copy of the iterator prior to incrementing.
    GP_FORCEINLINE_HINT constexpr BitIterator operator++(int) noexcept
    {
        const BitIterator copy(*this);
        ++m_bitIndex;
        return copy;
    }

    /// @brief Pre-decrements the cursor by one bit.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr BitIterator& operator--() noexcept
    {
        --m_bitIndex;
        return *this;
    }

    /// @brief Post-decrements the cursor by one bit.
    /// @return Copy of the iterator prior to decrementing.
    GP_FORCEINLINE_HINT constexpr BitIterator operator--(int) noexcept
    {
        const BitIterator copy(*this);
        --m_bitIndex;
        return copy;
    }

    /// @brief Advances the cursor by `offset` bits.
    /// @param[in] offset Signed bit step.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr BitIterator& operator+=(const difference_type offset) noexcept
    {
        m_bitIndex = static_cast<gp::USize>(static_cast<difference_type>(m_bitIndex) + offset);
        return *this;
    }

    /// @brief Moves the cursor backward by `offset` bits.
    /// @param[in] offset Signed bit step.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr BitIterator& operator-=(const difference_type offset) noexcept
    {
        m_bitIndex = static_cast<gp::USize>(static_cast<difference_type>(m_bitIndex) - offset);
        return *this;
    }

    /// @brief Returns an iterator advanced by `offset` bits.
    /// @param[in] it Base iterator.
    /// @param[in] offset Signed bit offset.
    /// @return Shifted iterator copy.
    [[nodiscard]] friend constexpr BitIterator operator+(BitIterator it, const difference_type offset) noexcept
    {
        it += offset;
        return it;
    }

    /// @brief Returns an iterator advanced by `offset` bits (commuted operands).
    /// @param[in] offset Signed bit offset.
    /// @param[in] it Base iterator.
    /// @return Shifted iterator copy.
    [[nodiscard]] friend constexpr BitIterator operator+(const difference_type offset, BitIterator it) noexcept
    {
        it += offset;
        return it;
    }

    /// @brief Returns an iterator moved backward by `offset` bits.
    /// @param[in] it Base iterator.
    /// @param[in] offset Signed bit offset.
    /// @return Shifted iterator copy.
    [[nodiscard]] friend constexpr BitIterator operator-(BitIterator it, const difference_type offset) noexcept
    {
        it -= offset;
        return it;
    }

    /// @brief Computes signed bit distance from `rhs` to `lhs`.
    /// @param[in] lhs Left-hand iterator.
    /// @param[in] rhs Right-hand iterator.
    /// @return Signed bit offset.
    [[nodiscard]] friend constexpr difference_type operator-(const BitIterator& lhs, const BitIterator& rhs) noexcept
    {
        return static_cast<difference_type>(lhs.m_bitIndex) - static_cast<difference_type>(rhs.m_bitIndex);
    }

    /// @brief Compares two iterators for position equality.
    /// @param[in] other Right-hand iterator.
    /// @return True if both refer to the same buffer and bit index.
    [[nodiscard]] constexpr bool operator==(const BitIterator& other) const noexcept
    {
        return m_bitIndex == other.m_bitIndex && m_words == other.m_words;
    }

    /// @brief Orders two iterators by bit cursor position.
    /// @param[in] other Right-hand iterator.
    /// @return Three-way comparison ordering of bit indices.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const BitIterator& other) const noexcept
    {
        GP_ASSERT(m_words == other.m_words && "Cannot order BitIterators from different bit containers.");
        return m_bitIndex <=> other.m_bitIndex;
    }

private:
    template <bool InOtherConst>
    friend class BitIterator;

    WordPointer m_words{ nullptr };
    gp::USize m_bitIndex{ 0u };
};

/// @brief Word-cached forward iterator visiting only set (`1`) or cleared (`0`) bit indices.
/// @details Caches the active 64-bit word in a local register-sized field and extracts indices with
///          `getAndClearNextBit()`. Empty 64-bit words are skipped in one comparison instead of 64 branch checks.
/// @tparam InMatchZeroBits False to iterate set bits (`1`), true to iterate zero bits (`0`).
template <bool InMatchZeroBits>
class BitScanIterator
{
public:
    using iterator_category = std::forward_iterator_tag;
    using iterator_concept = std::forward_iterator_tag;
    using value_type = gp::USize;
    using difference_type = gp::ISize;
    using pointer = void;
    using reference = gp::USize;

public:
    /// @brief Constructs an end sentinel iterator.
    constexpr BitScanIterator() noexcept = default;

    /// @brief Constructs a scan iterator starting at `startBit` within `[0, numBits)`.
    /// @param[in] words Backing word buffer pointer.
    /// @param[in] numBits Total logical bit count.
    /// @param[in] startBit First bit index to inspect (or `numBits` for `end()`).
    constexpr BitScanIterator(const BitWord* const words, const gp::USize numBits, const gp::USize startBit) noexcept
        : m_words(words)
        , m_numBits(numBits)
        , m_wordCount(wordCountForBits(numBits))
        , m_wordIndex(m_wordCount)
    {
        if (m_words == nullptr || startBit >= m_numBits)
        {
            return;
        }

        m_wordIndex = startBit >> kWordIndexShift;
        const BitWord startMask = (~BitWord{ 0 }) << (startBit & kBitInWordMask);
        m_currentWord = loadMaskedWord(m_wordIndex) & startMask;
        advanceToNonZeroWord();
    }

public:
    /// @brief Returns the zero-based index of the current matching bit.
    /// @return Bit index in `[0, numBits)`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::USize operator*() const noexcept
    {
        GP_ASSERT(m_currentWord != BitWord{ 0 } && "Cannot dereference an end bit-scan iterator.");
        return (m_wordIndex << kWordIndexShift) + static_cast<gp::USize>(gp::math::countTrailingZeros(m_currentWord));
    }

    /// @brief Advances to the next matching bit using `getAndClearNextBit()`.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr BitScanIterator& operator++() noexcept
    {
        GP_ASSERT(m_currentWord != BitWord{ 0 } && "Cannot increment an end bit-scan iterator.");
        (void)gp::math::getAndClearNextBit(m_currentWord);
        advanceToNonZeroWord();
        return *this;
    }

    /// @brief Post-increments to the next matching bit.
    /// @return Copy of the iterator prior to advancing.
    GP_FORCEINLINE_HINT constexpr BitScanIterator operator++(int) noexcept
    {
        const BitScanIterator copy(*this);
        ++(*this);
        return copy;
    }

    /// @brief Compares two scan iterators for position equality.
    /// @param[in] other Right-hand scan iterator.
    /// @return True if both reference the same buffer, word index, and remaining bit mask (including `end()`).
    [[nodiscard]] constexpr bool operator==(const BitScanIterator& other) const noexcept
    {
        return m_words == other.m_words && m_wordIndex == other.m_wordIndex && m_currentWord == other.m_currentWord;
    }

private:
    /// @brief Loads word `wordIdx`, inverting and tail-masking when scanning zero bits.
    /// @param[in] wordIdx Word index to load from `m_words`.
    /// @return Word mask whose set bits identify matching positions in word `wordIdx`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr BitWord loadMaskedWord(const gp::USize wordIdx) const noexcept
    {
        BitWord word = m_words[wordIdx];
        if constexpr (InMatchZeroBits)
        {
            word = ~word;
            if ((wordIdx + 1u) == m_wordCount)
            {
                word &= lastWordMask(m_numBits);
            }
        }
        return word;
    }

    /// @brief Skips forward across any empty 64-bit words until a match or `m_wordCount` is reached.
    GP_FORCEINLINE_HINT constexpr void advanceToNonZeroWord() noexcept
    {
        while (m_currentWord == BitWord{ 0 })
        {
            ++m_wordIndex;
            if (m_wordIndex >= m_wordCount)
            {
                break;
            }
            m_currentWord = loadMaskedWord(m_wordIndex);
        }
    }

private:
    const BitWord* m_words{ nullptr };
    gp::USize m_numBits{ 0u };
    gp::USize m_wordCount{ 0u };
    gp::USize m_wordIndex{ 0u };
    BitWord m_currentWord{ 0ULL };
};

/// @brief Forward iterator yielding set (`1`) bit indices.
using SetBitIterator = BitScanIterator<false>;

/// @brief Forward iterator yielding cleared (`0`) bit indices.
using ZeroBitIterator = BitScanIterator<true>;

/// @brief Lightweight range adapter enabling `for (gp::USize idx : bits.setBits())` and `zeroBits()`.
/// @tparam InMatchZeroBits False for set-bit indices, true for zero-bit indices.
template <bool InMatchZeroBits>
class BitScanRange
{
public:
    using Iterator = BitScanIterator<InMatchZeroBits>;
    using ConstIterator = BitScanIterator<InMatchZeroBits>;

public:
    /// @brief Constructs a range covering `[startBit, numBits)` over `words`.
    /// @param[in] words Backing word buffer pointer.
    /// @param[in] numBits Total logical bit count.
    /// @param[in] startBit First bit index included in the scan.
    constexpr BitScanRange(const BitWord* const words, const gp::USize numBits, const gp::USize startBit = 0u) noexcept
        : m_words(words)
        , m_numBits(numBits)
        , m_startBit(startBit)
    {}

    /// @brief Returns an iterator to the first matching bit at or after `m_startBit`.
    /// @return Start scan iterator.
    [[nodiscard]] constexpr Iterator begin() const noexcept
    {
        return Iterator(m_words, m_numBits, m_startBit);
    }

    /// @brief Returns the end sentinel scan iterator.
    /// @return End scan iterator.
    [[nodiscard]] constexpr Iterator end() const noexcept
    {
        return Iterator(m_words, m_numBits, m_numBits);
    }

private:
    const BitWord* m_words{ nullptr };
    gp::USize m_numBits{ 0u };
    gp::USize m_startBit{ 0u };
};

/// @brief Range view over all set (`1`) bit indices in a `BitArray` or `BitSet`.
using SetBitRange = BitScanRange<false>;

/// @brief Range view over all cleared (`0`) bit indices in a `BitArray` or `BitSet`.
using ZeroBitRange = BitScanRange<true>;

static_assert(std::random_access_iterator<BitIterator<false>>, "BitIterator<false> must be random access.");
static_assert(std::random_access_iterator<BitIterator<true>>, "BitIterator<true> must be random access.");
static_assert(std::forward_iterator<SetBitIterator>, "SetBitIterator must be a forward iterator.");
static_assert(std::forward_iterator<ZeroBitIterator>, "ZeroBitIterator must be a forward iterator.");

/// @brief Tests whether bit `index` is set inside `words`.
/// @param[in] words Backing word buffer.
/// @param[in] index Valid bit index.
/// @return True if bit `index` is 1, false otherwise.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool testBit(const BitWord* const words, const gp::USize index) noexcept
{
    return (words[index >> kWordIndexShift] & (BitWord{ 1 } << (index & kBitInWordMask))) != BitWord{ 0 };
}

/// @brief Sets bit `index` to 1 inside `words`.
/// @param[in,out] words Backing word buffer.
/// @param[in] index Valid bit index.
GP_FORCEINLINE_HINT constexpr void setBit(BitWord* const words, const gp::USize index) noexcept
{
    words[index >> kWordIndexShift] |= (BitWord{ 1 } << (index & kBitInWordMask));
}

/// @brief Clears bit `index` to 0 inside `words`.
/// @param[in,out] words Backing word buffer.
/// @param[in] index Valid bit index.
GP_FORCEINLINE_HINT constexpr void clearBit(BitWord* const words, const gp::USize index) noexcept
{
    words[index >> kWordIndexShift] &= ~(BitWord{ 1 } << (index & kBitInWordMask));
}

/// @brief Assigns `value` to bit `index` inside `words` without branches.
/// @param[in,out] words Backing word buffer.
/// @param[in] index Valid bit index.
/// @param[in] value Desired bit state.
GP_FORCEINLINE_HINT constexpr void assignBit(BitWord* const words, const gp::USize index, const bool value) noexcept
{
    BitWord& word = words[index >> kWordIndexShift];
    const BitWord mask = BitWord{ 1 } << (index & kBitInWordMask);
    word = (word & ~mask) | (value ? mask : BitWord{ 0 });
}

/// @brief Toggles bit `index` inside `words`.
/// @param[in,out] words Backing word buffer.
/// @param[in] index Valid bit index.
GP_FORCEINLINE_HINT constexpr void flipBit(BitWord* const words, const gp::USize index) noexcept
{
    words[index >> kWordIndexShift] ^= (BitWord{ 1 } << (index & kBitInWordMask));
}

/// @brief Sets bit `index` to 1 and returns its previous value.
/// @param[in,out] words Backing word buffer.
/// @param[in] index Valid bit index.
/// @return Previous boolean value of bit `index`.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool testAndSetBit(BitWord* const words, const gp::USize index) noexcept
{
    BitWord& word = words[index >> kWordIndexShift];
    const BitWord mask = BitWord{ 1 } << (index & kBitInWordMask);
    const bool previous = (word & mask) != BitWord{ 0 };
    word |= mask;
    return previous;
}

/// @brief Clears bit `index` to 0 and returns its previous value.
/// @param[in,out] words Backing word buffer.
/// @param[in] index Valid bit index.
/// @return Previous boolean value of bit `index`.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool testAndClearBit(BitWord* const words, const gp::USize index) noexcept
{
    BitWord& word = words[index >> kWordIndexShift];
    const BitWord mask = BitWord{ 1 } << (index & kBitInWordMask);
    const bool previous = (word & mask) != BitWord{ 0 };
    word &= ~mask;
    return previous;
}

/// @brief Decomposed word-index bounds and edge bitmasks for a contiguous bit slice `[startBit, startBit + count)`.
struct BitRangeSpan
{
    gp::USize firstWord{ 0u };
    gp::USize lastWord{ 0u };
    BitWord startMask{ 0ULL };
    BitWord endMask{ 0ULL };
};

/// @brief Computes word indices and partial-word edge masks for a non-empty range `[startBit, startBit + count)`.
/// @param[in] startBit Starting bit index.
/// @param[in] count Positive number of bits in the slice (`count > 0`).
/// @return `BitRangeSpan` holding first/last word indices and boundary bitmasks.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr BitRangeSpan
    makeBitRangeSpan(const gp::USize startBit, const gp::USize count) noexcept
{
    const gp::USize endBit = startBit + count;
    const gp::USize endOffset = endBit & kBitInWordMask;
    return BitRangeSpan{
        .firstWord = startBit >> kWordIndexShift,
        .lastWord = (endBit - 1u) >> kWordIndexShift,
        .startMask = (~BitWord{ 0 }) << (startBit & kBitInWordMask),
        .endMask = (endOffset == 0u) ? (~BitWord{ 0 }) : ((BitWord{ 1 } << endOffset) - BitWord{ 1 }),
    };
}

/// @brief Sets all bits in `[startBit, startBit + count)` to 1 using word-level masking.
/// @param[in,out] words Backing word buffer.
/// @param[in] startBit Starting bit index.
/// @param[in] count Number of consecutive bits to set.
constexpr void setBitRange(BitWord* const words, const gp::USize startBit, const gp::USize count) noexcept
{
    if (count == 0u || words == nullptr)
    {
        return;
    }

    const BitRangeSpan span = makeBitRangeSpan(startBit, count);
    if (span.firstWord == span.lastWord)
    {
        words[span.firstWord] |= (span.startMask & span.endMask);
        return;
    }

    words[span.firstWord] |= span.startMask;
    fillFullWordsOnes(words + span.firstWord + 1u, span.lastWord - span.firstWord - 1u);
    words[span.lastWord] |= span.endMask;
}

/// @brief Clears all bits in `[startBit, startBit + count)` to 0 using word-level masking.
/// @param[in,out] words Backing word buffer.
/// @param[in] startBit Starting bit index.
/// @param[in] count Number of consecutive bits to clear.
constexpr void clearBitRange(BitWord* const words, const gp::USize startBit, const gp::USize count) noexcept
{
    if (count == 0u || words == nullptr)
    {
        return;
    }

    const BitRangeSpan span = makeBitRangeSpan(startBit, count);
    if (span.firstWord == span.lastWord)
    {
        words[span.firstWord] &= ~(span.startMask & span.endMask);
        return;
    }

    words[span.firstWord] &= ~span.startMask;
    zeroWords(words + span.firstWord + 1u, span.lastWord - span.firstWord - 1u);
    words[span.lastWord] &= ~span.endMask;
}

/// @brief Flips all bits in `[startBit, startBit + count)` using word-level masking.
/// @param[in,out] words Backing word buffer.
/// @param[in] startBit Starting bit index.
/// @param[in] count Number of consecutive bits to flip.
constexpr void flipBitRange(BitWord* const words, const gp::USize startBit, const gp::USize count) noexcept
{
    if (count == 0u || words == nullptr)
    {
        return;
    }

    const BitRangeSpan span = makeBitRangeSpan(startBit, count);
    if (span.firstWord == span.lastWord)
    {
        words[span.firstWord] ^= (span.startMask & span.endMask);
        return;
    }

    words[span.firstWord] ^= span.startMask;
    for (gp::USize w = span.firstWord + 1u; w < span.lastWord; ++w)
    {
        words[w] = ~words[w];
    }
    words[span.lastWord] ^= span.endMask;
}

/// @brief Inverts all logical bits `[0, numBits)` and keeps trailing padding bits cleared.
/// @param[in,out] words Backing word buffer.
/// @param[in] numBits Logical bit count.
constexpr void flipAllWords(BitWord* const words, const gp::USize numBits) noexcept
{
    if (numBits == 0u || words == nullptr)
    {
        return;
    }
    const gp::USize numWords = wordCountForBits(numBits);
    for (gp::USize w = 0u; w < numWords; ++w)
    {
        words[w] = ~words[w];
    }
    sanitizeTailWord(words, numBits);
}

/// @brief Counts the total number of set bits across `wordCount` sanitized words.
/// @details Sums 64-bit hardware `popcnt` counts across contiguous words for linear cache throughput.
/// @param[in] words Backing word buffer.
/// @param[in] wordCount Number of active words.
/// @return Population count across all words.
[[nodiscard]] constexpr gp::USize countSetBits(const BitWord* const words, const gp::USize wordCount) noexcept
{
    if (words == nullptr || wordCount == 0u)
    {
        return 0u;
    }
    gp::USize total = 0u;
    for (gp::USize w = 0u; w < wordCount; ++w)
    {
        total += static_cast<gp::USize>(gp::math::countBits(words[w]));
    }
    return total;
}

/// @brief Counts set bits within `[startBit, startBit + count)` using word-level masking.
/// @param[in] words Backing word buffer.
/// @param[in] startBit Starting bit index.
/// @param[in] count Number of bits to inspect.
/// @return Number of set bits inside the slice.
[[nodiscard]] constexpr gp::USize
    countSetBitsInRange(const BitWord* const words, const gp::USize startBit, const gp::USize count) noexcept
{
    if (count == 0u || words == nullptr)
    {
        return 0u;
    }

    const BitRangeSpan span = makeBitRangeSpan(startBit, count);
    if (span.firstWord == span.lastWord)
    {
        return static_cast<gp::USize>(gp::math::countBits(words[span.firstWord] & span.startMask & span.endMask));
    }

    gp::USize total = static_cast<gp::USize>(gp::math::countBits(words[span.firstWord] & span.startMask));
    for (gp::USize w = span.firstWord + 1u; w < span.lastWord; ++w)
    {
        total += static_cast<gp::USize>(gp::math::countBits(words[w]));
    }
    total += static_cast<gp::USize>(gp::math::countBits(words[span.lastWord] & span.endMask));
    return total;
}

/// @brief Checks whether at least one bit is set across `wordCount` sanitized words.
/// @param[in] words Backing word buffer.
/// @param[in] wordCount Number of active words.
/// @return True if any word is non-zero.
[[nodiscard]] constexpr bool isAnySet(const BitWord* const words, const gp::USize wordCount) noexcept
{
    if (words == nullptr || wordCount == 0u)
    {
        return false;
    }
    for (gp::USize w = 0u; w < wordCount; ++w)
    {
        if (words[w] != BitWord{ 0 })
        {
            return true;
        }
    }
    return false;
}

/// @brief Checks whether every logical bit in `[0, numBits)` is set.
/// @param[in] words Backing word buffer.
/// @param[in] numBits Logical bit count (`0` returns true, matching universal quantification).
/// @return True if all bits in `[0, numBits)` equal 1.
[[nodiscard]] constexpr bool isAllSet(const BitWord* const words, const gp::USize numBits) noexcept
{
    if (numBits == 0u)
    {
        return true;
    }
    if (words == nullptr)
    {
        return false;
    }
    const gp::USize numWords = wordCountForBits(numBits);
    for (gp::USize w = 0u; (w + 1u) < numWords; ++w)
    {
        if (words[w] != ~BitWord{ 0 })
        {
            return false;
        }
    }
    const BitWord tailMask = lastWordMask(numBits);
    return (words[numWords - 1u] & tailMask) == tailMask;
}

/// @brief Finds the first set (`1`) bit at or after `startBit`.
/// @param[in] words Backing word buffer.
/// @param[in] numBits Logical bit count.
/// @param[in] startBit Starting search bit index.
/// @return Zero-based index of the first set bit, or `kBitNpos` if none found.
[[nodiscard]] constexpr gp::USize
    findFirstSet(const BitWord* const words, const gp::USize numBits, const gp::USize startBit = 0u) noexcept
{
    if (words == nullptr || startBit >= numBits)
    {
        return kBitNpos;
    }

    const gp::USize numWords = wordCountForBits(numBits);
    gp::USize w = startBit >> kWordIndexShift;
    BitWord word = words[w] & ((~BitWord{ 0 }) << (startBit & kBitInWordMask));

    while (word == BitWord{ 0 })
    {
        ++w;
        if (w >= numWords)
        {
            return kBitNpos;
        }
        word = words[w];
    }

    const gp::USize index = (w << kWordIndexShift) + static_cast<gp::USize>(gp::math::countTrailingZeros(word));
    return (index < numBits) ? index : kBitNpos;
}

/// @brief Finds the first cleared (`0`) bit at or after `startBit`.
/// @param[in] words Backing word buffer.
/// @param[in] numBits Logical bit count.
/// @param[in] startBit Starting search bit index.
/// @return Zero-based index of the first zero bit, or `kBitNpos` if all bits in `[startBit, numBits)` are 1.
[[nodiscard]] constexpr gp::USize
    findFirstZero(const BitWord* const words, const gp::USize numBits, const gp::USize startBit = 0u) noexcept
{
    if (words == nullptr || startBit >= numBits)
    {
        return kBitNpos;
    }

    const gp::USize numWords = wordCountForBits(numBits);
    gp::USize w = startBit >> kWordIndexShift;
    BitWord inverted = (~words[w]) & ((~BitWord{ 0 }) << (startBit & kBitInWordMask));
    if ((w + 1u) == numWords)
    {
        inverted &= lastWordMask(numBits);
    }

    while (inverted == BitWord{ 0 })
    {
        ++w;
        if (w >= numWords)
        {
            return kBitNpos;
        }
        inverted = ~words[w];
        if ((w + 1u) == numWords)
        {
            inverted &= lastWordMask(numBits);
        }
    }

    return (w << kWordIndexShift) + static_cast<gp::USize>(gp::math::countTrailingZeros(inverted));
}

/// @brief Finds the highest-indexed set (`1`) bit across `[0, numBits)`.
/// @param[in] words Backing word buffer.
/// @param[in] numBits Logical bit count.
/// @return Zero-based index of the last set bit, or `kBitNpos` if empty or all zero.
[[nodiscard]] constexpr gp::USize findLastSet(const BitWord* const words, const gp::USize numBits) noexcept
{
    if (words == nullptr || numBits == 0u)
    {
        return kBitNpos;
    }

    const gp::USize numWords = wordCountForBits(numBits);
    for (gp::USize w = numWords; w > 0u; --w)
    {
        const gp::USize wordIdx = w - 1u;
        const BitWord word = (w == numWords) ? (words[wordIdx] & lastWordMask(numBits)) : words[wordIdx];
        if (word != BitWord{ 0 })
        {
            const gp::USize bitInWord = 63u - static_cast<gp::USize>(gp::math::countLeadingZeros(word));
            return (wordIdx << kWordIndexShift) + bitInWord;
        }
    }
    return kBitNpos;
}

/// @brief Finds the highest-indexed cleared (`0`) bit across `[0, numBits)`.
/// @param[in] words Backing word buffer.
/// @param[in] numBits Logical bit count.
/// @return Zero-based index of the last zero bit, or `kBitNpos` if empty or all set.
[[nodiscard]] constexpr gp::USize findLastZero(const BitWord* const words, const gp::USize numBits) noexcept
{
    if (words == nullptr || numBits == 0u)
    {
        return kBitNpos;
    }

    const gp::USize numWords = wordCountForBits(numBits);
    for (gp::USize w = numWords; w > 0u; --w)
    {
        const gp::USize wordIdx = w - 1u;
        const BitWord inverted = (w == numWords) ? ((~words[wordIdx]) & lastWordMask(numBits)) : (~words[wordIdx]);
        if (inverted != BitWord{ 0 })
        {
            const gp::USize bitInWord = 63u - static_cast<gp::USize>(gp::math::countLeadingZeros(inverted));
            return (wordIdx << kWordIndexShift) + bitInWord;
        }
    }
    return kBitNpos;
}

/// @brief Searches for `requiredCount` contiguous zero bits starting at or after `startBit`.
/// @details Skips completely filled or completely empty 64-bit words in O(1) and jumps across alternating runs inside
///          mixed words via `countTrailingZeros()`.
/// @param[in] words Backing word buffer.
/// @param[in] numBits Logical bit count.
/// @param[in] requiredCount Number of contiguous zero bits needed.
/// @param[in] startBit Earliest bit index allowed for the run start.
/// @return Starting bit index of the first qualifying zero run, or `kBitNpos` if none fits.
[[nodiscard]] constexpr gp::USize findContiguousZeros(
    const BitWord* const words, const gp::USize numBits, const gp::USize requiredCount, const gp::USize startBit = 0u
) noexcept
{
    if (words == nullptr || startBit >= numBits)
    {
        return kBitNpos;
    }
    if (requiredCount == 0u)
    {
        return startBit;
    }
    if (requiredCount > (numBits - startBit))
    {
        return kBitNpos;
    }

    const gp::USize numWords = wordCountForBits(numBits);
    const gp::USize startWord = startBit >> kWordIndexShift;
    const gp::USize startOffset = startBit & kBitInWordMask;
    const gp::USize tailBits = numBits & kBitInWordMask;
    const BitWord finalMask = lastWordMask(numBits);

    gp::USize runStart = startBit;
    gp::USize runLength = 0u;

    for (gp::USize w = startWord; w < numWords; ++w)
    {
        const gp::USize validBits = ((w + 1u) == numWords && tailBits != 0u) ? tailBits : kBitsPerWord;
        const BitWord word = words[w];
        gp::USize pos = (w == startWord) ? startOffset : 0u;

        if (pos == 0u)
        {
            if (word == BitWord{ 0 })
            {
                if (runLength == 0u)
                {
                    runStart = w << kWordIndexShift;
                }
                runLength += validBits;
                if (runLength >= requiredCount)
                {
                    return runStart;
                }
                continue;
            }
            const BitWord validMask = ((w + 1u) == numWords) ? finalMask : (~BitWord{ 0 });
            if ((word & validMask) == validMask)
            {
                runLength = 0u;
                continue;
            }
        }

        while (pos < validBits)
        {
            const BitWord rem = word >> pos;
            gp::USize zeroRun =
                (rem == BitWord{ 0 }) ? (validBits - pos) : static_cast<gp::USize>(gp::math::countTrailingZeros(rem));
            if (zeroRun > (validBits - pos))
            {
                zeroRun = validBits - pos;
            }

            if (zeroRun > 0u)
            {
                if (runLength == 0u)
                {
                    runStart = (w << kWordIndexShift) + pos;
                }
                runLength += zeroRun;
                if (runLength >= requiredCount)
                {
                    return runStart;
                }
                pos += zeroRun;
                if (pos >= validBits)
                {
                    break;
                }
            }

            runLength = 0u;
            const BitWord invRem = (~word) >> pos;
            gp::USize oneRun = (invRem == BitWord{ 0 }) ? (validBits - pos)
                                                        : static_cast<gp::USize>(gp::math::countTrailingZeros(invRem));
            if (oneRun > (validBits - pos))
            {
                oneRun = validBits - pos;
            }
            pos += oneRun;
        }
    }

    return kBitNpos;
}

/// @brief Finds the first zero bit at or after `startBit`, sets it to 1, and returns its index.
/// @param[in,out] words Backing word buffer.
/// @param[in] numBits Logical bit count.
/// @param[in] startBit Starting search bit index.
/// @return Index of the newly claimed bit, or `kBitNpos` if all bits in `[startBit, numBits)` are already set.
[[nodiscard]] constexpr gp::USize
    findAndSetFirstZero(BitWord* const words, const gp::USize numBits, const gp::USize startBit = 0u) noexcept
{
    const gp::USize idx = findFirstZero(words, numBits, startBit);
    if (idx != kBitNpos)
    {
        setBit(words, idx);
    }
    return idx;
}

/// @brief Checks whether every bit set in `rhsWords` is also set in `lhsWords` (`(lhs & rhs) == rhs`).
/// @param[in] lhsWords Superset candidate words.
/// @param[in] lhsBits Logical bit count of `lhsWords`.
/// @param[in] rhsWords Subset candidate words.
/// @param[in] rhsBits Logical bit count of `rhsWords`.
/// @return True if `lhsWords` contains every set bit of `rhsWords`.
[[nodiscard]] constexpr bool containsAllBits(
    const BitWord* const lhsWords, const gp::USize lhsBits, const BitWord* const rhsWords, const gp::USize rhsBits
) noexcept
{
    if (rhsBits == 0u)
    {
        return true;
    }
    if (rhsWords == nullptr)
    {
        return false;
    }

    const gp::USize lhsWordCount = (lhsWords != nullptr) ? wordCountForBits(lhsBits) : 0u;
    const gp::USize rhsWordCount = wordCountForBits(rhsBits);
    const gp::USize sharedWords = (lhsWordCount < rhsWordCount) ? lhsWordCount : rhsWordCount;

    for (gp::USize w = 0u; w < sharedWords; ++w)
    {
        if ((lhsWords[w] & rhsWords[w]) != rhsWords[w])
        {
            return false;
        }
    }
    for (gp::USize w = sharedWords; w < rhsWordCount; ++w)
    {
        if (rhsWords[w] != BitWord{ 0 })
        {
            return false;
        }
    }
    return true;
}

/// @brief Checks whether `lhsWords` and `rhsWords` share at least one set bit (`(lhs & rhs) != 0`).
/// @param[in] lhsWords Left-hand words.
/// @param[in] lhsWordCount Word count of `lhsWords`.
/// @param[in] rhsWords Right-hand words.
/// @param[in] rhsWordCount Word count of `rhsWords`.
/// @return True if any bit position is 1 in both word arrays.
[[nodiscard]] constexpr bool intersectsBits(
    const BitWord* const lhsWords,
    const gp::USize lhsWordCount,
    const BitWord* const rhsWords,
    const gp::USize rhsWordCount
) noexcept
{
    if (lhsWords == nullptr || rhsWords == nullptr || lhsWordCount == 0u || rhsWordCount == 0u)
    {
        return false;
    }
    const gp::USize sharedWords = (lhsWordCount < rhsWordCount) ? lhsWordCount : rhsWordCount;
    for (gp::USize w = 0u; w < sharedWords; ++w)
    {
        if ((lhsWords[w] & rhsWords[w]) != BitWord{ 0 })
        {
            return true;
        }
    }
    return false;
}

/// @brief Shifts all `numBits` bits left by `shift` positions toward higher indices, filling low bits with 0.
/// @details Hoists `bitShift == 0` out of the word loop so word-aligned shifts run as a straight word move
///          without per-iteration branch checks or shift-by-64 edge branches.
/// @param[in,out] words Backing word buffer.
/// @param[in] numBits Logical bit count.
/// @param[in] shift Number of bit positions to shift left.
constexpr void shiftLeftWords(BitWord* const words, const gp::USize numBits, const gp::USize shift) noexcept
{
    if (numBits == 0u || shift == 0u || words == nullptr)
    {
        return;
    }

    const gp::USize numWords = wordCountForBits(numBits);
    if (shift >= numBits)
    {
        zeroWords(words, numWords);
        return;
    }

    const gp::USize wordShift = shift >> kWordIndexShift;
    const gp::USize bitShift = shift & kBitInWordMask;
    const gp::USize movedWords = numWords - wordShift;

    if (bitShift == 0u)
    {
        for (gp::USize i = movedWords; i > 0u; --i)
        {
            words[wordShift + i - 1u] = words[i - 1u];
        }
    }
    else
    {
        const gp::USize invShift = kBitsPerWord - bitShift;
        for (gp::USize i = movedWords - 1u; i > 0u; --i)
        {
            words[wordShift + i] = (words[i] << bitShift) | (words[i - 1u] >> invShift);
        }
        words[wordShift] = words[0] << bitShift;
    }

    zeroWords(words, wordShift);
    sanitizeTailWord(words, numBits);
}

/// @brief Shifts all `numBits` bits right by `shift` positions toward lower indices, filling high bits with 0.
/// @details Clears out-of-range tail bits up front so stale high bits never shift down into valid slots,
///          then separates aligned word copies from unaligned cross-word stitching.
/// @param[in,out] words Backing word buffer.
/// @param[in] numBits Logical bit count.
/// @param[in] shift Number of bit positions to shift right.
constexpr void shiftRightWords(BitWord* const words, const gp::USize numBits, const gp::USize shift) noexcept
{
    if (numBits == 0u || shift == 0u || words == nullptr)
    {
        return;
    }

    const gp::USize numWords = wordCountForBits(numBits);
    if (shift >= numBits)
    {
        zeroWords(words, numWords);
        return;
    }

    sanitizeTailWord(words, numBits);
    const gp::USize wordShift = shift >> kWordIndexShift;
    const gp::USize bitShift = shift & kBitInWordMask;
    const gp::USize activeDstWords = numWords - wordShift;

    if (bitShift == 0u)
    {
        for (gp::USize dstIdx = 0u; dstIdx < activeDstWords; ++dstIdx)
        {
            words[dstIdx] = words[dstIdx + wordShift];
        }
    }
    else
    {
        const gp::USize invShift = kBitsPerWord - bitShift;
        const gp::USize lastActive = activeDstWords - 1u;
        for (gp::USize dstIdx = 0u; dstIdx < lastActive; ++dstIdx)
        {
            const gp::USize srcIdx = dstIdx + wordShift;
            words[dstIdx] = (words[srcIdx] >> bitShift) | (words[srcIdx + 1u] << invShift);
        }
        words[lastActive] = words[lastActive + wordShift] >> bitShift;
    }

    zeroWords(words + activeDstWords, wordShift);
}

/// @brief Compares two equal-length sanitized word arrays for exact equality.
/// @param[in] lhsWords Left-hand words.
/// @param[in] rhsWords Right-hand words.
/// @param[in] wordCount Word count shared by both arrays.
/// @return True if every word is identical.
[[nodiscard]] constexpr bool
    equalWords(const BitWord* const lhsWords, const BitWord* const rhsWords, const gp::USize wordCount) noexcept
{
    if (wordCount == 0u || lhsWords == rhsWords)
    {
        return true;
    }
    if (lhsWords == nullptr || rhsWords == nullptr)
    {
        return false;
    }
    if (!std::is_constant_evaluated())
    {
        return gp::memory::compareMemory(lhsWords, rhsWords, wordCount * sizeof(BitWord)) == 0;
    }
    for (gp::USize w = 0u; w < wordCount; ++w)
    {
        if (lhsWords[w] != rhsWords[w])
        {
            return false;
        }
    }
    return true;
}

/// @brief Lexicographically compares two bit containers bit-by-bit (`index 0` first) using fast word mismatch checks.
/// @param[in] lhsWords Left-hand words.
/// @param[in] lhsBits Logical bit count of `lhsWords`.
/// @param[in] rhsWords Right-hand words.
/// @param[in] rhsBits Logical bit count of `rhsWords`.
/// @return `std::strong_ordering` at the first differing bit, falling back to `lhsBits <=> rhsBits`.
[[nodiscard]] constexpr std::strong_ordering compareWordsLexicographical(
    const BitWord* const lhsWords, const gp::USize lhsBits, const BitWord* const rhsWords, const gp::USize rhsBits
) noexcept
{
    const gp::USize minBits = (lhsBits < rhsBits) ? lhsBits : rhsBits;
    const gp::USize minWords = wordCountForBits(minBits);
    if (minWords == 0u || lhsWords == nullptr || rhsWords == nullptr)
    {
        return lhsBits <=> rhsBits;
    }

    for (gp::USize w = 0u; w < minWords; ++w)
    {
        const BitWord a = lhsWords[w];
        const BitWord b = rhsWords[w];
        if (a != b)
        {
            const gp::USize diffBit = static_cast<gp::USize>(gp::math::countTrailingZeros(a ^ b));
            const gp::USize globalBit = (w << kWordIndexShift) + diffBit;
            if (globalBit < minBits)
            {
                const bool aSet = ((a >> diffBit) & BitWord{ 1 }) != BitWord{ 0 };
                return aSet ? std::strong_ordering::greater : std::strong_ordering::less;
            }
            break;
        }
    }

    return lhsBits <=> rhsBits;
}

/// @brief Invokes `callback(gp::USize bitIndex)` for every set (`1`) bit in ascending index order.
/// @details Copies each non-zero word into a local register and drains bits with `getAndClearNextBit()`
///          while keeping the backing word buffer intact. Masks the final word to guard `[0, numBits)`.
/// @tparam InCallback Callable accepting `gp::USize`.
/// @param[in] words Backing word buffer.
/// @param[in] numBits Logical bit count.
/// @param[in,out] callback Visitor invoked once per set bit.
template <typename InCallback>
constexpr void forEachSetBit(const BitWord* const words, const gp::USize numBits, InCallback&& callback) noexcept
{
    if (words == nullptr || numBits == 0u)
    {
        return;
    }
    const gp::USize numWords = wordCountForBits(numBits);
    for (gp::USize w = 0u; w < numWords; ++w)
    {
        BitWord word = words[w];
        if ((w + 1u) == numWords)
        {
            word &= lastWordMask(numBits);
        }
        if (word == BitWord{ 0 })
        {
            continue;
        }
        const gp::USize baseBit = w << kWordIndexShift;
        while (word != BitWord{ 0 })
        {
            const gp::UInt32 localBit = gp::math::getAndClearNextBit(word);
            callback(baseBit + static_cast<gp::USize>(localBit));
        }
    }
}

/// @brief Invokes `callback(gp::USize bitIndex)` for every cleared (`0`) bit in ascending index order.
/// @tparam InCallback Callable accepting `gp::USize`.
/// @param[in] words Backing word buffer.
/// @param[in] numBits Logical bit count.
/// @param[in,out] callback Visitor invoked once per zero bit.
template <typename InCallback>
constexpr void forEachZeroBit(const BitWord* const words, const gp::USize numBits, InCallback&& callback) noexcept
{
    if (words == nullptr || numBits == 0u)
    {
        return;
    }
    const gp::USize numWords = wordCountForBits(numBits);
    for (gp::USize w = 0u; w < numWords; ++w)
    {
        BitWord word = ~words[w];
        if ((w + 1u) == numWords)
        {
            word &= lastWordMask(numBits);
        }
        if (word == BitWord{ 0 })
        {
            continue;
        }
        const gp::USize baseBit = w << kWordIndexShift;
        while (word != BitWord{ 0 })
        {
            const gp::UInt32 localBit = gp::math::getAndClearNextBit(word);
            callback(baseBit + static_cast<gp::USize>(localBit));
        }
    }
}

/// @brief Visits every set (`1`) bit in ascending order and clears each visited bit in `words`.
/// @details Snapshot-clears `words[w]` to `0` before draining the register copy via `getAndClearNextBit()`.
///          That order lets callbacks re-flag slots in `words[w]` for the next pass without losing new writes.
/// @tparam InCallback Callable accepting `gp::USize`.
/// @param[in,out] words Backing word buffer cleared as bits are consumed.
/// @param[in] numBits Logical bit count.
/// @param[in,out] callback Visitor invoked once per previously set bit.
template <typename InCallback>
constexpr void consumeSetBits(BitWord* const words, const gp::USize numBits, InCallback&& callback) noexcept
{
    if (words == nullptr || numBits == 0u)
    {
        return;
    }
    const gp::USize numWords = wordCountForBits(numBits);
    for (gp::USize w = 0u; w < numWords; ++w)
    {
        BitWord word = words[w];
        if ((w + 1u) == numWords)
        {
            word &= lastWordMask(numBits);
        }
        if (word == BitWord{ 0 })
        {
            continue;
        }
        words[w] = BitWord{ 0 };
        const gp::USize baseBit = w << kWordIndexShift;
        while (word != BitWord{ 0 })
        {
            const gp::UInt32 localBit = gp::math::getAndClearNextBit(word);
            callback(baseBit + static_cast<gp::USize>(localBit));
        }
    }
}

/// @brief Streams all active words and trailing `numBits` framing into `hasher` for unified `gp::hashing::hash()`.
/// @tparam InHasher Concrete streaming hasher type.
/// @param[in,out] hasher Target accumulator.
/// @param[in] words Backing word buffer.
/// @param[in] numBits Logical bit count.
template <typename InHasher>
constexpr void hashAppendWords(InHasher& hasher, const BitWord* const words, const gp::USize numBits) noexcept
{
    if (numBits > 0u && words != nullptr)
    {
        const gp::USize numWords = wordCountForBits(numBits);
        for (gp::USize w = 0u; (w + 1u) < numWords; ++w)
        {
            hasher.append(words[w]);
        }
        hasher.append(words[numWords - 1u] & lastWordMask(numBits));
    }
    hasher.append(static_cast<gp::UInt64>(numBits));
}

}   // namespace gp::container::detail
