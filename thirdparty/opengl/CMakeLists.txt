# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

if (NOT GP_USE_OPENGL)
  return()
endif()

find_package(OpenGL REQUIRED)
if(OpenGL_FOUND)
  # You will link your rendering target against OpenGL::GL
  message(STATUS "[GP] OpenGL system library found.")
endif()
