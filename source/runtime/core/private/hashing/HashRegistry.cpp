// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "hashing/HashRegistry.hpp"

namespace gp::hashing
{

HashRegistry& globalHashRegistry() noexcept
{
    static HashRegistry s_registry{};
    return s_registry;
}

}   // namespace gp::hashing
