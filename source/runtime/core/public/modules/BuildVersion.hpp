// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"

namespace gp::modules
{

/// @brief Represents the version information for a build.
struct BuildVersion
{
    /// @brief The major version number of the build.
    Int32 majorVersion{ 0 };

    /// @brief The minor version number of the build.
    Int32 minorVersion{ 0 };

    /// @brief The hotfix/patch version number of the build.
    Int32 patchVersion{ 0 };

    /// @brief Wether the build is a promoted build (e.g., release) or not (e.g., development).
    bool isPromotedBuild{ false };

    /// @brief The name of the branch from which the build was created.
    StringView branchName{};

    /// @brief The commit hash of the build.
    StringView commitHash{};
};

}   // namespace gp::modules
