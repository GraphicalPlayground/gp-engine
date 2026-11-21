// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <type_traits>

namespace gp::concepts
{

/// @brief Concept to check if a type is trivially copyable.
/// @note Pedagogical Note: Trivially copyable types can be copied using raw memory operations like
/// memcpy. In game engines, identifying these types is key to high-performance data migration.
template <typename T>
concept IsTriviallyCopyable = std::is_trivially_copyable_v<T>;

/// @brief Concept to check if a type is trivially destructible.
/// @note If a type is trivially destructible, its destructor does nothing. This allows us to
/// skip calling destructors when clearing large buffers of objects, saving CPU cycles.
template <typename T>
concept IsTriviallyDestructible = std::is_trivially_destructible_v<T>;

/// @brief Concept to check if a type is trivially default constructible.
template <typename T>
concept IsTriviallyConstructible = std::is_trivially_default_constructible_v<T>;

/// @brief Concept to check if a type has a standard layout.
/// @note Standard layout types have a memory representation compatible with C, making them
/// safe to pass to external libraries (like Vulkan or physics engines) or to use in union-based type punning.
template <typename T>
concept IsStandardLayout = std::is_standard_layout_v<T>;

/// @brief Concept to check if a type is Plain Old Data (POD).
/// @note POD types are both trivial and have standard layout. They are essentially just data
/// and can be safely serialized to disk or sent over a network by just looking at their bytes.
template <typename T>
concept IsPod = IsStandardLayout<T> && std::is_trivial_v<T>;

/// @brief Concept indicating a type can be copied safely using bitwise operations (memcpy).
/// @note This is often used interchangeably with IsTriviallyCopyable, but provides a clearer
/// semantic intent in engine code: "this type is safe for fast-path memory copying".
template <typename T>
concept IsBitwiseCopyable = IsTriviallyCopyable<T>;

/// @brief Customization point determining whether a type can be safely relocated in memory via bitwise `memmove`.
/// @details Defaults to `IsTriviallyCopyable<T>`. Non-trivially-copyable types without internal self-pointers (e.g.,
///          move-only handles, custom buffers, or non-owning wrappers) may opt into fast-path container relocation
///          (`gp::memory::relocateConstructItems`) by specializing this trait to `std::true_type`:
///          @code
///          template <>
///          struct gp::concepts::IsRelocatableTrait<MyHandle> : std::true_type
///          {};
///          @endcode
/// @warning Only specialize to `std::true_type` when relocating raw bytes to a new memory address leaves all invariants
///          intact without invoking a move constructor followed by a destructor.
/// @tparam T Unqualified type to query (`IsRelocatable` strips `const`/`volatile` automatically).
template <typename T>
struct IsRelocatableTrait : std::bool_constant<IsTriviallyCopyable<T>>
{};

/// @brief Concept for types that can be moved in memory via `memmove` without invoking move constructors and
/// destructors.
/// @note Pedagogical Note: Relocatability ("destructive move") lets dynamic containers reallocate storage with a single
///       bulk `memmove` instead of an $O(N)$ move-construct + destroy loop, while letting custom non-trivial types
///       explicitly opt in through `IsRelocatableTrait`.
template <typename T>
concept IsRelocatable = IsRelocatableTrait<std::remove_cv_t<T>>::value;

/// @brief Concept to explicitly check for raw pointers.
/// @note This helps distinguish between raw memory addresses and smart pointer objects (like std::unique_ptr).
template <typename T>
concept IsRawPointer = std::is_pointer_v<T>;

}   // namespace gp::concepts
