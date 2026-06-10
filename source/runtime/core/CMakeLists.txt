# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartModule(core)
  gpEnableTests()

  gpAddCompileDefinition(PUBLIC "GP_USE_TRACY_PROFILER=$<IF:$<CONFIG:Shipping>,0,1>")
  gpAddDependency(PUBLIC "$<BUILD_INTERFACE:$<$<NOT:$<CONFIG:Shipping>>:gp::thirdparty::tracy>>")
gpEndModule()
