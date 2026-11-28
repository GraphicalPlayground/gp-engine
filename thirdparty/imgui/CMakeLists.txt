# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground.com/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartThirdparty("imgui" VERSION "1.93.1")
  gpThirdpartySource(
    URL    "https://github.com/ocornut/imgui/archive/refs/tags/v1.93.1-docking.tar.gz"
    HASH   "SHA256=055a0b9023905e7ad9c0852706a7b19ca4cc8a34e56f4b25732edb9d8efda961"
  )

  gpThirdpartySetLibraryType(STATIC)

  gpThirdpartySetSources(
    imgui.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
  )

  gpThirdpartySetIncludeDirectories(".")
gpEndThirdparty()
