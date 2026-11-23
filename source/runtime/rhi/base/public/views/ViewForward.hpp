// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/MemoryForward.hpp"

namespace gp::rhi::view
{

/// @brief Scope of bindless descriptor indexing enabled for shaders.
enum class BindlessConfiguration : gp::UInt8;

/// @brief Categories of hardware descriptor heaps managed by modern backends.
enum class DescriptorHeapType : gp::UInt8;

/// @brief Concrete view type stored in a descriptor handle.
enum class DescriptorType : gp::UInt8;

/// @brief Bitmask over `DescriptorType` values for descriptor range and heap filtering.
enum class DescriptorTypeMask : gp::UInt16;

/// @brief High-level category of an RHI resource view descriptor.
enum class ViewType : gp::UInt8;

/// @brief Sub-kind describing how a buffer view interprets raw buffer memory.
enum class BufferViewType : gp::UInt8;

/// @brief Texture dimensionality targeted by a texture view descriptor.
enum class TextureViewDimension : gp::UInt8;

/// @brief Compact index-and-type handle identifying a bindless descriptor heap slot.
struct DescriptorHandle;

/// @brief Packed 16-byte descriptor specifying a buffer or texture SRV/UAV.
struct ViewDesc;

/// @brief Base reference-counted view retaining a target `ViewableResource` and immutable `ViewDesc`.
class View;

/// @brief Read-only shader view of a GPU buffer or texture.
class ShaderResourceView;

/// @brief Read-write unordered access view of a GPU buffer or texture.
class UnorderedAccessView;

/// @brief Intrusive smart pointer handle to a `View`.
using ViewRef = gp::RefCountedPtr<View, gp::DefaultRefCountPolicy<View>>;

/// @brief Intrusive smart pointer handle to a `ShaderResourceView`.
using ShaderResourceViewRef = gp::RefCountedPtr<ShaderResourceView, gp::DefaultRefCountPolicy<ShaderResourceView>>;

/// @brief Intrusive smart pointer handle to an `UnorderedAccessView`.
using UnorderedAccessViewRef = gp::RefCountedPtr<UnorderedAccessView, gp::DefaultRefCountPolicy<UnorderedAccessView>>;

}   // namespace gp::rhi::view
