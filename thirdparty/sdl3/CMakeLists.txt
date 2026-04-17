# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpFetchContent(
  NAME SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.0
  PACKAGE_NAME SDL3
  REQUIRED_VERSION 3.4.0
  OPTIONS
    "SDL_SHARED=OFF"
    "SDL_STATIC=ON"
    "SDL_TEST=OFF"
)

if(TARGET SDL3)
  gpSetTargetFolder(SDL_uclibc "thirdparty/sdl3")
  gpSetTargetFolder(SDL3 "thirdparty/sdl3")
  gpSetTargetFolder(SDL3-static "thirdparty/sdl3")
endif()
