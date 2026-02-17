// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/BasicStringView.hpp"
#include "CoreBuild.hpp"
#include "Memory/Allocator.hpp"
#include "Memory/MemoryUtilities.hpp"
#include "Templates/Concepts.hpp"
#include <algorithm>
#include <compare>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <utility>

namespace GP::Container
{

/// \brief A high-performance owning string class with Small String Optimization (SSO).
///
/// Short strings (up to SSO_CAPACITY characters) are stored inline within the object footprint,
/// avoiding heap allocation entirely. Longer strings are heap-allocated using the provided allocator.
///
/// The SSO layout uses the last byte of the object to distinguish between modes:
///   - SSO mode: the last byte stores (SSO_CAPACITY - size), which is always in [0, SSO_CAPACITY].
///   - Heap mode: the last byte has its high bit set (capacity stores the flag in its MSB).
///
/// \tparam CharType The character type (e.g., char, wchar_t, char16_t, char32_t).
template <Concepts::IsCharacter CharType>
class TBasicString
{
public:
    using ValueType = CharType;
    using Pointer = CharType*;
    using ConstPointer = const CharType*;
    using Reference = CharType&;
    using ConstReference = const CharType&;
    using SizeType = SizeT;
    using DifferenceType = OffsetT;
    using Iterator = CharType*;
    using ConstIterator = const CharType*;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;
    using ViewType = TBasicStringView<CharType>;
    using AllocatorType = Memory::TAllocator<CharType>;

public:
    static constexpr SizeType NPOS = static_cast<SizeType>(-1);

private:
    static constexpr SizeType HEAP_STRUCT_SIZE = sizeof(Pointer) + sizeof(SizeType) + sizeof(SizeType);
    static constexpr SizeType SSO_BUFFER_SIZE = HEAP_STRUCT_SIZE;
    static constexpr SizeType SSO_CAPACITY = SSO_BUFFER_SIZE / sizeof(CharType) - 1;
    static constexpr SizeType HEAP_FLAG_BIT = SizeType(1) << (sizeof(SizeType) * 8 - 1);

    /// \brief Internal structure for heap storage mode.
    struct HeapData
    {
        Pointer data;        //<! Pointer to the heap-allocated character array
        SizeType size;       //<! Number of characters currently stored (excluding null terminator)
        SizeType capacity;   //<! MSB is the heap flag, remaining bits store the capacity (excluding null terminator)
    };

    /// \brief Internal structure for SSO storage mode. The actual size is determined by SSO_BUFFER_SIZE.
    struct SSOData
    {
        CharType buffer[SSO_BUFFER_SIZE / sizeof(CharType)];
    };

    /// \brief Union to store either heap data or SSO data. The mode is determined by the last byte.
    union
    {
        HeapData m_heap;   //<! Used when the string is heap-allocated (size > SSO_CAPACITY)
        SSOData m_sso;     //<! Used when the string fits in SSO (size <= SSO_CAPACITY)
    };

    GP_MAYBE_UNUSED AllocatorType m_allocator;   //<! Allocator instance (only used in heap mode)

public:
    /// \brief Default constructor. Creates an empty SSO string.
    constexpr TBasicString() noexcept;

    /// \brief Constructs a string from a null-terminated C-string.
    /// \param str Pointer to the null-terminated string.
    TBasicString(ConstPointer str);

    /// \brief Constructs a string from a pointer and length.
    /// \param str Pointer to the first character.
    /// \param count Number of characters.
    TBasicString(ConstPointer str, SizeType count);

    /// \brief Constructs a string by repeating a character.
    /// \param count Number of times to repeat the character.
    /// \param ch The character to repeat.
    TBasicString(SizeType count, CharType ch);

    /// \brief Constructs a string from a string view.
    /// \param view The string view to copy from.
    explicit TBasicString(ViewType view);

    /// \brief Constructs a string from an initializer list of characters.
    /// \param initList The initializer list.
    TBasicString(std::initializer_list<CharType> initList);

    /// \brief Copy constructor.
    /// \param other The string to copy from.
    TBasicString(const TBasicString& other);

    /// \brief Move constructor.
    /// \param other The string to move from.
    TBasicString(TBasicString&& other) noexcept;

    /// \brief Destructor. Frees heap memory if not in SSO mode.
    ~TBasicString();

public:
    /// \brief Copy assignment operator.
    /// \param other The string to copy from.
    /// \return A reference to this string.
    TBasicString& operator=(const TBasicString& other);

    /// \brief Move assignment operator.
    /// \param other The string to move from.
    /// \return A reference to this string.
    TBasicString& operator=(TBasicString&& other) noexcept;

    /// \brief Assignment from a null-terminated C-string.
    /// \param str Pointer to the null-terminated string.
    /// \return A reference to this string.
    TBasicString& operator=(ConstPointer str);

    /// \brief Assignment from a single character.
    /// \param ch The character to assign.
    /// \return A reference to this string.
    TBasicString& operator=(CharType ch);

    /// \brief Assignment from a string view.
    /// \param view The string view to assign from.
    /// \return A reference to this string.
    TBasicString& operator=(ViewType view);

    /// \brief Assignment from an initializer list of characters.
    /// \param initList The initializer list.
    /// \return A reference to this string.
    TBasicString& operator=(std::initializer_list<CharType> initList);

    /// \brief Subscript operator for non-const access.
    /// \param index The index of the character to access.
    /// \return A reference to the character at the specified index.
    GP_NODISCARD Reference operator[](SizeType index) noexcept;

    /// \brief Subscript operator for const access.
    /// \param index The index of the character to access.
    /// \return A const reference to the character at the specified index.
    GP_NODISCARD ConstReference operator[](SizeType index) const noexcept;

    /// \brief Append operator for a string.
    /// \param other The string to append.
    /// \return A reference to this string.
    TBasicString& operator+=(const TBasicString& other);

    /// \brief Append operator for a null-terminated C-string.
    /// \param str The C-string to append.
    /// \return A reference to this string.
    TBasicString& operator+=(ConstPointer str);

    /// \brief Append operator for a single character.
    /// \param ch The character to append.
    /// \return A reference to this string.
    TBasicString& operator+=(CharType ch);

    /// \brief Append operator for a string view.
    /// \param view The string view to append.
    /// \return A reference to this string.
    TBasicString& operator+=(ViewType view);

    /// \brief Implicit conversion to TBasicStringView.
    /// \return A view of this string.
    GP_NODISCARD operator ViewType() const noexcept;

public:
    /// \brief Accesses the character at the specified index with bounds checking.
    /// \param index The index of the character to access.
    /// \return A reference to the character at the specified index.
    GP_NODISCARD Reference At(SizeType index);

    /// \brief Accesses the character at the specified index with bounds checking (const).
    /// \param index The index of the character to access.
    /// \return A const reference to the character at the specified index.
    GP_NODISCARD ConstReference At(SizeType index) const;

    /// \brief Returns a reference to the first character.
    /// \return A reference to the first character.
    GP_NODISCARD Reference Front() noexcept;

    /// \brief Returns a const reference to the first character.
    /// \return A const reference to the first character.
    GP_NODISCARD ConstReference Front() const noexcept;

    /// \brief Returns a reference to the last character.
    /// \return A reference to the last character.
    GP_NODISCARD Reference Back() noexcept;

    /// \brief Returns a const reference to the last character.
    /// \return A const reference to the last character.
    GP_NODISCARD ConstReference Back() const noexcept;

    /// \brief Returns a pointer to the underlying character array. Always null-terminated.
    /// \return A pointer to the first character.
    GP_NODISCARD Pointer Data() noexcept;

    /// \brief Returns a const pointer to the underlying character array. Always null-terminated.
    /// \return A const pointer to the first character.
    GP_NODISCARD ConstPointer Data() const noexcept;

    /// \brief Returns a const pointer to a null-terminated character array.
    /// \return A const pointer to the null-terminated character array.
    GP_NODISCARD ConstPointer CStr() const noexcept;

    /// \brief Returns the number of characters in the string (not including the null terminator).
    /// \return The size of the string.
    GP_NODISCARD SizeType Size() const noexcept;

    /// \brief Returns the number of characters in the string (alias for Size).
    /// \return The length of the string.
    GP_NODISCARD SizeType Length() const noexcept;

    /// \brief Returns the number of characters that the string has allocated space for.
    /// \return The capacity of the string (not including space for the null terminator).
    GP_NODISCARD SizeType Capacity() const noexcept;

    /// \brief Checks whether the string is empty.
    /// \return true if the string is empty, false otherwise.
    GP_NODISCARD bool IsEmpty() const noexcept;

    /// \brief Returns the maximum possible string length.
    /// \return The maximum size.
    GP_NODISCARD SizeType MaxSize() const noexcept;

    /// \brief Returns true if the string is currently using the small string optimization buffer.
    /// \return true if SSO is active, false if heap-allocated.
    GP_NODISCARD bool IsSSO() const noexcept;

    /// \brief Reserves storage for at least newCapacity characters (not including the null terminator).
    /// \param newCapacity The minimum capacity to reserve.
    void Reserve(SizeType newCapacity);

    /// \brief Reduces capacity to fit the current size. May transition from heap to SSO if possible.
    void ShrinkToFit();

    /// \brief Clears the string contents, resetting it to an empty SSO string.
    void Clear() noexcept;

    /// \brief Appends a single character to the end of the string.
    /// \param ch The character to append.
    void PushBack(CharType ch);

    /// \brief Removes the last character from the string.
    void PopBack();

    /// \brief Appends a string to the end.
    /// \param other The string to append.
    /// \return A reference to this string.
    TBasicString& Append(const TBasicString& other);

    /// \brief Appends a null-terminated C-string.
    /// \param str The C-string to append.
    /// \return A reference to this string.
    TBasicString& Append(ConstPointer str);

    /// \brief Appends characters from a pointer and length.
    /// \param str Pointer to the first character.
    /// \param count Number of characters to append.
    /// \return A reference to this string.
    TBasicString& Append(ConstPointer str, SizeType count);

    /// \brief Appends a character repeated count times.
    /// \param count Number of times to repeat.
    /// \param ch The character to append.
    /// \return A reference to this string.
    TBasicString& Append(SizeType count, CharType ch);

    /// \brief Appends a string view.
    /// \param view The string view to append.
    /// \return A reference to this string.
    TBasicString& Append(ViewType view);

    /// \brief Inserts characters at the specified position.
    /// \param index The position to insert at.
    /// \param str Pointer to the characters to insert.
    /// \param count Number of characters to insert.
    /// \return A reference to this string.
    TBasicString& Insert(SizeType index, ConstPointer str, SizeType count);

    /// \brief Inserts a null-terminated C-string at the specified position.
    /// \param index The position to insert at.
    /// \param str The C-string to insert.
    /// \return A reference to this string.
    TBasicString& Insert(SizeType index, ConstPointer str);

    /// \brief Inserts a character repeated count times at the specified position.
    /// \param index The position to insert at.
    /// \param count Number of times to repeat.
    /// \param ch The character to insert.
    /// \return A reference to this string.
    TBasicString& Insert(SizeType index, SizeType count, CharType ch);

    /// \brief Inserts a string view at the specified position.
    /// \param index The position to insert at.
    /// \param view The string view to insert.
    /// \return A reference to this string.
    TBasicString& Insert(SizeType index, ViewType view);

    /// \brief Erases characters from the string.
    /// \param index The starting position (default is 0).
    /// \param count The number of characters to erase (default is NPOS, meaning erase to end).
    /// \return A reference to this string.
    TBasicString& Erase(SizeType index = 0, SizeType count = NPOS);

    /// \brief Replaces a range of characters with new content.
    /// \param pos The starting position of the range to replace.
    /// \param count The number of characters to replace.
    /// \param str Pointer to the replacement characters.
    /// \param count2 Number of replacement characters.
    /// \return A reference to this string.
    TBasicString& Replace(SizeType pos, SizeType count, ConstPointer str, SizeType count2);

    /// \brief Replaces a range of characters with a null-terminated C-string.
    /// \param pos The starting position of the range to replace.
    /// \param count The number of characters to replace.
    /// \param str The replacement C-string.
    /// \return A reference to this string.
    TBasicString& Replace(SizeType pos, SizeType count, ConstPointer str);

    /// \brief Replaces a range of characters with a string view.
    /// \param pos The starting position of the range to replace.
    /// \param count The number of characters to replace.
    /// \param view The replacement string view.
    /// \return A reference to this string.
    TBasicString& Replace(SizeType pos, SizeType count, ViewType view);

    /// \brief Resizes the string to the specified size. New characters are value-initialized (null).
    /// \param newSize The new size.
    void Resize(SizeType newSize);

    /// \brief Resizes the string to the specified size. New characters are filled with ch.
    /// \param newSize The new size.
    /// \param ch The fill character.
    void Resize(SizeType newSize, CharType ch);

    /// \brief Swaps the contents of this string with another.
    /// \param other The string to swap with.
    void Swap(TBasicString& other) noexcept;

    /// \brief Finds the first occurrence of the specified substring.
    /// \param sv The substring to find.
    /// \param pos The position to start searching from (default is 0).
    /// \return The index of the first occurrence, or NPOS if not found.
    GP_NODISCARD SizeType Find(ViewType sv, SizeType pos = 0) const noexcept;

    /// \brief Finds the first occurrence of the specified character.
    /// \param ch The character to find.
    /// \param pos The position to start searching from (default is 0).
    /// \return The index of the first occurrence, or NPOS if not found.
    GP_NODISCARD SizeType Find(CharType ch, SizeType pos = 0) const noexcept;

    /// \brief Finds the first occurrence of the specified null-terminated C-string.
    /// \param str The C-string to find.
    /// \param pos The position to start searching from (default is 0).
    /// \return The index of the first occurrence, or NPOS if not found.
    GP_NODISCARD SizeType Find(ConstPointer str, SizeType pos = 0) const noexcept;

    /// \brief Finds the last occurrence of the specified substring.
    /// \param sv The substring to find.
    /// \param pos The position to start searching backwards from (default is NPOS).
    /// \return The index of the last occurrence, or NPOS if not found.
    GP_NODISCARD SizeType RFind(ViewType sv, SizeType pos = NPOS) const noexcept;

    /// \brief Finds the last occurrence of the specified character.
    /// \param ch The character to find.
    /// \param pos The position to start searching backwards from (default is NPOS).
    /// \return The index of the last occurrence, or NPOS if not found.
    GP_NODISCARD SizeType RFind(CharType ch, SizeType pos = NPOS) const noexcept;

    /// \brief Finds the first occurrence of any character in the given set.
    /// \param sv The set of characters to search for.
    /// \param pos The position to start searching from (default is 0).
    /// \return The index of the first match, or NPOS if not found.
    GP_NODISCARD SizeType FindFirstOf(ViewType sv, SizeType pos = 0) const noexcept;

    /// \brief Finds the first occurrence of the specified character.
    /// \param ch The character to search for.
    /// \param pos The position to start searching from (default is 0).
    /// \return The index of the first match, or NPOS if not found.
    GP_NODISCARD SizeType FindFirstOf(CharType ch, SizeType pos = 0) const noexcept;

    /// \brief Finds the last occurrence of any character in the given set.
    /// \param sv The set of characters to search for.
    /// \param pos The position to start searching backwards from (default is NPOS).
    /// \return The index of the last match, or NPOS if not found.
    GP_NODISCARD SizeType FindLastOf(ViewType sv, SizeType pos = NPOS) const noexcept;

    /// \brief Finds the last occurrence of the specified character.
    /// \param ch The character to search for.
    /// \param pos The position to start searching backwards from (default is NPOS).
    /// \return The index of the last match, or NPOS if not found.
    GP_NODISCARD SizeType FindLastOf(CharType ch, SizeType pos = NPOS) const noexcept;

    /// \brief Finds the first character not in the given set.
    /// \param sv The set of characters to avoid.
    /// \param pos The position to start searching from (default is 0).
    /// \return The index of the first non-matching character, or NPOS if not found.
    GP_NODISCARD SizeType FindFirstNotOf(ViewType sv, SizeType pos = 0) const noexcept;

    /// \brief Finds the first character not equal to the specified character.
    /// \param ch The character to avoid.
    /// \param pos The position to start searching from (default is 0).
    /// \return The index of the first non-matching character, or NPOS if not found.
    GP_NODISCARD SizeType FindFirstNotOf(CharType ch, SizeType pos = 0) const noexcept;

    /// \brief Finds the last character not in the given set.
    /// \param sv The set of characters to avoid.
    /// \param pos The position to start searching backwards from (default is NPOS).
    /// \return The index of the last non-matching character, or NPOS if not found.
    GP_NODISCARD SizeType FindLastNotOf(ViewType sv, SizeType pos = NPOS) const noexcept;

    /// \brief Finds the last character not equal to the specified character.
    /// \param ch The character to avoid.
    /// \param pos The position to start searching backwards from (default is NPOS).
    /// \return The index of the last non-matching character, or NPOS if not found.
    GP_NODISCARD SizeType FindLastNotOf(CharType ch, SizeType pos = NPOS) const noexcept;

    /// \brief Checks if the string contains the specified substring.
    /// \param sv The substring to search for.
    /// \return true if found, false otherwise.
    GP_NODISCARD bool Contains(ViewType sv) const noexcept;

    /// \brief Checks if the string contains the specified character.
    /// \param ch The character to search for.
    /// \return true if found, false otherwise.
    GP_NODISCARD bool Contains(CharType ch) const noexcept;

    /// \brief Checks if the string starts with the specified prefix.
    /// \param sv The prefix to check for.
    /// \return true if the string starts with the prefix, false otherwise.
    GP_NODISCARD bool StartsWith(ViewType sv) const noexcept;

    /// \brief Checks if the string starts with the specified character.
    /// \param ch The character to check for.
    /// \return true if the string starts with the character, false otherwise.
    GP_NODISCARD bool StartsWith(CharType ch) const noexcept;

    /// \brief Checks if the string ends with the specified suffix.
    /// \param sv The suffix to check for.
    /// \return true if the string ends with the suffix, false otherwise.
    GP_NODISCARD bool EndsWith(ViewType sv) const noexcept;

    /// \brief Checks if the string ends with the specified character.
    /// \param ch The character to check for.
    /// \return true if the string ends with the character, false otherwise.
    GP_NODISCARD bool EndsWith(CharType ch) const noexcept;

    /// \brief Returns a substring as a new TBasicString.
    /// \param pos The starting position (default is 0).
    /// \param count The number of characters (default is NPOS, meaning to end).
    /// \return A new string containing the substring.
    GP_NODISCARD TBasicString Substr(SizeType pos = 0, SizeType count = NPOS) const;

    /// \brief Compares this string with another string view.
    /// \param other The string view to compare with.
    /// \return Negative if less, zero if equal, positive if greater.
    GP_NODISCARD int Compare(ViewType other) const noexcept;

    /// \brief Calculates a hash value for the string.
    /// \return A hash value representing the contents of the string.
    GP_NODISCARD SizeType GetHash() const noexcept;

    /// \brief Returns an iterator to the beginning of the string.
    /// \return An iterator to the first character.
    Iterator Begin() noexcept;

    /// \brief Returns an iterator to the end of the string.
    /// \return An iterator to one past the last character.
    Iterator End() noexcept;

    /// \brief Returns an iterator to the beginning of the string (const).
    /// \return An iterator to the first character.
    ConstIterator Begin() const noexcept;

    /// \brief Returns an iterator to the end of the string (const).
    /// \return An iterator to one past the last character.
    ConstIterator End() const noexcept;

    /// \brief Returns an iterator to the beginning of the string (const).
    /// \return An iterator to the first character.
    ConstIterator CBegin() const noexcept;

    /// \brief Returns an iterator to the end of the string (const).
    /// \return An iterator to one past the last character.
    ConstIterator CEnd() const noexcept;

    /// \brief Returns a reverse iterator to the beginning of the reversed string.
    /// \return A reverse iterator to the last character.
    ReverseIterator RBegin() noexcept;

    /// \brief Returns a reverse iterator to the end of the reversed string.
    /// \return A reverse iterator to one before the first character.
    ReverseIterator REnd() noexcept;

    /// \brief Returns a reverse iterator to the beginning of the reversed string (const).
    /// \return A reverse iterator to the last character.
    ConstReverseIterator CRBegin() const noexcept;

    /// \brief Returns a reverse iterator to the end of the reversed string (const).
    /// \return A reverse iterator to one before the first character.
    ConstReverseIterator CREnd() const noexcept;

    /// \brief Returns an iterator to the beginning of the string.
    /// \return An iterator to the first character.
    Iterator begin() noexcept;

    /// \brief Returns an iterator to the end of the string.
    /// \return An iterator to one past the last character.
    Iterator end() noexcept;

    /// \brief Returns an iterator to the beginning of the string (const).
    /// \return An iterator to the first character.
    ConstIterator begin() const noexcept;

    /// \brief Returns an iterator to the end of the string (const).
    /// \return An iterator to one past the last character.
    ConstIterator end() const noexcept;

    /// \brief Returns an iterator to the beginning of the string (const).
    /// \return An iterator to the first character.
    ConstIterator cbegin() const noexcept;

    /// \brief Returns an iterator to the end of the string (const).
    /// \return An iterator to one past the last character.
    ConstIterator cend() const noexcept;

    /// \brief Returns a reverse iterator to the beginning of the reversed string.
    /// \return A reverse iterator to the last character.
    ReverseIterator rbegin() noexcept;

    /// \brief Returns a reverse iterator to the end of the reversed string.
    /// \return A reverse iterator to one before the first character.
    ReverseIterator rend() noexcept;

    /// \brief Returns a reverse iterator to the beginning of the reversed string (const).
    /// \return A reverse iterator to the last character.
    ConstReverseIterator crbegin() const noexcept;

    /// \brief Returns a reverse iterator to the end of the reversed string (const).
    /// \return A reverse iterator to one before the first character.
    ConstReverseIterator crend() const noexcept;

private:
    /// \brief Sets the string to SSO mode with the given size.
    /// \param size The size of the string (must be <= SSO_CAPACITY).
    void SetSSOSize(SizeType size) noexcept;

    /// \brief Gets the size when in SSO mode.
    /// \return The size of the string in SSO mode.
    GP_NODISCARD SizeType GetSSOSize() const noexcept;

    /// \brief Gets the pointer to the SSO buffer.
    /// \return A pointer to the SSO buffer.
    GP_NODISCARD Pointer GetSSOBuffer() noexcept;

    /// \brief Gets the const pointer to the SSO buffer.
    /// \return A const pointer to the SSO buffer.
    GP_NODISCARD ConstPointer GetSSOBuffer() const noexcept;

    /// \brief Sets heap mode fields.
    /// \param data Pointer to the heap-allocated character array.
    /// \param size The size of the string (number of characters).
    void SetHeapData(Pointer data, SizeType size, SizeType capacity) noexcept;

    /// \brief Gets the raw capacity without the heap flag bit.
    /// \return The capacity of the heap buffer (not including null terminator).
    GP_NODISCARD SizeType GetHeapCapacity() const noexcept;

    /// \brief Calculates the growth capacity for a required size.
    /// \param required The required size that needs to be accommodated.
    /// \return The new capacity to grow to, which is at least as large as required and follows a growth strategy.
    GP_NODISCARD SizeType CalculateGrowth(SizeType required) const noexcept;

    /// \brief Ensures the string has enough capacity for at least newCapacity characters. Transitions from SSO to heap
    /// if necessary.
    /// \param newCapacity The minimum capacity to ensure.
    void GrowTo(SizeType newCapacity);

    /// \brief Allocates a new buffer for the given capacity (number of characters, not including null).
    /// \param capacity The capacity to allocate for.
    /// \return A pointer to the newly allocated buffer.
    GP_NODISCARD Pointer AllocateBuffer(SizeType capacity);

    /// \brief Frees the heap buffer if currently in heap mode.
    void FreeHeapBuffer();

    /// \brief Assigns content from a character pointer and length, handling SSO/heap transitions.
    /// \param str Pointer to the first character.
    /// \param count Number of characters to assign.
    void AssignInternal(ConstPointer str, SizeType count);

    /// \brief Appends content from a character pointer and length.
    /// \param str Pointer to the first character.
    /// \param count Number of characters to append.
    void AppendInternal(ConstPointer str, SizeType count);
};

}   // namespace GP::Container

/// \brief Equality operator for TBasicString.
/// \param lhs The left-hand side string.
/// \param rhs The right-hand side string.
/// \return true if the strings are equal, false otherwise.
template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD constexpr bool operator==(
    const GP::Container::TBasicString<CharType>& lhs, const GP::Container::TBasicString<CharType>& rhs
) noexcept;

/// \brief Three-way comparison operator for TBasicString.
/// \param lhs The left-hand side string.
/// \param rhs The right-hand side string.
/// \return A strong ordering indicating the relative order of the strings.
template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD constexpr std::strong_ordering operator<=>(
    const GP::Container::TBasicString<CharType>& lhs, const GP::Container::TBasicString<CharType>& rhs
) noexcept;

/// \brief Equality operator for TBasicString vs TBasicStringView.
/// \param lhs The left-hand side string.
/// \param rhs The right-hand side string view.
/// \return true if the string and string view are equal, false otherwise.
template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD constexpr bool operator==(
    const GP::Container::TBasicString<CharType>& lhs, GP::Container::TBasicStringView<CharType> rhs
) noexcept;

/// \brief Three-way comparison operator for TBasicString vs TBasicStringView.
/// \param lhs The left-hand side string.
/// \param rhs The right-hand side string view.
/// \return A strong ordering indicating the relative order of the string and string view.
template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD constexpr std::strong_ordering operator<=>(
    const GP::Container::TBasicString<CharType>& lhs, GP::Container::TBasicStringView<CharType> rhs
) noexcept;

/// \brief Equality operator for TBasicString vs C-string.
/// \param lhs The left-hand side string.
/// \param rhs The right-hand side null-terminated C-string.
/// \return true if the string and C-string are equal, false otherwise.
template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD constexpr bool operator==(const GP::Container::TBasicString<CharType>& lhs, const CharType* rhs) noexcept;

/// \brief Three-way comparison operator for TBasicString vs C-string.
/// \param lhs The left-hand side string.
/// \param rhs The right-hand side null-terminated C-string.
/// \return A strong ordering indicating the relative order of the string and C-string.
template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD constexpr std::strong_ordering
    operator<=>(const GP::Container::TBasicString<CharType>& lhs, const CharType* rhs) noexcept;

/// \brief Concatenation operator for two TBasicStrings.
/// \param lhs The left-hand side string.
/// \param rhs The right-hand side string.
/// \return A new TBasicString containing the concatenation of lhs and rhs.
template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD GP::Container::TBasicString<CharType>
    operator+(const GP::Container::TBasicString<CharType>& lhs, const GP::Container::TBasicString<CharType>& rhs);

/// \brief Concatenation operator for TBasicString and C-string.
/// \param lhs The left-hand side string.
/// \param rhs The right-hand side null-terminated C-string.
/// \return A new TBasicString containing the concatenation of the string and C-string.
template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD GP::Container::TBasicString<CharType>
    operator+(const GP::Container::TBasicString<CharType>& lhs, const CharType* rhs);

/// \brief Concatenation operator for C-string and TBasicString.
/// \param lhs The left-hand side null-terminated C-string.
/// \param rhs The right-hand side string.
/// \return A new TBasicString containing the concatenation of the C-string and string.
template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD GP::Container::TBasicString<CharType>
    operator+(const CharType* lhs, const GP::Container::TBasicString<CharType>& rhs);

/// \brief Concatenation operator for TBasicString and a character.
/// \param lhs The left-hand side string.
/// \param rhs The right-hand side character.
/// \return A new TBasicString containing the concatenation of the string and character.
template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD GP::Container::TBasicString<CharType>
    operator+(const GP::Container::TBasicString<CharType>& lhs, CharType rhs);

/// \brief Concatenation operator for a character and TBasicString.
/// \param lhs The left-hand side character.
/// \param rhs The right-hand side string.
/// \return A new TBasicString containing the concatenation of the character and string.
template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD GP::Container::TBasicString<CharType>
    operator+(CharType lhs, const GP::Container::TBasicString<CharType>& rhs);

// Include the implementation
#include "Container/BasicString.inl"
