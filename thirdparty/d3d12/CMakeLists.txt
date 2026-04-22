# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

option(GP_USE_D3D12 "Enable D3D12 support" ON)

if (NOT GP_USE_D3D12 OR NOT WIN32)
  return()
endif()

gpFetchContent(
  NAME DirectXHeaders
  GIT_REPOSITORY https://github.com/microsoft/DirectX-Headers.git
  GIT_TAG v1.619.1
  REQUIRED_VERSION 1.619.1
  PACKAGE_NAME DirectX-Headers
  OPTIONS
    "BUILD_TESTING=OFF"
)

if(TARGET DirectX-Headers)
  gpSetTargetFolder(DirectX-Headers "thirdparty/graphics")
  # DirectX-Headers also generates a GUIDs target
  if(TARGET DirectX-Guids)
    gpSetTargetFolder(DirectX-Guids "thirdparty/graphics")
  endif()
endif()
