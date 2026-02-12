// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Memory/Allocator.hpp"
#include "Memory/MemoryForward.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Container
{

template <Concepts::IsCharacter CharType>
class TBasicStringView;

template <Concepts::IsCharacter CharType>
class TBasicString;

template <typename T, typename Alloc = Memory::TAllocator<T>>
class TArray;
template <typename T, SizeT N, typename Alloc = Memory::TAllocator<T>>
class TStaticArray;
template <typename T, typename Alloc = Memory::TAllocator<T>>
class TSet;
template <typename T, typename Alloc = Memory::TAllocator<T>>
class TMap;
template <typename T, typename Alloc = Memory::TAllocator<T>>
class TList;
template <typename T, typename Alloc = Memory::TAllocator<T>>
class TQueue;
template <typename T, typename Alloc = Memory::TAllocator<T>>
class TDeque;

}   // namespace GP::Container

using FStringView = GP::Container::TBasicStringView<GP::Char8>;
using FWStringView = GP::Container::TBasicStringView<GP::WideChar>;
using FString16View = GP::Container::TBasicStringView<GP::Char16>;
using FString32View = GP::Container::TBasicStringView<GP::Char32>;

using FString = GP::Container::TBasicString<GP::Char8>;
using FWString = GP::Container::TBasicString<GP::WideChar>;
using FString16 = GP::Container::TBasicString<GP::Char16>;
using FString32 = GP::Container::TBasicString<GP::Char32>;
