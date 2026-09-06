# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartThirdparty("d3d11" VERSION "sdk")
  gpThirdpartyRequiresPlatforms(Windows)
  gpThirdpartySystem(
    WINDOWS_SDK
    LIBS d3d11 dxgi dxguid d3dcompiler
  )
gpEndThirdparty()
