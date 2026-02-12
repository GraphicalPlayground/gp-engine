// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include <span>

namespace GP::Container
{

/// \brief A constant representing a dynamic extent for spans. When used as the `Extent` template parameter in `TSpan`,
/// it indicates that the span can have any size and is not fixed at compile time.
static inline constexpr SizeT DynamicExtent = static_cast<SizeT>(-1);

/// \brief A view over a contiguous sequence of objects. It does not own the underlying data and is typically used for
/// read-only access to a range of elements.
/// \tparam T The type of elements in the span.
/// \tparam Extent The number of elements in the span. If set to `static_cast<SizeT>(-1)`, the span is dynamic and can
/// have any size.
template <typename T, SizeT Extent = DynamicExtent>
class TSpan
{
private:
};

}   // namespace GP::Container
