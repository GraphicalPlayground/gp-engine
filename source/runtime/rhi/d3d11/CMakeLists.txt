# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

if (NOT GP_USE_D3D11 OR NOT WIN32)
  return()
endif()

gpStartModule(rhi/d3d11)
gpEndModule()
