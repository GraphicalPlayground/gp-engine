---
sidebar_position: 2
title: Include What You Use (IWYU)
description: Include What You Use (IWYU) is a tool that helps ensure you only include the headers you actually need in your C++ files.
tags:
    - c++
    - coding standard
    - iwyu
    - includes
    - architecture
---

# Include-What-You-Use (IWYU)

At Graphical Playground, our C++ codebase strictly adheres to the "Include-What-You-Use" (IWYU)
philosophy. Managing physical dependencies effectively is crucial for preventing bloated translation
units, reducing compilation times, and making code easier to refactor.

The following guidelines outline our standards for managing header inclusions and forward
declarations.

## Explicit and Direct Inclusions

You must explicitly include the headers for the types and functions you use in your code.

- **Never rely on indirect inclusions**: Do not rely on a header that is included indirectly by
  another header you include. If your .cpp or .hpp file uses a type, it must include the header
  that defines that type directly.
- **Include everything you need**: Try to include every header you need directly to make
  fine-grained inclusion easier.

:::danger Hidden Dependencies
Relying on indirect (transitive) includes creates fragile code. If a third-party or engine header
is updated and removes an `#include` internally, your code will suddenly fail to compile even if you
didn't change it. Always include what you use directly!
:::

## Fine-Grained Headers

We avoid monolithic headers that drag in massive amounts of unrelated code.

- **Be as fine-grained as possible**: When including a header, be as fine grained as possible.
- **Avoid module-level headers**: For example, do not include `Core.hpp`. Instead, you should
  include the specific headers in Core that you need definitions from.

```cpp
// Bad: Pulls in the entire Core module, drastically increasing compile times
#include "core/Core.hpp"

// Good: Only includes exactly what is needed for this translation unit
#include "core/memory/UniquePtr.hpp"
#include "core/math/Vec3.hpp"
```

## Forward Declarations

Forward declarations are a powerful tool for minimizing physical coupling between files.

- **Prefer forward declarations**: Forward declarations are preferred to including headers.
- **Namespace rules**: Forward-declared types need to be declared within their respective namespace.
  If you don't do this, you will get link errors.

If your header file only utilizes a type by pointer (`*`) or reference (`&`), you should forward
declare it rather than including its full header definition.

```cpp showLineNumbers
#pragma once

// Good: Forward declaring the type inside its namespace avoids an #include
namespace gp
{
    class RenderDevice; 
}

class MyClass
{
public:
    // Only uses a pointer, so the full definition of RenderDevice isn't needed here
    void initialize(gp::RenderDevice* device);
};
```

## Header Protection

To prevent compiler errors caused by circular dependencies or multiple inclusions during the
preprocessor phase, all header files must be protected.

- **Use pragma once**: All headers should protect against multiple includes with the `#pragma once`
  directive. All compilers we use support this directive.

```cpp
#pragma once

// <file contents>
```
