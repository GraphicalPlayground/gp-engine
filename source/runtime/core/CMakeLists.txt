# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartModule(core)
  gpTargetSetTestsEnabled(TRUE)
  # gpTargetSetBenchmarksEnabled(FALSE)
  # gpTargetSetExamplesEnabled(FALSE)

  if (NOT WIN32)
    gpTargetExcludeDirectory("platform/windows")
  endif()

  if (NOT UNIX OR APPLE)
    gpTargetExcludeDirectory("platform/linux")
  endif()

  if (NOT APPLE)
    gpTargetExcludeDirectory("platform/macos")
  endif()
gpEndModule()
