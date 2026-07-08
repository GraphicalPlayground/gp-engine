// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::modules
{

/// @brief Enumerates the possible results of a module load operation.
enum class LoadResult
{
    /// @brief Module loaded successfully.
    Success,

    /// @brief The specified module file could not be found.
    FileNotFound,

    /// @brief The specified module file is incompatible with the module system.
    FileIncompatible,

    /// @brief The operating system failed to load the module file.
    CouldNotBeLoadedByOs,

    /// @brief Module initialization failed.
    FailedToInitialize,

    /// @brief A thread attempted to load the module before the Game thread did.
    NotLoadedByGameThread
};

/// @brief Enumerates the possible results of a module unload operation.
enum class UnloadResult
{
    /// @brief Module unloaded successfully.
    Success,

    /// @brief Module does not support dynamic reloading.
    UnloadNotSupported
};

/// @brief Enumerates the possible reasons for a module change event.
enum class ChangeReason
{
    /// @brief A module has been loaded and is ready to be used.
    ModuleLoaded,

    /// @brief A module has been unloaded and should no longer be used.
    ModuleUnloaded,

    /// @brief The paths controlling which plug-ins are loaded have been changed and the given module has been found,
    /// but not yet loaded.
    PluginDirectoryChanged
};

/// @brief Enumerates the possible types of active reloads that can occur during development.
enum class ActiveReloadType
{
    None,
    Reinstancing,
#if GP_HAS_HOT_RELOAD
    HotReload,
#endif
#if GP_HAS_LIVE_CODING
    LiveCoding
#endif
};

}   // namespace gp::modules
