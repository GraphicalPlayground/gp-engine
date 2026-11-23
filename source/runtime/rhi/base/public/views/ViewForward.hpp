// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

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

/// @brief Read-only shader view of a GPU buffer or texture.
class ShaderResourceView;

/// @brief Read-write unordered access view of a GPU buffer or texture.
class UnorderedAccessView;

}   // namespace gp::rhi::view
