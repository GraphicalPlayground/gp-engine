// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"
#include "containers/ContainerForward.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/generic/system/SharedLibrary.hpp"

namespace gp::platform::windows
{

struct SharedLibrary final : public gp::platform::generic::SharedLibrary
{
private:
    /// @brief Since Windows can only have one directory at a time, this stack is used to reset the previous directory.
    // static gp::Vector<gp::String> s_searchPathsStack;

    /// @brief All the directories we want to load shared libraries from.
    // static gp::Vector<gp::String> s_searchPaths;

    /// @brief A cache of the shared libraries found in each directory in @p s_searchPaths.
    // static gp::Map<gp::Name, gp::Vector<gp::String>> s_searchPathsCache;

public:
    /// @brief Get the Export object
    /// @param[in] handle
    /// @param[in] procName
    /// @return
    [[nodiscard]] static GP_CORE_API void* getExport(void* handle, gp::StringView procName) noexcept;

    /// @brief
    /// @param[in] handle
    /// @return
    static GP_CORE_API void freeHandle(void* handle) noexcept;
};

}   // namespace gp::platform::windows
