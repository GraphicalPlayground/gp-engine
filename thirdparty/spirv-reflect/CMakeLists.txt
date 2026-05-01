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
  if(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # /W0 silences all warnings, /WX- ensures warnings aren't treated as errors
    # We use 'PRIVATE' so it doesn't affect the engine targets linking to it
    target_compile_options(spirv-reflect PRIVATE /W0 /WX-)
  endif()

  gpSetTargetFolder(spirv-reflect "thirdparty/graphics")
  gpSetTargetFolder(spirv-reflect-static "thirdparty/graphics")
endif()
