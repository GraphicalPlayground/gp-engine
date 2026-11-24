// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/apple/system/AppleSharedLibrary.hpp"

namespace gp::platform::macos
{

/// @brief macOS implementation of dynamic shared library (`.dylib` and `.framework`) loading and search-path
///        management.
/// @details Inherits Darwin `dlopen`/`dlsym`/`dlclose`, `.framework` bundle resolution, `.app` bundle directory
///          probing, and dyld shared cache support from `gp::platform::apple::SharedLibrary`.
struct GP_CORE_API SharedLibrary final : public gp::platform::apple::SharedLibrary
{
public:
    SharedLibrary() = delete;
    ~SharedLibrary() = delete;
    SharedLibrary(const SharedLibrary&) = delete;
    SharedLibrary& operator=(const SharedLibrary&) = delete;
    SharedLibrary(SharedLibrary&&) = delete;
    SharedLibrary& operator=(SharedLibrary&&) = delete;
};

}   // namespace gp::platform::macos
