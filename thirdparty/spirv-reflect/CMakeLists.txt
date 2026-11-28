# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground.com/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartThirdparty("spirv-reflect" VERSION "1.4.350.0")
  gpThirdpartyRequiresPlatforms(Windows Linux macOS)

  gpThirdpartySource(
    URL  "https://github.com/KhronosGroup/SPIRV-Reflect/archive/refs/tags/vulkan-sdk-1.4.350.0.tar.gz"
    HASH "SHA256=c81ea49449d77189574ce0ff1374350533c283f41a9dd1b59f351c26961302b9"
  )

  # Keep STATIC even in non-monolithic builds; it only lives inside your ShaderCompiler module
  gpThirdpartySetLibraryType(STATIC)

  gpThirdpartySetSources(
    spirv_reflect.c
  )

  gpThirdpartySetIncludeDirectories(".")
gpEndThirdparty()
