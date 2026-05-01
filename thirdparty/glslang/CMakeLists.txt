# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpFetchContent(
  NAME Glslang
  GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
  GIT_TAG vulkan-sdk-1.4.341.0
  REQUIRED_VERSION 1.4.341
  PACKAGE_NAME Glslang
  OPTIONS
    "GLSLANG_TESTS=OFF"
    "ENABLE_GLSLANG_INSTALL=OFF"
    "ENABLE_HLSL=ON"
)

if(TARGET glslang)
  gpSetTargetFolder(glslang "thirdparty/graphics")
  gpSetTargetFolder(glslang-default-resource-limits "thirdparty/graphics")
  gpSetTargetFolder(MachineIndependent "thirdparty/graphics")
  gpSetTargetFolder(GenericCodeGen "thirdparty/graphics")
  gpSetTargetFolder(OSDependent "thirdparty/graphics")
  gpSetTargetFolder(SPIRV "thirdparty/graphics")
endif()
