# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartExecutable(editor)
  gpSetTargetOutputName(gpeditor)
  gpExecutableSetIsTerminal(TRUE)
  gpExecutableAddResource(${__GP_TARGET_LOCATION}/resources/AppIcon.rc)

  gpAddPublicDependency(core)
  gpAddPublicDependency(hal)
gpEndExecutable()
