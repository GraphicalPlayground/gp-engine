// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/MemoryOverrides.hpp"

// If we are not forcing the standard ANSI allocator,
// inject all the custom memory overloads into this compiled unit.
GP_OVERLOAD_OPERATOR_NEW_AND_DELETE
