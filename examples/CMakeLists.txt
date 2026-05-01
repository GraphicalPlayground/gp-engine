# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

option(GP_BUILD_EXAMPLES "Build examples" ON)

if (GP_BUILD_EXAMPLES)
  add_subdirectory(01-hello-window)
endif()
