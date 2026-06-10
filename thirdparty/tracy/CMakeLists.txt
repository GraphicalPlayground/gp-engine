# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartThirdparty("tracy" VERSION "0.13.2")
  gpThirdpartyRequiresPlatforms(Windows Linux macOS iOS Android)

  gpThirdpartySource(
    URL    "https://github.com/wolfpld/tracy/archive/9588912aa9721e49abbb2106f59c98ecc7e5c17d.tar.gz"
    HASH   "SHA256=3378ace46deace1b99098c708eed57c94e48d102f23811497d542ac612639fcc"
    TARGET "Tracy::TracyClient"
  )

  gpThirdpartySetCMakeArgs(
    TRACY_ENABLE=ON
  )
gpEndThirdparty()
