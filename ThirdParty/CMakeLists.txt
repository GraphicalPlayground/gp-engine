# Copyright (c) - Graphical Playground. All rights reserved.

message(STATUS "[GP] Configuring Third-Party Dependencies")

# TODO: Add option to disable building tests and examples for third-party dependencies
# TODO: Add option to disable specific third-party dependencies dependent on the platform (e.g., DirectX on non-Windows platforms)

add_subdirectory(ISPC)
add_subdirectory(Catch2)
add_subdirectory(SDL3)
add_subdirectory(SPIRV-Cross)
add_subdirectory(SPIRV-Headers)
add_subdirectory(SPIRV-Tools)
add_subdirectory(Vulkan-Headers)
add_subdirectory(glslang)

if(WIN32)
    add_subdirectory(DirectX-Headers)
    add_subdirectory(DirectX-Agility)
elseif(APPLE)
    add_subdirectory(Metal-Headers)
endif()

message(STATUS "[GP] Third-Party Dependencies Configured")
