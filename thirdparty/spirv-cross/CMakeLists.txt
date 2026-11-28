# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground.com/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartThirdparty("spirv-cross" VERSION "1.4.350.0")
  gpThirdpartyRequiresPlatforms(Windows Linux macOS)

  gpThirdpartySystem(
    FIND_PACKAGE spirv_cross_c
    TARGET       spirv-cross-c
  )

  gpThirdpartySource(
    URL    "https://github.com/KhronosGroup/SPIRV-Cross/archive/refs/tags/vulkan-sdk-1.4.350.0.tar.gz"
    HASH   "SHA256=fbf9bee521545557357679173d39787a954bd8187e4b2fcaa09044c70201b434"
    TARGET "spirv-cross-c"
  )

  gpThirdpartySetCMakeArgs(
    SPIRV_CROSS_CLI=OFF
    SPIRV_CROSS_ENABLE_TESTS=OFF
    SPIRV_CROSS_SHARED=OFF
    SPIRV_CROSS_STATIC=ON
    SPIRV_CROSS_ENABLE_GLSL=ON
    SPIRV_CROSS_ENABLE_HLSL=ON
    SPIRV_CROSS_ENABLE_MSL=ON
    SPIRV_CROSS_ENABLE_C_API=ON
    SPIRV_CROSS_ENABLE_CPP=OFF
    SPIRV_CROSS_ENABLE_REFLECT=OFF
    SPIRV_CROSS_ENABLE_UTIL=OFF
    SPIRV_CROSS_SKIP_INSTALL=ON
    SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS=ON
  )
gpEndThirdparty()
