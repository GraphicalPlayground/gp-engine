// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "CoreMinimal.hpp"
#include "Launch.hpp"
#include <windows.h>

namespace gp::detail
{

/// @brief Common entry point for both WinMain and main.
/// @param[in] argc The number of command line arguments.
/// @param[in] argv The command line arguments.
/// @return The exit code of the application.
inline int launch(int argc, char* argv[])
{
    return gp::launch(argc, argv);
}

}   // namespace gp::detail

/// @brief GUI entry point for the application.
int WINAPI WinMain(
    GP_MAYBE_UNUSED HINSTANCE hInstance,
    GP_MAYBE_UNUSED HINSTANCE hPrevInstance,
    GP_MAYBE_UNUSED LPSTR lpCmdLine,
    GP_MAYBE_UNUSED int nCmdShow
)
{
    return gp::detail::launch(__argc, __argv);
}

/// @brief Console entry point for the application.
int main(int argc, char* argv[])
{
    return gp::detail::launch(argc, argv);
}
