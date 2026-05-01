# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

if(NOT GP_BUILD_EDITOR)
  return()
endif()

gpFetchContent(
  NAME SPIRV-Tools
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Tools.git
  GIT_TAG vulkan-sdk-1.4.341.0
  REQUIRED_VERSION 1.4.341
  PACKAGE_NAME SPIRV-Tools
  OPTIONS
    "SPIRV_SKIP_TESTS=ON"
    "SPIRV_SKIP_EXECUTABLES=ON"
    "SPIRV_WERROR=OFF"
)

if(TARGET SPIRV-Tools)
  gpSetTargetFolder(SPIRV-Tools "thirdparty/graphics")
  gpSetTargetFolder(SPIRV-Tools-opt "thirdparty/graphics")
  gpSetTargetFolder(SPIRV-Tools-link "thirdparty/graphics")
  gpSetTargetFolder(SPIRV-Tools-reduce "thirdparty/graphics")
  gpSetTargetFolder(SPIRV-Tools-shared "thirdparty/graphics")
endif()
