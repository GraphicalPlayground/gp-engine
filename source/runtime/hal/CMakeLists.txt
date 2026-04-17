# Copyright (c) - Graphical Playground. All rights reserved.

include(gp-build-tool)

gpStartModule(hal)
  gpAddPublicDependency(core)

  if (NOT WIN32)
    gpTargetExcludeDirectory("windows")
  endif()

  if (NOT UNIX AND NOT APPLE)
    gpTargetExcludeDirectory("linux")
  endif()

  if (NOT APPLE)
    gpTargetExcludeDirectory("macos")
  endif()
gpEndModule()
