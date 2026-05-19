# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartModule(hal/sdl3)
  gpAddDependency(PUBLIC core)
  gpAddDependency(PUBLIC hal/base)
gpEndModule()
