# Copyright (c) - Graphical Playground. All rights reserved.

include(gp-build-tool)

gpStartExecutable(editor)
  gpSetTargetOutputName(gpeditor)
  gpAddPublicDependency(gp::runtime::core)
  gpExecutableSetIsTerminal(TRUE)
  gpExecutableAddResource(${__GP_TARGET_LOCATION}/resources/AppIcon.rc)
gpEndExecutable()
