# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartThirdparty("tracy" VERSION "0.14.1")
  gpThirdpartyRequiresPlatforms(Windows Linux macOS iOS Android)

  gpThirdpartySource(
    URL    "https://github.com/wolfpld/tracy/archive/refs/tags/v0.14.1.tar.gz"
    HASH   "SHA256=bf4af567e9c7524d07f3caa745fad02fb33bd5694f11910750382d1efbb251c1"
    TARGET "Tracy::TracyClient"
  )

  gpThirdpartySetCMakeArgs(
    TRACY_ENABLE=ON
    TRACY_STATIC=ON
  )
gpEndThirdparty()
