// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "CoreMinimal.hpp"
#include "Launch.hpp"
#include "platforms/base/Platform.hpp"
#include "platforms/windows/WindowsHeaderWrapper.hpp"   // IWYU pragma: keep
#include <cstdlib>

// Included after windows.h to avoid conflicts with the min/max macros
#include <shellapi.h>

namespace gp::detail
{

inline int launchWindowsEngine()
{
    int argc = 0;
    LPWSTR* argvW = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (!argvW)
    {
        return 1;
    }

    // Calculate the total memory required for all UTF-8 strings
    gp::UInt64 totalStringBytes = 0;
    for (int i = 0; i < argc; ++i)
    {
        // WideCharToMultiByte returns required bytes including the null terminator
        totalStringBytes +=
            static_cast<gp::UInt64>(::WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, nullptr, 0, nullptr, nullptr));
    }

    // Allocate one single contiguous block for the pointers AND the string data
    gp::UInt64 pointerBlockSize = sizeof(char*) * static_cast<gp::UInt64>(argc);
    void* rawMemory = std::malloc(pointerBlockSize + totalStringBytes);
    if (!rawMemory)
    {
        ::LocalFree(argvW);
        return 1;
    }

    // Set up our pointers into the contiguous block
    char** argv = static_cast<char**>(rawMemory);
    char* stringDataCursor = static_cast<char*>(rawMemory) + pointerBlockSize;

    // Perform the conversions directly into the buffer
    for (int i = 0; i < argc; ++i)
    {
        argv[i] = stringDataCursor;
        int bytesWritten = ::WideCharToMultiByte(
            CP_UTF8, 0, argvW[i], -1, stringDataCursor, static_cast<int>(totalStringBytes), nullptr, nullptr
        );

        stringDataCursor += bytesWritten;
        totalStringBytes -= static_cast<gp::UInt64>(bytesWritten);   // Shrink remaining buffer capacity safely
    }

    ::LocalFree(argvW);

    // Launch the engine with the converted arguments
    int exitCode = gp::launch(argc, argv);

    // Clean up the single block
    std::free(rawMemory);

    return exitCode;
}

}   // namespace gp::detail

/// @brief GUI entry point for the application.
int WINAPI WinMain(
    [[maybe_unused]] HINSTANCE hInstance,
    [[maybe_unused]] HINSTANCE hPrevInstance,
    [[maybe_unused]] LPSTR lpCmdLine,
    [[maybe_unused]] int nCmdShow
)
{
    return gp::detail::launchWindowsEngine();
}

/// @brief Console entry point for the application.
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    return gp::detail::launchWindowsEngine();
}
