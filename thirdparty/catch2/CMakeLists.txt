# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpFetchContent(
  NAME Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.14.0
  REQUIRED_VERSION 3.14.0
  PACKAGE_NAME Catch2
  OPTIONS
    "CATCH_BUILD_TESTING=OFF"
    "CATCH_BUILD_EXAMPLES=OFF"
    "CATCH_BUILD_EXTRA_TESTS=OFF"
    "CATCH_ENABLE_COVERAGE=OFF"
    "CATCH_ENABLE_WERROR=OFF"
    "CATCH_INSTALL_DOCS=OFF"
    "CATCH_INSTALL_EXTRAS=ON"
)

if(TARGET Catch2)
  gpSetTargetFolder(Catch2 "thirdparty/testing")
  gpSetTargetFolder(Catch2WithMain "thirdparty/testing")

  list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} PARENT_SCOPE)
endif()
