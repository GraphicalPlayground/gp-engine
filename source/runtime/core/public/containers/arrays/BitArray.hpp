// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "containers/ContainerForward.hpp"   // IWYU pragma: keep
#include "containers/details/BitWordOps.hpp"
#include "CoreMinimal.hpp"
#include "memory/allocators/AllocatorConcepts.hpp"
#include "memory/allocators/HeapAllocator.hpp"   // IWYU pragma: keep
#include "memory/MemoryBase.hpp"
#include "memory/utils/AllocatorUtilities.hpp"
#include <compare>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>

namespace gp
{

/// @brief Dynamically sized, allocator-aware bit array packed into 64-bit words.
/// @details Modeled after Unreal's `TBitArray` and Decima / Anvil sparse occupancy masks. Storage occupies 24 bytes
///          for stateless allocators via `GP_NO_UNIQUE_ADDRESS`, grows through the bound `gp::concepts::IsAllocator`
///          using amortized slack calculation, and packs 512 bits per 64-byte cache line. Unused padding bits in the
///          final active word stay strictly zeroed so word-wide `popcnt`, comparisons, scans, and hashing stay
///          branchless and canonical across containers and allocators.
/// @tparam InAllocator Engine memory allocator satisfying `gp::concepts::IsAllocator`.
template <typename InAllocator>
class BitArray
{
    static_assert(gp::concepts::IsAllocator<InAllocator>, "BitArray requires InAllocator to satisfy IsAllocator.");

private:
    template <typename InOtherAllocator>
    friend class BitArray;

public:
    using WordType = container::detail::BitWord;
    using AllocatorType = InAllocator;
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

    /// @brief Sentinel index returned when a bit search finds no match.
    static constexpr SizeType npos = container::detail::kBitNpos;

    /// @brief Number of bits packed into each storage word.
    static constexpr SizeType kBitsPerWord = container::detail::kBitsPerWord;

    /// @brief Byte alignment requested from `m_allocator` for the backing word buffer.
    static constexpr gp::UInt32 kAllocAlignment = static_cast<gp::UInt32>(
        alignof(WordType) > gp::memory::kMinimumAlignment ? alignof(WordType) : gp::memory::kMinimumAlignment
    );

public:
    /// @brief Constructs an empty bit array with zero allocations.
    constexpr BitArray() noexcept = default;

    /// @brief Constructs an empty bit array bound to a copy of `allocator`.
    /// @param[in] allocator Allocator instance to copy.
    constexpr explicit BitArray(const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_allocator(allocator)
    {}

    /// @brief Constructs an empty bit array by moving `allocator`.
    /// @param[in,out] allocator Allocator instance to move from.
    constexpr explicit BitArray(InAllocator&& allocator) noexcept requires std::is_move_constructible_v<InAllocator>
        : m_allocator(std::move(allocator))
    {}

    /// @brief Constructs a bit array holding `bitCount` bits initialized to `initialValue`.
    /// @param[in] bitCount Initial number of bits.
    /// @param[in] initialValue Fill state for every bit (defaults to `false`).
    explicit BitArray(const SizeType bitCount, const bool initialValue = false) noexcept
    {
        assign(bitCount, initialValue);
    }

    /// @brief Constructs a bit array holding `bitCount` bits initialized to `initialValue` using `allocator`.
    /// @param[in] bitCount Initial number of bits.
    /// @param[in] initialValue Fill state for every bit.
    /// @param[in] allocator Allocator instance to copy.
    BitArray(const SizeType bitCount, const bool initialValue, const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_allocator(allocator)
    {
        assign(bitCount, initialValue);
    }

    /// @brief Constructs a bit array from an initializer list of boolean values.
    /// @param[in] init Initializer list ordered from index `0` upward.
    BitArray(const std::initializer_list<bool> init) noexcept
    {
        assign(init);
    }

    /// @brief Constructs a bit array from an initializer list using `allocator`.
    /// @param[in] init Initializer list ordered from index `0` upward.
    /// @param[in] allocator Allocator instance to copy.
    BitArray(const std::initializer_list<bool> init, const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_allocator(allocator)
    {
        assign(init);
    }

    /// @brief Constructs a dynamic bit array initialized from a fixed-size `gp::BitSet`.
    /// @tparam InBitCount Compile-time bit count of the source `BitSet`.
    /// @param[in] bitSet Source bitset to copy.
    template <gp::USize InBitCount>
    explicit BitArray(const BitSet<InBitCount>& bitSet) noexcept
    {
        assign(bitSet);
    }

    /// @brief Constructs a dynamic bit array initialized from a fixed-size `gp::BitSet` using `allocator`.
    /// @tparam InBitCount Compile-time bit count of the source `BitSet`.
    /// @param[in] bitSet Source bitset to copy.
    /// @param[in] allocator Allocator instance to copy.
    template <gp::USize InBitCount>
    BitArray(const BitSet<InBitCount>& bitSet, const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_allocator(allocator)
    {
        assign(bitSet);
    }

    /// @brief Copy constructor when `InAllocator` is copy-constructible.
    /// @param[in] other Source bit array to copy.
    BitArray(const BitArray& other) noexcept requires std::is_copy_constructible_v<InAllocator>
        : m_allocator(other.m_allocator)
    {
        copyWordsFrom(other.m_words, other.m_numBits);
    }

    /// @brief Copy constructor when `InAllocator` is non-copyable (such as `InlineAllocator`).
    /// @param[in] other Source bit array to copy into a default-constructed allocator.
    BitArray(const BitArray& other) noexcept
        requires(!std::is_copy_constructible_v<InAllocator> && std::is_default_constructible_v<InAllocator>)
    {
        copyWordsFrom(other.m_words, other.m_numBits);
    }

    /// @brief Cross-allocator copy constructor from any `BitArray` specialization.
    /// @tparam InOtherAllocator Source bit array's allocator type.
    /// @param[in] other Source bit array to copy bits from.
    template <gp::concepts::IsAllocator InOtherAllocator>
    requires(!gp::concepts::IsSameAs<InAllocator, InOtherAllocator>)
    explicit BitArray(const BitArray<InOtherAllocator>& other) noexcept
    {
        copyWordsFrom(other.m_words, other.m_numBits);
    }

    /// @brief Move constructor when `InAllocator` is move-constructible.
    /// @details Steals the backing word pointer in O(1) when allocators compare equal; otherwise copies words into
    ///          `m_allocator` and frees `other`.
    /// @param[in,out] other Source bit array left empty after the move.
    BitArray(BitArray&& other) noexcept requires std::is_move_constructible_v<InAllocator>
        : m_allocator(std::move(other.m_allocator))
    {
        if (m_allocator == other.m_allocator)
        {
            stealFrom(other);
        }
        else
        {
            copyWordsFrom(other.m_words, other.m_numBits);
            other.reset();
        }
    }

    /// @brief Move constructor when `InAllocator` is non-movable (such as `InlineAllocator`).
    /// @param[in,out] other Source bit array left empty after the move.
    BitArray(BitArray&& other) noexcept
        requires(!std::is_move_constructible_v<InAllocator> && std::is_default_constructible_v<InAllocator>)
    {
        copyWordsFrom(other.m_words, other.m_numBits);
        other.reset();
    }

    /// @brief Destructor releasing the backing word buffer back to `m_allocator`.
    ~BitArray() noexcept
    {
        reset();
    }

public:
    /// @brief Copy assignment operator.
    /// @param[in] other Source bit array to copy from.
    /// @return Reference to `*this`.
    BitArray& operator=(const BitArray& other) noexcept
    {
        if (this != &other)
        {
            assignFromWords(other.m_words, other.m_numBits);
        }
        return *this;
    }

    /// @brief Move assignment operator.
    /// @param[in,out] other Source bit array left empty after the move.
    /// @return Reference to `*this`.
    BitArray& operator=(BitArray&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        if (m_allocator == other.m_allocator)
        {
            reset();
            stealFrom(other);
            return *this;
        }

        assignFromWords(other.m_words, other.m_numBits);
        other.reset();
        return *this;
    }

    /// @brief Initializer-list assignment operator.
    /// @param[in] init Initializer list replacing the current bits.
    /// @return Reference to `*this`.
    BitArray& operator=(const std::initializer_list<bool> init) noexcept
    {
        assign(init);
        return *this;
    }

    /// @brief Replaces contents with `bitCount` copies of `initialValue`.
    /// @param[in] bitCount Target logical bit count.
    /// @param[in] initialValue Fill state for all bits.
    void assign(const SizeType bitCount, const bool initialValue = false) noexcept
    {
        clear();
        resize(bitCount, initialValue);
    }

    /// @brief Replaces contents with bits from an initializer list.
    /// @param[in] init Initializer list ordered from bit `0` upward.
    void assign(const std::initializer_list<bool> init) noexcept
    {
        clear();
        const SizeType count = init.size();
        if (count == 0u)
        {
            return;
        }

        const SizeType neededWords = container::detail::wordCountForBits(count);
        if (neededWords > m_wordCapacity)
        {
            reallocateWords(computeReserveWords(neededWords));
        }
        if (m_wordCapacity < neededWords) [[unlikely]]
        {
            return;
        }

        container::detail::zeroWords(m_words, neededWords);
        SizeType idx = 0u;
        for (const bool bit: init)
        {
            if (bit)
            {
                container::detail::setBit(m_words, idx);
            }
            ++idx;
        }
        m_numBits = count;
    }

    /// @brief Replaces contents from an arbitrary `BitArray` regardless of allocator type.
    /// @tparam InOtherAllocator Source bit array's allocator type.
    /// @param[in] other Source bit array to copy.
    template <gp::concepts::IsAllocator InOtherAllocator>
    void assign(const BitArray<InOtherAllocator>& other) noexcept
    {
        if (static_cast<const void*>(this) != static_cast<const void*>(&other))
        {
            assignFromWords(other.m_words, other.m_numBits);
        }
    }

    /// @brief Replaces contents from a fixed-size `gp::BitSet`.
    /// @tparam InBitCount Compile-time bit count of the source `BitSet`.
    /// @param[in] bitSet Source bitset to copy.
    template <gp::USize InBitCount>
    void assign(const BitSet<InBitCount>& bitSet) noexcept
    {
        assignFromWords(bitSet.data(), InBitCount);
    }

    /// @brief Bulk-loads `bitCount` bits from a raw word array and masks any unused tail bits.
    /// @param[in] words Pointer to at least `wordCountForBits(bitCount)` words (zero-fills if `nullptr`).
    /// @param[in] bitCount Logical number of bits to adopt.
    void assignFromWords(const WordType* const words, const SizeType bitCount) noexcept
    {
        if (bitCount == 0u)
        {
            m_numBits = 0u;
            return;
        }

        const SizeType neededWords = container::detail::wordCountForBits(bitCount);
        if (words != nullptr && words == m_words)
        {
            GP_ASSERT(neededWords <= m_wordCapacity && "BitArray::assignFromWords() self-slice exceeds capacity.");
            if (neededWords > m_wordCapacity) [[unlikely]]
            {
                return;
            }
            container::detail::sanitizeTailWord(m_words, bitCount);
            m_numBits = bitCount;
            return;
        }

        if (neededWords > m_wordCapacity)
        {
            reallocateWords(computeReserveWords(neededWords));
        }
        if (m_wordCapacity < neededWords) [[unlikely]]
        {
            return;
        }

        if (words != nullptr)
        {
            container::detail::copyWords(m_words, words, neededWords);
            container::detail::sanitizeTailWord(m_words, bitCount);
        }
        else
        {
            GP_ASSERT(false && "BitArray::assignFromWords() called with nullptr for non-zero bitCount.");
            container::detail::zeroWords(m_words, neededWords);
        }
        m_numBits = bitCount;
    }

public:
    /// @brief Returns a writable proxy reference to bit `index`.
    /// @param[in] index Zero-based bit index (`< size()`).
    /// @return `BitReference` bound to bit `index`.
    [[nodiscard]] GP_FORCEINLINE_HINT Reference operator[](const SizeType index) noexcept
    {
        GP_ASSERT(index < m_numBits && "BitArray index out of bounds.");
        return Reference(
            m_words + (index >> container::detail::kWordIndexShift),
            WordType{ 1 } << (index & container::detail::kBitInWordMask)
        );
    }

    /// @brief Reads bit `index` directly as a `bool`.
    /// @param[in] index Zero-based bit index (`< size()`).
    /// @return True if bit `index` is set, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool operator[](const SizeType index) const noexcept
    {
        GP_ASSERT(index < m_numBits && "BitArray index out of bounds.");
        return container::detail::testBit(m_words, index);
    }

    /// @brief Reads bit `index` directly as a `bool` with debug bounds assertion.
    /// @param[in] index Zero-based bit index (`< size()`).
    /// @return True if bit `index` is set, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool test(const SizeType index) const noexcept
    {
        GP_ASSERT(index < m_numBits && "BitArray::test() index out of bounds.");
        return container::detail::testBit(m_words, index);
    }

    /// @brief Assigns `value` to bit `index`.
    /// @param[in] index Zero-based bit index (`< size()`).
    /// @param[in] value Desired bit state (defaults to `true`).
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT BitArray& set(const SizeType index, const bool value = true) noexcept
    {
        GP_ASSERT(index < m_numBits && "BitArray::set() index out of bounds.");
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
    /// @param[in] index Zero-based bit index (`< size()`).
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT BitArray& clearBit(const SizeType index) noexcept
    {
        GP_ASSERT(index < m_numBits && "BitArray::clearBit() index out of bounds.");
        container::detail::clearBit(m_words, index);
        return *this;
    }

    /// @brief Toggles bit `index`.
    /// @param[in] index Zero-based bit index (`< size()`).
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT BitArray& flipBit(const SizeType index) noexcept
    {
        GP_ASSERT(index < m_numBits && "BitArray::flipBit() index out of bounds.");
        container::detail::flipBit(m_words, index);
        return *this;
    }

    /// @brief Sets bit `index` to 1 and returns its previous state.
    /// @param[in] index Zero-based bit index (`< size()`).
    /// @return True if bit `index` was already 1, false if it was 0.
    [[nodiscard]] GP_FORCEINLINE_HINT bool testAndSet(const SizeType index) noexcept
    {
        GP_ASSERT(index < m_numBits && "BitArray::testAndSet() index out of bounds.");
        return container::detail::testAndSetBit(m_words, index);
    }

    /// @brief Clears bit `index` to 0 and returns its previous state.
    /// @param[in] index Zero-based bit index (`< size()`).
    /// @return True if bit `index` was 1 before clearing, false if it was already 0.
    [[nodiscard]] GP_FORCEINLINE_HINT bool testAndClear(const SizeType index) noexcept
    {
        GP_ASSERT(index < m_numBits && "BitArray::testAndClear() index out of bounds.");
        return container::detail::testAndClearBit(m_words, index);
    }

    /// @brief Sets `count` consecutive bits starting at `startBit` to 1.
    /// @param[in] startBit First bit index in the range.
    /// @param[in] count Number of consecutive bits to set.
    void setRange(const SizeType startBit, const SizeType count) noexcept
    {
        GP_ASSERT(
            startBit <= m_numBits && count <= (m_numBits - startBit) && "BitArray::setRange() range out of bounds."
        );
        container::detail::setBitRange(m_words, startBit, count);
    }

    /// @brief Clears `count` consecutive bits starting at `startBit` to 0.
    /// @param[in] startBit First bit index in the range.
    /// @param[in] count Number of consecutive bits to clear.
    void clearRange(const SizeType startBit, const SizeType count) noexcept
    {
        GP_ASSERT(
            startBit <= m_numBits && count <= (m_numBits - startBit) && "BitArray::clearRange() range out of bounds."
        );
        container::detail::clearBitRange(m_words, startBit, count);
    }

    /// @brief Toggles `count` consecutive bits starting at `startBit`.
    /// @param[in] startBit First bit index in the range.
    /// @param[in] count Number of consecutive bits to toggle.
    void flipRange(const SizeType startBit, const SizeType count) noexcept
    {
        GP_ASSERT(
            startBit <= m_numBits && count <= (m_numBits - startBit) && "BitArray::flipRange() range out of bounds."
        );
        container::detail::flipBitRange(m_words, startBit, count);
    }

    /// @brief Sets all active bits `[0, size())` to 1 without changing `size()`.
    void setAll() noexcept
    {
        container::detail::fillOnesWords(m_words, m_numBits);
    }

    /// @brief Clears all active bits `[0, size())` to 0 without changing `size()`.
    /// @note Unlike `clear()`, this preserves the logical bit count so indexed flags remain addressable.
    void clearAll() noexcept
    {
        container::detail::zeroWords(m_words, wordCount());
    }

    /// @brief Sets every active bit in `[0, size())` to `value` without changing `size()`.
    /// @param[in] value Desired bit state.
    void fill(const bool value) noexcept
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

    /// @brief Inverts every active bit in `[0, size())` while keeping trailing padding bits cleared.
    void flipAll() noexcept
    {
        container::detail::flipAllWords(m_words, m_numBits);
    }

    /// @brief Finds the first zero bit at or after `startBit`, sets it to 1, and returns its index.
    /// @param[in] startBit Earliest bit index to inspect.
    /// @return Index of the newly set bit, or `npos` if all bits from `startBit` onward are 1.
    [[nodiscard]] SizeType findAndSetFirstZero(const SizeType startBit = 0u) noexcept
    {
        return container::detail::findAndSetFirstZero(m_words, m_numBits, startBit);
    }

public:
    /// @brief Appends a single bit with value `value` at the end of the array, growing storage if needed.
    /// @param[in] value Bit state to append.
    void pushBack(const bool value) noexcept
    {
        const SizeType newIndex = m_numBits;
        const SizeType wordIdx = newIndex >> container::detail::kWordIndexShift;
        if (wordIdx >= m_wordCapacity) [[unlikely]]
        {
            growWordsForCount(wordIdx + 1u);
            if (wordIdx >= m_wordCapacity) [[unlikely]]
            {
                return;
            }
        }

        if ((newIndex & container::detail::kBitInWordMask) == 0u)
        {
            m_words[wordIdx] = value ? WordType{ 1 } : WordType{ 0 };
        }
        else if (value)
        {
            m_words[wordIdx] |= (WordType{ 1 } << (newIndex & container::detail::kBitInWordMask));
        }
        else
        {
            m_words[wordIdx] &= ~(WordType{ 1 } << (newIndex & container::detail::kBitInWordMask));
        }
        m_numBits = newIndex + 1u;
    }

    /// @brief Appends `count` copies of `value` to the end of the array.
    /// @param[in] value Bit state to append.
    /// @param[in] count Number of bits to append.
    void pushBack(const bool value, const SizeType count) noexcept
    {
        if (count == 0u)
        {
            return;
        }
        GP_ASSERT(count <= (maxSize() - m_numBits) && "BitArray::pushBack() bit count overflow.");
        resize(m_numBits + count, value);
    }

    /// @brief Removes the final bit and clears its vacated bit slot to keep trailing bits sanitized.
    void popBack() noexcept
    {
        GP_ASSERT(m_numBits > 0u && "BitArray::popBack() called on an empty BitArray.");
        --m_numBits;
        container::detail::clearBit(m_words, m_numBits);
    }

    /// @brief Removes bit `index` in O(1) time by overwriting it with the last bit and popping the back.
    /// @param[in] index Zero-based bit index (`< size()`).
    void removeAtSwap(const SizeType index) noexcept
    {
        GP_ASSERT(index < m_numBits && "BitArray::removeAtSwap() index out of bounds.");
        const SizeType lastIdx = m_numBits - 1u;
        if (index != lastIdx)
        {
            container::detail::assignBit(m_words, index, container::detail::testBit(m_words, lastIdx));
        }
        popBack();
    }

    /// @brief Ensures the backing buffer can hold at least `minBitCapacity` bits without reallocating.
    /// @param[in] minBitCapacity Minimum bit capacity requested.
    void reserve(const SizeType minBitCapacity) noexcept
    {
        const SizeType requiredWords = container::detail::wordCountForBits(minBitCapacity);
        if (requiredWords <= m_wordCapacity)
        {
            return;
        }
        reallocateWords(computeReserveWords(requiredWords));
    }

    /// @brief Resizes the array to `newBitCount` bits, initializing any newly appended bits to `fillValue`.
    /// @param[in] newBitCount Desired logical bit count.
    /// @param[in] fillValue Value assigned to newly added bits when growing (defaults to `false`).
    void resize(const SizeType newBitCount, const bool fillValue = false) noexcept
    {
        if (newBitCount == m_numBits)
        {
            return;
        }

        if (newBitCount < m_numBits)
        {
            m_numBits = newBitCount;
            container::detail::sanitizeTailWord(m_words, m_numBits);
            return;
        }

        const SizeType oldWordCount = wordCount();
        const SizeType neededWords = container::detail::wordCountForBits(newBitCount);
        if (neededWords > m_wordCapacity)
        {
            growWordsForCount(neededWords);
            if (m_wordCapacity < neededWords) [[unlikely]]
            {
                return;
            }
        }

        if (!fillValue)
        {
            if (neededWords > oldWordCount)
            {
                container::detail::zeroWords(m_words + oldWordCount, neededWords - oldWordCount);
            }
        }
        else if (neededWords == oldWordCount)
        {
            container::detail::setBitRange(m_words, m_numBits, newBitCount - m_numBits);
            container::detail::sanitizeTailWord(m_words, newBitCount);
        }
        else
        {
            // Fill newly entered storage words directly to avoid reading uninitialized memory in MSan/Valgrind builds.
            if (oldWordCount > 0u)
            {
                const SizeType oldRem = m_numBits & container::detail::kBitInWordMask;
                if (oldRem != 0u)
                {
                    m_words[oldWordCount - 1u] |= (~WordType{ 0 }) << oldRem;
                }
            }
            const SizeType fullNewWords = (neededWords - 1u) - oldWordCount;
            container::detail::fillFullWordsOnes(m_words + oldWordCount, fullNewWords);
            m_words[neededWords - 1u] = container::detail::lastWordMask(newBitCount);
        }

        m_numBits = newBitCount;
    }

    /// @brief Resizes the array to `newBitCount` bits without initializing newly appended bits.
    /// @details Useful before bulk-populating words via `data()`. Trailing unused bits past `newBitCount` in the final
    ///          word are still cleared so scans and comparisons remain valid once the caller writes the payload bits.
    /// @param[in] newBitCount Desired logical bit count.
    void resizeUninitialized(const SizeType newBitCount) noexcept
    {
        if (newBitCount <= m_numBits)
        {
            m_numBits = newBitCount;
            container::detail::sanitizeTailWord(m_words, m_numBits);
            return;
        }

        const SizeType neededWords = container::detail::wordCountForBits(newBitCount);
        if (neededWords > m_wordCapacity)
        {
            growWordsForCount(neededWords);
            if (m_wordCapacity < neededWords) [[unlikely]]
            {
                return;
            }
        }

        m_numBits = newBitCount;
        container::detail::sanitizeTailWord(m_words, m_numBits);
    }

    /// @brief Shrinks excess word capacity to tightly fit `size()`, or releases storage when empty.
    void shrinkToFit() noexcept
    {
        if (m_numBits == 0u)
        {
            reset();
            return;
        }

        const SizeType neededWords = wordCount();
        const SizeType targetWords = computeReserveWords(neededWords);
        if (targetWords < m_wordCapacity)
        {
            reallocateWords(targetWords);
        }
    }

    /// @brief Sets `size()` to `0` in O(1) time while keeping the allocated word buffer reserved.
    /// @note Does not release memory back to `m_allocator`; use `reset()` to deallocate immediately.
    void clear() noexcept
    {
        m_numBits = 0u;
    }

    /// @brief Sets `size()` to `0` and releases the backing word buffer to `m_allocator`.
    void reset() noexcept
    {
        if (m_words != nullptr)
        {
            m_allocator.deallocate(m_words, m_wordCapacity * sizeof(WordType), kAllocAlignment);
            m_words = nullptr;
        }
        m_numBits = 0u;
        m_wordCapacity = 0u;
    }

public:
    /// @brief Returns the number of addressable bits in the array.
    /// @return Logical bit count.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType size() const noexcept
    {
        return m_numBits;
    }

    /// @brief Returns the number of active 64-bit words backing `[0, size())`.
    /// @return Active word count (`wordCountForBits(size())`).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType wordCount() const noexcept
    {
        return container::detail::wordCountForBits(m_numBits);
    }

    /// @brief Returns the allocated capacity in bits (`wordCapacity() * 64`).
    /// @return Total capacity in bits before reallocation.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType capacity() const noexcept
    {
        return m_wordCapacity * kBitsPerWord;
    }

    /// @brief Returns the allocated capacity in 64-bit words.
    /// @return Total allocated word count.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType wordCapacity() const noexcept
    {
        return m_wordCapacity;
    }

    /// @brief Returns the active storage size in bytes (`wordCount() * sizeof(WordType)`).
    /// @return Byte footprint of active bit words.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType sizeInBytes() const noexcept
    {
        return wordCount() * sizeof(WordType);
    }

    /// @brief Returns the maximum theoretical bit count representable without iterator difference overflow.
    /// @return Maximum addressable bit count (`std::numeric_limits<DifferenceType>::max()`).
    [[nodiscard]] constexpr SizeType maxSize() const noexcept
    {
        return static_cast<SizeType>(std::numeric_limits<DifferenceType>::max());
    }

    /// @brief Checks whether the array contains zero bits (`size() == 0`).
    /// @return True if empty, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isEmpty() const noexcept
    {
        return m_numBits == 0u;
    }

    /// @brief Returns a mutable pointer to the backing `WordType` buffer, or `nullptr` if unallocated.
    /// @return Raw word buffer pointer.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr WordType* data() noexcept
    {
        return m_words;
    }

    /// @brief Returns a const pointer to the backing `WordType` buffer, or `nullptr` if unallocated.
    /// @return Const raw word buffer pointer.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr const WordType* data() const noexcept
    {
        return m_words;
    }

    /// @brief Reads the 64-bit storage word at `wordIndex`.
    /// @param[in] wordIndex Zero-based word index (`< wordCount()`).
    /// @return 64-bit storage word.
    [[nodiscard]] GP_FORCEINLINE_HINT WordType getWord(const SizeType wordIndex) const noexcept
    {
        GP_ASSERT(wordIndex < wordCount() && "BitArray::getWord() index out of bounds.");
        return m_words[wordIndex];
    }

    /// @brief Overwrites the 64-bit storage word at `wordIndex` and masks any unused tail bits in the final word.
    /// @param[in] wordIndex Zero-based word index (`< wordCount()`).
    /// @param[in] value New 64-bit word value.
    GP_FORCEINLINE_HINT void setWord(const SizeType wordIndex, const WordType value) noexcept
    {
        const SizeType activeWords = wordCount();
        GP_ASSERT(wordIndex < activeWords && "BitArray::setWord() index out of bounds.");
        m_words[wordIndex] = value;
        if ((wordIndex + 1u) == activeWords)
        {
            container::detail::sanitizeTailWord(m_words, m_numBits);
        }
    }

public:
    /// @brief Counts all set (`1`) bits in `[0, size())`.
    /// @return Total number of set bits.
    [[nodiscard]] SizeType countSetBits() const noexcept
    {
        return container::detail::countSetBits(m_words, wordCount());
    }

    /// @brief Counts all cleared (`0`) bits in `[0, size())`.
    /// @return Total number of zero bits.
    [[nodiscard]] GP_FORCEINLINE_HINT SizeType countZeroBits() const noexcept
    {
        return m_numBits - countSetBits();
    }

    /// @brief Counts set (`1`) bits inside `[startBit, startBit + count)`.
    /// @param[in] startBit Starting bit index.
    /// @param[in] count Number of bits to inspect.
    /// @return Number of set bits inside the slice.
    [[nodiscard]] SizeType countSetBitsInRange(const SizeType startBit, const SizeType count) const noexcept
    {
        GP_ASSERT(
            startBit <= m_numBits && count <= (m_numBits - startBit) &&
            "BitArray::countSetBitsInRange() range out of bounds."
        );
        return container::detail::countSetBitsInRange(m_words, startBit, count);
    }

    /// @brief Checks whether any bit in `[0, size())` is set to 1.
    /// @return True if at least one bit is set, false otherwise.
    [[nodiscard]] bool isAnySet() const noexcept
    {
        return container::detail::isAnySet(m_words, wordCount());
    }

    /// @brief Checks whether every bit in `[0, size())` is set to 1 (returns true when empty).
    /// @return True if all active bits equal 1.
    [[nodiscard]] bool isAllSet() const noexcept
    {
        return container::detail::isAllSet(m_words, m_numBits);
    }

    /// @brief Checks whether every bit in `[0, size())` is 0 (returns true when empty).
    /// @return True if zero bits are set.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isNoneSet() const noexcept
    {
        return !isAnySet();
    }

    /// @brief Finds the first set (`1`) bit at or after `startBit`.
    /// @param[in] startBit Earliest bit index to inspect.
    /// @return Zero-based index of the first set bit, or `npos` if none exists.
    [[nodiscard]] SizeType findFirstSet(const SizeType startBit = 0u) const noexcept
    {
        return container::detail::findFirstSet(m_words, m_numBits, startBit);
    }

    /// @brief Finds the first cleared (`0`) bit at or after `startBit`.
    /// @param[in] startBit Earliest bit index to inspect.
    /// @return Zero-based index of the first zero bit, or `npos` if none exists.
    [[nodiscard]] SizeType findFirstZero(const SizeType startBit = 0u) const noexcept
    {
        return container::detail::findFirstZero(m_words, m_numBits, startBit);
    }

    /// @brief Finds the highest-indexed set (`1`) bit.
    /// @return Index of the last set bit, or `npos` if empty or all zero.
    [[nodiscard]] SizeType findLastSet() const noexcept
    {
        return container::detail::findLastSet(m_words, m_numBits);
    }

    /// @brief Finds the highest-indexed cleared (`0`) bit.
    /// @return Index of the last zero bit, or `npos` if empty or all set.
    [[nodiscard]] SizeType findLastZero() const noexcept
    {
        return container::detail::findLastZero(m_words, m_numBits);
    }

    /// @brief Searches for `requiredCount` consecutive zero bits starting at or after `startBit`.
    /// @param[in] requiredCount Minimum run length of cleared bits required.
    /// @param[in] startBit Earliest bit index allowed for the run start.
    /// @return Starting index of the first qualifying run, or `npos` if none fits.
    [[nodiscard]] SizeType
        findContiguousZeros(const SizeType requiredCount, const SizeType startBit = 0u) const noexcept
    {
        return container::detail::findContiguousZeros(m_words, m_numBits, requiredCount, startBit);
    }

    /// @brief Checks whether every bit set in `other` is also set in `*this` (`(*this & other) == other`).
    /// @tparam InOtherAllocator Right-hand allocator type.
    /// @param[in] other Subset candidate bit array.
    /// @return True if `*this` contains all set bits of `other`.
    template <gp::concepts::IsAllocator InOtherAllocator>
    [[nodiscard]] bool containsAll(const BitArray<InOtherAllocator>& other) const noexcept
    {
        return container::detail::containsAllBits(m_words, m_numBits, other.m_words, other.m_numBits);
    }

    /// @brief Checks whether `*this` and `other` share at least one set bit (`(*this & other) != 0`).
    /// @tparam InOtherAllocator Right-hand allocator type.
    /// @param[in] other Right-hand bit array.
    /// @return True if any bit position is 1 in both arrays.
    template <gp::concepts::IsAllocator InOtherAllocator>
    [[nodiscard]] bool intersects(const BitArray<InOtherAllocator>& other) const noexcept
    {
        return container::detail::intersectsBits(m_words, wordCount(), other.m_words, other.wordCount());
    }

    /// @brief Clears every bit in `*this` that is set in `other` (`*this &= ~other`) without allocating.
    /// @tparam InOtherAllocator Right-hand allocator type.
    /// @param[in] other Bitmask of bits to subtract.
    /// @return Reference to `*this`.
    template <gp::concepts::IsAllocator InOtherAllocator>
    BitArray& subtract(const BitArray<InOtherAllocator>& other) noexcept
    {
        if (static_cast<const void*>(this) == static_cast<const void*>(&other))
        {
            clearAll();
            return *this;
        }
        if (m_words == nullptr || other.m_words == nullptr)
        {
            return *this;
        }
        const SizeType myWords = wordCount();
        const SizeType otherWords = other.wordCount();
        const SizeType sharedWords = (myWords < otherWords) ? myWords : otherWords;
        for (SizeType w = 0u; w < sharedWords; ++w)
        {
            m_words[w] &= ~other.m_words[w];
        }
        return *this;
    }

public:
    /// @brief In-place bitwise AND with `other`, clearing any bits beyond `other.size()`.
    /// @tparam InOtherAllocator Right-hand allocator type.
    /// @param[in] other Right-hand bit array.
    /// @return Reference to `*this`.
    template <gp::concepts::IsAllocator InOtherAllocator>
    BitArray& operator&=(const BitArray<InOtherAllocator>& other) noexcept
    {
        if (static_cast<const void*>(this) == static_cast<const void*>(&other))
        {
            return *this;
        }
        if (m_words == nullptr)
        {
            return *this;
        }
        const SizeType myWords = wordCount();
        const SizeType otherWords = (other.m_words != nullptr) ? other.wordCount() : 0u;
        const SizeType sharedWords = (myWords < otherWords) ? myWords : otherWords;
        for (SizeType w = 0u; w < sharedWords; ++w)
        {
            m_words[w] &= other.m_words[w];
        }
        if (myWords > sharedWords)
        {
            container::detail::zeroWords(m_words + sharedWords, myWords - sharedWords);
        }
        return *this;
    }

    /// @brief In-place bitwise OR with `other`, growing `size()` to `max(size(), other.size())` if `other` is longer.
    /// @tparam InOtherAllocator Right-hand allocator type.
    /// @param[in] other Right-hand bit array.
    /// @return Reference to `*this`.
    template <gp::concepts::IsAllocator InOtherAllocator>
    BitArray& operator|=(const BitArray<InOtherAllocator>& other) noexcept
    {
        if (static_cast<const void*>(this) == static_cast<const void*>(&other))
        {
            return *this;
        }
        if (other.m_numBits > m_numBits)
        {
            resize(other.m_numBits, false);
        }
        if (m_words == nullptr || other.m_words == nullptr)
        {
            return *this;
        }
        const SizeType otherWords = other.wordCount();
        const SizeType limitWords = (otherWords < wordCount()) ? otherWords : wordCount();
        for (SizeType w = 0u; w < limitWords; ++w)
        {
            m_words[w] |= other.m_words[w];
        }
        container::detail::sanitizeTailWord(m_words, m_numBits);
        return *this;
    }

    /// @brief In-place bitwise XOR with `other`, growing `size()` to `max(size(), other.size())` if `other` is longer.
    /// @tparam InOtherAllocator Right-hand allocator type.
    /// @param[in] other Right-hand bit array.
    /// @return Reference to `*this`.
    template <gp::concepts::IsAllocator InOtherAllocator>
    BitArray& operator^=(const BitArray<InOtherAllocator>& other) noexcept
    {
        if (static_cast<const void*>(this) == static_cast<const void*>(&other))
        {
            clearAll();
            return *this;
        }
        if (other.m_numBits > m_numBits)
        {
            resize(other.m_numBits, false);
        }
        if (m_words == nullptr || other.m_words == nullptr)
        {
            return *this;
        }
        const SizeType otherWords = other.wordCount();
        const SizeType limitWords = (otherWords < wordCount()) ? otherWords : wordCount();
        for (SizeType w = 0u; w < limitWords; ++w)
        {
            m_words[w] ^= other.m_words[w];
        }
        container::detail::sanitizeTailWord(m_words, m_numBits);
        return *this;
    }

    /// @brief Returns a bitwise-inverted copy of `*this`.
    /// @return Complement bit array with the same `size()` and bound allocator.
    [[nodiscard]] BitArray operator~() const noexcept
    {
        BitArray copy(*this);
        copy.flipAll();
        return copy;
    }

    /// @brief Shifts all bits left (toward higher indices) by `shift`, zero-filling low positions.
    /// @param[in] shift Number of bit positions to shift.
    /// @return Reference to `*this`.
    BitArray& operator<<=(const SizeType shift) noexcept
    {
        container::detail::shiftLeftWords(m_words, m_numBits, shift);
        return *this;
    }

    /// @brief Shifts all bits right (toward lower indices) by `shift`, zero-filling high positions.
    /// @param[in] shift Number of bit positions to shift.
    /// @return Reference to `*this`.
    BitArray& operator>>=(const SizeType shift) noexcept
    {
        container::detail::shiftRightWords(m_words, m_numBits, shift);
        return *this;
    }

    /// @brief Returns a copy shifted left by `shift` bits.
    /// @param[in] shift Number of bit positions to shift.
    /// @return Left-shifted copy.
    [[nodiscard]] BitArray operator<<(const SizeType shift) const noexcept
    {
        BitArray copy(*this);
        copy <<= shift;
        return copy;
    }

    /// @brief Returns a copy shifted right by `shift` bits.
    /// @param[in] shift Number of bit positions to shift.
    /// @return Right-shifted copy.
    [[nodiscard]] BitArray operator>>(const SizeType shift) const noexcept
    {
        BitArray copy(*this);
        copy >>= shift;
        return copy;
    }

    /// @brief Returns the bitwise intersection of `lhs` and `rhs` with length `min(lhs.size(), rhs.size())`.
    /// @details Declared as a hidden friend found only via ADL, preventing unrelated enum bitmask lookups
    ///          in outer namespaces while supporting heterogeneous right-hand allocators.
    /// @tparam InOtherAllocator Right-hand allocator type.
    /// @param[in] lhs Left-hand bit array.
    /// @param[in] rhs Right-hand bit array.
    /// @return Intersection bit array using `lhs`'s bound allocator when copy-constructible.
    template <gp::concepts::IsAllocator InOtherAllocator>
    [[nodiscard]] friend BitArray operator&(const BitArray& lhs, const BitArray<InOtherAllocator>& rhs) noexcept
    {
        const SizeType minBits = (lhs.m_numBits < rhs.size()) ? lhs.m_numBits : rhs.size();
        BitArray result = makeEmptyLike(lhs);
        result.resizeUninitialized(minBits);
        const SizeType words = result.wordCount();
        const WordType* const rhsWords = rhs.data();
        if (result.m_words != nullptr && lhs.m_words != nullptr && rhsWords != nullptr &&
            result.m_wordCapacity >= words)
        {
            for (SizeType w = 0u; w < words; ++w)
            {
                result.m_words[w] = lhs.m_words[w] & rhsWords[w];
            }
            container::detail::sanitizeTailWord(result.m_words, minBits);
        }
        return result;
    }

    /// @brief Returns the bitwise union of `lhs` and `rhs` with length `max(lhs.size(), rhs.size())`.
    /// @tparam InOtherAllocator Right-hand allocator type.
    /// @param[in] lhs Left-hand bit array.
    /// @param[in] rhs Right-hand bit array.
    /// @return Union bit array using `lhs`'s bound allocator.
    template <gp::concepts::IsAllocator InOtherAllocator>
    [[nodiscard]] friend BitArray operator|(const BitArray& lhs, const BitArray<InOtherAllocator>& rhs) noexcept
    {
        BitArray result(lhs);
        result |= rhs;
        return result;
    }

    /// @brief Returns the bitwise symmetric difference of `lhs` and `rhs` with length `max(lhs.size(), rhs.size())`.
    /// @tparam InOtherAllocator Right-hand allocator type.
    /// @param[in] lhs Left-hand bit array.
    /// @param[in] rhs Right-hand bit array.
    /// @return Symmetric-difference bit array using `lhs`'s bound allocator.
    template <gp::concepts::IsAllocator InOtherAllocator>
    [[nodiscard]] friend BitArray operator^(const BitArray& lhs, const BitArray<InOtherAllocator>& rhs) noexcept
    {
        BitArray result(lhs);
        result ^= rhs;
        return result;
    }

public:
    /// @brief Returns a range view over all set (`1`) bit indices in ascending order.
    /// @return `SetBitRange` usable directly in range-based `for` loops.
    [[nodiscard]] SetBitRange setBits() const noexcept
    {
        return SetBitRange(m_words, m_numBits, 0u);
    }

    /// @brief Returns a range view over all cleared (`0`) bit indices in ascending order.
    /// @return `ZeroBitRange` usable directly in range-based `for` loops.
    [[nodiscard]] ZeroBitRange zeroBits() const noexcept
    {
        return ZeroBitRange(m_words, m_numBits, 0u);
    }

    /// @brief Returns a `SetBitIterator` starting at or after `startBit`.
    /// @param[in] startBit Earliest bit index to inspect.
    /// @return Start set-bit iterator.
    [[nodiscard]] SetBitIterator beginSetBits(const SizeType startBit = 0u) const noexcept
    {
        return SetBitIterator(m_words, m_numBits, startBit);
    }

    /// @brief Returns the end sentinel for `SetBitIterator`.
    /// @return End set-bit iterator.
    [[nodiscard]] SetBitIterator endSetBits() const noexcept
    {
        return SetBitIterator(m_words, m_numBits, m_numBits);
    }

    /// @brief Returns a `ZeroBitIterator` starting at or after `startBit`.
    /// @param[in] startBit Earliest bit index to inspect.
    /// @return Start zero-bit iterator.
    [[nodiscard]] ZeroBitIterator beginZeroBits(const SizeType startBit = 0u) const noexcept
    {
        return ZeroBitIterator(m_words, m_numBits, startBit);
    }

    /// @brief Returns the end sentinel for `ZeroBitIterator`.
    /// @return End zero-bit iterator.
    [[nodiscard]] ZeroBitIterator endZeroBits() const noexcept
    {
        return ZeroBitIterator(m_words, m_numBits, m_numBits);
    }

    /// @brief Visits every set (`1`) bit index in ascending order using `getAndClearNextBit()`.
    /// @tparam InCallback Callable accepting `SizeType`.
    /// @param[in,out] callback Visitor invoked once per set bit.
    template <typename InCallback>
    void forEachSetBit(InCallback&& callback) const noexcept
    {
        container::detail::forEachSetBit(m_words, m_numBits, std::forward<InCallback>(callback));
    }

    /// @brief Visits every cleared (`0`) bit index in ascending order using `getAndClearNextBit()`.
    /// @tparam InCallback Callable accepting `SizeType`.
    /// @param[in,out] callback Visitor invoked once per zero bit.
    template <typename InCallback>
    void forEachZeroBit(InCallback&& callback) const noexcept
    {
        container::detail::forEachZeroBit(m_words, m_numBits, std::forward<InCallback>(callback));
    }

    /// @brief Visits every set (`1`) bit index in ascending order and clears all bits as they are consumed.
    /// @tparam InCallback Callable accepting `SizeType`.
    /// @param[in,out] callback Visitor invoked once per previously set bit.
    template <typename InCallback>
    void consumeSetBits(InCallback&& callback) noexcept
    {
        container::detail::consumeSetBits(m_words, m_numBits, std::forward<InCallback>(callback));
    }

    /// @brief Returns a mutable random-access iterator to bit `0`.
    /// @return Start bit iterator.
    [[nodiscard]] constexpr Iterator begin() noexcept
    {
        return Iterator(m_words, 0u);
    }

    /// @brief Returns a const random-access iterator to bit `0`.
    /// @return Const start bit iterator.
    [[nodiscard]] constexpr ConstIterator begin() const noexcept
    {
        return ConstIterator(m_words, 0u);
    }

    /// @brief Returns a mutable random-access iterator one past bit `size() - 1`.
    /// @return End bit iterator.
    [[nodiscard]] constexpr Iterator end() noexcept
    {
        return Iterator(m_words, m_numBits);
    }

    /// @brief Returns a const random-access iterator one past bit `size() - 1`.
    /// @return Const end bit iterator.
    [[nodiscard]] constexpr ConstIterator end() const noexcept
    {
        return ConstIterator(m_words, m_numBits);
    }

    /// @brief Returns a const random-access iterator to bit `0`.
    /// @return Const start bit iterator.
    [[nodiscard]] constexpr ConstIterator cbegin() const noexcept
    {
        return begin();
    }

    /// @brief Returns a const random-access iterator one past bit `size() - 1`.
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
    /// @brief Streams all active words and trailing bit count into `hasher` for unified `gp::hashing::hash()`.
    /// @tparam InHasher Concrete streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        container::detail::hashAppendWords(hasher, m_words, m_numBits);
    }

    /// @brief Returns a mutable reference to the bound allocator instance.
    /// @return Reference to `m_allocator`.
    [[nodiscard]] constexpr InAllocator& getAllocator() noexcept
    {
        return m_allocator;
    }

    /// @brief Returns a const reference to the bound allocator instance.
    /// @return Const reference to `m_allocator`.
    [[nodiscard]] constexpr const InAllocator& getAllocator() const noexcept
    {
        return m_allocator;
    }

    /// @brief Exchanges contents with `other` in O(1) when allocators are equal, or via copy/move fallback otherwise.
    /// @param[in,out] other Target bit array to swap with.
    void swap(BitArray& other) noexcept
    {
        if (this == &other)
        {
            return;
        }

        if (m_allocator == other.m_allocator)
        {
            std::swap(m_words, other.m_words);
            std::swap(m_numBits, other.m_numBits);
            std::swap(m_wordCapacity, other.m_wordCapacity);
            return;
        }

        BitArray temp(std::move(*this));
        *this = std::move(other);
        other = std::move(temp);
    }

public:
    /// @brief Compares two bit arrays (across any allocators) for equal bit counts and identical bit values.
    /// @tparam InOtherAllocator Right-hand allocator type.
    /// @param[in] other Bit array to compare against.
    /// @return True if `size()` and all active bits match, false otherwise.
    template <gp::concepts::IsAllocator InOtherAllocator>
    [[nodiscard]] bool operator==(const BitArray<InOtherAllocator>& other) const noexcept
    {
        if (m_numBits != other.m_numBits)
        {
            return false;
        }
        return container::detail::equalWords(m_words, other.m_words, wordCount());
    }

    /// @brief Performs three-way lexicographical comparison starting from bit `0` across any allocators.
    /// @tparam InOtherAllocator Right-hand allocator type.
    /// @param[in] other Bit array to compare against.
    /// @return `std::strong_ordering` at the first differing bit, falling back to `size()` comparison.
    template <gp::concepts::IsAllocator InOtherAllocator>
    [[nodiscard]] std::strong_ordering operator<=>(const BitArray<InOtherAllocator>& other) const noexcept
    {
        return container::detail::compareWordsLexicographical(m_words, m_numBits, other.m_words, other.m_numBits);
    }

private:
    /// @brief Constructs an empty `BitArray` bound to `source`'s allocator when copy-constructible.
    /// @param[in] source Source bit array whose bound allocator is copied when permitted.
    /// @return Empty `BitArray` ready for result storage.
    [[nodiscard]] static BitArray makeEmptyLike(const BitArray& source) noexcept
    {
        if constexpr (std::is_copy_constructible_v<InAllocator>)
        {
            return BitArray(source.m_allocator);
        }
        else
        {
            return BitArray();
        }
    }

    /// @brief Quantizes an explicit word reservation against `m_allocator`.
    /// @param[in] minWords Minimum number of 64-bit words required.
    /// @return Quantized word capacity at least as large as `minWords`.
    [[nodiscard]] SizeType computeReserveWords(const SizeType minWords) const noexcept
    {
        const SizeType quantized =
            gp::memory::calculateSlackReserve<SizeType>(m_allocator, minWords, sizeof(WordType), true, kAllocAlignment);
        return (quantized >= minWords) ? quantized : minWords;
    }

    /// @brief Expands storage to fit at least `minWords` words using geometric allocator growth.
    /// @param[in] minWords Minimum number of 64-bit words required after growth.
    void growWordsForCount(const SizeType minWords) noexcept
    {
        const SizeType grownWords = gp::memory::calculateSlackGrow<SizeType>(
            m_allocator, minWords, m_wordCapacity, sizeof(WordType), true, kAllocAlignment
        );
        const SizeType targetWords = (grownWords >= minWords) ? grownWords : minWords;
        reallocateWords(targetWords);
    }

    /// @brief Reallocates the backing word buffer to `targetWordCapacity` words.
    /// @param[in] targetWordCapacity Desired allocated word capacity.
    void reallocateWords(const SizeType targetWordCapacity) noexcept
    {
        GP_ASSERT(targetWordCapacity >= wordCount());
        if (targetWordCapacity == m_wordCapacity)
        {
            return;
        }

        if (targetWordCapacity == 0u)
        {
            if (m_words != nullptr)
            {
                m_allocator.deallocate(m_words, m_wordCapacity * sizeof(WordType), kAllocAlignment);
                m_words = nullptr;
                m_wordCapacity = 0u;
            }
            return;
        }

        const SizeType newBytes = targetWordCapacity * sizeof(WordType);

        if (m_words == nullptr)
        {
            WordType* const fresh = static_cast<WordType*>(m_allocator.allocate(newBytes, kAllocAlignment));
            GP_ASSERT(fresh != nullptr && "BitArray allocation failed.");
            if (fresh == nullptr) [[unlikely]]
            {
                return;
            }
            m_words = fresh;
            m_wordCapacity = targetWordCapacity;
            return;
        }

        if (m_numBits == 0u)
        {
            m_allocator.deallocate(m_words, m_wordCapacity * sizeof(WordType), kAllocAlignment);
            m_words = static_cast<WordType*>(m_allocator.allocate(newBytes, kAllocAlignment));
            GP_ASSERT(m_words != nullptr && "BitArray reallocation failed.");
            m_wordCapacity = (m_words != nullptr) ? targetWordCapacity : 0u;
            return;
        }

        WordType* const resized = static_cast<WordType*>(
            m_allocator.reallocate(m_words, m_wordCapacity * sizeof(WordType), newBytes, kAllocAlignment)
        );
        GP_ASSERT(resized != nullptr && "BitArray reallocation failed.");
        if (resized == nullptr) [[unlikely]]
        {
            return;
        }
        m_words = resized;
        m_wordCapacity = targetWordCapacity;
    }

    /// @brief Copies `bitCount` bits from `words` into freshly reserved storage.
    /// @param[in] words Source word buffer (`nullptr` or `bitCount == 0` leaves `*this` unchanged).
    /// @param[in] bitCount Logical number of bits to copy.
    void copyWordsFrom(const WordType* const words, const SizeType bitCount) noexcept
    {
        if (bitCount == 0u || words == nullptr)
        {
            return;
        }
        const SizeType neededWords = container::detail::wordCountForBits(bitCount);
        if (neededWords > m_wordCapacity)
        {
            reallocateWords(computeReserveWords(neededWords));
            if (m_wordCapacity < neededWords) [[unlikely]]
            {
                return;
            }
        }
        container::detail::copyWords(m_words, words, neededWords);
        container::detail::sanitizeTailWord(m_words, bitCount);
        m_numBits = bitCount;
    }

    /// @brief Steals internal buffer pointers from `other` in O(1) time.
    /// @param[in,out] other Source bit array reset to empty state.
    void stealFrom(BitArray& other) noexcept
    {
        m_words = other.m_words;
        m_numBits = other.m_numBits;
        m_wordCapacity = other.m_wordCapacity;

        other.m_words = nullptr;
        other.m_numBits = 0u;
        other.m_wordCapacity = 0u;
    }

private:
    WordType* m_words{ nullptr };
    SizeType m_numBits{ 0u };
    SizeType m_wordCapacity{ 0u };
    GP_NO_UNIQUE_ADDRESS InAllocator m_allocator{};
};

/// @brief Exchanges two `BitArray` instances via `lhs.swap(rhs)`.
/// @tparam InAllocator Allocator type.
/// @param[in,out] lhs First bit array.
/// @param[in,out] rhs Second bit array.
template <gp::concepts::IsAllocator InAllocator>
inline void swap(BitArray<InAllocator>& lhs, BitArray<InAllocator>& rhs) noexcept
{
    lhs.swap(rhs);
}

static_assert(sizeof(BitArray<>) == 24u, "gp::BitArray with stateless DefaultAllocator must occupy exactly 24 bytes.");

}   // namespace gp

namespace gp::concepts
{

namespace detail
{

template <typename InType>
constexpr bool IsBitArrayV = false;

template <typename InAllocator>
constexpr bool IsBitArrayV<gp::BitArray<InAllocator>> = true;

}   // namespace detail

/// @brief Concept satisfied by any instantiation of `gp::BitArray`.
/// @tparam InType Candidate type to test.
template <typename InType>
concept IsBitArray = detail::IsBitArrayV<std::remove_cvref_t<InType>>;

/// @brief Specialization enabling bitwise relocation of `gp::BitArray` when its allocator is bitwise relocatable.
/// @tparam InAllocator Allocator type.
template <typename InAllocator>
struct IsRelocatableTrait<gp::BitArray<InAllocator>>
    : std::bool_constant<std::is_move_constructible_v<InAllocator> && IsRelocatable<InAllocator>>
{};

}   // namespace gp::concepts
