# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartThirdparty("openal" VERSION "1.25.2")
  gpThirdpartyRequiresPlatforms(Windows Linux macOS iOS Android)

  gpThirdpartySystem(
    FIND_PACKAGE OpenAL
    TARGET       OpenAL::OpenAL
  )

  gpThirdpartySource(
    URL    "https://github.com/kcat/openal-soft/archive/refs/tags/1.25.2.tar.gz"
    HASH   "SHA256=fb27e5839aa11f0e5b9d33756965291fad5d6909ab928ea1f796f4a1a6877894"
    TARGET "OpenAL"
  )

  gpThirdpartySetCMakeArgs(
    ALSOFT_UTILS=OFF
    ALSOFT_EXAMPLES=OFF
    ALSOFT_TESTS=OFF
    ALSOFT_INSTALL=OFF
  )
gpEndThirdparty()
