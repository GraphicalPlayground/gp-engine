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
    /// @brief Get the handle of a shared library.
    /// @param[in] filename The name of the shared library file.
    /// @return A pointer to the handle of the shared library, or nullptr if the library could not be loaded.
    [[nodiscard]] static GP_CORE_API void* getHandle(gp::StringView filename) noexcept;

    /// @brief Get the address of an exported function from a shared library.
    /// @param[in] handle A pointer to the handle of the shared library.
    /// @param[in] procName The name of the exported function.
    /// @return A pointer to the address of the exported function, or nullptr if the function could not be found.
    [[nodiscard]] static GP_CORE_API void* getExport(void* handle, gp::StringView procName) noexcept;

    /// @brief Frees the handle of a shared library.
    /// @param[in] handle A pointer to the handle of the shared library to be freed.
    static GP_CORE_API void freeHandle(void* handle) noexcept;

    /// @brief Adds a directory to the search path for shared libraries.
    /// @param[in] directory The directory to be added to the search path.
    static void addDirectory(gp::StringView directory) noexcept;

    /// @brief Pushes a directory onto the search path for shared libraries.
    /// @param[in] directory The directory to be pushed onto the search path.
    static void pushDirectory(gp::StringView directory) noexcept;

    /// @brief Removes the most recently added directory from the search path for shared libraries.
    /// @param[in] directory The directory to be removed from the search path.
    static void popDirectory(gp::StringView directory) noexcept;

private:
    /// @brief Loads a shared library from the search paths.
    /// @param[in] filename The name of the shared library file.
    /// @return A pointer to the handle of the shared library, or nullptr if the library could not be loaded.
    [[nodiscard]] void* loadLibraryFromSearchPaths(gp::StringView filename) noexcept;
};

}   // namespace gp::platform::windows
