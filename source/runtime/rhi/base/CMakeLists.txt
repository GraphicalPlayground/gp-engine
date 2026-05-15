# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartModule(rhi/base)
  gpAddDependency(PUBLIC core)

  gpAddDependency(DYNAMIC rhi/null)

  if(WIN32)
    gpAddDependency(DYNAMIC rhi/d3d11)
    gpAddDependency(DYNAMIC rhi/d3d12)
  endif()

  if(UNIX OR WIN32)
    gpAddDependency(DYNAMIC rhi/vulkan)
    gpAddDependency(DYNAMIC rhi/opengl)
  endif()

  if(APPLE)
    gpAddDependency(DYNAMIC rhi/metal)
  endif()
gpEndModule()
