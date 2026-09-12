// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"

namespace gp::platform::generic
{

/// @brief A class for managing shared libraries.
struct SharedLibrary
{
public:
    /// @brief Get the Handle object
    /// @param[in] filename
    /// @return
    [[nodiscard]] static GP_CORE_API void* getHandle(gp::StringView filename) noexcept;

    /// @brief Get the Export object
    /// @param[in] handle
    /// @param[in] procName
    /// @return
    [[nodiscard]] static GP_CORE_API void* getExport(void* handle, gp::StringView procName) noexcept;

    /// @brief
    /// @param[in] handle
    /// @return
    static GP_CORE_API void freeHandle(void* handle) noexcept;

    /// @brief
    /// @param[in] directory
    static inline void addDirectory([[maybe_unused]] gp::StringView directory) noexcept
    {}

    /// @brief
    /// @param[in] directory
    static inline void pushDirectory([[maybe_unused]] gp::StringView directory) noexcept
    {}

    /// @brief
    /// @param[in] directory
    static inline void popDirectory([[maybe_unused]] gp::StringView directory) noexcept
    {}
};

}   // namespace gp::platform::generic
