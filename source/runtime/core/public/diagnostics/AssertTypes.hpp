// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "diagnostics/AssertEnums.hpp"
#include "platforms/base/Platform.hpp"

namespace gp::diagnostics
{

/// @brief Plain metadata describing a failed assertion site passed to custom assert handlers.
struct AssertContext final
{
    /// @brief Which assertion family fired (`Check`, `Verify`, `Ensure`, etc.).
    AssertKind kind = AssertKind::Check;

    /// @brief Stringified boolean condition expression (`#expr`), or `nullptr` for entry traps.
    const char* expression = nullptr;

    /// @brief Formatted message string, or `nullptr` when no message was supplied.
    const char* message = nullptr;

    /// @brief Source file path from `__FILE__`.
    const char* file = nullptr;

    /// @brief Function name from `__func__`.
    const char* function = nullptr;

    /// @brief Source line number from `__LINE__`.
    Int32 line = 0;
};

/// @brief Callback signature for intercepting assertion failures across module boundaries without heap allocation.
using AssertHandlerFn = AssertAction (*)(const AssertContext& context);

}   // namespace gp::diagnostics
