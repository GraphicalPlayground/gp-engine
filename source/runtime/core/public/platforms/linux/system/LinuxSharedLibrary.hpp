// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/unix/system/UnixSharedLibrary.hpp"

namespace gp::platform::linux
{

/// @brief Linux implementation of dynamic shared library (`.so`) loading and search-path management.
/// @details Inherits POSIX `dlopen`/`dlsym`/`dlclose`, ELF versioned soname handling (`.so` and `.so.<N>`),
///          and thread-safe search-path resolution from `gp::platform::unix::SharedLibrary`.
struct GP_CORE_API SharedLibrary final : public gp::platform::unix::SharedLibrary
{
public:
    SharedLibrary() = delete;
    ~SharedLibrary() = delete;
    SharedLibrary(const SharedLibrary&) = delete;
    SharedLibrary& operator=(const SharedLibrary&) = delete;
    SharedLibrary(SharedLibrary&&) = delete;
    SharedLibrary& operator=(SharedLibrary&&) = delete;
};

}   // namespace gp::platform::linux
