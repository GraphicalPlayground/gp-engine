# Copyright (c) - Graphical Playground. All rights reserved.

include(gp-build-tool)

gpStartExecutable(editor)
  gpSetTargetOutputName(gpeditor)
  gpAddPublicDependency(core)
  gpExecutableSetIsTerminal(TRUE)
  gpExecutableAddResource(${__GP_TARGET_LOCATION}/resources/AppIcon.rc)
gpEndExecutable()
