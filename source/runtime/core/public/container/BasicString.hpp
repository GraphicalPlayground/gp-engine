// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "container/BasicStringView.hpp"
#include "container/Forward.hpp"
#include "CoreMinimal.hpp"
#include "math/LinearAlgebra.hpp"
#include "memory/AllocatorTraits.hpp"
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <ostream>

namespace gp
{

/// @brief Dynamically sized string with Small String Optimization (SSO).
/// @details
/// sizeof(BasicString<char>) == 32, giving a 23-character SSO buffer.
/// Two gp::String instances fit exactly into a 64-byte cache line.
///
/// Memory layout (little-endian, 64-bit):
///   [allocator_ptr : 8 bytes]
///   [union : 24 bytes]
///     SSO  mode: [chars[23] | tag]   tag = remaining_capacity (0..23)
///     Heap mode: [data_ptr:8 | size:8 | capacity_with_flag:8]
///                MSB of capacity_with_flag is set to 1 to mark heap mode.
///                tag byte (last byte of the union) will be >= 0x80.
///
/// All allocations route through AllocatorTraits. No global new/delete.
/// @tparam CharT  Character type.
/// @tparam Traits Character traits (defaults to std::char_traits<CharT>).
/// @tparam Alloc  Allocator type (defaults to gp::memory::PolymorphicAllocator<CharT>).
template <typename CharT, typename Traits, typename Alloc>
class BasicString
{
public:
    using TraitsType = Traits;
    using ValueType = CharT;
    using AllocatorType = Alloc;
    using AllocTraits = gp::memory::AllocatorTraits<Alloc>;
    using SizeType = gp::USize;
    using DifferenceType = gp::ISize;
    using Reference = ValueType&;
    using ConstReference = const ValueType&;
    using Pointer = ValueType*;
    using ConstPointer = const ValueType*;
    using Iterator = Pointer;
    using ConstIterator = ConstPointer;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

    static constexpr SizeType npos = static_cast<SizeType>(-1);

private:
    static constexpr SizeType ssoUnionSize = 24;
    static constexpr SizeType ssoMaxChars = ssoUnionSize / sizeof(CharT) - 1;
    static constexpr gp::UInt8 heapFlag = 0x80;

    /// @brief Storage layout for heap-allocated strings.
    struct HeapData
    {
        Pointer data;
        SizeType size;
        SizeType capacity;
    };

    /// @brief Union of SSO buffer and heap data, discriminated by the MSB of the last byte.
    union StorageUnion
    {
        HeapData heap;
        CharT sso[ssoUnionSize / sizeof(CharT)];
    };

    static_assert(sizeof(StorageUnion) == ssoUnionSize, "SSO union size mismatch");

    AllocatorType m_allocator{};
    StorageUnion m_storage{};

public:
    /// @brief Default constructs an empty string (SSO mode, zero length).
    BasicString() noexcept { _initSsoEmpty(); }

    /// @brief Constructs an empty string with the given allocator.
    /// @param[in] alloc The allocator instance to use for future allocations.
    explicit BasicString(const AllocatorType& alloc) noexcept
        : m_allocator(alloc)
    {
        _initSsoEmpty();
    }

    /// @brief Constructs a string from a null-terminated C string.
    /// @param[in] str Pointer to a null-terminated character array.
    /// @param[in] alloc The allocator instance.
    BasicString(const CharT* str, const AllocatorType& alloc = AllocatorType{})
        : m_allocator(alloc)
    {
        std::memset(&m_storage, 0, ssoUnionSize);
        if (str)
        {
            const SizeType len = Traits::length(str);
            _initFrom(str, len);
        }
        else { _setSsoSize(0); }
    }

    /// @brief Constructs a string from a character buffer and length.
    /// @param[in] str Pointer to the character buffer.
    /// @param[in] count Number of characters.
    /// @param[in] alloc The allocator instance.
    BasicString(const CharT* str, SizeType count, const AllocatorType& alloc = AllocatorType{})
        : m_allocator(alloc)
    {
        std::memset(&m_storage, 0, ssoUnionSize);
        _initFrom(str, count);
    }

    /// @brief Constructs a string with count copies of character ch.
    /// @param[in] count Number of characters.
    /// @param[in] ch The fill character.
    /// @param[in] alloc The allocator instance.
    BasicString(SizeType count, CharT ch, const AllocatorType& alloc = AllocatorType{})
        : m_allocator(alloc)
    {
        std::memset(&m_storage, 0, ssoUnionSize);
        if (count <= ssoMaxChars)
        {
            for (SizeType i = 0; i < count; ++i) { _ssoData()[i] = ch; }
            _ssoData()[count] = CharT{};
            _setSsoSize(count);
        }
        else
        {
            Pointer buffer = _heapAlloc(count);
            for (SizeType i = 0; i < count; ++i) { buffer[i] = ch; }
            buffer[count] = CharT{};
            m_storage.heap.data = buffer;
            m_storage.heap.size = count;
            _setHeapCapacity(count);
        }
    }

    /// @brief Constructs from an initializer list.
    /// @param[in] init The initializer list of characters.
    /// @param[in] alloc The allocator instance.
    BasicString(std::initializer_list<CharT> init, const AllocatorType& alloc = AllocatorType{})
        : m_allocator(alloc)
    {
        std::memset(&m_storage, 0, ssoUnionSize);
        _initFrom(init.begin(), init.size());
    }

    /// @brief Constructs from a string_view.
    /// @param[in] sv The string_view to copy characters from.
    /// @param[in] alloc The allocator instance.
    BasicString(BasicStringView<CharT, Traits> sv, const AllocatorType& alloc = AllocatorType{})
        : m_allocator(alloc)
    {
        std::memset(&m_storage, 0, ssoUnionSize);
        _initFrom(sv.data(), sv.size());
    }

    /// @brief Copy constructor.
    /// @param[in] other The string to copy from.
    BasicString(const BasicString& other)
        : m_allocator(AllocTraits::selectOnContainerCopyConstruction(other.m_allocator))
    {
        std::memset(&m_storage, 0, ssoUnionSize);
        _initFrom(other.data(), other.size());
    }

    /// @brief Copy constructor with explicit allocator.
    /// @param[in] other The string to copy from.
    /// @param[in] alloc The allocator instance to use for this string.
    BasicString(const BasicString& other, const AllocatorType& alloc)
        : m_allocator(alloc)
    {
        std::memset(&m_storage, 0, ssoUnionSize);
        _initFrom(other.data(), other.size());
    }

    /// @brief Move constructor.
    /// @param[in] other The string to move from. After moving, other is left in an empty SSO state.
    BasicString(BasicString&& other) noexcept
        : m_allocator(static_cast<AllocatorType&&>(other.m_allocator))
    {
        std::memcpy(&m_storage, &other.m_storage, ssoUnionSize);
        other._initSsoEmpty();
    }

    /// @brief Move constructor with explicit allocator.
    /// @param[in] other The string to move from. After moving, other is left in an empty SSO state.
    /// @param[in] alloc The allocator instance to use for this string.
    BasicString(BasicString&& other, const AllocatorType& alloc) noexcept
        : m_allocator(alloc)
    {
        if (m_allocator == other.m_allocator)
        {
            std::memcpy(&m_storage, &other.m_storage, ssoUnionSize);
            other._initSsoEmpty();
        }
        else
        {
            std::memset(&m_storage, 0, ssoUnionSize);
            _initFrom(other.data(), other.size());
        }
    }

    /// @brief Constructs from a substring of another string.
    /// @param[in] other The source string.
    /// @param[in] pos Starting position.
    /// @param[in] count Number of characters (clamped to remaining).
    /// @param[in] alloc The allocator instance.
    BasicString(
        const BasicString& other, SizeType pos, SizeType count = npos, const AllocatorType& alloc = AllocatorType{}
    )
        : m_allocator(alloc)
    {
        GP_ASSERT(pos <= other.size());
        std::memset(&m_storage, 0, ssoUnionSize);
        const SizeType rcount = gp::math::min(count, other.size() - pos);
        _initFrom(other.data() + pos, rcount);
    }

    /// @brief Destructor. Frees any allocated heap memory.
    ~BasicString() { _heapFree(); }

    /// @brief Copy assignment operator.
    /// @param[in] other The string to copy from.
    /// @return Reference to this string.
    BasicString& operator=(const BasicString& other)
    {
        if (this == &other) { return *this; }

        constexpr bool propagate = AllocTraits::PropagateOnContainerCopyAssignment::value;
        if constexpr (propagate)
        {
            if (m_allocator != other.m_allocator)
            {
                _heapFree();
                _replaceAllocator(other.m_allocator);
            }
            else { _replaceAllocator(other.m_allocator); }
        }

        _assignChars(other.data(), other.size());
        return *this;
    }

    /// @brief Move assignment operator.
    /// @param[in] other The string to move from.
    /// @return Reference to this string.
    BasicString& operator=(BasicString&& other) noexcept
    {
        if (this == &other) { return *this; }

        constexpr bool propagate = AllocTraits::PropagateOnContainerMoveAssignment::value;

        if constexpr (propagate)
        {
            _heapFree();
            _replaceAllocator(static_cast<AllocatorType&&>(other.m_allocator));
            std::memcpy(&m_storage, &other.m_storage, ssoUnionSize);
            other._initSsoEmpty();
        }
        else
        {
            if (m_allocator == other.m_allocator)
            {
                _heapFree();
                std::memcpy(&m_storage, &other.m_storage, ssoUnionSize);
                other._initSsoEmpty();
            }
            else { _assignChars(other.data(), other.size()); }
        }
        return *this;
    }

    /// @brief Assigns from a null-terminated C string.
    /// @param[in] str The C string to assign.
    /// @return Reference to this string.
    BasicString& operator=(const CharT* str)
    {
        _assignChars(str, Traits::length(str));
        return *this;
    }

    /// @brief Assigns a single character.
    /// @param[in] ch The character to assign.
    /// @return Reference to this string.
    BasicString& operator=(CharT ch)
    {
        _assignChars(&ch, 1);
        return *this;
    }

    /// @brief Assigns from a string_view.
    /// @param[in] sv The string_view to assign.
    /// @return Reference to this string.
    BasicString& operator=(BasicStringView<CharT, Traits> sv)
    {
        _assignChars(sv.data(), sv.size());
        return *this;
    }

public:
    /// @brief Unchecked character access.
    /// @param[in] pos The index of the character to access.
    /// @return Reference to the character at the specified index.
    GP_NODISCARD GP_FORCEINLINE Reference operator[](SizeType pos) noexcept { return data()[pos]; }

    /// @brief Unchecked const character access.
    /// @param[in] pos The index of the character to access.
    /// @return Const reference to the character at the specified index.
    GP_NODISCARD GP_FORCEINLINE ConstReference operator[](SizeType pos) const noexcept { return data()[pos]; }

    /// @brief String concatenation (append).
    /// @param[in] other The string to append.
    /// @return Reference to this string after appending.
    BasicString& operator+=(const BasicString& other) { return append(other); }

    /// @brief String concatenation with a C string.
    /// @param[in] str The C string to append.
    /// @return Reference to this string after appending.
    BasicString& operator+=(const CharT* str) { return append(str); }

    /// @brief String concatenation with a single character.
    /// @param[in] ch The character to append.
    /// @return Reference to this string after appending.
    BasicString& operator+=(CharT ch)
    {
        pushBack(ch);
        return *this;
    }

    /// @brief String concatenation with a string_view.
    /// @param[in] sv The string_view to append.
    /// @return Reference to this string after appending.
    BasicString& operator+=(BasicStringView<CharT, Traits> sv) { return append(sv); }

    /// @brief Equality comparison.
    /// @param[in] other The string to compare against.
    /// @return True if the strings are equal, false otherwise.
    GP_NODISCARD bool operator==(const BasicString& other) const noexcept
    {
        return size() == other.size() && Traits::compare(data(), other.data(), size()) == 0;
    }

    /// @brief Equality comparison with a C string.
    /// @param[in] str The C string to compare against.
    /// @return True if this string is equal to the C string, false otherwise.
    GP_NODISCARD bool operator==(const CharT* str) const noexcept
    {
        const SizeType len = Traits::length(str);
        return size() == len && Traits::compare(data(), str, len) == 0;
    }

    /// @brief Inequality comparison.
    /// @param[in] other The string to compare against.
    /// @return True if the strings are not equal, false otherwise.
    GP_NODISCARD bool operator!=(const BasicString& other) const noexcept { return !(*this == other); }

    /// @brief Inequality comparison with a C string.
    /// @param[in] str The C string to compare against.
    /// @return True if this string is not equal to the C string, false otherwise.
    GP_NODISCARD bool operator!=(const CharT* str) const noexcept { return !(*this == str); }

    /// @brief Three-way comparison.
    /// @param[in] other The string to compare against.
    /// @return A value less than, equal to, or greater than zero if this string is found, respectively, to be less
    ///         than, to match, or be greater than other.
    GP_NODISCARD auto operator<=>(const BasicString& other) const noexcept
    {
        return _view() <=> BasicStringView<CharT, Traits>(other.data(), other.size());
    }

    /// @brief Three-way comparison with a C string.
    /// @param[in] str The C string to compare against.
    /// @return A value less than, equal to, or greater than zero if this string is found, respectively, to be less
    ///         than, to match, or be greater than the C string.
    GP_NODISCARD auto operator<=>(const CharT* str) const noexcept
    {
        return _view() <=> BasicStringView<CharT, Traits>(str);
    }

    /// @brief Implicit conversion to string_view.
    /// @return A BasicStringView representing the contents of this string.
    GP_NODISCARD operator BasicStringView<CharT, Traits>() const noexcept { return _view(); }

public:
    /// @brief Replaces contents with count copies of character ch.
    /// @param[in] count Number of characters.
    /// @param[in] ch The character to assign.
    /// @return Reference to this string after assignment.
    BasicString& assign(SizeType count, CharT ch)
    {
        _heapFree();
        std::memset(&m_storage, 0, ssoUnionSize);
        if (count <= ssoMaxChars)
        {
            for (SizeType i = 0; i < count; ++i) { _ssoData()[i] = ch; }
            _ssoData()[count] = CharT{};
            _setSsoSize(count);
        }
        else
        {
            Pointer buffer = _heapAlloc(count);
            for (SizeType i = 0; i < count; ++i) { buffer[i] = ch; }
            buffer[count] = CharT{};
            m_storage.heap.data = buffer;
            m_storage.heap.size = count;
            _setHeapCapacity(count);
        }
        return *this;
    }

    /// @brief Replaces contents with a substring of str.
    /// @param[in] str The source string.
    /// @param[in] pos Starting position in str.
    /// @param[in] count Number of characters to assign (clamped to remaining).
    /// @return Reference to this string after assignment.
    BasicString& assign(const BasicString& str, SizeType pos = 0, SizeType count = npos)
    {
        GP_ASSERT(pos <= str.size());
        const SizeType rcount = gp::math::min(count, str.size() - pos);
        _assignChars(str.data() + pos, rcount);
        return *this;
    }

    /// @brief Replaces contents from a buffer and length.
    /// @param[in] str Pointer to the character buffer.
    /// @param[in] count Number of characters in the buffer.
    /// @return Reference to this string after assignment.
    BasicString& assign(const CharT* str, SizeType count)
    {
        _assignChars(str, count);
        return *this;
    }

    /// @brief Replaces contents from a null-terminated C string.
    /// @param[in] str Pointer to a null-terminated character array.
    /// @return Reference to this string after assignment.
    BasicString& assign(const CharT* str) { return *this = str; }

    /// @brief Gets a reference to the character at the specified position with bounds checking.
    /// @param[in] pos The index of the character to access.
    /// @return Reference to the character at the specified index.
    GP_NODISCARD Reference at(SizeType pos)
    {
        GP_ASSERT(pos < size());
        return data()[pos];
    }

    /// @brief Gets a const reference to the character at the specified position with bounds checking.
    /// @param[in] pos The index of the character to access.
    /// @return Const reference to the character at the specified index.
    GP_NODISCARD ConstReference at(SizeType pos) const
    {
        GP_ASSERT(pos < size());
        return data()[pos];
    }

    /// @brief Returns a reference to the first character.
    /// @return Reference to the first character in the string.
    GP_NODISCARD GP_FORCEINLINE Reference front() noexcept { return data()[0]; }

    /// @brief Returns a const reference to the first character.
    /// @return Const reference to the first character in the string.
    GP_NODISCARD GP_FORCEINLINE ConstReference front() const noexcept { return data()[0]; }

    /// @brief Returns a reference to the last character.
    /// @return Reference to the last character in the string.
    GP_NODISCARD GP_FORCEINLINE Reference back() noexcept { return data()[size() - 1]; }

    /// @brief Returns a const reference to the last character.
    /// @return Const reference to the last character in the string.
    GP_NODISCARD GP_FORCEINLINE ConstReference back() const noexcept { return data()[size() - 1]; }

    /// @brief Returns a pointer to the underlying character array (always null-terminated).
    /// @return Pointer to the character data.
    GP_NODISCARD GP_FORCEINLINE Pointer data() noexcept { return _isHeap() ? m_storage.heap.data : _ssoData(); }

    /// @brief Returns a const pointer to the underlying character array (always null-terminated).
    /// @return Const pointer to the character data.
    GP_NODISCARD GP_FORCEINLINE ConstPointer data() const noexcept
    {
        return _isHeap() ? m_storage.heap.data : _ssoData();
    }

    /// @brief Returns a null-terminated C string.
    /// @return Pointer to the character data.
    GP_NODISCARD GP_FORCEINLINE ConstPointer cStr() const noexcept { return data(); }

    /// @brief Gets an iterator to the beginning of the string.
    /// @return Iterator pointing to the first character.
    GP_NODISCARD GP_FORCEINLINE Iterator begin() noexcept { return data(); }

    /// @brief Gets a const iterator to the beginning of the string.
    /// @return Const iterator pointing to the first character.
    GP_NODISCARD GP_FORCEINLINE ConstIterator begin() const noexcept { return data(); }

    /// @brief Gets an iterator to the end of the string.
    /// @return Iterator pointing to one past the last character.
    GP_NODISCARD GP_FORCEINLINE Iterator end() noexcept { return data() + size(); }

    /// @brief Gets a const iterator to the end of the string.
    /// @return Const iterator pointing to one past the last character.
    GP_NODISCARD GP_FORCEINLINE ConstIterator end() const noexcept { return data() + size(); }

    /// @brief Gets a const iterator to the beginning of the string.
    /// @return Const iterator pointing to the first character.
    GP_NODISCARD GP_FORCEINLINE ConstIterator cbegin() const noexcept { return data(); }

    /// @brief Gets a const iterator to the end of the string.
    /// @return Const iterator pointing to one past the last character.
    GP_NODISCARD GP_FORCEINLINE ConstIterator cend() const noexcept { return data() + size(); }

    /// @brief Gets a reverse iterator to the end of the string.
    /// @return Reverse iterator pointing to the last character.
    GP_NODISCARD GP_FORCEINLINE ReverseIterator rbegin() noexcept { return ReverseIterator(end()); }

    /// @brief Gets a const reverse iterator to the end of the string.
    /// @return Const reverse iterator pointing to the last character.
    GP_NODISCARD GP_FORCEINLINE ConstReverseIterator rbegin() const noexcept { return ConstReverseIterator(end()); }

    /// @brief Gets a reverse iterator to the beginning of the string.
    /// @return Reverse iterator pointing to one before the first character.
    GP_NODISCARD GP_FORCEINLINE ReverseIterator rend() noexcept { return ReverseIterator(begin()); }

    /// @brief Gets a const reverse iterator to the beginning of the string.
    /// @return Const reverse iterator pointing to one before the first character.
    GP_NODISCARD GP_FORCEINLINE ConstReverseIterator rend() const noexcept { return ConstReverseIterator(begin()); }

    /// @brief Gets a const reverse iterator to the end of the string.
    /// @return Const reverse iterator pointing to the last character.
    GP_NODISCARD GP_FORCEINLINE ConstReverseIterator crbegin() const noexcept { return ConstReverseIterator(cend()); }

    /// @brief Gets a const reverse iterator to the beginning of the string.
    /// @return Const reverse iterator pointing to one before the first character.
    GP_NODISCARD GP_FORCEINLINE ConstReverseIterator crend() const noexcept { return ConstReverseIterator(cbegin()); }

    /// @brief Returns the number of characters (not counting the null terminator).
    /// @return The size of the string.
    GP_NODISCARD GP_FORCEINLINE SizeType size() const noexcept { return _isHeap() ? m_storage.heap.size : _ssoSize(); }

    /// @brief Alias for size().
    /// @return The size of the string.
    GP_NODISCARD GP_FORCEINLINE SizeType length() const noexcept { return size(); }

    /// @brief Checks if the string is empty.
    /// @return True if the string is empty, false otherwise.
    GP_NODISCARD GP_FORCEINLINE bool empty() const noexcept { return size() == 0; }

    /// @brief Returns the current allocated capacity (not counting the null terminator).
    /// @return The capacity of the string.
    GP_NODISCARD GP_FORCEINLINE SizeType capacity() const noexcept { return _isHeap() ? _heapCapacity() : ssoMaxChars; }

    /// @brief Returns the maximum possible string length.
    /// @return The maximum size of the string.
    GP_NODISCARD constexpr SizeType maxSize() const noexcept { return AllocTraits::maxSize(m_allocator) - 1; }

    /// @brief Reserves storage for at least newCap characters (not counting the null terminator).
    /// @param[in] newCap The minimum capacity to reserve.
    void reserve(SizeType newCap)
    {
        if (newCap > capacity()) { _ensureHeapCapacity(newCap); }
    }

    /// @brief Releases unused heap memory, or transitions back to SSO if possible.
    void shrinkToFit()
    {
        if (!_isHeap()) { return; }
        const SizeType len = m_storage.heap.size;
        if (len <= ssoMaxChars)
        {
            Pointer oldBuffer = m_storage.heap.data;
            const SizeType oldCapacity = _heapCapacity();
            CharT tmp[ssoUnionSize / sizeof(CharT)];
            Traits::copy(tmp, oldBuffer, len);
            tmp[len] = CharT{};
            AllocTraits::deallocate(m_allocator, oldBuffer, oldCapacity + 1);
            std::memset(&m_storage, 0, ssoUnionSize);
            Traits::copy(_ssoData(), tmp, len);
            _ssoData()[len] = CharT{};
            _setSsoSize(len);
        }
        else if (_heapCapacity() > len)
        {
            Pointer newBuffer = _heapAlloc(len);
            Traits::copy(newBuffer, m_storage.heap.data, len);
            newBuffer[len] = CharT{};
            AllocTraits::deallocate(m_allocator, m_storage.heap.data, _heapCapacity() + 1);
            m_storage.heap.data = newBuffer;
            m_storage.heap.size = len;
            _setHeapCapacity(len);
        }
    }

    /// @brief Erases all characters, setting size to 0.
    void clear() noexcept
    {
        if (_isHeap())
        {
            m_storage.heap.size = 0;
            m_storage.heap.data[0] = CharT{};
        }
        else
        {
            _ssoData()[0] = CharT{};
            _setSsoSize(0);
        }
    }

    /// @brief Appends a character.
    /// @param[in] ch The character to append.
    void pushBack(CharT ch)
    {
        const SizeType len = size();
        const SizeType cap = capacity();
        if (len == cap) { _ensureHeapCapacity(_growCap(cap, len + 1)); }
        Pointer d = data();
        d[len] = ch;
        d[len + 1] = CharT{};
        _setSize(len + 1);
    }

    /// @brief Removes the last character.
    void popBack() noexcept
    {
        GP_ASSERT(size() > 0);
        const SizeType newLen = size() - 1;
        data()[newLen] = CharT{};
        _setSize(newLen);
    }

    /// @brief Appends another string.
    /// @param[in] other The string to append.
    /// @return Reference to this string after appending.
    BasicString& append(const BasicString& other) { return append(other.data(), other.size()); }

    /// @brief Appends a null-terminated C string.
    /// @param[in] str The C string to append.
    /// @return Reference to this string after appending.
    BasicString& append(const CharT* str) { return append(str, Traits::length(str)); }

    /// @brief Appends a buffer of characters.
    /// @param[in] str Pointer to the character buffer to append.
    /// @param[in] count Number of characters to append.
    /// @return Reference to this string after appending.
    BasicString& append(const CharT* str, SizeType count)
    {
        if (count == 0) { return *this; }
        const SizeType oldLen = size();
        const SizeType newLen = oldLen + count;
        if (newLen > capacity()) { _ensureHeapCapacity(_growCap(capacity(), newLen)); }
        Traits::copy(data() + oldLen, str, count);
        data()[newLen] = CharT{};
        _setSize(newLen);
        return *this;
    }

    /// @brief Appends count copies of character ch.
    /// @param[in] count Number of characters to append.
    /// @param[in] ch The character to append.
    /// @return Reference to this string after appending.
    BasicString& append(SizeType count, CharT ch)
    {
        if (count == 0) { return *this; }
        const SizeType oldLen = size();
        const SizeType newLen = oldLen + count;
        if (newLen > capacity()) { _ensureHeapCapacity(_growCap(capacity(), newLen)); }
        Pointer d = data() + oldLen;
        for (SizeType i = 0; i < count; ++i) { d[i] = ch; }
        data()[newLen] = CharT{};
        _setSize(newLen);
        return *this;
    }

    /// @brief Appends from a string_view.
    /// @param[in] sv The string_view to append.
    /// @return Reference to this string after appending.
    BasicString& append(BasicStringView<CharT, Traits> sv) { return append(sv.data(), sv.size()); }

    /// @brief Inserts count copies of ch at position pos.
    /// @param[in] pos The index at which to insert.
    /// @param[in] count Number of characters to insert.
    /// @param[in] ch The character to insert.
    /// @return Reference to this string after insertion.
    BasicString& insert(SizeType pos, SizeType count, CharT ch)
    {
        GP_ASSERT(pos <= size());
        if (count == 0) { return *this; }
        const SizeType oldLen = size();
        const SizeType newLen = oldLen + count;
        if (newLen > capacity()) { _ensureHeapCapacity(_growCap(capacity(), newLen)); }
        Pointer d = data();
        Traits::move(d + pos + count, d + pos, oldLen - pos);
        for (SizeType i = 0; i < count; ++i) { d[pos + i] = ch; }
        d[newLen] = CharT{};
        _setSize(newLen);
        return *this;
    }

    /// @brief Inserts a C string at position pos.
    /// @param[in] pos The index at which to insert.
    /// @param[in] str The C string to insert.
    /// @return Reference to this string after insertion.
    BasicString& insert(SizeType pos, const CharT* str) { return insert(pos, str, Traits::length(str)); }

    /// @brief Inserts a buffer at position pos.
    /// @param[in] pos The index at which to insert.
    /// @param[in] str Pointer to the character buffer to insert.
    /// @param[in] count Number of characters to insert.
    /// @return Reference to this string after insertion.
    BasicString& insert(SizeType pos, const CharT* str, SizeType count)
    {
        GP_ASSERT(pos <= size());
        if (count == 0) { return *this; }
        const SizeType oldLen = size();
        const SizeType newLen = oldLen + count;
        if (newLen > capacity()) { _ensureHeapCapacity(_growCap(capacity(), newLen)); }
        Pointer d = data();
        Traits::move(d + pos + count, d + pos, oldLen - pos);
        Traits::copy(d + pos, str, count);
        d[newLen] = CharT{};
        _setSize(newLen);
        return *this;
    }

    /// @brief Inserts another string at position pos.
    /// @param[in] pos The index at which to insert.
    /// @param[in] str The string to insert.
    /// @return Reference to this string after insertion.
    BasicString& insert(SizeType pos, const BasicString& str) { return insert(pos, str.data(), str.size()); }

    /// @brief Erases count characters starting at position pos.
    /// @param[in] pos The index of the first character to erase.
    /// @param[in] count Number of characters to erase (clamped to remaining).
    /// @return Reference to this string after erasing.
    BasicString& erase(SizeType pos = 0, SizeType count = npos)
    {
        GP_ASSERT(pos <= size());
        const SizeType rcount = gp::math::min(count, size() - pos);
        const SizeType oldLen = size();
        const SizeType newLen = oldLen - rcount;
        Pointer d = data();
        Traits::move(d + pos, d + pos + rcount, oldLen - pos - rcount);
        d[newLen] = CharT{};
        _setSize(newLen);
        return *this;
    }

    /// @brief Erases the character at the given iterator position.
    /// @param[in] pos Iterator pointing to the character to erase.
    /// @return Iterator pointing to the character following the erased character.
    Iterator erase(ConstIterator pos)
    {
        const SizeType idx = static_cast<SizeType>(pos - data());
        erase(idx, 1);
        return data() + idx;
    }

    /// @brief Erases the range [first, last).
    /// @param[in] first Iterator pointing to the first character to erase.
    /// @param[in] last Iterator pointing to one past the last character to erase.
    /// @return Iterator pointing to the character following the last erased character.
    Iterator erase(ConstIterator first, ConstIterator last)
    {
        const SizeType idx = static_cast<SizeType>(first - data());
        const SizeType count = static_cast<SizeType>(last - first);
        erase(idx, count);
        return data() + idx;
    }

    /// @brief Replaces count characters starting at pos with str.
    /// @param[in] pos The index of the first character to replace.
    /// @param[in] count Number of characters to replace (clamped to remaining).
    /// @param[in] str Pointer to the character buffer to replace with.
    /// @param[in] strCount Number of characters in the replacement buffer.
    /// @return Reference to this string after replacement.
    BasicString& replace(SizeType pos, SizeType count, const CharT* str, SizeType strCount)
    {
        GP_ASSERT(pos <= size());
        const SizeType rcount = gp::math::min(count, size() - pos);
        const SizeType oldLen = size();
        const SizeType newLen = oldLen - rcount + strCount;
        if (newLen > capacity()) { _ensureHeapCapacity(_growCap(capacity(), newLen)); }
        Pointer d = data();
        Traits::move(d + pos + strCount, d + pos + rcount, oldLen - pos - rcount);
        Traits::copy(d + pos, str, strCount);
        d[newLen] = CharT{};
        _setSize(newLen);
        return *this;
    }

    /// @brief Replaces count characters starting at pos with another string.
    /// @param[in] pos The index of the first character to replace.
    /// @param[in] count Number of characters to replace (clamped to remaining).
    /// @param[in] str The string to replace with.
    /// @return Reference to this string after replacement.
    BasicString& replace(SizeType pos, SizeType count, const BasicString& str)
    {
        return replace(pos, count, str.data(), str.size());
    }

    /// @brief Replaces count characters starting at pos with a C string.
    /// @param[in] pos The index of the first character to replace.
    /// @param[in] count Number of characters to replace (clamped to remaining).
    /// @param[in] str Pointer to a null-terminated C string to replace with.
    /// @return Reference to this string after replacement.
    BasicString& replace(SizeType pos, SizeType count, const CharT* str)
    {
        return replace(pos, count, str, Traits::length(str));
    }

    /// @brief Resizes the string to count characters. New characters are set to ch.
    /// @param[in] count The new size of the string.
    /// @param[in] ch The character to use for new characters if the string is grown
    /// @return Reference to this string after resizing.
    void resize(SizeType count, CharT ch = CharT{})
    {
        const SizeType oldLen = size();
        if (count <= oldLen)
        {
            data()[count] = CharT{};
            _setSize(count);
        }
        else { append(count - oldLen, ch); }
    }

    /// @brief Swaps contents with another string.
    /// @param[in] other The string to swap with.
    void swap(BasicString& other) noexcept
    {
        constexpr bool propagate = AllocTraits::PropagateOnContainerSwap::value;
        if constexpr (!propagate) { GP_ASSERT(m_allocator == other.m_allocator); }

        StorageUnion tmp;
        std::memcpy(&tmp, &m_storage, ssoUnionSize);
        std::memcpy(&m_storage, &other.m_storage, ssoUnionSize);
        std::memcpy(&other.m_storage, &tmp, ssoUnionSize);

        if constexpr (propagate)
        {
            AllocatorType tmp_alloc(m_allocator);
            _replaceAllocator(other.m_allocator);
            other._replaceAllocator(tmp_alloc);
        }
    }

    /// @brief Copies a substring into dest.
    /// @param[out] dest Pointer to the destination buffer.
    /// @param[in] count Number of characters to copy.
    /// @param[in] pos Starting position in the string (clamped to size).
    /// @return The number of characters actually copied.
    SizeType copy(CharT* dest, SizeType count, SizeType pos = 0) const
    {
        GP_ASSERT(pos <= size());
        const SizeType rcount = gp::math::min(count, size() - pos);
        Traits::copy(dest, data() + pos, rcount);
        return rcount;
    }

    /// @brief Returns a substring.
    /// @param[in] pos Starting position in the string (clamped to size).
    /// @param[in] count Number of characters in the substring (clamped to remaining).
    /// @return A new BasicString containing the specified substring.
    GP_NODISCARD BasicString substr(SizeType pos = 0, SizeType count = npos) const
    {
        GP_ASSERT(pos <= size());
        return BasicString(data() + pos, gp::math::min(count, size() - pos), m_allocator);
    }

    /// @brief Compares with another string.
    /// @param[in] other The string to compare against.
    /// @return A value less than, equal to, or greater than zero if this string is found, respectively, to be less
    ///         than, to match, or be greater than other.
    GP_NODISCARD int compare(const BasicString& other) const noexcept { return _view().compare(_viewOf(other)); }

    /// @brief Compares with a C string.
    /// @param[in] str The C string to compare against.
    /// @return A value less than, equal to, or greater than zero if this string is found, respectively, to be less
    ///         than, to match, or be greater than the C string.
    GP_NODISCARD int compare(const CharT* str) const { return _view().compare(BasicStringView<CharT, Traits>(str)); }

    /// @brief Compares a substring with another string.
    /// @param[in] pos Starting position in this string (clamped to size).
    /// @param[in] count Number of characters in the substring (clamped to remaining).
    /// @param[in] other The string to compare against.
    /// @return A value less than, equal to, or greater than zero if the specified substring of this string is found,
    ///         respectively, to be less than, to match, or be greater than other.
    GP_NODISCARD int compare(SizeType pos, SizeType count, const BasicString& other) const
    {
        return _view().compare(pos, count, _viewOf(other));
    }

    /// @brief Compares a substring with a C string.
    /// @param[in] pos Starting position in this string (clamped to size).
    /// @param[in] count Number of characters in the substring (clamped to remaining).
    /// @param[in] str The C string to compare against.
    /// @return A value less than, equal to, or greater than zero if the specified substring of this string is found,
    ///         respectively, to be less than, to match, or be greater than the C string.
    GP_NODISCARD int compare(SizeType pos, SizeType count, const CharT* str) const
    {
        return _view().compare(pos, count, BasicStringView<CharT, Traits>(str));
    }

    /// @brief Finds the first occurrence of a substring.
    /// @param[in] str The string to find.
    /// @param[in] pos Starting position for the search (clamped to size).
    /// @return The index of the first occurrence of str, or npos if not found.
    GP_NODISCARD SizeType find(const BasicString& str, SizeType pos = 0) const noexcept
    {
        return _view().find(_viewOf(str), pos);
    }

    /// @brief Finds the first occurrence of a C string.
    /// @param[in] str The C string to find.
    /// @param[in] pos Starting position for the search (clamped to size).
    /// @return The index of the first occurrence of str, or npos if not found.
    GP_NODISCARD SizeType find(const CharT* str, SizeType pos = 0) const { return _view().find(str, pos); }

    /// @brief Finds the first occurrence of a character.
    /// @param[in] ch The character to find.
    /// @param[in] pos Starting position for the search (clamped to size).
    /// @return The index of the first occurrence of ch, or npos if not found.
    GP_NODISCARD SizeType find(CharT ch, SizeType pos = 0) const noexcept { return _view().find(ch, pos); }

    /// @brief Finds the last occurrence of a substring.
    /// @param[in] str The string to find.
    /// @param[in] pos Starting position for the search (clamped to size).
    /// @return The index of the last occurrence of str, or npos if not found.
    GP_NODISCARD SizeType rfind(const BasicString& str, SizeType pos = npos) const noexcept
    {
        return _view().rfind(_viewOf(str), pos);
    }

    /// @brief Finds the last occurrence of a C string.
    /// @param[in] str The C string to find.
    /// @param[in] pos Starting position for the search (clamped to size).
    /// @return The index of the last occurrence of str, or npos if not found.
    GP_NODISCARD SizeType rfind(const CharT* str, SizeType pos = npos) const { return _view().rfind(str, pos); }

    /// @brief Finds the last occurrence of a character.
    /// @param[in] ch The character to find.
    /// @param[in] pos Starting position for the search (clamped to size).
    /// @return The index of the last occurrence of ch, or npos if not found.
    GP_NODISCARD SizeType rfind(CharT ch, SizeType pos = npos) const noexcept { return _view().rfind(ch, pos); }

    /// @brief Finds the first occurrence of any character from a set.
    /// @param[in] str The string containing the set of characters to find.
    /// @param[in] pos Starting position for the search (clamped to size).
    /// @return The index of the first occurrence of any character from str, or npos if not found.
    GP_NODISCARD SizeType findFirstOf(const BasicString& str, SizeType pos = 0) const noexcept
    {
        return _view().findFirstOf(_viewOf(str), pos);
    }

    /// @brief Finds the first occurrence of any character from a set.
    /// @param[in] str The C string containing the set of characters to find.
    /// @param[in] pos Starting position for the search (clamped to size).
    /// @return The index of the first occurrence of any character from str, or npos if not found.
    GP_NODISCARD SizeType findFirstOf(const CharT* str, SizeType pos = 0) const
    {
        return _view().findFirstOf(str, pos);
    }

    /// @brief Finds the first occurrence of any character from a set.
    /// @param[in] ch The character to find.
    /// @param[in] pos Starting position for the search (clamped to size).
    /// @return The index of the first occurrence of ch, or npos if not found.
    GP_NODISCARD SizeType findFirstOf(CharT ch, SizeType pos = 0) const noexcept
    {
        return _view().findFirstOf(ch, pos);
    }

    /// @brief Finds the last occurrence of any character from a set.
    /// @param[in] str The string containing the set of characters to find.
    /// @param[in] pos Starting position for the search (clamped to size).
    /// @return The index of the last occurrence of any character from str, or npos if not found.
    GP_NODISCARD SizeType findLastOf(const BasicString& str, SizeType pos = npos) const noexcept
    {
        return _view().findLastOf(_viewOf(str), pos);
    }

    /// @brief Finds the last occurrence of any character from a set.
    /// @param[in] str The C string containing the set of characters to find.
    /// @param[in] pos Starting position for the search (clamped to size).
    /// @return The index of the last occurrence of any character from str, or npos if not found.
    GP_NODISCARD SizeType findLastOf(const CharT* str, SizeType pos = npos) const
    {
        return _view().findLastOf(str, pos);
    }

    /// @brief Finds the last occurrence of any character from a set.
    /// @param[in] ch The character to find.
    /// @param[in] pos Starting position for the search (clamped to size).
    /// @return The index of the last occurrence of ch, or npos if not found.
    GP_NODISCARD SizeType findLastOf(CharT ch, SizeType pos = npos) const noexcept
    {
        return _view().findLastOf(ch, pos);
    }

    /// @brief Finds the first occurrence of any character not in a set.
    /// @param[in] str The string containing the set of characters to not find.
    /// @param[in] pos Starting position for the search (clamped to size).
    /// @return The index of the first occurrence of any character not in str, or npos if not found.
    GP_NODISCARD SizeType findFirstNotOf(const BasicString& str, SizeType pos = 0) const noexcept
    {
        return _view().findFirstNotOf(_viewOf(str), pos);
    }

    /// @brief Finds the first occurrence of any character not in a set.
    /// @param[in] str The C string containing the set of characters to not find.
    /// @param[in] pos Starting position for the search (clamped to size).
    /// @return The index of the first occurrence of any character not in str, or npos if not found.
    GP_NODISCARD SizeType findFirstNotOf(const CharT* str, SizeType pos = 0) const
    {
        return _view().findFirstNotOf(str, pos);
    }

    /// @brief Finds the first occurrence of any character not in a set.
    /// @param[in] ch The character to not find.
    /// @param[in] pos Starting position for the search (clamped to size).
    /// @return The index of the first occurrence of any character not equal to ch, or npos if not found.
    GP_NODISCARD SizeType findFirstNotOf(CharT ch, SizeType pos = 0) const noexcept
    {
        return _view().findFirstNotOf(ch, pos);
    }

    /// @brief Finds the last occurrence of any character not in a set.
    /// @param[in] str The string containing the set of characters to not find.
    /// @param[in] pos Starting position for the search (clamped to size).
    /// @return The index of the last occurrence of any character not in str, or npos if not found.
    GP_NODISCARD SizeType findLastNotOf(const BasicString& str, SizeType pos = npos) const noexcept
    {
        return _view().findLastNotOf(_viewOf(str), pos);
    }

    /// @brief Finds the last occurrence of any character not in a set.
    /// @param[in] str The C string containing the set of characters to not find.
    /// @param[in] pos Starting position for the search (clamped to size).
    /// @return The index of the last occurrence of any character not in str, or npos if not found.
    GP_NODISCARD SizeType findLastNotOf(const CharT* str, SizeType pos = npos) const
    {
        return _view().findLastNotOf(str, pos);
    }

    /// @brief Finds the last occurrence of any character not in a set.
    /// @param[in] ch The character to not find.
    /// @param[in] pos Starting position for the search (clamped to size).
    /// @return The index of the last occurrence of any character not equal to ch, or npos if not found.
    GP_NODISCARD SizeType findLastNotOf(CharT ch, SizeType pos = npos) const noexcept
    {
        return _view().findLastNotOf(ch, pos);
    }

    /// @brief Checks if the string starts with a substring.
    /// @param[in] sv The substring to check.
    /// @return True if this string starts with sv, false otherwise.
    GP_NODISCARD bool startsWith(BasicStringView<CharT, Traits> sv) const noexcept { return _view().startsWith(sv); }

    /// @brief Checks if the string starts with a character.
    /// @param[in] ch The character to check.
    /// @return True if this string starts with ch, false otherwise.
    GP_NODISCARD bool startsWith(CharT ch) const noexcept { return _view().startsWith(ch); }

    /// @brief Checks if the string starts with a C string.
    /// @param[in] str The C string to check.
    /// @return True if this string starts with str, false otherwise.
    GP_NODISCARD bool startsWith(const CharT* str) const { return _view().startsWith(str); }

    /// @brief Checks if the string ends with a substring.
    /// @param[in] sv The substring to check.
    /// @return True if this string ends with sv, false otherwise.
    GP_NODISCARD bool endsWith(BasicStringView<CharT, Traits> sv) const noexcept { return _view().endsWith(sv); }

    /// @brief Checks if the string ends with a character.
    /// @param[in] ch The character to check.
    /// @return True if this string ends with ch, false otherwise.
    GP_NODISCARD bool endsWith(CharT ch) const noexcept { return _view().endsWith(ch); }

    /// @brief Checks if the string ends with a C string.
    /// @param[in] str The C string to check.
    /// @return True if this string ends with str, false otherwise.
    GP_NODISCARD bool endsWith(const CharT* str) const { return _view().endsWith(str); }

    /// @brief Checks if the string contains a substring.
    /// @param[in] sv The substring to check.
    /// @return True if this string contains sv, false otherwise.
    GP_NODISCARD bool contains(BasicStringView<CharT, Traits> sv) const noexcept { return _view().contains(sv); }

    /// @brief Checks if the string contains a character.
    /// @param[in] ch The character to check.
    /// @return True if this string contains ch, false otherwise.
    GP_NODISCARD bool contains(CharT ch) const noexcept { return _view().contains(ch); }

    /// @brief Checks if the string contains a C string.
    /// @param[in] str The C string to check.
    /// @return True if this string contains str, false otherwise.
    GP_NODISCARD bool contains(const CharT* str) const { return _view().contains(str); }

    /// @brief Returns a copy of the allocator.
    /// @return The allocator used by this string.
    GP_NODISCARD AllocatorType getAllocator() const noexcept { return m_allocator; }

private:
    /// @brief Sets the size in either SSO or heap mode.
    /// @param[in] len The new size of the string (not counting null terminator).
    GP_FORCEINLINE void _setSize(SizeType len) noexcept
    {
        if (_isHeap()) { m_storage.heap.size = len; }
        else { _setSsoSize(len); }
    }

    /// @brief Common assignment path: replaces current content with [str, str+len).
    /// @param[in] str Pointer to the character buffer to assign from.
    /// @param[in] len Number of characters to assign.
    void _assignChars(const CharT* str, SizeType len)
    {
        if (len <= capacity())
        {
            Traits::copy(data(), str, len);
            data()[len] = CharT{};
            _setSize(len);
        }
        else
        {
            _heapFree();
            std::memset(&m_storage, 0, ssoUnionSize);
            _initFrom(str, len);
        }
    }

    /// @brief Creates a string_view of this string.
    /// @return A BasicStringView representing the contents of this string.
    GP_NODISCARD GP_FORCEINLINE BasicStringView<CharT, Traits> _view() const noexcept
    {
        return BasicStringView<CharT, Traits>(data(), size());
    }

    /// @brief Creates a string_view from another BasicString.
    /// @param[in] s The BasicString to create a view of.
    /// @return A BasicStringView representing the contents of s.
    GP_NODISCARD static GP_FORCEINLINE BasicStringView<CharT, Traits> _viewOf(const BasicString& s) noexcept
    {
        return BasicStringView<CharT, Traits>(s.data(), s.size());
    }

    /// @brief Returns the tag byte (last byte of the union).
    GP_NODISCARD GP_FORCEINLINE gp::UInt8& _tag() noexcept
    {
        return reinterpret_cast<gp::UInt8*>(&m_storage)[ssoUnionSize - 1];
    }

    /// @brief Returns the tag byte (last byte of the union) as const.
    GP_NODISCARD GP_FORCEINLINE gp::UInt8 _tag() const noexcept
    {
        return reinterpret_cast<const gp::UInt8*>(&m_storage)[ssoUnionSize - 1];
    }

    /// @brief Returns true if the string is using heap storage.
    /// @return True if in heap mode, false if in SSO mode.
    GP_NODISCARD GP_FORCEINLINE bool _isHeap() const noexcept { return (_tag() & heapFlag) != 0; }

    /// @brief Sets the SSO tag to indicate the current length.
    /// @param[in] len The length of the string in SSO mode (not counting null terminator).
    GP_FORCEINLINE void _setSsoSize(SizeType len) noexcept { _tag() = static_cast<gp::UInt8>(ssoMaxChars - len); }

    /// @brief Returns the string length when in SSO mode.
    /// @return The length of the string in SSO mode (not counting null terminator).
    GP_NODISCARD GP_FORCEINLINE SizeType _ssoSize() const noexcept
    {
        return ssoMaxChars - static_cast<SizeType>(_tag());
    }

    /// @brief Returns a mutable pointer to the SSO buffer.
    /// @return Pointer to the SSO character buffer.
    GP_NODISCARD GP_FORCEINLINE Pointer _ssoData() noexcept { return m_storage.sso; }

    GP_NODISCARD GP_FORCEINLINE ConstPointer _ssoData() const noexcept { return m_storage.sso; }

    /// @brief Sets the heap capacity with the heap flag bit set in the MSB.
    /// @param[in] cap The capacity to set (not counting null terminator).
    GP_FORCEINLINE void _setHeapCapacity(SizeType cap) noexcept
    {
        m_storage.heap.capacity = cap | (static_cast<SizeType>(1) << (sizeof(SizeType) * 8 - 1));
    }

    /// @brief Returns the raw heap capacity (without the flag bit).
    /// @return The capacity of the heap buffer (not counting null terminator).
    GP_NODISCARD GP_FORCEINLINE SizeType _heapCapacity() const noexcept
    {
        return m_storage.heap.capacity & ~(static_cast<SizeType>(1) << (sizeof(SizeType) * 8 - 1));
    }

    /// @brief Initializes SSO storage to empty.
    GP_FORCEINLINE void _initSsoEmpty() noexcept
    {
        std::memset(&m_storage, 0, ssoUnionSize);
        _setSsoSize(0);
    }

    /// @brief Allocates heap storage for a string of the given capacity plus null terminator.
    /// @return Pointer to the heap buffer.
    GP_NODISCARD Pointer _heapAlloc(SizeType cap) { return AllocTraits::allocate(m_allocator, cap + 1); }

    /// @brief Frees the heap buffer.
    void _heapFree()
    {
        if (_isHeap() && m_storage.heap.data)
        {
            AllocTraits::deallocate(m_allocator, m_storage.heap.data, _heapCapacity() + 1);
        }
    }

    /// @brief Writes len chars from src into a fresh buffer, choosing SSO or heap.
    /// @param[in] src Pointer to the character buffer to initialize from.
    /// @param[in] len Number of characters to initialize from.
    void _initFrom(const CharT* src, SizeType len)
    {
        if (len <= ssoMaxChars)
        {
            Traits::copy(_ssoData(), src, len);
            _ssoData()[len] = CharT{};
            _setSsoSize(len);
        }
        else
        {
            Pointer buf = _heapAlloc(len);
            Traits::copy(buf, src, len);
            buf[len] = CharT{};
            m_storage.heap.data = buf;
            m_storage.heap.size = len;
            _setHeapCapacity(len);
        }
    }

    /// @brief Returns the 1.5x growth capacity.
    /// @param[in] oldCap The current capacity.
    /// @param[in] required The required capacity that must be met or exceeded.
    /// @return The new capacity, which is the larger of 1.5x oldCap or required.
    GP_NODISCARD static SizeType _growCap(SizeType oldCap, SizeType required) noexcept
    {
        SizeType newCap = oldCap + (oldCap >> 1);
        return newCap > required ? newCap : required;
    }

    /// @brief Ensures the heap buffer can hold at least newCap characters (not counting null).
    /// @param[in] newCap The required capacity to ensure.
    void _ensureHeapCapacity(SizeType newCap)
    {
        const SizeType oldLen = size();
        const ConstPointer oldData = data();
        const SizeType oldHeapCap = _isHeap() ? _heapCapacity() : 0;

        Pointer newBuffer = _heapAlloc(newCap);
        Traits::copy(newBuffer, oldData, oldLen);
        newBuffer[oldLen] = CharT{};

        if (_isHeap() && m_storage.heap.data)
        {
            AllocTraits::deallocate(m_allocator, m_storage.heap.data, oldHeapCap + 1);
        }

        m_storage.heap.data = newBuffer;
        m_storage.heap.size = oldLen;
        _setHeapCapacity(newCap);
    }

    /// @brief Replaces the allocator via destroy + placement new (works even if operator= is deleted).
    /// @param[in] newAlloc The new allocator to set.
    void _replaceAllocator(const AllocatorType& newAlloc) noexcept
    {
        m_allocator.~AllocatorType();
        ::new (static_cast<void*>(&m_allocator)) AllocatorType(newAlloc);
    }

    /// @brief Replaces the allocator via destroy + placement new from an rvalue.
    /// @param[in] newAlloc The new allocator to set.
    void _replaceAllocator(AllocatorType&& newAlloc) noexcept
    {
        m_allocator.~AllocatorType();
        ::new (static_cast<void*>(&m_allocator)) AllocatorType(static_cast<AllocatorType&&>(newAlloc));
    }
};

static_assert(sizeof(BasicString<char>) == 32, "gp::string must be exactly 32 bytes");

}   // namespace gp

/// @brief Addition operator for two BasicStrings. Concatenates lhs and rhs into a new BasicString.
/// @param[in] lhs The first BasicString to concatenate.
/// @param[in] rhs The second BasicString to concatenate.
/// @return A new BasicString containing the result of the concatenation.
template <typename CharT, typename Traits, typename Alloc>
GP_NODISCARD gp::BasicString<CharT, Traits, Alloc>
    operator+(const gp::BasicString<CharT, Traits, Alloc>& lhs, const gp::BasicString<CharT, Traits, Alloc>& rhs)
{
    gp::BasicString<CharT, Traits, Alloc> result(lhs);
    result.append(rhs);
    return result;
}

/// @brief Addition operator for BasicString and C string.
/// @param[in] lhs The BasicString to append to.
/// @param[in] rhs The C string to append.
/// @return A new BasicString containing the result of the concatenation.
template <typename CharT, typename Traits, typename Alloc>
GP_NODISCARD gp::BasicString<CharT, Traits, Alloc>
    operator+(const gp::BasicString<CharT, Traits, Alloc>& lhs, const CharT* rhs)
{
    gp::BasicString<CharT, Traits, Alloc> result(lhs);
    result.append(rhs);
    return result;
}

/// @brief Addition operator for C string and BasicString.
/// @param[in] lhs The C string to append to.
/// @param[in] rhs The BasicString to append.
/// @return A new BasicString containing the result of the concatenation.
template <typename CharT, typename Traits, typename Alloc>
GP_NODISCARD gp::BasicString<CharT, Traits, Alloc>
    operator+(const CharT* lhs, const gp::BasicString<CharT, Traits, Alloc>& rhs)
{
    gp::BasicString<CharT, Traits, Alloc> result(lhs, rhs.getAllocator());
    result.append(rhs);
    return result;
}

/// @brief Addition operator for rvalue BasicString and lvalue BasicString.
/// @param[in] lhs The BasicString to append to (moved from).
/// @param[in] rhs The BasicString to append.
/// @return A new BasicString containing the result of the concatenation.
template <typename CharT, typename Traits, typename Alloc>
GP_NODISCARD gp::BasicString<CharT, Traits, Alloc>
    operator+(gp::BasicString<CharT, Traits, Alloc>&& lhs, const gp::BasicString<CharT, Traits, Alloc>& rhs)
{
    lhs.append(rhs);
    return static_cast<gp::BasicString<CharT, Traits, Alloc>&&>(lhs);
}

/// @brief Addition operator for BasicString and C string.
/// @param[in] lhs The BasicString to append to (moved from).
/// @param[in] rhs The C string to append.
/// @return A new BasicString containing the result of the concatenation.
template <typename CharT, typename Traits, typename Alloc>
GP_NODISCARD gp::BasicString<CharT, Traits, Alloc>
    operator+(gp::BasicString<CharT, Traits, Alloc>&& lhs, const CharT* rhs)
{
    lhs.append(rhs);
    return static_cast<gp::BasicString<CharT, Traits, Alloc>&&>(lhs);
}

/// @brief Output stream operator for BasicString. Writes the string contents to the stream.
/// @param[in] os The output stream to write to.
/// @param[in] str The BasicString to write to the stream.
/// @return A reference to the output stream after writing.
template <typename CharT, typename Traits, typename Alloc>
std::ostream& operator<<(std::ostream& os, const gp::BasicString<CharT, Traits, Alloc>& str)
{
    return os << str.cStr();
}
