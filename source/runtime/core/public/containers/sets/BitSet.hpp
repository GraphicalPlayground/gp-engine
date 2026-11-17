// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "containers/ContainerForward.hpp"   // IWYU pragma: keep
#include "containers/details/BitWordOps.hpp"
#include "CoreMinimal.hpp"
#include <compare>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>

namespace gp
{

/// @brief Compile-time fixed-size bitset packed into 64-bit machine words with zero heap allocations.
/// @details Modeled after Decima and Anvil component signature masks, render pass filters, and job dependency masks.
///          Every 64-bit storage word packs 512 flags into one 64-byte cache line inline inside `m_words`.
///          Zero-bit instantiations retain a single standard-layout word (`kStorageWords = 1`) with `data() ==
///          nullptr`. All mutating paths zero unused padding bits past `InBitCount` so word-wide scans, `popcnt`,
///          hashing, and comparisons run branchlessly across whole words.
/// @tparam InBitCount Fixed number of addressable bits (`0` or greater).
template <gp::USize InBitCount>
class BitSet
{
public:
    using WordType = container::detail::BitWord;
    using SizeType = gp::USize;
    using DifferenceType = gp::ISize;
    using Reference = container::detail::BitReference;
    using ConstReference = bool;
    using Iterator = container::detail::BitIterator<false>;
    using ConstIterator = container::detail::BitIterator<true>;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;
    using SetBitIterator = container::detail::SetBitIterator;
    using ZeroBitIterator = container::detail::ZeroBitIterator;
    using SetBitRange = container::detail::SetBitRange;
    using ZeroBitRange = container::detail::ZeroBitRange;

    /// @brief Sentinel index returned when a bit scan finds no match.
    static constexpr SizeType npos = container::detail::kBitNpos;

    /// @brief Compile-time capacity in bits.
    static constexpr SizeType kBitCount = InBitCount;

    /// @brief Number of bits packed into each storage word.
    static constexpr SizeType kBitsPerWord = container::detail::kBitsPerWord;

    /// @brief Number of 64-bit words required to hold `InBitCount` bits (`0` when `InBitCount == 0`).
    static constexpr SizeType kWordCount = container::detail::wordCountForBits(InBitCount);

private:
    static constexpr SizeType kStorageWords = (kWordCount > 0u) ? kWordCount : 1u;

public:
    /// @brief Default-constructs a bitset with every bit cleared to 0.
    constexpr BitSet() noexcept = default;

    /// @brief Constructs a bitset with all `InBitCount` bits initialized to `initialValue`.
    /// @param[in] initialValue Desired state for every bit (`true` sets all bits, `false` zeroes them).
    constexpr explicit BitSet(const bool initialValue) noexcept
    {
        if (initialValue)
        {
            setAll();
        }
    }

    /// @brief Constructs a bitset from an initializer list of boolean values (`init[i]` maps to bit `i`).
    /// @param[in] init Initializer list whose size must not exceed `InBitCount`.
    constexpr BitSet(const std::initializer_list<bool> init) noexcept
    {
        GP_ASSERT(init.size() <= InBitCount && "Too many initializers for BitSet.");
        SizeType idx = 0u;
        for (const bool bit: init)
        {
            if (idx >= InBitCount)
            {
                break;
            }
            if (bit)
            {
                container::detail::setBit(m_words, idx);
            }
            ++idx;
        }
    }

    /// @brief Builds a `BitSet` from the low bits of a 64-bit integer mask (clamping bits `>= InBitCount`).
    /// @param[in] mask Raw 64-bit pattern applied to word 0.
    /// @return Initialized `BitSet<InBitCount>`.
    [[nodiscard]] static constexpr BitSet fromMask(const WordType mask) noexcept
    {
        BitSet result;
        if constexpr (kWordCount > 0u)
        {
            result.m_words[0] = mask;
            container::detail::sanitizeTailWord(result.m_words, InBitCount);
        }
        else
        {
            (void)mask;
        }
        return result;
    }

    /// @brief Builds a `BitSet` by copying up to `kWordCount` words from `words` and zeroing unused tail bits.
    /// @param[in] words Source word buffer (safe when `nullptr` if `wordCount == 0`).
    /// @param[in] wordCount Number of words readable at `words`.
    /// @return Initialized `BitSet<InBitCount>`.
    [[nodiscard]] static constexpr BitSet fromWords(const WordType* const words, const SizeType wordCount) noexcept
    {
        BitSet result;
        if constexpr (kWordCount > 0u)
        {
            if (words != nullptr && wordCount > 0u)
            {
                const SizeType copyCount = (wordCount < kWordCount) ? wordCount : kWordCount;
                container::detail::copyWords(result.m_words, words, copyCount);
                container::detail::sanitizeTailWord(result.m_words, InBitCount);
            }
        }
        else
        {
            (void)words;
            (void)wordCount;
        }
        return result;
    }

public:
    /// @brief Returns a writable proxy reference to bit `index`.
    /// @param[in] index Zero-based bit index (`< InBitCount`).
    /// @return `BitReference` bound to bit `index`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Reference operator[](const SizeType index) noexcept
    {
        GP_ASSERT(index < InBitCount && "BitSet index out of bounds.");
        return Reference(
            m_words + (index >> container::detail::kWordIndexShift),
            WordType{ 1 } << (index & container::detail::kBitInWordMask)
        );
    }

    /// @brief Reads bit `index` directly as a `bool`.
    /// @param[in] index Zero-based bit index (`< InBitCount`).
    /// @return True if bit `index` is set, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool operator[](const SizeType index) const noexcept
    {
        GP_ASSERT(index < InBitCount && "BitSet index out of bounds.");
        return container::detail::testBit(m_words, index);
    }

    /// @brief Reads bit `index` directly as a `bool` with debug bounds assertion.
    /// @param[in] index Zero-based bit index (`< InBitCount`).
    /// @return True if bit `index` is set, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool test(const SizeType index) const noexcept
    {
        GP_ASSERT(index < InBitCount && "BitSet::test() index out of bounds.");
        return container::detail::testBit(m_words, index);
    }

    /// @brief Assigns `value` to bit `index`.
    /// @param[in] index Zero-based bit index (`< InBitCount`).
    /// @param[in] value Desired bit state (defaults to `true`).
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr BitSet& set(const SizeType index, const bool value = true) noexcept
    {
        GP_ASSERT(index < InBitCount && "BitSet::set() index out of bounds.");
        if (value)
        {
            container::detail::setBit(m_words, index);
        }
        else
        {
            container::detail::clearBit(m_words, index);
        }
        return *this;
    }

    /// @brief Clears bit `index` to 0.
    /// @param[in] index Zero-based bit index (`< InBitCount`).
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr BitSet& clearBit(const SizeType index) noexcept
    {
        GP_ASSERT(index < InBitCount && "BitSet::clearBit() index out of bounds.");
        container::detail::clearBit(m_words, index);
        return *this;
    }

    /// @brief Toggles bit `index`.
    /// @param[in] index Zero-based bit index (`< InBitCount`).
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr BitSet& flipBit(const SizeType index) noexcept
    {
        GP_ASSERT(index < InBitCount && "BitSet::flipBit() index out of bounds.");
        container::detail::flipBit(m_words, index);
        return *this;
    }

    /// @brief Sets bit `index` to 1 and returns its previous state.
    /// @param[in] index Zero-based bit index (`< InBitCount`).
    /// @return True if bit `index` was already 1, false if it was 0.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool testAndSet(const SizeType index) noexcept
    {
        GP_ASSERT(index < InBitCount && "BitSet::testAndSet() index out of bounds.");
        return container::detail::testAndSetBit(m_words, index);
    }

    /// @brief Clears bit `index` to 0 and returns its previous state.
    /// @param[in] index Zero-based bit index (`< InBitCount`).
    /// @return True if bit `index` was 1 before clearing, false if it was already 0.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool testAndClear(const SizeType index) noexcept
    {
        GP_ASSERT(index < InBitCount && "BitSet::testAndClear() index out of bounds.");
        return container::detail::testAndClearBit(m_words, index);
    }

    /// @brief Sets `count` consecutive bits starting at `startBit` to 1.
    /// @param[in] startBit First bit index in the range.
    /// @param[in] count Number of consecutive bits to set.
    constexpr void setRange(const SizeType startBit, const SizeType count) noexcept
    {
        GP_ASSERT(
            startBit <= InBitCount && count <= (InBitCount - startBit) && "BitSet::setRange() range out of bounds."
        );
        if constexpr (kWordCount > 0u)
        {
            container::detail::setBitRange(m_words, startBit, count);
        }
    }

    /// @brief Clears `count` consecutive bits starting at `startBit` to 0.
    /// @param[in] startBit First bit index in the range.
    /// @param[in] count Number of consecutive bits to clear.
    constexpr void clearRange(const SizeType startBit, const SizeType count) noexcept
    {
        GP_ASSERT(
            startBit <= InBitCount && count <= (InBitCount - startBit) && "BitSet::clearRange() range out of bounds."
        );
        if constexpr (kWordCount > 0u)
        {
            container::detail::clearBitRange(m_words, startBit, count);
        }
    }

    /// @brief Toggles `count` consecutive bits starting at `startBit`.
    /// @param[in] startBit First bit index in the range.
    /// @param[in] count Number of consecutive bits to toggle.
    constexpr void flipRange(const SizeType startBit, const SizeType count) noexcept
    {
        GP_ASSERT(
            startBit <= InBitCount && count <= (InBitCount - startBit) && "BitSet::flipRange() range out of bounds."
        );
        if constexpr (kWordCount > 0u)
        {
            container::detail::flipBitRange(m_words, startBit, count);
        }
    }

    /// @brief Sets all `InBitCount` bits to 1.
    constexpr void setAll() noexcept
    {
        if constexpr (kWordCount > 0u)
        {
            container::detail::fillOnesWords(m_words, InBitCount);
        }
    }

    /// @brief Clears all `InBitCount` bits to 0.
    constexpr void clearAll() noexcept
    {
        if constexpr (kWordCount > 0u)
        {
            container::detail::zeroWords(m_words, kWordCount);
        }
    }

    /// @brief Clears all `InBitCount` bits to 0 (matches `gp::Array::clear()`).
    constexpr void clear() noexcept
    {
        clearAll();
    }

    /// @brief Sets every bit to `value`.
    /// @param[in] value Desired bit state for all bits.
    constexpr void fill(const bool value) noexcept
    {
        if (value)
        {
            setAll();
        }
        else
        {
            clearAll();
        }
    }

    /// @brief Inverts every bit in the bitset while keeping unused high padding bits cleared.
    constexpr void flipAll() noexcept
    {
        if constexpr (kWordCount > 0u)
        {
            container::detail::flipAllWords(m_words, InBitCount);
        }
    }

    /// @brief Finds the first zero bit at or after `startBit`, sets it to 1, and returns its index.
    /// @param[in] startBit Earliest bit index to consider.
    /// @return Index of the claimed bit, or `npos` if no zero bit exists at or after `startBit`.
    [[nodiscard]] constexpr SizeType findAndSetFirstZero(const SizeType startBit = 0u) noexcept
    {
        if constexpr (kWordCount > 0u)
        {
            return container::detail::findAndSetFirstZero(m_words, InBitCount, startBit);
        }
        else
        {
            (void)startBit;
            return npos;
        }
    }

public:
    /// @brief Returns the fixed bit count (`InBitCount`).
    /// @return Number of bits in this bitset.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType size() const noexcept
    {
        return InBitCount;
    }

    /// @brief Returns the fixed bit capacity (`InBitCount`).
    /// @return Total capacity in bits.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType capacity() const noexcept
    {
        return InBitCount;
    }

    /// @brief Returns the number of 64-bit words backing this bitset (`kWordCount`).
    /// @return Word count (`0` when `InBitCount == 0`).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType wordCount() const noexcept
    {
        return kWordCount;
    }

    /// @brief Returns the active word storage footprint in bytes (`kWordCount * sizeof(WordType)`).
    /// @return Storage size in bytes.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType sizeInBytes() const noexcept
    {
        return kWordCount * sizeof(WordType);
    }

    /// @brief Checks whether the bitset has zero addressable bits (`InBitCount == 0`).
    /// @return True if `InBitCount == 0`, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isEmpty() const noexcept
    {
        return InBitCount == 0u;
    }

    /// @brief Returns a mutable pointer to the underlying `WordType` array, or `nullptr` if `InBitCount == 0`.
    /// @return Word buffer pointer.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr WordType* data() noexcept
    {
        return (kWordCount > 0u) ? m_words : nullptr;
    }

    /// @brief Returns a const pointer to the underlying `WordType` array, or `nullptr` if `InBitCount == 0`.
    /// @return Const word buffer pointer.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr const WordType* data() const noexcept
    {
        return (kWordCount > 0u) ? m_words : nullptr;
    }

    /// @brief Reads the raw 64-bit word at `wordIndex`.
    /// @param[in] wordIndex Zero-based word index (`< kWordCount`).
    /// @return 64-bit storage word.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr WordType getWord(const SizeType wordIndex) const noexcept
    {
        GP_ASSERT(wordIndex < kWordCount && "BitSet::getWord() index out of bounds.");
        return m_words[wordIndex];
    }

    /// @brief Overwrites the 64-bit word at `wordIndex` and masks any unused tail bits in the final word.
    /// @param[in] wordIndex Zero-based word index (`< kWordCount`).
    /// @param[in] value New 64-bit word mask.
    GP_FORCEINLINE_HINT constexpr void setWord(const SizeType wordIndex, const WordType value) noexcept
    {
        GP_ASSERT(wordIndex < kWordCount && "BitSet::setWord() index out of bounds.");
        m_words[wordIndex] = value;
        if ((wordIndex + 1u) == kWordCount)
        {
            container::detail::sanitizeTailWord(m_words, InBitCount);
        }
    }

    /// @brief Returns the bitset contents packed into a 64-bit unsigned integer (`InBitCount <= 64`).
    /// @return 64-bit mask representing bits `[0, InBitCount)`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64 toUInt64() const noexcept requires(InBitCount <= 64u)
    {
        if constexpr (kWordCount == 0u)
        {
            return 0ULL;
        }
        else
        {
            return m_words[0] & container::detail::lastWordMask(InBitCount);
        }
    }

public:
    /// @brief Counts the total number of set (`1`) bits using hardware population count.
    /// @return Number of set bits in `[0, InBitCount)`.
    [[nodiscard]] constexpr SizeType countSetBits() const noexcept
    {
        if constexpr (kWordCount == 0u)
        {
            return 0u;
        }
        else
        {
            return container::detail::countSetBits(m_words, kWordCount);
        }
    }

    /// @brief Counts the total number of cleared (`0`) bits.
    /// @return Number of zero bits in `[0, InBitCount)`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType countZeroBits() const noexcept
    {
        return InBitCount - countSetBits();
    }

    /// @brief Counts set (`1`) bits within `[startBit, startBit + count)`.
    /// @param[in] startBit Starting bit index.
    /// @param[in] count Number of bits to inspect.
    /// @return Number of set bits inside the range.
    [[nodiscard]] constexpr SizeType countSetBitsInRange(const SizeType startBit, const SizeType count) const noexcept
    {
        GP_ASSERT(
            startBit <= InBitCount && count <= (InBitCount - startBit) &&
            "BitSet::countSetBitsInRange() range out of bounds."
        );
        if constexpr (kWordCount == 0u)
        {
            return 0u;
        }
        else
        {
            return container::detail::countSetBitsInRange(m_words, startBit, count);
        }
    }

    /// @brief Checks whether at least one bit is set to 1.
    /// @return True if any bit is set, false when all bits are 0.
    [[nodiscard]] constexpr bool isAnySet() const noexcept
    {
        if constexpr (kWordCount == 0u)
        {
            return false;
        }
        else
        {
            return container::detail::isAnySet(m_words, kWordCount);
        }
    }

    /// @brief Checks whether every bit in `[0, InBitCount)` is set to 1.
    /// @return True if all bits are set (or `InBitCount == 0`), false otherwise.
    [[nodiscard]] constexpr bool isAllSet() const noexcept
    {
        return container::detail::isAllSet(m_words, InBitCount);
    }

    /// @brief Checks whether all bits are 0.
    /// @return True if zero bits are set, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isNoneSet() const noexcept
    {
        return !isAnySet();
    }

    /// @brief Finds the lowest-indexed set (`1`) bit at or after `startBit`.
    /// @param[in] startBit Earliest bit index to check.
    /// @return Index of the first set bit, or `npos` if none exists.
    [[nodiscard]] constexpr SizeType findFirstSet(const SizeType startBit = 0u) const noexcept
    {
        if constexpr (kWordCount == 0u)
        {
            (void)startBit;
            return npos;
        }
        else
        {
            return container::detail::findFirstSet(m_words, InBitCount, startBit);
        }
    }

    /// @brief Finds the lowest-indexed cleared (`0`) bit at or after `startBit`.
    /// @param[in] startBit Earliest bit index to check.
    /// @return Index of the first zero bit, or `npos` if none exists.
    [[nodiscard]] constexpr SizeType findFirstZero(const SizeType startBit = 0u) const noexcept
    {
        if constexpr (kWordCount == 0u)
        {
            (void)startBit;
            return npos;
        }
        else
        {
            return container::detail::findFirstZero(m_words, InBitCount, startBit);
        }
    }

    /// @brief Finds the highest-indexed set (`1`) bit.
    /// @return Index of the last set bit, or `npos` if all bits are 0.
    [[nodiscard]] constexpr SizeType findLastSet() const noexcept
    {
        if constexpr (kWordCount == 0u)
        {
            return npos;
        }
        else
        {
            return container::detail::findLastSet(m_words, InBitCount);
        }
    }

    /// @brief Finds the highest-indexed cleared (`0`) bit.
    /// @return Index of the last zero bit, or `npos` if all bits are 1.
    [[nodiscard]] constexpr SizeType findLastZero() const noexcept
    {
        if constexpr (kWordCount == 0u)
        {
            return npos;
        }
        else
        {
            return container::detail::findLastZero(m_words, InBitCount);
        }
    }

    /// @brief Searches for `requiredCount` consecutive zero bits starting at or after `startBit`.
    /// @param[in] requiredCount Minimum contiguous zero-run length required.
    /// @param[in] startBit Earliest bit index allowed for the start of the run.
    /// @return Starting index of the first matching zero run, or `npos` if none fits.
    [[nodiscard]] constexpr SizeType
        findContiguousZeros(const SizeType requiredCount, const SizeType startBit = 0u) const noexcept
    {
        if constexpr (kWordCount == 0u)
        {
            (void)requiredCount;
            (void)startBit;
            return npos;
        }
        else
        {
            return container::detail::findContiguousZeros(m_words, InBitCount, requiredCount, startBit);
        }
    }

    /// @brief Checks whether every bit set in `other` is also set in `*this` (`(*this & other) == other`).
    /// @param[in] other Subset mask to test against.
    /// @return True if `*this` is a superset of `other`.
    [[nodiscard]] constexpr bool containsAll(const BitSet& other) const noexcept
    {
        if constexpr (kWordCount == 0u)
        {
            (void)other;
            return true;
        }
        else
        {
            return container::detail::containsAllBits(m_words, InBitCount, other.m_words, InBitCount);
        }
    }

    /// @brief Checks whether `*this` and `other` share at least one set bit (`(*this & other) != 0`).
    /// @param[in] other Bitset to test overlap against.
    /// @return True if any bit is 1 in both bitsets.
    [[nodiscard]] constexpr bool intersects(const BitSet& other) const noexcept
    {
        if constexpr (kWordCount == 0u)
        {
            (void)other;
            return false;
        }
        else
        {
            return container::detail::intersectsBits(m_words, kWordCount, other.m_words, kWordCount);
        }
    }

    /// @brief Clears every bit in `*this` that is set in `other` (`*this &= ~other`) without temporary copies.
    /// @param[in] other Bitmask of bits to subtract.
    /// @return Reference to `*this`.
    constexpr BitSet& subtract(const BitSet& other) noexcept
    {
        if (this == &other)
        {
            clearAll();
            return *this;
        }
        if constexpr (kWordCount > 0u)
        {
            for (SizeType w = 0u; w < kWordCount; ++w)
            {
                m_words[w] &= ~other.m_words[w];
            }
        }
        return *this;
    }

public:
    /// @brief Bitwise AND assignment with `other`.
    /// @param[in] other Right-hand bitset.
    /// @return Reference to `*this`.
    constexpr BitSet& operator&=(const BitSet& other) noexcept
    {
        if constexpr (kWordCount > 0u)
        {
            for (SizeType w = 0u; w < kWordCount; ++w)
            {
                m_words[w] &= other.m_words[w];
            }
        }
        else
        {
            (void)other;
        }
        return *this;
    }

    /// @brief Bitwise OR assignment with `other`.
    /// @param[in] other Right-hand bitset.
    /// @return Reference to `*this`.
    constexpr BitSet& operator|=(const BitSet& other) noexcept
    {
        if constexpr (kWordCount > 0u)
        {
            for (SizeType w = 0u; w < kWordCount; ++w)
            {
                m_words[w] |= other.m_words[w];
            }
        }
        else
        {
            (void)other;
        }
        return *this;
    }

    /// @brief Bitwise XOR assignment with `other`.
    /// @param[in] other Right-hand bitset.
    /// @return Reference to `*this`.
    constexpr BitSet& operator^=(const BitSet& other) noexcept
    {
        if (this == &other)
        {
            clearAll();
            return *this;
        }
        if constexpr (kWordCount > 0u)
        {
            for (SizeType w = 0u; w < kWordCount; ++w)
            {
                m_words[w] ^= other.m_words[w];
            }
        }
        return *this;
    }

    /// @brief Returns a bitwise-inverted copy of `*this`.
    /// @return Complement bitset with trailing padding bits cleared.
    [[nodiscard]] constexpr BitSet operator~() const noexcept
    {
        BitSet copy(*this);
        copy.flipAll();
        return copy;
    }

    /// @brief Shifts all bits left (toward higher indices) by `shift`, zero-filling low positions.
    /// @param[in] shift Number of bit positions to shift.
    /// @return Reference to `*this`.
    constexpr BitSet& operator<<=(const SizeType shift) noexcept
    {
        if constexpr (kWordCount > 0u)
        {
            container::detail::shiftLeftWords(m_words, InBitCount, shift);
        }
        else
        {
            (void)shift;
        }
        return *this;
    }

    /// @brief Shifts all bits right (toward lower indices) by `shift`, zero-filling high positions.
    /// @param[in] shift Number of bit positions to shift.
    /// @return Reference to `*this`.
    constexpr BitSet& operator>>=(const SizeType shift) noexcept
    {
        if constexpr (kWordCount > 0u)
        {
            container::detail::shiftRightWords(m_words, InBitCount, shift);
        }
        else
        {
            (void)shift;
        }
        return *this;
    }

    /// @brief Returns a copy shifted left by `shift` bits.
    /// @param[in] shift Number of bit positions to shift.
    /// @return Left-shifted copy.
    [[nodiscard]] constexpr BitSet operator<<(const SizeType shift) const noexcept
    {
        BitSet copy(*this);
        copy <<= shift;
        return copy;
    }

    /// @brief Returns a copy shifted right by `shift` bits.
    /// @param[in] shift Number of bit positions to shift.
    /// @return Right-shifted copy.
    [[nodiscard]] constexpr BitSet operator>>(const SizeType shift) const noexcept
    {
        BitSet copy(*this);
        copy >>= shift;
        return copy;
    }

    /// @brief Returns the bitwise AND of `lhs` and `rhs`.
    /// @param[in] lhs Left-hand bitset.
    /// @param[in] rhs Right-hand bitset.
    /// @return Intersection bitset.
    [[nodiscard]] friend constexpr BitSet operator&(BitSet lhs, const BitSet& rhs) noexcept
    {
        lhs &= rhs;
        return lhs;
    }

    /// @brief Returns the bitwise OR of `lhs` and `rhs`.
    /// @param[in] lhs Left-hand bitset.
    /// @param[in] rhs Right-hand bitset.
    /// @return Union bitset.
    [[nodiscard]] friend constexpr BitSet operator|(BitSet lhs, const BitSet& rhs) noexcept
    {
        lhs |= rhs;
        return lhs;
    }

    /// @brief Returns the bitwise XOR of `lhs` and `rhs`.
    /// @param[in] lhs Left-hand bitset.
    /// @param[in] rhs Right-hand bitset.
    /// @return Symmetric-difference bitset.
    [[nodiscard]] friend constexpr BitSet operator^(BitSet lhs, const BitSet& rhs) noexcept
    {
        lhs ^= rhs;
        return lhs;
    }

public:
    /// @brief Returns a range view over all set (`1`) bit indices in ascending order.
    /// @return `SetBitRange` usable directly in range-based `for` loops.
    [[nodiscard]] constexpr SetBitRange setBits() const noexcept
    {
        return SetBitRange(data(), InBitCount, 0u);
    }

    /// @brief Returns a range view over all cleared (`0`) bit indices in ascending order.
    /// @return `ZeroBitRange` usable directly in range-based `for` loops.
    [[nodiscard]] constexpr ZeroBitRange zeroBits() const noexcept
    {
        return ZeroBitRange(data(), InBitCount, 0u);
    }

    /// @brief Returns a `SetBitIterator` starting at or after `startBit`.
    /// @param[in] startBit Earliest bit index to visit.
    /// @return Start set-bit iterator.
    [[nodiscard]] constexpr SetBitIterator beginSetBits(const SizeType startBit = 0u) const noexcept
    {
        return SetBitIterator(data(), InBitCount, startBit);
    }

    /// @brief Returns the end sentinel for `SetBitIterator`.
    /// @return End set-bit iterator.
    [[nodiscard]] constexpr SetBitIterator endSetBits() const noexcept
    {
        return SetBitIterator(data(), InBitCount, InBitCount);
    }

    /// @brief Returns a `ZeroBitIterator` starting at or after `startBit`.
    /// @param[in] startBit Earliest bit index to visit.
    /// @return Start zero-bit iterator.
    [[nodiscard]] constexpr ZeroBitIterator beginZeroBits(const SizeType startBit = 0u) const noexcept
    {
        return ZeroBitIterator(data(), InBitCount, startBit);
    }

    /// @brief Returns the end sentinel for `ZeroBitIterator`.
    /// @return End zero-bit iterator.
    [[nodiscard]] constexpr ZeroBitIterator endZeroBits() const noexcept
    {
        return ZeroBitIterator(data(), InBitCount, InBitCount);
    }

    /// @brief Visits every set (`1`) bit index in ascending order using `getAndClearNextBit()`.
    /// @tparam InCallback Callable accepting `SizeType`.
    /// @param[in,out] callback Visitor invoked once per set bit.
    template <typename InCallback>
    constexpr void forEachSetBit(InCallback&& callback) const noexcept
    {
        if constexpr (kWordCount > 0u)
        {
            container::detail::forEachSetBit(m_words, InBitCount, std::forward<InCallback>(callback));
        }
        else
        {
            (void)callback;
        }
    }

    /// @brief Visits every cleared (`0`) bit index in ascending order using `getAndClearNextBit()`.
    /// @tparam InCallback Callable accepting `SizeType`.
    /// @param[in,out] callback Visitor invoked once per zero bit.
    template <typename InCallback>
    constexpr void forEachZeroBit(InCallback&& callback) const noexcept
    {
        if constexpr (kWordCount > 0u)
        {
            container::detail::forEachZeroBit(m_words, InBitCount, std::forward<InCallback>(callback));
        }
        else
        {
            (void)callback;
        }
    }

    /// @brief Visits every set (`1`) bit index in ascending order and clears all bits as they are consumed.
    /// @tparam InCallback Callable accepting `SizeType`.
    /// @param[in,out] callback Visitor invoked once per previously set bit.
    template <typename InCallback>
    constexpr void consumeSetBits(InCallback&& callback) noexcept
    {
        if constexpr (kWordCount > 0u)
        {
            container::detail::consumeSetBits(m_words, InBitCount, std::forward<InCallback>(callback));
        }
        else
        {
            (void)callback;
        }
    }

    /// @brief Returns a mutable random-access iterator to bit `0`.
    /// @return Start bit iterator.
    [[nodiscard]] constexpr Iterator begin() noexcept
    {
        return Iterator(data(), 0u);
    }

    /// @brief Returns a const random-access iterator to bit `0`.
    /// @return Const start bit iterator.
    [[nodiscard]] constexpr ConstIterator begin() const noexcept
    {
        return ConstIterator(data(), 0u);
    }

    /// @brief Returns a mutable random-access iterator one past bit `InBitCount - 1`.
    /// @return End bit iterator.
    [[nodiscard]] constexpr Iterator end() noexcept
    {
        return Iterator(data(), InBitCount);
    }

    /// @brief Returns a const random-access iterator one past bit `InBitCount - 1`.
    /// @return Const end bit iterator.
    [[nodiscard]] constexpr ConstIterator end() const noexcept
    {
        return ConstIterator(data(), InBitCount);
    }

    /// @brief Returns a const random-access iterator to bit `0`.
    /// @return Const start bit iterator.
    [[nodiscard]] constexpr ConstIterator cbegin() const noexcept
    {
        return begin();
    }

    /// @brief Returns a const random-access iterator one past bit `InBitCount - 1`.
    /// @return Const end bit iterator.
    [[nodiscard]] constexpr ConstIterator cend() const noexcept
    {
        return end();
    }

    /// @brief Returns a mutable reverse iterator starting at the highest bit.
    /// @return Reverse start iterator.
    [[nodiscard]] constexpr ReverseIterator rbegin() noexcept
    {
        return ReverseIterator(end());
    }

    /// @brief Returns a const reverse iterator starting at the highest bit.
    /// @return Const reverse start iterator.
    [[nodiscard]] constexpr ConstReverseIterator rbegin() const noexcept
    {
        return ConstReverseIterator(end());
    }

    /// @brief Returns a mutable reverse iterator one before bit `0`.
    /// @return Reverse end iterator.
    [[nodiscard]] constexpr ReverseIterator rend() noexcept
    {
        return ReverseIterator(begin());
    }

    /// @brief Returns a const reverse iterator one before bit `0`.
    /// @return Const reverse end iterator.
    [[nodiscard]] constexpr ConstReverseIterator rend() const noexcept
    {
        return ConstReverseIterator(begin());
    }

    /// @brief Returns a const reverse iterator starting at the highest bit.
    /// @return Const reverse start iterator.
    [[nodiscard]] constexpr ConstReverseIterator crbegin() const noexcept
    {
        return ConstReverseIterator(end());
    }

    /// @brief Returns a const reverse iterator one before bit `0`.
    /// @return Const reverse end iterator.
    [[nodiscard]] constexpr ConstReverseIterator crend() const noexcept
    {
        return ConstReverseIterator(begin());
    }

public:
    /// @brief Streams all storage words and trailing bit count into `hasher` for unified `gp::hashing::hash()`.
    /// @tparam InHasher Concrete streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        container::detail::hashAppendWords(hasher, data(), InBitCount);
    }

    /// @brief Exchanges all word values with `other`.
    /// @param[in,out] other Bitset to swap with.
    constexpr void swap(BitSet& other) noexcept
    {
        if (this == &other)
        {
            return;
        }
        if constexpr (kWordCount > 0u)
        {
            for (SizeType w = 0u; w < kWordCount; ++w)
            {
                const WordType tmp = m_words[w];
                m_words[w] = other.m_words[w];
                other.m_words[w] = tmp;
            }
        }
    }

    /// @brief Compares two `BitSet` instances word-by-word for equality.
    /// @param[in] other Bitset to compare against.
    /// @return True if all bits match, false otherwise.
    [[nodiscard]] constexpr bool operator==(const BitSet& other) const noexcept
    {
        if constexpr (kWordCount == 0u)
        {
            (void)other;
            return true;
        }
        else
        {
            return container::detail::equalWords(m_words, other.m_words, kWordCount);
        }
    }

    /// @brief Three-way lexicographical comparison starting from bit `0`.
    /// @param[in] other Bitset to compare against.
    /// @return `std::strong_ordering` at the lowest differing bit index.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const BitSet& other) const noexcept
    {
        if constexpr (kWordCount == 0u)
        {
            (void)other;
            return std::strong_ordering::equal;
        }
        else
        {
            return container::detail::compareWordsLexicographical(m_words, InBitCount, other.m_words, InBitCount);
        }
    }

private:
    WordType m_words[kStorageWords]{};
};

/// @brief Exchanges two `BitSet` instances via `lhs.swap(rhs)`.
/// @tparam InBitCount Fixed bit count.
/// @param[in,out] lhs First bitset.
/// @param[in,out] rhs Second bitset.
template <gp::USize InBitCount>
constexpr void swap(BitSet<InBitCount>& lhs, BitSet<InBitCount>& rhs) noexcept
{
    lhs.swap(rhs);
}

static_assert(sizeof(BitSet<0u>) == 8u, "gp::BitSet<0> uses a 1-word dummy storage floor.");
static_assert(sizeof(BitSet<64u>) == 8u, "gp::BitSet<64> must fit in one 8-byte word.");
static_assert(sizeof(BitSet<128u>) == 16u, "gp::BitSet<128> must occupy 16 bytes.");
static_assert(sizeof(BitSet<256u>) == 32u, "gp::BitSet<256> must occupy 32 bytes.");
static_assert(gp::concepts::IsTriviallyCopyable<BitSet<0u>>, "gp::BitSet<0> must remain trivially copyable.");
static_assert(gp::concepts::IsTriviallyCopyable<BitSet<128u>>, "gp::BitSet must be trivially copyable.");
static_assert(gp::concepts::IsStandardLayout<BitSet<128u>>, "gp::BitSet must have standard layout.");

}   // namespace gp

namespace gp::concepts
{

namespace detail
{

template <typename InType>
constexpr bool IsBitSetV = false;

template <gp::USize InBitCount>
constexpr bool IsBitSetV<gp::BitSet<InBitCount>> = true;

}   // namespace detail

/// @brief Concept satisfied by any instantiation of `gp::BitSet`.
/// @tparam InType Candidate type to test.
template <typename InType>
concept IsBitSet = detail::IsBitSetV<std::remove_cvref_t<InType>>;

}   // namespace gp::concepts
