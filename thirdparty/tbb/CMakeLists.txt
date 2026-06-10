# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartThirdparty("tbb" VERSION "2023.0.0")
  gpThirdpartyRequiresPlatforms(Windows Linux macOS)

  gpThirdpartySystem(
    FIND_PACKAGE TBB
    TARGET       TBB::tbbmalloc
  )

  gpThirdpartySource(
    URL    "https://github.com/uxlfoundation/oneTBB/archive/refs/tags/v2023.0.0.tar.gz"
    HASH   "SHA256=f8767b971ec6aea25dde58ae0f593e94e7aa75a739a86f67967012f69e2199b1"
    TARGET "TBB::tbbmalloc"
  )

  gpThirdpartySetCMakeArgs(
    TBB_TEST=OFF
    TBB_EXAMPLES=OFF
    TBB_STRICT=OFF
    TBBMALLOC_BUILD=ON
    TBB_BUILD=OFF
  )
gpEndThirdparty()
