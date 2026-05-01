# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

if (NOT GP_USE_D3D12 OR NOT WIN32)
  return()
endif()

gpFetchContent(
  NAME D3D12MemoryAllocator
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator.git
  GIT_TAG v3.1.0
  REQUIRED_VERSION 3.1.0
  PACKAGE_NAME D3D12MemoryAllocator
  OPTIONS
    "D3D12MA_BUILD_SAMPLE=OFF"
)

if(TARGET D3D12MemoryAllocator)
  gpSetTargetFolder(D3D12MemoryAllocator "thirdparty/graphics/allocators")
endif()
