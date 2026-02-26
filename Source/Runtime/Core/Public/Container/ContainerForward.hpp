// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Memory/Allocator.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Container
{

template <Concepts::IsCharacter CharType>
class TBasicStringView;

template <Concepts::IsCharacter CharType>
class TBasicString;

template <typename T, Concepts::IsAllocator Alloc>
class TArray;
template <typename T, SizeT N, Concepts::IsAllocator Alloc>
class TStaticArray;
template <typename T, Concepts::IsAllocator Alloc>
class TSet;
template <typename Key, typename Value, Concepts::IsAllocator Alloc>
class TMap;
template <typename T, Concepts::IsAllocator Alloc>
class TList;
template <typename T, Concepts::IsAllocator Alloc>
class TQueue;
template <typename T, Concepts::IsAllocator Alloc>
class TDeque;

template <typename T, SizeT Extent>
class TSpan;

}   // namespace GP::Container

using FStringView = GP::Container::TBasicStringView<GP::Char8>;
using FWStringView = GP::Container::TBasicStringView<GP::WideChar>;
using FString16View = GP::Container::TBasicStringView<GP::Char16>;
using FString32View = GP::Container::TBasicStringView<GP::Char32>;

using FString = GP::Container::TBasicString<GP::Char8>;
using FWString = GP::Container::TBasicString<GP::WideChar>;
using FString16 = GP::Container::TBasicString<GP::Char16>;
using FString32 = GP::Container::TBasicString<GP::Char32>;

template <typename T, GP::Concepts::IsAllocator Alloc = GP::Memory::TAllocator<T>>
using TArray = GP::Container::TArray<T, Alloc>;
template <typename T, GP::SizeT N, GP::Concepts::IsAllocator Alloc = GP::Memory::TAllocator<T>>
using TStaticArray = GP::Container::TStaticArray<T, N, Alloc>;
template <typename T, GP::Concepts::IsAllocator Alloc = GP::Memory::TAllocator<T>>
using TSet = GP::Container::TSet<T, Alloc>;
template <typename Key, typename Value, GP::Concepts::IsAllocator Alloc = GP::Memory::TAllocator<Key>>
using TMap = GP::Container::TMap<Key, Value, Alloc>;
template <typename T, GP::Concepts::IsAllocator Alloc = GP::Memory::TAllocator<T>>
using TList = GP::Container::TList<T, Alloc>;
template <typename T, GP::Concepts::IsAllocator Alloc = GP::Memory::TAllocator<T>>
using TQueue = GP::Container::TQueue<T, Alloc>;
template <typename T, GP::Concepts::IsAllocator Alloc = GP::Memory::TAllocator<T>>
using TDeque = GP::Container::TDeque<T, Alloc>;

template <typename T, GP::SizeT Extent>
using TSpan = GP::Container::TSpan<T, Extent>;
