# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

if (NOT GP_USE_VULKAN)
  return()
endif()

gpFetchContent(
  NAME VulkanMemoryAllocator
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
  GIT_TAG v3.3.0
  REQUIRED_VERSION 3.3.0
  PACKAGE_NAME VulkanMemoryAllocator
  OPTIONS
    "VMA_BUILD_SAMPLE=OFF"
    "VMA_BUILD_DOCUMENTATION=OFF"
)

if(TARGET VulkanMemoryAllocator)
  gpSetTargetFolder(VulkanMemoryAllocator "thirdparty/graphics/allocators")
endif()
