# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

option(GP_USE_D3D11 "Enable D3D11 support" ON)

if (NOT GP_USE_D3D11 AND NOT WIN32)
  return()
endif()

message(STATUS "[GP] D3D11 enabled (Provided by Windows SDK).")
