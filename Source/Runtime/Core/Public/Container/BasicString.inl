// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/BasicString.hpp"
#include "Crypto/Hash/FNV1a.hpp"

namespace GP::Container
{

template <Concepts::IsCharacter CharType>
void TBasicString<CharType>::SetSSOSize(SizeType size) noexcept
{
    GP_ASSERT(size <= SSO_CAPACITY, "SSO size exceeds capacity");
    auto* raw = reinterpret_cast<UInt8*>(&m_sso);
    raw[SSO_BUFFER_SIZE - 1] = static_cast<UInt8>(SSO_CAPACITY - size);
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType TBasicString<CharType>::GetSSOSize() const noexcept
{
    const auto* raw = reinterpret_cast<const UInt8*>(&m_sso);
    return SSO_CAPACITY - static_cast<SizeType>(raw[SSO_BUFFER_SIZE - 1]);
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::Pointer TBasicString<CharType>::GetSSOBuffer() noexcept
{
    return m_sso.buffer;
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ConstPointer TBasicString<CharType>::GetSSOBuffer() const noexcept
{
    return m_sso.buffer;
}

template <Concepts::IsCharacter CharType>
void TBasicString<CharType>::SetHeapData(Pointer data, SizeType size, SizeType capacity) noexcept
{
    m_heap.data = data;
    m_heap.size = size;
    // Store capacity with the heap flag. On little-endian, the MSB of capacity
    // lives in the last byte of the union, which is the discriminator byte.
    // On big-endian, we also need to explicitly set the last byte's MSB.
    m_heap.capacity = capacity | HEAP_FLAG_BIT;

#if GP_ENDIAN_BIG
    // On big-endian, HEAP_FLAG_BIT sets the MSB of capacity which is the FIRST
    // byte of the capacity field, not the last byte of the union. We need the
    // last byte of the union to also have its MSB set for the IsSSO() check.
    auto* raw = reinterpret_cast<UInt8*>(&m_sso);
    raw[SSO_BUFFER_SIZE - 1] |= 0x80;
#endif
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType TBasicString<CharType>::GetHeapCapacity() const noexcept
{
    return m_heap.capacity & ~HEAP_FLAG_BIT;
}

template <Concepts::IsCharacter CharType>
bool TBasicString<CharType>::IsSSO() const noexcept
{
    const auto* raw = reinterpret_cast<const UInt8*>(&m_sso);
    return (raw[SSO_BUFFER_SIZE - 1] & 0x80) == 0;
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType TBasicString<CharType>::CalculateGrowth(SizeType required) const noexcept
{
    SizeType current = Capacity();
    SizeType newCapacity = current + current / 2;   // 1.5x
    if (newCapacity < required) { newCapacity = required; }
    if (newCapacity < 16) { newCapacity = 16; }
    return newCapacity;
}

template <Concepts::IsCharacter CharType>
void TBasicString<CharType>::GrowTo(SizeType newCapacity)
{
    if (newCapacity <= Capacity()) { return; }

    SizeType currentSize = Size();
    ConstPointer currentData = Data();
    Pointer newBuffer = AllocateBuffer(newCapacity);

    std::char_traits<CharType>::copy(newBuffer, currentData, currentSize);
    newBuffer[currentSize] = CharType(0);

    if (!IsSSO()) { m_allocator.Deallocate(m_heap.data); }

    SetHeapData(newBuffer, currentSize, newCapacity);
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::Pointer TBasicString<CharType>::AllocateBuffer(SizeType capacity)
{
    return static_cast<Pointer>(m_allocator.Allocate((capacity + 1) * sizeof(CharType), alignof(CharType)));
}

template <Concepts::IsCharacter CharType>
void TBasicString<CharType>::FreeHeapBuffer()
{
    if (!IsSSO()) { m_allocator.Deallocate(m_heap.data); }
}

template <Concepts::IsCharacter CharType>
void TBasicString<CharType>::AssignInternal(ConstPointer str, SizeType count)
{
    if (count <= SSO_CAPACITY)
    {
        FreeHeapBuffer();
        std::char_traits<CharType>::copy(GetSSOBuffer(), str, count);
        GetSSOBuffer()[count] = CharType(0);
        SetSSOSize(count);
    }
    else
    {
        if (!IsSSO() && count <= GetHeapCapacity())
        {
            std::char_traits<CharType>::copy(m_heap.data, str, count);
            m_heap.data[count] = CharType(0);
            m_heap.size = count;
        }
        else
        {
            Pointer newBuffer = AllocateBuffer(count);
            std::char_traits<CharType>::copy(newBuffer, str, count);
            newBuffer[count] = CharType(0);
            FreeHeapBuffer();
            SetHeapData(newBuffer, count, count);
        }
    }
}

template <Concepts::IsCharacter CharType>
void TBasicString<CharType>::AppendInternal(ConstPointer str, SizeType count)
{
    if (count == 0) { return; }

    SizeType currentSize = Size();
    SizeType newSize = currentSize + count;

    if (newSize <= Capacity())
    {
        Pointer dest = Data();
        std::char_traits<CharType>::move(dest + currentSize, str, count);
        dest[newSize] = CharType(0);
        if (IsSSO()) { SetSSOSize(newSize); }
        else { m_heap.size = newSize; }
    }
    else
    {
        SizeType newCapacity = CalculateGrowth(newSize);
        Pointer newBuffer = AllocateBuffer(newCapacity);

        ConstPointer currentData = Data();
        std::char_traits<CharType>::copy(newBuffer, currentData, currentSize);

        std::char_traits<CharType>::copy(newBuffer + currentSize, str, count);
        newBuffer[newSize] = CharType(0);

        FreeHeapBuffer();
        SetHeapData(newBuffer, newSize, newCapacity);
    }
}

template <Concepts::IsCharacter CharType>
constexpr TBasicString<CharType>::TBasicString() noexcept
{
    m_sso.buffer[0] = CharType(0);
    SetSSOSize(0);
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>::TBasicString(ConstPointer str)
{
    if (str)
    {
        SizeType len = std::char_traits<CharType>::length(str);
        if (len <= SSO_CAPACITY)
        {
            std::char_traits<CharType>::copy(GetSSOBuffer(), str, len);
            GetSSOBuffer()[len] = CharType(0);
            SetSSOSize(len);
        }
        else
        {
            Pointer buf = AllocateBuffer(len);
            std::char_traits<CharType>::copy(buf, str, len);
            buf[len] = CharType(0);
            SetHeapData(buf, len, len);
        }
    }
    else
    {
        m_sso.buffer[0] = CharType(0);
        SetSSOSize(0);
    }
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>::TBasicString(ConstPointer str, SizeType count)
{
    if (count <= SSO_CAPACITY)
    {
        std::char_traits<CharType>::copy(GetSSOBuffer(), str, count);
        GetSSOBuffer()[count] = CharType(0);
        SetSSOSize(count);
    }
    else
    {
        Pointer buf = AllocateBuffer(count);
        std::char_traits<CharType>::copy(buf, str, count);
        buf[count] = CharType(0);
        SetHeapData(buf, count, count);
    }
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>::TBasicString(SizeType count, CharType ch)
{
    if (count <= SSO_CAPACITY)
    {
        std::fill_n(GetSSOBuffer(), count, ch);
        GetSSOBuffer()[count] = CharType(0);
        SetSSOSize(count);
    }
    else
    {
        Pointer buf = AllocateBuffer(count);
        std::fill_n(buf, count, ch);
        buf[count] = CharType(0);
        SetHeapData(buf, count, count);
    }
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>::TBasicString(ViewType view)
    : TBasicString(view.Data(), view.Size())
{}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>::TBasicString(std::initializer_list<CharType> initList)
    : TBasicString(initList.begin(), static_cast<SizeType>(initList.size()))
{}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>::TBasicString(const TBasicString& other)
{
    SizeType len = other.Size();
    if (len <= SSO_CAPACITY)
    {
        std::char_traits<CharType>::copy(GetSSOBuffer(), other.Data(), len);
        GetSSOBuffer()[len] = CharType(0);
        SetSSOSize(len);
    }
    else
    {
        Pointer buf = AllocateBuffer(len);
        std::char_traits<CharType>::copy(buf, other.Data(), len);
        buf[len] = CharType(0);
        SetHeapData(buf, len, len);
    }
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>::TBasicString(TBasicString&& other) noexcept
{
    if (other.IsSSO()) { std::memcpy(&m_sso, &other.m_sso, sizeof(SSOData)); }
    else
    {
        m_heap.data = other.m_heap.data;
        m_heap.size = other.m_heap.size;
        m_heap.capacity = other.m_heap.capacity;
    }
    m_allocator = std::move(other.m_allocator);

    other.m_sso.buffer[0] = CharType(0);
    other.SetSSOSize(0);
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>::~TBasicString()
{
    FreeHeapBuffer();
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::operator=(const TBasicString& other)
{
    if (this != &other) { AssignInternal(other.Data(), other.Size()); }
    return *this;
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::operator=(TBasicString&& other) noexcept
{
    if (this != &other)
    {
        FreeHeapBuffer();

        if (other.IsSSO()) { std::memcpy(&m_sso, &other.m_sso, sizeof(SSOData)); }
        else
        {
            m_heap.data = other.m_heap.data;
            m_heap.size = other.m_heap.size;
            m_heap.capacity = other.m_heap.capacity;
        }
        m_allocator = std::move(other.m_allocator);

        other.m_sso.buffer[0] = CharType(0);
        other.SetSSOSize(0);
    }
    return *this;
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::operator=(ConstPointer str)
{
    if (str) { AssignInternal(str, std::char_traits<CharType>::length(str)); }
    else { Clear(); }
    return *this;
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::operator=(CharType ch)
{
    AssignInternal(&ch, 1);
    return *this;
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::operator=(ViewType view)
{
    AssignInternal(view.Data(), view.Size());
    return *this;
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::operator=(std::initializer_list<CharType> initList)
{
    AssignInternal(initList.begin(), static_cast<SizeType>(initList.size()));
    return *this;
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::Reference TBasicString<CharType>::operator[](SizeType index) noexcept
{
    GP_ASSERT(index < Size(), "String index out of bounds");
    return Data()[index];
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ConstReference TBasicString<CharType>::operator[](SizeType index) const noexcept
{
    GP_ASSERT(index < Size(), "String index out of bounds");
    return Data()[index];
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::operator+=(const TBasicString& other)
{
    return Append(other);
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::operator+=(ConstPointer str)
{
    return Append(str);
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::operator+=(CharType ch)
{
    PushBack(ch);
    return *this;
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::operator+=(ViewType view)
{
    return Append(view);
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>::operator ViewType() const noexcept
{
    return ViewType(Data(), Size());
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::Reference TBasicString<CharType>::At(SizeType index)
{
    GP_ASSERT(index < Size(), "String::At index out of bounds");
    return Data()[index];
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ConstReference TBasicString<CharType>::At(SizeType index) const
{
    GP_ASSERT(index < Size(), "String::At index out of bounds");
    return Data()[index];
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::Reference TBasicString<CharType>::Front() noexcept
{
    GP_ASSERT(!IsEmpty(), "Front() on empty string");
    return Data()[0];
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ConstReference TBasicString<CharType>::Front() const noexcept
{
    GP_ASSERT(!IsEmpty(), "Front() on empty string");
    return Data()[0];
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::Reference TBasicString<CharType>::Back() noexcept
{
    GP_ASSERT(!IsEmpty(), "Back() on empty string");
    return Data()[Size() - 1];
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ConstReference TBasicString<CharType>::Back() const noexcept
{
    GP_ASSERT(!IsEmpty(), "Back() on empty string");
    return Data()[Size() - 1];
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::Pointer TBasicString<CharType>::Data() noexcept
{
    return IsSSO() ? GetSSOBuffer() : m_heap.data;
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ConstPointer TBasicString<CharType>::Data() const noexcept
{
    return IsSSO() ? GetSSOBuffer() : m_heap.data;
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ConstPointer TBasicString<CharType>::CStr() const noexcept
{
    return Data();
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType TBasicString<CharType>::Size() const noexcept
{
    return IsSSO() ? GetSSOSize() : m_heap.size;
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType TBasicString<CharType>::Length() const noexcept
{
    return Size();
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType TBasicString<CharType>::Capacity() const noexcept
{
    return IsSSO() ? SSO_CAPACITY : GetHeapCapacity();
}

template <Concepts::IsCharacter CharType>
bool TBasicString<CharType>::IsEmpty() const noexcept
{
    return Size() == 0;
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType TBasicString<CharType>::MaxSize() const noexcept
{
    return HEAP_FLAG_BIT - 1;
}

template <Concepts::IsCharacter CharType>
void TBasicString<CharType>::Reserve(SizeType newCapacity)
{
    if (newCapacity > Capacity()) { GrowTo(newCapacity); }
}

template <Concepts::IsCharacter CharType>
void TBasicString<CharType>::ShrinkToFit()
{
    if (IsSSO()) { return; }

    SizeType currentSize = m_heap.size;
    if (currentSize <= SSO_CAPACITY)
    {
        Pointer oldData = m_heap.data;
        std::char_traits<CharType>::copy(GetSSOBuffer(), oldData, currentSize);
        GetSSOBuffer()[currentSize] = CharType(0);
        SetSSOSize(currentSize);
        m_allocator.Deallocate(oldData);
    }
    else if (GetHeapCapacity() > currentSize)
    {
        Pointer newBuffer = AllocateBuffer(currentSize);
        std::char_traits<CharType>::copy(newBuffer, m_heap.data, currentSize);
        newBuffer[currentSize] = CharType(0);
        m_allocator.Deallocate(m_heap.data);
        SetHeapData(newBuffer, currentSize, currentSize);
    }
}

template <Concepts::IsCharacter CharType>
void TBasicString<CharType>::Clear() noexcept
{
    if (IsSSO())
    {
        m_sso.buffer[0] = CharType(0);
        SetSSOSize(0);
    }
    else
    {
        m_heap.data[0] = CharType(0);
        m_heap.size = 0;
    }
}

template <Concepts::IsCharacter CharType>
void TBasicString<CharType>::PushBack(CharType ch)
{
    AppendInternal(&ch, 1);
}

template <Concepts::IsCharacter CharType>
void TBasicString<CharType>::PopBack()
{
    GP_ASSERT(!IsEmpty(), "PopBack() on empty string");
    SizeType newSize = Size() - 1;
    Pointer data = Data();
    data[newSize] = CharType(0);
    if (IsSSO()) { SetSSOSize(newSize); }
    else { m_heap.size = newSize; }
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::Append(const TBasicString& other)
{
    AppendInternal(other.Data(), other.Size());
    return *this;
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::Append(ConstPointer str)
{
    if (str) { AppendInternal(str, std::char_traits<CharType>::length(str)); }
    return *this;
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::Append(ConstPointer str, SizeType count)
{
    AppendInternal(str, count);
    return *this;
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::Append(SizeType count, CharType ch)
{
    if (count == 0) { return *this; }

    SizeType currentSize = Size();
    SizeType newSize = currentSize + count;

    if (newSize > Capacity()) { GrowTo(CalculateGrowth(newSize)); }

    Pointer data = Data();
    std::fill_n(data + currentSize, count, ch);
    data[newSize] = CharType(0);

    if (IsSSO()) { SetSSOSize(newSize); }
    else { m_heap.size = newSize; }

    return *this;
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::Append(ViewType view)
{
    AppendInternal(view.Data(), view.Size());
    return *this;
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::Insert(SizeType index, ConstPointer str, SizeType count)
{
    SizeType currentSize = Size();
    GP_ASSERT(index <= currentSize, "Insert position out of bounds");

    if (count == 0) { return *this; }

    SizeType newSize = currentSize + count;
    if (newSize > Capacity()) { GrowTo(CalculateGrowth(newSize)); }

    Pointer data = Data();
    std::char_traits<CharType>::move(data + index + count, data + index, currentSize - index);
    std::char_traits<CharType>::copy(data + index, str, count);
    data[newSize] = CharType(0);

    if (IsSSO()) { SetSSOSize(newSize); }
    else { m_heap.size = newSize; }

    return *this;
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::Insert(SizeType index, ConstPointer str)
{
    return Insert(index, str, std::char_traits<CharType>::length(str));
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::Insert(SizeType index, SizeType count, CharType ch)
{
    SizeType currentSize = Size();
    GP_ASSERT(index <= currentSize, "Insert position out of bounds");

    if (count == 0) { return *this; }

    SizeType newSize = currentSize + count;
    if (newSize > Capacity()) { GrowTo(CalculateGrowth(newSize)); }

    Pointer data = Data();
    std::char_traits<CharType>::move(data + index + count, data + index, currentSize - index);
    std::fill_n(data + index, count, ch);
    data[newSize] = CharType(0);

    if (IsSSO()) { SetSSOSize(newSize); }
    else { m_heap.size = newSize; }

    return *this;
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::Insert(SizeType index, ViewType view)
{
    return Insert(index, view.Data(), view.Size());
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::Erase(SizeType index, SizeType count)
{
    SizeType currentSize = Size();
    GP_ASSERT(index <= currentSize, "Erase position out of bounds");

    SizeType eraseCount = std::min(count, currentSize - index);
    if (eraseCount == 0) { return *this; }

    SizeType newSize = currentSize - eraseCount;
    Pointer data = Data();

    std::char_traits<CharType>::move(data + index, data + index + eraseCount, newSize - index);
    data[newSize] = CharType(0);

    if (IsSSO()) { SetSSOSize(newSize); }
    else { m_heap.size = newSize; }

    return *this;
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::Replace(SizeType pos, SizeType count, ConstPointer str, SizeType count2)
{
    SizeType currentSize = Size();
    GP_ASSERT(pos <= currentSize, "Replace position out of bounds");

    SizeType eraseCount = std::min(count, currentSize - pos);
    SizeType newSize = currentSize - eraseCount + count2;

    if (newSize > Capacity()) { GrowTo(CalculateGrowth(newSize)); }

    Pointer data = Data();
    std::char_traits<CharType>::move(data + pos + count2, data + pos + eraseCount, currentSize - pos - eraseCount);
    std::char_traits<CharType>::copy(data + pos, str, count2);
    data[newSize] = CharType(0);

    if (IsSSO()) { SetSSOSize(newSize); }
    else { m_heap.size = newSize; }

    return *this;
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::Replace(SizeType pos, SizeType count, ConstPointer str)
{
    return Replace(pos, count, str, std::char_traits<CharType>::length(str));
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType>& TBasicString<CharType>::Replace(SizeType pos, SizeType count, ViewType view)
{
    return Replace(pos, count, view.Data(), view.Size());
}

template <Concepts::IsCharacter CharType>
void TBasicString<CharType>::Resize(SizeType newSize)
{
    Resize(newSize, CharType(0));
}

template <Concepts::IsCharacter CharType>
void TBasicString<CharType>::Resize(SizeType newSize, CharType ch)
{
    SizeType currentSize = Size();

    if (newSize > currentSize)
    {
        if (newSize > Capacity()) { GrowTo(CalculateGrowth(newSize)); }
        Pointer data = Data();
        std::fill_n(data + currentSize, newSize - currentSize, ch);
        data[newSize] = CharType(0);
    }
    else if (newSize < currentSize) { Data()[newSize] = CharType(0); }
    else { return; }

    if (IsSSO()) { SetSSOSize(newSize); }
    else { m_heap.size = newSize; }
}

template <Concepts::IsCharacter CharType>
void TBasicString<CharType>::Swap(TBasicString& other) noexcept
{
    SSOData temp;
    std::memcpy(&temp, &m_sso, sizeof(SSOData));
    std::memcpy(&m_sso, &other.m_sso, sizeof(SSOData));
    std::memcpy(&other.m_sso, &temp, sizeof(SSOData));
    std::swap(m_allocator, other.m_allocator);
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType TBasicString<CharType>::Find(ViewType sv, SizeType pos) const noexcept
{
    return ViewType(*this).Find(sv, pos);
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType TBasicString<CharType>::Find(CharType ch, SizeType pos) const noexcept
{
    return ViewType(*this).Find(ch, pos);
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType TBasicString<CharType>::Find(ConstPointer str, SizeType pos) const noexcept
{
    return ViewType(*this).Find(ViewType(str), pos);
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType TBasicString<CharType>::RFind(ViewType sv, SizeType pos) const noexcept
{
    return ViewType(*this).RFind(sv, pos);
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType TBasicString<CharType>::RFind(CharType ch, SizeType pos) const noexcept
{
    return ViewType(*this).RFind(ch, pos);
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType TBasicString<CharType>::FindFirstOf(ViewType sv, SizeType pos) const noexcept
{
    return ViewType(*this).FindFirstOf(sv, pos);
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType TBasicString<CharType>::FindFirstOf(CharType ch, SizeType pos) const noexcept
{
    return ViewType(*this).FindFirstOf(ch, pos);
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType TBasicString<CharType>::FindLastOf(ViewType sv, SizeType pos) const noexcept
{
    return ViewType(*this).FindLastOf(sv, pos);
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType TBasicString<CharType>::FindLastOf(CharType ch, SizeType pos) const noexcept
{
    return ViewType(*this).FindLastOf(ch, pos);
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType
    TBasicString<CharType>::FindFirstNotOf(ViewType sv, SizeType pos) const noexcept
{
    return ViewType(*this).FindFirstNotOf(sv, pos);
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType
    TBasicString<CharType>::FindFirstNotOf(CharType ch, SizeType pos) const noexcept
{
    return ViewType(*this).FindFirstNotOf(ch, pos);
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType
    TBasicString<CharType>::FindLastNotOf(ViewType sv, SizeType pos) const noexcept
{
    return ViewType(*this).FindLastNotOf(sv, pos);
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType
    TBasicString<CharType>::FindLastNotOf(CharType ch, SizeType pos) const noexcept
{
    return ViewType(*this).FindLastNotOf(ch, pos);
}

template <Concepts::IsCharacter CharType>
bool TBasicString<CharType>::Contains(ViewType sv) const noexcept
{
    return Find(sv) != NPOS;
}

template <Concepts::IsCharacter CharType>
bool TBasicString<CharType>::Contains(CharType ch) const noexcept
{
    return Find(ch) != NPOS;
}

template <Concepts::IsCharacter CharType>
bool TBasicString<CharType>::StartsWith(ViewType sv) const noexcept
{
    return ViewType(*this).StartsWith(sv);
}

template <Concepts::IsCharacter CharType>
bool TBasicString<CharType>::StartsWith(CharType ch) const noexcept
{
    return ViewType(*this).StartsWith(ch);
}

template <Concepts::IsCharacter CharType>
bool TBasicString<CharType>::EndsWith(ViewType sv) const noexcept
{
    return ViewType(*this).EndsWith(sv);
}

template <Concepts::IsCharacter CharType>
bool TBasicString<CharType>::EndsWith(CharType ch) const noexcept
{
    return ViewType(*this).EndsWith(ch);
}

template <Concepts::IsCharacter CharType>
TBasicString<CharType> TBasicString<CharType>::Substr(SizeType pos, SizeType count) const
{
    SizeType currentSize = Size();
    GP_ASSERT(pos <= currentSize, "Substr position out of bounds");
    SizeType subLen = std::min(count, currentSize - pos);
    return TBasicString(Data() + pos, subLen);
}

template <Concepts::IsCharacter CharType>
int TBasicString<CharType>::Compare(ViewType other) const noexcept
{
    return ViewType(*this).Compare(other);
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::SizeType TBasicString<CharType>::GetHash() const noexcept
{
    return ViewType(*this).GetHash();
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::Iterator TBasicString<CharType>::Begin() noexcept
{
    return Data();
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::Iterator TBasicString<CharType>::End() noexcept
{
    return Data() + Size();
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ConstIterator TBasicString<CharType>::Begin() const noexcept
{
    return Data();
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ConstIterator TBasicString<CharType>::End() const noexcept
{
    return Data() + Size();
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ConstIterator TBasicString<CharType>::CBegin() const noexcept
{
    return Data();
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ConstIterator TBasicString<CharType>::CEnd() const noexcept
{
    return Data() + Size();
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ReverseIterator TBasicString<CharType>::RBegin() noexcept
{
    return ReverseIterator(End());
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ReverseIterator TBasicString<CharType>::REnd() noexcept
{
    return ReverseIterator(Begin());
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ConstReverseIterator TBasicString<CharType>::CRBegin() const noexcept
{
    return ConstReverseIterator(CEnd());
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ConstReverseIterator TBasicString<CharType>::CREnd() const noexcept
{
    return ConstReverseIterator(CBegin());
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::Iterator TBasicString<CharType>::begin() noexcept
{
    return Data();
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::Iterator TBasicString<CharType>::end() noexcept
{
    return Data() + Size();
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ConstIterator TBasicString<CharType>::begin() const noexcept
{
    return Data();
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ConstIterator TBasicString<CharType>::end() const noexcept
{
    return Data() + Size();
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ConstIterator TBasicString<CharType>::cbegin() const noexcept
{
    return Data();
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ConstIterator TBasicString<CharType>::cend() const noexcept
{
    return Data() + Size();
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ReverseIterator TBasicString<CharType>::rbegin() noexcept
{
    return ReverseIterator(end());
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ReverseIterator TBasicString<CharType>::rend() noexcept
{
    return ReverseIterator(begin());
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ConstReverseIterator TBasicString<CharType>::crbegin() const noexcept
{
    return ConstReverseIterator(cend());
}

template <Concepts::IsCharacter CharType>
typename TBasicString<CharType>::ConstReverseIterator TBasicString<CharType>::crend() const noexcept
{
    return ConstReverseIterator(cbegin());
}

}   // namespace GP::Container

template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD constexpr bool operator==(
    const GP::Container::TBasicString<CharType>& lhs, const GP::Container::TBasicString<CharType>& rhs
) noexcept
{
    return lhs.Compare(GP::Container::TBasicStringView<CharType>(rhs)) == 0;
}

template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD constexpr std::strong_ordering operator<=>(
    const GP::Container::TBasicString<CharType>& lhs, const GP::Container::TBasicString<CharType>& rhs
) noexcept
{
    return lhs.Compare(GP::Container::TBasicStringView<CharType>(rhs)) <=> 0;
}

template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD constexpr bool
    operator==(const GP::Container::TBasicString<CharType>& lhs, GP::Container::TBasicStringView<CharType> rhs) noexcept
{
    return lhs.Compare(rhs) == 0;
}

template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD constexpr std::strong_ordering operator<=>(
    const GP::Container::TBasicString<CharType>& lhs, GP::Container::TBasicStringView<CharType> rhs
) noexcept
{
    return lhs.Compare(rhs) <=> 0;
}

template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD constexpr bool operator==(const GP::Container::TBasicString<CharType>& lhs, const CharType* rhs) noexcept
{
    return lhs.Compare(GP::Container::TBasicStringView<CharType>(rhs)) == 0;
}

template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD constexpr std::strong_ordering
    operator<=>(const GP::Container::TBasicString<CharType>& lhs, const CharType* rhs) noexcept
{
    return lhs.Compare(GP::Container::TBasicStringView<CharType>(rhs)) <=> 0;
}

template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD GP::Container::TBasicString<CharType>
    operator+(const GP::Container::TBasicString<CharType>& lhs, const GP::Container::TBasicString<CharType>& rhs)
{
    GP::Container::TBasicString<CharType> result;
    result.Reserve(lhs.Size() + rhs.Size());
    result.Append(lhs);
    result.Append(rhs);
    return result;
}

template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD GP::Container::TBasicString<CharType>
    operator+(const GP::Container::TBasicString<CharType>& lhs, const CharType* rhs)
{
    GP::Container::TBasicString<CharType> result(lhs);
    result.Append(rhs);
    return result;
}

template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD GP::Container::TBasicString<CharType>
    operator+(const CharType* lhs, const GP::Container::TBasicString<CharType>& rhs)
{
    GP::Container::TBasicString<CharType> result(lhs);
    result.Append(rhs);
    return result;
}

template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD GP::Container::TBasicString<CharType>
    operator+(const GP::Container::TBasicString<CharType>& lhs, CharType rhs)
{
    GP::Container::TBasicString<CharType> result(lhs);
    result.PushBack(rhs);
    return result;
}

template <GP::Concepts::IsCharacter CharType>
GP_NODISCARD GP::Container::TBasicString<CharType>
    operator+(CharType lhs, const GP::Container::TBasicString<CharType>& rhs)
{
    GP::Container::TBasicString<CharType> result(1, lhs);
    result.Append(rhs);
    return result;
}
