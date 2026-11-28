# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground.com/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

# Dear ImGui keeps its whole state behind a single global context pointer (GImGui). If the library were linked
# statically into several engine DLLs (plugin/imgui and the executable that builds UI), each binary would carry its
# own copy of that global and UI calls from the executable would hit a context the renderer never sees.
# In modular builds ImGui is therefore a SHARED library with an explicit IMGUI_API export/import contract; in
# monolithic builds everything lands in one binary and a STATIC library is the correct choice.
if(GPBT_IS_MONOLITHIC)
  set(_gpImGuiLibraryType "STATIC")
else()
  set(_gpImGuiLibraryType "SHARED")
endif()

gpStartThirdparty("imgui" VERSION "1.93.1")
  gpThirdpartySource(
    URL    "https://github.com/ocornut/imgui/archive/refs/tags/v1.93.1-docking.tar.gz"
    HASH   "SHA256=055a0b9023905e7ad9c0852706a7b19ca4cc8a34e56f4b25732edb9d8efda961"
  )

  gpThirdpartySetLibraryType(${_gpImGuiLibraryType})

  gpThirdpartySetSources(
    imgui.cpp
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
  )

  gpThirdpartySetIncludeDirectories(".")

  # Engine-wide ImGui configuration: obsolete APIs are disabled so the engine only ever compiles against the modern
  # (1.92+) texture protocol, and ImDrawIdx stays 16-bit to match the RHI index buffer stride used by plugin/imgui.
  gpThirdpartySetCompileDefinitions(PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)

  if(NOT GPBT_IS_MONOLITHIC)
    if(WIN32)
      gpThirdpartySetCompileDefinitions(PRIVATE "IMGUI_API=__declspec(dllexport)")
      gpThirdpartySetCompileDefinitions(INTERFACE "IMGUI_API=__declspec(dllimport)")
    else()
      gpThirdpartySetCompileDefinitions(PUBLIC "IMGUI_API=__attribute__((visibility(\"default\")))")
    endif()
  endif()
gpEndThirdparty()
