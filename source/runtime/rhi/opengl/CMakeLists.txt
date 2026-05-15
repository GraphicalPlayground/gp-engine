# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartModule(rhi/opengl)
  gpAddDependency(PUBLIC core)
  gpAddDependency(PUBLIC rhi/base)
gpEndModule()
