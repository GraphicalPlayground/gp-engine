---
sidebar_position: 3
title: Testing & Benchmarks
tags:
  - c++
  - build system
  - cmake
  - tests
  - benchmarks
  - gpbt
  - architecture
---

<p style={{ color: '#ffffffa6' }}>
  First-class automated test and benchmark generation for every engine module, zero boilerplate
  required.
</p>

The Graphical Playground Build Tool treats testing and performance measurement as core parts of the
build pipeline, not optional add-ons. Rather than asking you to manually create test executables,
configure linker flags, and register CTest commands for each module, GPBT does it automatically.

Both systems are powered by [Catch2](https://github.com/catchorg/Catch2), which the engine fetches
and configures as part of its standard third-party setup. A `Catch2WithMain` target must be
available in the global CMake environment; if it is not, GPBT emits a fatal error before proceeding.

---

## Tests

### Enabling Tests for a Module

Add a single line inside your target definition to opt in:

```cmake showLineNumbers
include(gp-build-tool)

gpStartModule(core)
  gpAddPublicDependency(engine)
  gpTargetSetTestsEnabled(TRUE)
gpEndModule()
```

Tests are only generated if the global `GP_BUILD_TESTS` CMake option is `ON`. If it is `OFF`, the
`gpTargetSetTestsEnabled` call is silently ignored. You do not need to guard it yourself.

### Directory Structure

GPBT expects your test sources to live in a `tests/` subdirectory next to your `CMakeLists.txt`:

```text
source/runtime/core/
├── CMakeLists.txt
├── public/
│   └── core/
│       └── Math.hpp
├── private/
│   └── Math.cpp
└── tests/
    ├── MathTests.cpp
    └── StringTests.cpp
```

GPBT automatically globs all `*.cpp` and `*.cc` files from that directory. If the `tests/` folder
does not exist, or if it contains no source files, GPBT emits a warning and skips test generation
for that target, it does not fail the configuration.

### What GPBT Creates Automatically

When test sources are found, GPBT performs the following steps automatically:

1. **Test executable**, A new executable target is created, named
   `<export_name>_tests` (for example, `gp_core_tests` for the `core` module).
2. **Framework linking**, `Catch2WithMain` is linked privately so you get a pre-built `main`
   entry point for free. You never need to write your own `main`.
3. **Module linking**, Your module itself is linked privately into the test executable, giving
   your tests access to its public and internal APIs.
4. **C++23 standard**, The test executable is compiled with `cxx_std_23` to match the rest of
   the engine.
5. **Engine definitions**, All standard GP compile definitions (`GP_BUILD_DEBUG`, `GP_USE_VULKAN`,
   etc.) are stamped onto the test executable, keeping it consistent with the modules it tests.
6. **CTest registration**, The executable is registered with `add_test`, tagged with the labels
   `gp`, `<target_name>`, and `tests`. This makes it trivial to filter and run specific test suites
   from the command line.

### Writing Tests

Your test files use standard Catch2 syntax:

```cpp showLineNumbers
// tests/MathTests.cpp
#include <catch2/catch_test_macros.hpp>
#include <core/Math.hpp>

TEST_CASE("Vector3 addition is commutative", "[core][math][vector]")
{
    const gp::Vec3 a = { 1.0f, 2.0f, 3.0f };
    const gp::Vec3 b = { 4.0f, 5.0f, 6.0f };

    REQUIRE(a + b == b + a);
}

TEST_CASE("Division by zero returns NaN", "[core][math]")
{
    REQUIRE(std::isnan(gp::Math::safeDiv(1.0f, 0.0f)));
}
```

### Running Tests

**Via CTest (recommended for CI):**

```bash
# Run all GP tests
ctest --test-dir build/linux-release -L gp

# Run only the core module tests
ctest --test-dir build/linux-release -L core

# Run with verbose output (shows test names and pass/fail per assertion)
ctest --test-dir build/linux-release -V -L core
```

**Via the test binary directly (recommended during development):**

```bash
# Run all tests in the binary
./binaries/bin/gp_core_tests

# Run only tests tagged with [math]
./binaries/bin/gp_core_tests [math]

# List all available test cases
./binaries/bin/gp_core_tests --list-test-names-only
```

---

## Benchmarks

### Enabling Benchmarks for a Module

```cmake showLineNumbers
include(gp-build-tool)

gpStartModule(core)
  gpTargetSetBenchmarksEnabled(TRUE)
gpEndModule()
```

As with tests, benchmarks are only generated when the `GP_BUILD_BENCHMARKS` CMake option is `ON`.

### Benchmark Directory Structure

GPBT expects your benchmark sources in a `benchmarks/` subdirectory:

```text
source/runtime/core/
├── CMakeLists.txt
├── public/
├── private/
├── tests/
└── benchmarks/
    ├── MathBenchmarks.cpp
    └── StringBenchmarks.cpp
```

If the `benchmarks/` folder does not exist or is empty, GPBT emits a warning and skips benchmark
generation for that target.

### What GPBT Generates for Benchmarks

1. **Benchmark executable**, A new executable named `<export_name>_benchmarks`
   (e.g., `gp_core_benchmarks`).
2. **Framework linking**, `Catch2WithMain` is linked privately. Catch2's built-in benchmarking
   support is activated via the `CATCH_CONFIG_ENABLE_BENCHMARKING=1` compile definition.
3. **Module linking**, Your module is linked privately into the benchmark executable.
4. **C++23 standard** and **engine definitions**, Applied identically to tests.
5. **CTest registration**, Registered with `add_test` and tagged `gp`, `<target_name>`,
   and `benchmarks`.

### Writing Benchmarks

Benchmarks use Catch2's `BENCHMARK` macro, which integrates naturally alongside regular tests.

```cpp showLineNumbers
// benchmarks/MathBenchmarks.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <core/Math.hpp>

TEST_CASE("Vec3 dot product performance", "[core][math][benchmark]")
{
    const gp::Vec3 a = { 1.0f, 2.0f, 3.0f };
    const gp::Vec3 b = { 4.0f, 5.0f, 6.0f };

    BENCHMARK("dot product")
    {
        return gp::Math::dot(a, b);
    };
}

TEST_CASE("Matrix multiplication performance", "[core][math][benchmark]")
{
    const gp::Mat4 a = gp::Mat4::identity();
    const gp::Mat4 b = gp::Mat4::identity();

    BENCHMARK("4x4 matrix multiply")
    {
        return a * b;
    };

    BENCHMARK("4x4 matrix multiply inverse")
    {
        return gp::Math::inverse(a * b);
    };
}
```

:::tip For students
The `BENCHMARK` macro runs your code thousands of times and reports the median runtime, standard
deviation, and outlier counts. You do not need to write a timer loop or averaging logic yourself.
Catch2 handles all the statistical measurement.
:::

### Running Benchmarks

Benchmarks are registered in CTest but must be invoked with the `[benchmark]` tag filter to
avoid slowing down the standard test run:

```bash
# Run all benchmarks for a module via CTest
ctest --test-dir build/linux-release -L benchmarks

# Run directly with benchmark output
./binaries/bin/gp_core_benchmarks [benchmark]

# Run benchmarks and output results to a file
./binaries/bin/gp_core_benchmarks [benchmark] --reporter xml > results.xml
```

:::note
Catch2 benchmarks by default run in a warm-up phase followed by multiple timed samples. Running
benchmark binaries in a Debug build will give unreliable results, always use `Release` or
`RelWithDebInfo` for performance measurement.
:::

---

## Having Both Tests and Benchmarks

Tests and benchmarks are independent and can both be enabled on the same module:

```cmake showLineNumbers
include(gp-build-tool)

gpStartModule(renderer)
  gpAddPublicDependency(rhi)
  gpAddPublicDependency(core)

  gpTargetSetTestsEnabled(TRUE)
  gpTargetSetBenchmarksEnabled(TRUE)
gpEndModule()
```

This generates two separate executables: `gp_renderer_tests` and `gp_renderer_benchmarks`. They
are kept separate intentionally, test runs should be fast and suitable for CI, while benchmark
runs are longer and intended to be run on a controlled machine before and after performance-sensitive
changes.

---

## Full Worked Example

### Directory Layout

```text
source/runtime/core/
├── CMakeLists.txt
├── public/
│   └── core/
│       └── StringUtils.hpp
├── private/
│   └── StringUtils.cpp
├── tests/
│   └── StringUtilsTests.cpp
└── benchmarks/
    └── StringUtilsBenchmarks.cpp
```

### CMakeLists.txt

```cmake showLineNumbers
include(gp-build-tool)

gpStartModule(core)
  gpTargetSetTestsEnabled(TRUE)
  gpTargetSetBenchmarksEnabled(TRUE)
gpEndModule()
```

### StringUtilsTests.cpp

```cpp showLineNumbers
#include <catch2/catch_test_macros.hpp>
#include <core/StringUtils.hpp>

TEST_CASE("trimLeft removes leading spaces", "[core][string]")
{
    REQUIRE(gp::StringUtils::trimLeft("   hello") == "hello");
    REQUIRE(gp::StringUtils::trimLeft("hello") == "hello");
    REQUIRE(gp::StringUtils::trimLeft("") == "");
}
```

### StringUtilsBenchmarks.cpp

```cpp showLineNumbers
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <core/StringUtils.hpp>

TEST_CASE("StringUtils performance", "[core][string][benchmark]")
{
    const gp::String longString(1024, ' ');

    BENCHMARK("trimLeft on 1024-char string")
    {
        return gp::StringUtils::trimLeft(longString);
    };
}
```

### Running Both

```bash
# Tests
ctest --test-dir build/linux-release -L "core" -L "tests"
# or
./binaries/bin/gp_core_tests

# Benchmarks (always use Release)
./binaries/bin/gp_core_benchmarks [benchmark]
```
