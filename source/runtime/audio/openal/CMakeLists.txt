# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartModule(audio/openal)
  gpAddDependency(PUBLIC core)
  gpAddDependency(PUBLIC audio/base)
gpEndModule()
