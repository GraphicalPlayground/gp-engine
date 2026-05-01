# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(FetchContent)

if(CMAKE_HOST_WIN32)
  set(DXC_URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602/dxc_2026_02_20.zip")
elseif(CMAKE_HOST_UNIX)
  set(DXC_URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602/linux_dxc_2026_02_20.x86_64.tar.gz")
else()
  message(FATAL_ERROR "Unsupported host OS for pre-compiled DXC")
endif()

FetchContent_Declare(
  DXC_Binaries
  URL ${DXC_URL}
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(DXC_Binaries)

if(NOT TARGET dxcompiler)
  add_library(dxcompiler SHARED IMPORTED)
  
  if(CMAKE_HOST_WIN32)
    # On Windows, you link against the .lib and need the headers from /inc
    set_target_properties(dxcompiler PROPERTIES
      IMPORTED_LOCATION "${dxc_binaries_SOURCE_DIR}/bin/x64/dxcompiler.dll"
      IMPORTED_IMPLIB "${dxc_binaries_SOURCE_DIR}/lib/x64/dxcompiler.lib"
      INTERFACE_INCLUDE_DIRECTORIES "${dxc_binaries_SOURCE_DIR}/inc"
    )
  elseif(CMAKE_HOST_UNIX)
    # On Linux, you link directly to the .so and need headers from /include
    set_target_properties(dxcompiler PROPERTIES
      IMPORTED_LOCATION "${dxc_binaries_SOURCE_DIR}/lib/libdxcompiler.so"
      INTERFACE_INCLUDE_DIRECTORIES "${dxc_binaries_SOURCE_DIR}/include"
    )
  endif()
endif()

# TODO:
# Because you are linking a pre-compiled shared library, the dxcompiler.dll (or .so) won't automatically move to
# your build folder where your executable is generated. When you try to run your asset compiler, your OS
# will complain that the DLL is missing.
#
# add_custom_command(TARGET __myTargetExportName POST_BUILD
#   COMMAND ${CMAKE_COMMAND} -E copy_if_different
#     "$<TARGET_FILE:dxcompiler>"         # Grabs the .dll/.so path automatically
#     "$<TARGET_FILE_DIR:__myTargetExportName>" # Where your .exe lives
# )
