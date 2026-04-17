# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

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

  gpAddPrivateDependency(SDL3::SDL3-static)
gpEndModule()
