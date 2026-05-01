# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

if(NOT GP_ENABLE_PROFILING)
  return()
endif()

gpFetchContent(
  NAME Tracy
  GIT_REPOSITORY https://github.com/wolfpld/tracy.git
  GIT_TAG v0.13.1
  REQUIRED_VERSION 0.13.1
  PACKAGE_NAME Tracy
  OPTIONS
    "TRACY_ENABLE=ON"
    "TRACY_STATIC=ON"
)

if(TARGET TracyClient)
  gpSetTargetFolder(TracyClient "thirdparty/profiling")
endif()
