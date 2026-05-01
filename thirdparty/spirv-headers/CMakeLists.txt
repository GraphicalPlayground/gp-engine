# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

if(NOT GP_BUILD_EDITOR)
  return()
endif()

gpFetchContent(
  NAME SPIRV-Headers
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
  GIT_TAG vulkan-sdk-1.4.341.0
  REQUIRED_VERSION 1.4.341
  PACKAGE_NAME SPIRV-Headers
  OPTIONS
    "SPIRV_HEADERS_ENABLE_TESTS=OFF"
    "SPIRV_HEADERS_ENABLE_INSTALL=OFF"
)

if(TARGET SPIRV-Headers)
  gpSetTargetFolder(SPIRV-Headers "thirdparty/graphics")
endif()
