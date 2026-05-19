# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartModule(hal/base)
  gpAddDependency(PUBLIC core)

  gpAddDependency(DYNAMIC hal/sdl3)
gpEndModule()
