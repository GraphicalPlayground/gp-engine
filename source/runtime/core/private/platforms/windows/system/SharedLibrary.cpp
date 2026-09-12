// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/windows/system/SharedLibrary.hpp"
#include <Windows.h>

namespace gp::platform::windows
{

void SharedLibrary::freeHandle(void* handle) noexcept
{
    if (handle != nullptr)
    {
        ::FreeLibrary(static_cast<HMODULE>(handle));
    }
}

void* SharedLibrary::getExport(void* handle, gp::StringView procName) noexcept
{
    if (handle == nullptr || procName.isEmpty())
    {
        return nullptr;
    }

    const FARPROC proc = ::GetProcAddress(static_cast<HMODULE>(handle), procName.data());

    return reinterpret_cast<void*>(proc);
}

}   // namespace gp::platform::windows
