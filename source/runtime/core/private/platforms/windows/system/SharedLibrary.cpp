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

void* SharedLibrary::getHandle(gp::StringView filename) noexcept
{
    (void)filename;

    // TODO: Add logic to combine the search paths with the contents of the directory.

    DWORD errorMode = 0;
    if (/* dllerrors */ false)
    {
        errorMode |= SEM_NOOPENFILEERRORBOX;
        if (/* unattended */ false)
        {
            errorMode |= SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX;
        }
    }

    DWORD previousErrorMode = 0;
    const BOOL havePreviousErrorMode = ::SetThreadErrorMode(errorMode, &previousErrorMode);

    void* handle = nullptr;   // TODO: Add logic to load the library with search paths.

    if (havePreviousErrorMode)
    {
        ::SetThreadErrorMode(previousErrorMode, nullptr);
    }

    return handle;
}

void SharedLibrary::addDirectory(gp::StringView directory) noexcept
{
    // TODO: Implement directory addition logic, including normalization and caching of the dlls.

    (void)directory;
    // gp::String normalizedDirectory = gp::Path::resolve(directory);
    // gp::Path::normalizeDirectory(normalizedDirectory);
    // gp::Path::makePlatformFilename(normalizedDirectory);

    if (/* !s_searchPaths.contains(directory) */ false)
    {
        // s_searchPaths.pushBack(normalizedDirectory);

        // Enumerate the shared libraries in the directory and cache them
        {
            // TODO: Implement directory enumeration for .dll files and cache them to s_searchPathsCache normalized.
        }
    }
}

void SharedLibrary::pushDirectory(gp::StringView directory) noexcept
{
    // Set the DLL search directory to the specified directory
    ::SetDllDirectory(directory.data());

    // TODO: Add the directory to the search paths.
    // Save the directory to the stack for later restoration
    // s_searchPathsStack.pushBack(directory);
}

void SharedLibrary::popDirectory(gp::StringView directory) noexcept
{
    // TODO: Implement logic to pop the directory from the stack and restore the previous search path.
    (void)directory;
    // Check for an empty stack before popping, indicating a potential error in the code flow
    // ensure(!s_searchPathsStack.isEmpty(), "Attempted to pop from an empty DLL directory stack");

    // Verify that the directory being popped matches the top of the stack
    // check(s_searchPathsStack.top() == directory, "Mismatch in Push/Pop DLL directory operations");

    // Pop the directory from the stack
    // s_searchPathsStack.popBack();

    // Restore the previous DLL search directory or reset to the default if the stack is empty
    if (/* !s_searchPathsStack.isEmpty() */ false)
    {
        // ::SetDllDirectory(s_searchPathsStack.top().data());
    }
    else
    {
        ::SetDllDirectory("");
    }
}

}   // namespace gp::platform::windows
