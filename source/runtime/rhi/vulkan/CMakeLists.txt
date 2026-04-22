# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

if (NOT GP_USE_VULKAN)
  return()
endif()

gpStartModule(rhi/vulkan)
gpEndModule()
