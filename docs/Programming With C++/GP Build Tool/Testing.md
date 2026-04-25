---
sidebar_position: 3
title: Testing
tags:
  - c++
  - build system
  - cmake
  - tests
  - gpbt
  - architecture
---

# Testing

The Graphical Playground Build Tool (GPBT) treats testing as a first-class citizen. Rather than
forcing you to manually configure test executables, link testing frameworks, and register CTest
commands for every module, GPBT automates the entire process.

Under the hood, GPBT is tightly integrated with Catch2 (specifically expecting a `Catch2WithMain`
target) and standard CTest.

## Enabling Tests

To enable testing for a specific module or executable, simply call the `gpTargetSetTestsEnabled`
macro before ending your target definition:

```cmake showLineNumbers
include(gp-build-tool)

gpStartModule(core)
  # ... dependencies and configuration ...

  # Flag this target to generate a test executable
  gpTargetSetTestsEnabled(TRUE)
gpEndModule()
```

## Directory Structure & Discovery

When `gpTargetSetTestsEnabled(TRUE)` is evaluated during the configuration phase, GPBT automatically
looks for a specific directory structure:

- **The `tests` folder**: GPBT scans `<target_location>/tests/` for any `*.cpp` or `*.cc` files.
- **Missing Sources**: If the directory does not exist, or if it is empty, GPBT will safely skip
  test generation for that target and output a warning to the console
  (`WARNING: No test sources found in '...'`).

## How It Works Under the Hood

If test sources are found, GPBT performs the following actions automatically:

1. **Executable Generation**: It creates a new executable target appended with `_tests` (e.g., if
   your target export name is `gp_core`, the test executable becomes `gp_core_tests`).
2. **Framework Linking**: It privately links the `Catch2WithMain` dependency to your test
   executable. _Note: If `Catch2WithMain` is not found in your global CMake environment, GPBT
   will throw a fatal error._
3. **Module Linking**: It links your actual target (e.g., `gp_core`) into the test executable so
   you can test its public and internal APIs.
4. **Modern C++**: It forces the C++23 standard (`cxx_std_23`) on the test executable to match the
   rest of the engine.
5. **CTest Registration**: It registers the executable with CMake's testing facility (`add_test`)
   and tags it with specific labels: `gp`, `<target_name>`, and `tests`. This makes it incredibly
   easy to filter and run specific tests from the command line.

## Example Workflow

Here is what a standard module with tests looks like in practice:

### Directory Layout:

```
source/runtime/core/
├── public/
│   └── core/Math.hpp
├── private/
│   └── Math.cpp
├── tests/
│   └── MathTests.cpp
└── CMakeLists.txt
```

### CMakeLists.txt:

```cmake showLineNumbers
include(gp-build-tool)

gpStartModule(core)
  # Automatically pulls in private/Math.cpp and public/core/Math.hpp
  
  # Tell GPBT to look in the tests/ folder and generate gp_core_tests
  gpTargetSetTestsEnabled(TRUE)
gpEndModule()
```

### MathTests.cpp:

```cpp showLineNumbers
#include <catch2/catch_test_macros.hpp>
#include <core/Math.hpp>

TEST_CASE("Math operations work correctly", "[core][math]") 
{
    REQUIRE(gp::Math::Add(2, 2) == 4);
}
```

Once built, you can run these tests through your IDE's test explorer, or via the command line using
CTest:

```bash
ctest -L core
```

or directly execute the test binary:

```bash
./binaries/bin/gp_core_tests
```
