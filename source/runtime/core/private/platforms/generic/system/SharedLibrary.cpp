// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/generic/system/SharedLibrary.hpp"

namespace gp::platform::generic
{

void* SharedLibrary::getHandle([[maybe_unused]] gp::StringView filename) noexcept
{
    // TODO: Add a fatal log: SharedLibrary::getHandle is not implemented for this platform.
    return nullptr;
}

void* SharedLibrary::getExport([[maybe_unused]] void* handle, [[maybe_unused]] gp::StringView procName) noexcept
{
    // TODO: Add a fatal log: SharedLibrary::getExport is not implemented for this platform.
    return nullptr;
}

void SharedLibrary::freeHandle([[maybe_unused]] void* handle) noexcept
{
    // TODO: Add a fatal log: SharedLibrary::freeHandle is not implemented for this platform.
}

}   // namespace gp::platform::generic
