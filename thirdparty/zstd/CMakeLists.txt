# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartThirdparty("zstd" VERSION "1.5.7")
  gpThirdpartyRequiresPlatforms(Windows Linux macOS iOS Android)

  gpThirdpartySource(
    URL    "https://github.com/facebook/zstd/archive/refs/tags/v1.5.7.tar.gz"
    HASH   "SHA256=37d7284556b20954e56e1ca85b80226768902e2edabd3b649e9e72c0c9012ee3"
    TARGET "zstd::libzstd_static"
  )

  gpThirdpartySetCMakeArgs(
    ZSTD_BUILD_PROGRAMS=OFF
    ZSTD_BUILD_SHARED=OFF
    ZSTD_BUILD_STATIC=ON
    ZSTD_BUILD_TESTS=OFF
    ZSTD_MULTITHREAD_SUPPORT=ON
  )
gpEndThirdparty()
