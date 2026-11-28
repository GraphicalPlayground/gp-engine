# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground.com/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

# DXC exposes a COM/C-ABI so the Release import lib is ABI-safe in Debug builds.
# Pre-populate Vulkan_dxc_LIBRARY_DEBUG before FindVulkan runs to silence the warning.
if(WIN32 AND DEFINED ENV{VULKAN_SDK} AND NOT Vulkan_dxc_LIBRARY_DEBUG)
  if(EXISTS "$ENV{VULKAN_SDK}/Lib/dxcompiler.lib")
    set(Vulkan_dxc_LIBRARY_DEBUG "$ENV{VULKAN_SDK}/Lib/dxcompiler.lib" CACHE FILEPATH "" FORCE)
  endif()
endif()

find_package(Vulkan COMPONENTS dxc QUIET)

gpStartThirdparty("dxc" VERSION "1.9.2609")
  gpThirdpartyRequiresPlatforms(Windows Linux macOS)

  # SYSTEM: LunarG Vulkan SDK (works on Windows, Linux, and macOS)
  gpThirdpartySystem(
    FIND_PACKAGE Vulkan
    TARGET       Vulkan::dxc_lib
  )

  # BINARY fallback: Linux official release already uses include/, lib/, bin/
  gpThirdpartyBinary(
    PLATFORMS Linux
    COMPILERS GCC Clang
    URL  "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2607/linux_dxc_2026_07_29.x86_x64.tar.gz"
    HASH "SHA256=55665c87824051ed4774ff3280a79ccbbb7d39243b9736ca5e98222134112d54"
  )

  # BINARY fallback: Windows requires repacking inc/ + lib/x64/ + bin/x64/ into include/ + lib/ + bin/
  # gpThirdpartyBinary(
  #   PLATFORMS Windows
  #   COMPILERS MSVC Clang
  #   URL  "https://github.com/GraphicalPlayground/gp-thirdparty-binaries/releases/download/dxc-1.9.2607/dxc_2026_07_29.zip"
  #   HASH "SHA256=<RUN_CMAKE_E_SHA256SUM>"
  # )
gpEndThirdparty()
