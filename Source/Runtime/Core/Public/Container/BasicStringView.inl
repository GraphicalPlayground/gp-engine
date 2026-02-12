// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/BasicStringView.hpp"
#include "Crypto/Hash/FNV1a.hpp"
#include <algorithm>

namespace GP::Container
{

template <Concepts::IsCharacter CharType>
constexpr TBasicStringView<CharType>::TBasicStringView() noexcept
    : m_data(nullptr)
    , m_size(0)
{}

template <Concepts::IsCharacter CharType>
constexpr TBasicStringView<CharType>::TBasicStringView(ConstPointer str, SizeType length) noexcept
    : m_data(str)
    , m_size(length)
{}

template <Concepts::IsCharacter CharType>
constexpr TBasicStringView<CharType>::TBasicStringView(ConstPointer str) noexcept
    : m_data(str)
    , m_size(str ? std::char_traits<CharType>::length(str) : 0)
{}

template <Concepts::IsCharacter CharType>
constexpr TBasicStringView<CharType>::operator std::basic_string_view<CharType>() const noexcept
{
    return std::basic_string_view<CharType>(m_data, m_size);
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::ConstReference TBasicStringView<CharType>::operator[](SizeType index
) const noexcept
{
    GP_ASSERT(index < m_size, "Index out of bounds");
    return m_data[index];
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::ConstReference TBasicStringView<CharType>::At(SizeType index) const
{
    GP_ASSERT(index < m_size, "Index out of bounds");
    return m_data[index];
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::ConstReference TBasicStringView<CharType>::Front() const noexcept
{
    GP_ASSERT(m_size > 0, "String view is empty");
    return m_data[0];
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::ConstReference TBasicStringView<CharType>::Back() const noexcept
{
    GP_ASSERT(m_size > 0, "String view is empty");
    return m_data[m_size - 1];
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::ConstPointer TBasicStringView<CharType>::Data() const noexcept
{
    return m_data;
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType TBasicStringView<CharType>::Size() const noexcept
{
    return m_size;
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType TBasicStringView<CharType>::Length() const noexcept
{
    return m_size;
}

template <Concepts::IsCharacter CharType>
constexpr bool TBasicStringView<CharType>::IsEmpty() const noexcept
{
    return m_size == 0;
}

template <Concepts::IsCharacter CharType>
constexpr void TBasicStringView<CharType>::RemovePrefix(SizeType n) noexcept
{
    GP_ASSERT(n <= m_size, "Prefix length exceeds string view size");
    m_data += n;
    m_size -= n;
}

template <Concepts::IsCharacter CharType>
constexpr void TBasicStringView<CharType>::RemoveSuffix(SizeType n) noexcept
{
    GP_ASSERT(n <= m_size, "Suffix length exceeds string view size");
    m_size -= n;
}

template <Concepts::IsCharacter CharType>
constexpr void TBasicStringView<CharType>::Swap(TBasicStringView& other) noexcept
{
    std::swap(m_data, other.m_data);
    std::swap(m_size, other.m_size);
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::Copy(CharType* dest, SizeType count, SizeType pos) const
{
    GP_ASSERT(pos <= m_size, "Position out of bounds");
    SizeType copyCount = std::min(count, m_size - pos);
    std::copy_n(m_data + pos, copyCount, dest);
    return copyCount;
}

template <Concepts::IsCharacter CharType>
constexpr TBasicStringView<CharType> TBasicStringView<CharType>::Substr(SizeType pos, SizeType count) const
{
    GP_ASSERT(pos <= m_size, "Position out of bounds");
    SizeType subSize = std::min(count, m_size - pos);
    return TBasicStringView(m_data + pos, subSize);
}

template <Concepts::IsCharacter CharType>
constexpr int TBasicStringView<CharType>::Compare(TBasicStringView other) const noexcept
{
    SizeType minSize = std::min(m_size, other.m_size);
    int result = std::char_traits<CharType>::compare(m_data, other.m_data, minSize);
    if (result == 0)
    {
        if (m_size < other.m_size) { return -1; }
        else if (m_size > other.m_size) { return 1; }
    }
    return result;
}

template <Concepts::IsCharacter CharType>
constexpr bool TBasicStringView<CharType>::StartsWith(TBasicStringView prefix) const noexcept
{
    return m_size >= prefix.m_size && std::char_traits<CharType>::compare(m_data, prefix.m_data, prefix.m_size) == 0;
}

template <Concepts::IsCharacter CharType>
constexpr bool TBasicStringView<CharType>::StartsWith(CharType c) const noexcept
{
    return !IsEmpty() && m_data[0] == c;
}

template <Concepts::IsCharacter CharType>
constexpr bool TBasicStringView<CharType>::EndsWith(TBasicStringView suffix) const noexcept
{
    return m_size >= suffix.m_size &&
           std::char_traits<CharType>::compare(m_data + m_size - suffix.m_size, suffix.m_data, suffix.m_size) == 0;
}

template <Concepts::IsCharacter CharType>
constexpr bool TBasicStringView<CharType>::EndsWith(CharType c) const noexcept
{
    return !IsEmpty() && m_data[m_size - 1] == c;
}

template <Concepts::IsCharacter CharType>
constexpr bool TBasicStringView<CharType>::Contains(TBasicStringView sv) const noexcept
{
    return Find(sv) != NPOS;
}

template <Concepts::IsCharacter CharType>
constexpr bool TBasicStringView<CharType>::Contains(CharType c) const noexcept
{
    return Find(c) != NPOS;
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::Find(TBasicStringView sv, SizeType pos) const noexcept
{
    if (pos > m_size) { return NPOS; }
    if (sv.m_size == 0) { return pos; }
    if (sv.m_size > m_size - pos) { return NPOS; }

    for (SizeType i = pos; i <= m_size - sv.m_size; ++i)
    {
        if (std::char_traits<CharType>::compare(m_data + i, sv.m_data, sv.m_size) == 0) { return i; }
    }
    return NPOS;
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::Find(CharType c, SizeType pos) const noexcept
{
    if (pos > m_size) { return NPOS; }
    for (SizeType i = pos; i < m_size; ++i)
    {
        if (m_data[i] == c) { return i; }
    }
    return NPOS;
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::RFind(TBasicStringView sv, SizeType pos) const noexcept
{
    if (sv.m_size == 0) { return std::min(pos, m_size); }
    if (sv.m_size > m_size) { return NPOS; }

    SizeType searchEnd = std::min(pos, m_size - sv.m_size);
    for (SizeType i = searchEnd + 1; i > 0; --i)
    {
        if (std::char_traits<CharType>::compare(m_data + i - 1, sv.m_data, sv.m_size) == 0) { return i - 1; }
    }
    return NPOS;
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::RFind(CharType c, SizeType pos) const noexcept
{
    if (m_size == 0) { return NPOS; }
    SizeType searchEnd = std::min(pos, m_size - 1);
    for (SizeType i = searchEnd + 1; i > 0; --i)
    {
        if (m_data[i - 1] == c) { return i - 1; }
    }
    return NPOS;
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType TBasicStringView<CharType>::GetHash() const noexcept
{
    return static_cast<SizeType>(Crypto::FNV1a::Hash(reinterpret_cast<const char*>(m_data), m_size * sizeof(CharType)));
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::Iterator TBasicStringView<CharType>::begin() const noexcept
{
    return m_data;
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::Iterator TBasicStringView<CharType>::end() const noexcept
{
    return m_data + m_size;
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::ConstIterator TBasicStringView<CharType>::cbegin() const noexcept
{
    return m_data;
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::ConstIterator TBasicStringView<CharType>::cend() const noexcept
{
    return m_data + m_size;
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::ReverseIterator TBasicStringView<CharType>::rbegin() const noexcept
{
    return ReverseIterator(end());
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::ReverseIterator TBasicStringView<CharType>::rend() const noexcept
{
    return ReverseIterator(begin());
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::ConstReverseIterator TBasicStringView<CharType>::crbegin() const noexcept
{
    return ConstReverseIterator(cend());
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::ConstReverseIterator TBasicStringView<CharType>::crend() const noexcept
{
    return ConstReverseIterator(cbegin());
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::FindFirstOf(TBasicStringView sv, SizeType pos) const noexcept
{
    if (pos >= m_size || sv.IsEmpty()) { return NPOS; }

    for (SizeType i = pos; i < m_size; ++i)
    {
        for (SizeType j = 0; j < sv.m_size; ++j)
        {
            if (m_data[i] == sv.m_data[j]) { return i; }
        }
    }
    return NPOS;
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::FindFirstOf(CharType c, SizeType pos) const noexcept
{
    return Find(c, pos);
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::FindFirstOf(ConstPointer str, SizeType pos, SizeType count) const noexcept
{
    return FindFirstOf(TBasicStringView(str, count), pos);
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::FindFirstOf(ConstPointer str, SizeType pos) const noexcept
{
    return FindFirstOf(TBasicStringView(str), pos);
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::FindLastOf(TBasicStringView sv, SizeType pos) const noexcept
{
    if (m_size == 0 || sv.IsEmpty()) { return NPOS; }

    SizeType searchPos = std::min(pos, m_size - 1);
    for (SizeType i = searchPos + 1; i > 0; --i)
    {
        for (SizeType j = 0; j < sv.m_size; ++j)
        {
            if (m_data[i - 1] == sv.m_data[j]) { return i - 1; }
        }
    }
    return NPOS;
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::FindLastOf(CharType c, SizeType pos) const noexcept
{
    return RFind(c, pos);
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::FindLastOf(ConstPointer str, SizeType pos, SizeType count) const noexcept
{
    return FindLastOf(TBasicStringView(str, count), pos);
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::FindLastOf(ConstPointer str, SizeType pos) const noexcept
{
    return FindLastOf(TBasicStringView(str), pos);
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::FindFirstNotOf(TBasicStringView sv, SizeType pos) const noexcept
{
    if (pos >= m_size) { return NPOS; }
    if (sv.IsEmpty()) { return pos; }

    for (SizeType i = pos; i < m_size; ++i)
    {
        bool found = false;
        for (SizeType j = 0; j < sv.m_size; ++j)
        {
            if (m_data[i] == sv.m_data[j])
            {
                found = true;
                break;
            }
        }
        if (!found) { return i; }
    }
    return NPOS;
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::FindFirstNotOf(CharType c, SizeType pos) const noexcept
{
    if (pos >= m_size) { return NPOS; }

    for (SizeType i = pos; i < m_size; ++i)
    {
        if (m_data[i] != c) { return i; }
    }
    return NPOS;
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::FindFirstNotOf(ConstPointer str, SizeType pos, SizeType count) const noexcept
{
    return FindFirstNotOf(TBasicStringView(str, count), pos);
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::FindFirstNotOf(ConstPointer str, SizeType pos) const noexcept
{
    return FindFirstNotOf(TBasicStringView(str), pos);
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::FindLastNotOf(TBasicStringView sv, SizeType pos) const noexcept
{
    if (m_size == 0) { return NPOS; }
    if (sv.IsEmpty()) { return std::min(pos, m_size - 1); }

    SizeType searchPos = std::min(pos, m_size - 1);
    for (SizeType i = searchPos + 1; i > 0; --i)
    {
        bool found = false;
        for (SizeType j = 0; j < sv.m_size; ++j)
        {
            if (m_data[i - 1] == sv.m_data[j])
            {
                found = true;
                break;
            }
        }
        if (!found) { return i - 1; }
    }
    return NPOS;
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::FindLastNotOf(CharType c, SizeType pos) const noexcept
{
    if (m_size == 0) { return NPOS; }

    SizeType searchPos = std::min(pos, m_size - 1);
    for (SizeType i = searchPos + 1; i > 0; --i)
    {
        if (m_data[i - 1] != c) { return i - 1; }
    }
    return NPOS;
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::FindLastNotOf(ConstPointer str, SizeType pos, SizeType count) const noexcept
{
    return FindLastNotOf(TBasicStringView(str, count), pos);
}

template <Concepts::IsCharacter CharType>
constexpr typename TBasicStringView<CharType>::SizeType
    TBasicStringView<CharType>::FindLastNotOf(ConstPointer str, SizeType pos) const noexcept
{
    return FindLastNotOf(TBasicStringView(str), pos);
}

}   // namespace GP::Container

template <GP::Concepts::IsCharacter CharType>
constexpr bool
    operator==(GP::Container::TBasicStringView<CharType> lhs, GP::Container::TBasicStringView<CharType> rhs) noexcept
{
    return lhs.Compare(rhs) == 0;
}

template <GP::Concepts::IsCharacter CharType>
constexpr std::strong_ordering
    operator<=>(GP::Container::TBasicStringView<CharType> lhs, GP::Container::TBasicStringView<CharType> rhs) noexcept
{
    return lhs.Compare(rhs) <=> 0;
}
