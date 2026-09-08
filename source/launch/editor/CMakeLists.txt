# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartExecutable(editor)
  # gpSetGuiExecutable()

  gpAddResourceFile(resources/gp-editor.rc)

  if(WIN32)
    gpAddLinkOption(PRIVATE "/MANIFEST:NO")
  endif()

  gpAddDependency(PUBLIC core)
  gpAddDependency(PUBLIC launch)
  gpAddDependency(PUBLIC renderer)
  gpAddDependency(PUBLIC engine)
  gpAddDependency(PUBLIC application)
gpEndExecutable()
