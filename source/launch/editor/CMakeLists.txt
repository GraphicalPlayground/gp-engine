# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartExecutable(editor)
  gpSetTargetOutputName(gpeditor)
  gpExecutableSetIsGUI(FALSE)
  gpExecutableAddResource(${__targetLocation}/resources/AppIcon.rc)

  gpAddPublicDependency(core)
  gpAddPublicDependency(hal)
gpEndExecutable()
