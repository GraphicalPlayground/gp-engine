---
title: Enumerations
description: Enumerations utilities for the C++ runtime.
tags:
    - c++
    - utilities
    - runtime
    - enums
---

# Enumerations Utilities

Modern C++ `enum class` types provide excellent type safety and scoping, but they lack the ergonomic
of C-Style enums when it comes to bitwise manipulation and logical ordering. The Graphical
Playground Enum Utilities provide a trait-based "opt-in" mechanism to restore these features safely
using C++20/23 concepts.

## Bitwise Operators

### Overview

By default, scoped enumerations cannot be treated as bitmasks. Developers are usually forced to
pepper their code with `static_cast<underlying_type>(...)` just to combine two flags.

The `gp::BitwiseEnum` concept allows you to enable standard bitwise operators (`|`, `&`, `^`, `~`)
and their assignment variants (`|=`, `&=`, `^=`) for specific enum types. This ensures your flags
remain type-safe while behaving like the bitfields they are meant to be.

### Usage

To enable bitwise operations, use the `GP_ENABLE_ENUM_BITWISE_OPERATIONS` macro in the global
namespace.

```cpp showLineNumbers
enum class MyFlags : gp::UInt8
{
    None  = 0,
    FlagA = 1 << 0,
    FlagB = 1 << 1,
    FlagC = 1 << 2,
};

// Enable bitwise operations for MyFlags
GP_ENABLE_ENUM_BITWISE_OPERATIONS(MyFlags);

// Now you can use bitwise operators without casts
MyFlags flags = MyFlags::FlagA | MyFlags::FlagC;
```

### Semantic Helpers

In addition to operators, the `gp::enums` namespace provides high-level helpers for more
readable logic:

| Function                       | Description                                                     |
|--------------------------------|-----------------------------------------------------------------|
| `gp::enums::hasAnyFlags`       | Returns true if the value is non-zero.                          |
| `gp::enums::hasAllFlags`       | Returns true if all specified flags are set.                    |
| `gp::enums::hasNoFlags`        | Returns true if none of the specified flags are set.            |
| `gp::enums::setFlags`          | Sets the specified flags on a value.                            |
| `gp::enums::clearFlags`        | Clears the specified flags from a value.                        |
| `gp::enums::toggleFlags`       | Toggles the specified flags on a value.                         |

## Comparison Operators

### Overview

While `enum class` supports basic comparison by default, it is often useful to explicitly mark an
enum as comparable. This is particularly relevant in generic programming when using the
`gp::ComparableEnum` concept to constrain templates.

Enabling this explicitly signals that the underlying integer order of the enum is a meaningful
representation of "scale" or "priority" (e.g., `low < high`).

### Usage

Use the `GP_ENABLE_ENUM_COMPARISON_OPERATIONS` macro to register the enum.

```cpp showLineNumbers
enum class ThreadPriority : gp::UInt8
{
    Idle,
    Lowest,
    Normal,
    Highest,
    TimeCritical
};

GP_ENABLE_ENUM_COMPARISON_OPERATIONS(ThreadPriority);

// Now valid for systems constrained by gp::ComparableEnum
if (currentPriority > ThreadPriority::Normal)
{
    boostPerformance();
}
```

## Concept Constraints

The primary power of these utilities lies in their integration with C++ concepts. You can write
generic code that strictly only accepts your "enhanced" enums:

```cpp showLineNumbers
template <gp::BitwiseEnum E>
void processFlags(E flags)
{
    if (gp::enums::hasAnyFlags(flags))
    {
        // Process flags
    }
}

template <gp::ComparableEnum E>
void comparePriorities(E a, E b)
{
    if (a < b)
    {
        // a has lower priority than b
    }
}
```

:::note
These operations are implemented using constexpr and `GP_INLINE`, resulting in zero runtime overhead.
The generated assembly is identical to performing operations on raw integers.
:::
