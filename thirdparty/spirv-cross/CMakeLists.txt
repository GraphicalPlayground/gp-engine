# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

if(NOT GP_BUILD_EDITOR)
  return()
endif()

gpFetchContent(
  NAME SPIRV-Cross
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
  GIT_TAG vulkan-sdk-1.4.341.0
  REQUIRED_VERSION 1.4.341
  PACKAGE_NAME SPIRV-Cross
  OPTIONS
    "SPIRV_CROSS_CLI=OFF"
    "SPIRV_CROSS_TESTING=OFF"
    "SPIRV_CROSS_SHARED=OFF"
    "SPIRV_CROSS_STATIC=ON"
)

if(TARGET spirv-cross-core)
  gpSetTargetFolder(spirv-cross-core "thirdparty/graphics")
  gpSetTargetFolder(spirv-cross-glsl "thirdparty/graphics")
  gpSetTargetFolder(spirv-cross-hlsl "thirdparty/graphics")
  gpSetTargetFolder(spirv-cross-msl "thirdparty/graphics")
  gpSetTargetFolder(spirv-cross-cpp "thirdparty/graphics")
  gpSetTargetFolder(spirv-cross-reflect "thirdparty/graphics")
  gpSetTargetFolder(spirv-cross-c "thirdparty/graphics")
endif()
