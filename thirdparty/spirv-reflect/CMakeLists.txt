# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

if(NOT GP_BUILD_EDITOR)
  return()
endif()

gpFetchContent(
  NAME SPIRV-Reflect
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
  GIT_TAG vulkan-sdk-1.4.341.0
  REQUIRED_VERSION 1.4.341
  PACKAGE_NAME SPIRV-Reflect
  OPTIONS
    "SPIRV_REFLECT_BUILD_TESTS=OFF"
    "SPIRV_REFLECT_BUILD_EXECUTABLES=OFF"
)

if(TARGET spirv-reflect)
  # SPIRV-Reflect exposes three linkable targets; all three need warning suppression
  # because they share the same problematic headers (spirv.h trailing-comma enumerators).
  set(_SPIRV_REFLECT_TARGETS spirv-reflect spirv-reflect-static spirv-reflect-pp)

  foreach(_target IN LISTS _SPIRV_REFLECT_TARGETS)
    if(TARGET ${_target})
      if(MSVC)
        # /W0   - suppress all MSVC-level diagnostics
        # /WX-  - prevent any remaining warnings from being promoted to errors
        target_compile_options(${_target} PRIVATE /W0 /WX-)

        # clang-cl accepts MSVC flags at the driver level, but clang-specific
        # diagnostics (like -Wc++98-compat-pedantic) live in the clang frontend
        # and must be silenced through the /clang: passthrough prefix.
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
          target_compile_options(${_target} PRIVATE /clang:-Wno-everything)
        endif()
      else()
        # GCC / vanilla Clang: -w disables all warnings outright
        target_compile_options(${_target} PRIVATE -w)
      endif()

      gpSetTargetFolder(${_target} "thirdparty/graphics")
    endif()
  endforeach()
endif()
