# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

# Cross-platform dependencies
add_subdirectory(catch2)
add_subdirectory(sdl3)

# Graphics Backends
# Note: Automatically be excluded from the build if the platform doesn't support it
add_subdirectory(d3d12)
add_subdirectory(d3d11)
add_subdirectory(vulkan)
add_subdirectory(metal)
add_subdirectory(opengl)

# Allocation Libraries
add_subdirectory(vma)
add_subdirectory(d3d12ma)

# Shader Toolchain
add_subdirectory(spirv-cross)
add_subdirectory(spirv-headers)
add_subdirectory(spirv-tools)
add_subdirectory(spirv-reflect)
add_subdirectory(glslang)
add_subdirectory(dxc)

# Profiling Tools
add_subdirectory(tracy)
