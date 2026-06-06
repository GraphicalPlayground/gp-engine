# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartThirdparty("vulkan" VERSION "1.4.350.0")
  gpThirdpartyRequiresPlatforms(Windows Linux macOS iOS Android)

  gpThirdpartySystem(
    FIND_PACKAGE Vulkan
    TARGET       Vulkan::Vulkan
  )

  gpThirdpartySource(
    URL    "https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-1.4.350.0.tar.gz"
    HASH   "SHA256=70270d10bf2c1e074a06ee37a50b75d332993d1b80a1d9526eeed2da6d82ed22"
    TARGET "Vulkan-Headers::Vulkan-Headers"
  )

  gpThirdpartySetCMakeArgs(
    VULKAN_HEADERS_ENABLE_MODULE=OFF
    VULKAN_HEADERS_ENABLE_TESTS=OFF
  )
gpEndThirdparty()
