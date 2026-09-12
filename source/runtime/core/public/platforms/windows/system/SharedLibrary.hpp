// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/generic/system/SharedLibrary.hpp"

namespace gp::platform::windows
{

struct SharedLibrary final : public gp::platform::generic::SharedLibrary
{
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
