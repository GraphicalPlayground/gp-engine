// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/generic/system/GenericSharedLibrary.hpp"
#include "logging/LogCategory.hpp"
#include "logging/LogMacros.hpp"

namespace gp::platform::generic
{

void* SharedLibrary::getHandle(const gp::StringView filename) noexcept
{
    GP_LOG_ERROR(
        gp::log::LogCore, "SharedLibrary::getHandle('{}') is not implemented for the generic platform.", filename
    );
    return nullptr;
}

void* SharedLibrary::getExport([[maybe_unused]] void* const handle, const gp::StringView procName) noexcept
{
    GP_LOG_ERROR(
        gp::log::LogCore, "SharedLibrary::getExport('{}') is not implemented for the generic platform.", procName
    );
    return nullptr;
}

void SharedLibrary::freeHandle([[maybe_unused]] void* const handle) noexcept
{
    GP_LOG_ERROR(gp::log::LogCore, "SharedLibrary::freeHandle() is not implemented for the generic platform.");
}

}   // namespace gp::platform::generic
