# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

option(GP_USE_VULKAN "Enable Vulkan support" ON)

if (NOT GP_USE_VULKAN)
  return()
endif()

gpFetchContent(
  NAME VulkanHeaders
  GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
  GIT_TAG v1.4.341
  REQUIRED_VERSION 1.4.341
  PACKAGE_NAME VulkanHeaders
  OPTIONS
    "VULKAN_HEADERS_ENABLE_TESTS=OFF"
    "VULKAN_HEADERS_ENABLE_INSTALL=OFF"
)

if(TARGET Vulkan-Headers)
  gpSetTargetFolder(Vulkan-Headers "thirdparty/graphics")
endif()
