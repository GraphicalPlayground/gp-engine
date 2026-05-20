# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartThirdparty("sdl3" VERSION "3.4.8")
  gpThirdpartyRequiresPlatforms(Windows Linux macOS iOS Android)

  gpThirdpartySystem(
    FIND_PACKAGE SDL3
    TARGET       SDL3::SDL3-static
  )

  # TODO: We should host our own binaries for SDL3 to speedup CI builds and avoid relying on third-party hosting.
  #       For now, we can use the official SDL3 release tarball.

  gpThirdpartySource(
    URL    "https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.8.tar.gz"
    HASH   "SHA256=429a9f38483f834da5727a63dd7b5127a7c9d06a16439d7c6de4b7ebbfdb6374"
    TARGET "SDL3::SDL3-static"
  )

  gpThirdpartySetCMakeArgs(
    SDL_STATIC=ON
    SDL_SHARED=OFF
    SDL_TEST_LIBRARY=OFF
    SDL_TESTS=OFF
    SDL_EXAMPLES=OFF
  )
gpEndThirdparty()
