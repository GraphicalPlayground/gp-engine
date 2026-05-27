# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartModule(parser/json)
  gpAddDependency(PUBLIC core)
  gpAddDependency(PRIVATE gp::thirdparty::rapidjson)
gpEndModule()
