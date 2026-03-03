// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreTypes.hpp"
#include "Templates/Concepts/TypeConcepts.hpp"

namespace GP
{

template <typename T, SizeT InlineCapacity = 8>
class TArray;
template <typename T>
class TStaticArray;
template <typename T>
class TMap;
template <typename T>
class TMultiMap;
template <typename T>
class TMultiHashMap;
template <typename T>
class TMultiSet;
template <typename T>
class TMultiHashSet;
template <typename T>
class THashMap;
template <typename T>
class THashSet;
template <typename T>
class TSet;
template <typename T>
class TAny;
template <typename T>
class TOptional;
template <typename T>
class TVariant;
template <typename T>
class TDeque;
template <typename T>
class TQueue;
template <typename T>
class TStack;
template <typename TFirst, typename TSecond>
class TPair;
template <typename T>
class TSpan;
template <typename T>
class TStridedSpan;
template <CCharacter T>
class TString;
template <CCharacter T>
class TName;
template <typename T>
class TTuple;
template <CCharacter T>
class TStringView;
template <typename T>
class TArrayView;
template <typename T>
class TMapView;

class FBitSet;
class FBitArray;

using FStringView = TStringView<Char8>;
using FWideStringView = TStringView<WideChar>;
using FNameView = TStringView<Char8>;
using FString = TString<Char8>;
using FWideString = TString<WideChar>;

}   // namespace GP
