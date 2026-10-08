// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"

namespace gp::platform::generic
{

/// @brief A class for managing shared libraries.
/// @details This class provides a platform-independent interface for loading and unloading shared libraries, as well as
/// retrieving function pointers from them. It is designed to be used in a generic context, where the specific platform
/// implementation is not known at compile time. The actual implementation of the shared library management is delegated
/// to platform-specific classes, which are included based on the target platform.
/// @see gp::platform::windows::SharedLibrary, gp::platform::linux::SharedLibrary, gp::platform::macos::SharedLibrary
struct SharedLibrary
{
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
    GP_FORCEINLINE_HINT static void addDirectory([[maybe_unused]] gp::StringView directory) noexcept
    {}

    /// @brief Pushes a directory onto the search path for shared libraries.
    /// @param[in] directory The directory to be pushed onto the search path.
    GP_FORCEINLINE_HINT static void pushDirectory([[maybe_unused]] gp::StringView directory) noexcept
    {}

    /// @brief Removes the most recently added directory from the search path for shared libraries.
    /// @param[in] directory The directory to be removed from the search path.
    GP_FORCEINLINE_HINT static void popDirectory([[maybe_unused]] gp::StringView directory) noexcept
    {}
};

}   // namespace gp::platform::generic
