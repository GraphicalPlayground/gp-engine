# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartModule(core)
  gpEnableTests()

  gpAddCompileDefinition(PUBLIC "GP_USE_TRACY_PROFILER=$<IF:$<CONFIG:Shipping>,0,1>")
  gpAddDependency(PUBLIC "$<BUILD_INTERFACE:$<$<NOT:$<CONFIG:Shipping>>:gp::thirdparty::tracy>>")

  if(NOT WIN32)
    gpExcludeSourceDirectory(private/platforms/windows)
  endif()

  if(NOT APPLE)
    gpExcludeSourceDirectory(private/platforms/apple)
    gpExcludeSourceDirectory(private/platforms/macos)
  endif()

  if(NOT LINUX)
    gpExcludeSourceDirectory(private/platforms/linux)
  endif()

  if(NOT UNIX)
    gpExcludeSourceDirectory(private/platforms/unix)
  endif()
gpEndModule()
