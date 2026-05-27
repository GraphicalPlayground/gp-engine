# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartThirdparty("rapidjson" VERSION "1.1.0")
  gpThirdpartyRequiresPlatforms(Windows Linux macOS iOS Android)

  # We are using a specific commit hash instead because the latest release (v1.1.0) is from 2016 and does not include some important bug fixes and features that we need.
  gpThirdpartyGit(
    REPOSITORY    "https://github.com/Tencent/rapidjson.git"
    TAG           "24b5e7a8b27f42fa16b96fc70aade9106cf7102f"
    TARGET        "RapidJSON"
    PATCH_COMMAND git apply "${CMAKE_CURRENT_SOURCE_DIR}/patches/disable_travis.patch"
  )

  gpThirdpartySetCMakeArgs(
    RAPIDJSON_HAS_STDSTRING=OFF
    RAPIDJSON_USE_MEMBERSMAP=OFF
    RAPIDJSON_BUILD_DOC=OFF
    RAPIDJSON_BUILD_EXAMPLES=OFF
    RAPIDJSON_BUILD_TESTS=OFF
    RAPIDJSON_BUILD_THIRDPARTY_GTEST=OFF
    # Remove CMAKE Warnings and deprecation messages
    CMAKE_WARN_DEPRECATED=OFF
  )
gpEndThirdparty()
