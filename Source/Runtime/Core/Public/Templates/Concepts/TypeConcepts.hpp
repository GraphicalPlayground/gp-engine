// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Concepts/CoreConcepts.hpp"
#include "Templates/Traits/IsSameAny.hpp"
#include "Templates/Traits/TypeCategories.hpp"
#include "Templates/Traits/TypeRelationships.hpp"

namespace GP
{

// clang-format off

/// @brief Satisfied when T is a narrow character type.
///        These are the types the standard defines as character types
///        for byte-level memory access and UTF-8 text.
///
///        Note: signed char and unsigned char are explicitly included
///        because the standard grants them aliasing privileges identical
///        to char — they are the canonical byte types.
///
///        Note: bool is intentionally excluded despite being 1 byte.
template <typename T>
concept CNarrowCharacter =
    TIsSameAny_V<TRemoveCV_T<T>, char, signed char, unsigned char, char8_t>;

/// @brief Satisfied when T is a wide character type.
///        Used for UTF-16, UTF-32, and platform-native wide strings.
///
///        wchar_t is 2 bytes on Windows, 4 bytes on Linux/Mac.
///        char16_t is always 2 bytes (UTF-16 code unit).
///        char32_t is always 4 bytes (UTF-32 code point).
template <typename T>
concept CWideCharacter =
    TIsSameAny_V<TRemoveCV_T<T>, wchar_t, char16_t, char32_t>;

/// @brief Satisfied when T is any character type defined by the C++ standard.
///        Covers all narrow and wide character types.
///        Explicitly excludes bool and integer types (int8_t, uint8_t etc).
template <typename T>
concept CCharacter = CNarrowCharacter<T> || CWideCharacter<T>;

/// @brief Satisfied when T is a character type that represents a single byte.
///        The only types the standard guarantees are exactly 1 byte.
///        This is the correct concept for buffer / serialisation code
///        that requires byte-granularity access.
template <typename T>
concept CByteCharacter =
    TIsSameAny_V<TRemoveCV_T<T>, char, signed char, unsigned char, char8_t>;

// clang-format on

}   // namespace GP
