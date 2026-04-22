# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

if (NOT GP_USE_D3D12 OR NOT WIN32)
  return()
endif()

gpStartModule(rhi/d3d12)
gpEndModule()
