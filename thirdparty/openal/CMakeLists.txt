# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartThirdparty("openal" VERSION "1.25.3")
  gpThirdpartyRequiresPlatforms(Windows Linux macOS iOS Android)

  gpThirdpartySystem(
    FIND_PACKAGE OpenAL
    TARGET       OpenAL::OpenAL
  )

  gpThirdpartySource(
    URL    "https://github.com/kcat/openal-soft/archive/dc98bfbe6064d851e32428db83b06732a448810b.tar.gz"
    HASH   "SHA256=af1096b79f488724ca068a9d3632f05eb56897c6d3ce0984c946ece512ddb020"
    TARGET "OpenAL"
  )

  gpThirdpartySetCMakeArgs(
    ALSOFT_UTILS=OFF
    ALSOFT_EXAMPLES=OFF
    ALSOFT_TESTS=OFF
    ALSOFT_INSTALL=OFF
    ALSOFT_ENABLE_MODULES=OFF
    CMAKE_CXX_SCAN_FOR_MODULES=OFF
  )
gpEndThirdparty()
