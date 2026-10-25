# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

if(NOT WIN32)
  return()
endif()

gpStartModule(rhi/d3d12)
  gpAddDependency(PUBLIC core)
  gpAddDependency(PUBLIC rhi/base)
  gpAddDependency(PRIVATE gp::thirdparty::d3d12)
gpEndModule()
