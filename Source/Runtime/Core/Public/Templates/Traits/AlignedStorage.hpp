// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include <new>

namespace GP
{

/// @brief Type trait for defining an aligned storage buffer of at least Size bytes and alignment Align. See
///        std::aligned_storage.
using AlignVal_T = std::align_val_t;

/// @brief A trivially default-constructible, trivially destructible buffer with at least Size bytes and alignment
///        Align. Equivalent to std::aligned_storage.
/// @tparam Size Minimum buffer size in bytes.
/// @tparam Align Required alignment. Defaults to the maximum fundamental alignment.
template <SizeT Size, SizeT Align = alignof(MaxAlignT)>
struct TAlignedStorage
{
    GP_ALIGN(Align) unsigned char buffer[Size];
};

/// @brief A buffer large enough and correctly aligned to hold a T in-place. Use placement new to construct and explicit
///        destructor calls to destroy.
///
/// @tparam T The type to store.
template <typename T>
struct TAlignedStorageFor
{
public:
    alignas(T) unsigned char buffer[sizeof(T)];

public:
    /// @brief Returns a typed pointer to the buffer.
    GP_NODISCARD T* GetPtr() noexcept { return reinterpret_cast<T*>(buffer); }

    /// @brief Returns a typed const pointer to the buffer.
    GP_NODISCARD const T* GetPtr() const noexcept { return reinterpret_cast<const T*>(buffer); }
};

}   // namespace GP
