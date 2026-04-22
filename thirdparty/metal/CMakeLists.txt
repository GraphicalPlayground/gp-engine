# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

option(GP_USE_METAL "Enable Metal support" ON)

if (NOT GP_USE_METAL OR NOT APPLE)
  return()
endif()

find_library(METAL_FRAMEWORK Metal REQUIRED)
if(METAL_FRAMEWORK)
  # You will link your rendering target against ${METAL_FRAMEWORK}
  message(STATUS "[GP] Metal system framework found.")
endif()
