# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartModule(launch)
  gpSetStatic()

  gpAddDependency(PUBLIC core)

  if(NOT WIN32)
    gpExcludeSourceDirectory(private/windows)
  endif()

  if(NOT APPLE)
    gpExcludeSourceDirectory(private/macos)
  endif()

  if(NOT LINUX)
    gpExcludeSourceDirectory(private/linux)
  endif()
gpEndModule()
