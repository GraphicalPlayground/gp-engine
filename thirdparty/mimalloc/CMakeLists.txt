# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartThirdparty("mimalloc" VERSION "3.3.2")
  gpThirdpartyRequiresPlatforms(Windows Linux macOS)

  gpThirdpartySystem(
    FIND_PACKAGE mimalloc
    TARGET       mimalloc-static
  )

  gpThirdpartySource(
    URL    "https://github.com/microsoft/mimalloc/archive/refs/tags/v3.3.2.tar.gz"
    HASH   "SHA256=ca02384e007f46950598500dfaebde5ff9948c1d231f5a81b058799afa64bbbb"
    TARGET "mimalloc-static"
  )

  gpThirdpartySetCMakeArgs(
    MI_BUILD_SHARED=OFF
    MI_BUILD_STATIC=ON
    MI_BUILD_TESTS=OFF
    MI_BUILD_OBJECT=OFF
  )
gpEndThirdparty()
