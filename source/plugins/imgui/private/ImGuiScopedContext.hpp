// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "imgui.h"

namespace gp::imgui::detail
{

/// @brief RAII guard making a given ImGui context current for the enclosing scope.
/// @details Every ImGui call operates on the implicit global `GImGui`. Swapping it per backend call makes the
///          plugin robust when an application owns several contexts (editor UI + in-game debug UI), for the cost of
///          two pointer writes.
class ScopedImGuiContext final
{
public:
    /// @brief Makes `context` current and remembers the previous context.
    /// @param[in] context Context to activate.
    explicit ScopedImGuiContext(ImGuiContext* const context) noexcept
        : m_previous(ImGui::GetCurrentContext())
    {
        ImGui::SetCurrentContext(context);
    }

    /// @brief Restores the previous context.
    ~ScopedImGuiContext() noexcept
    {
        ImGui::SetCurrentContext(m_previous);
    }

    ScopedImGuiContext(const ScopedImGuiContext&) = delete;
    ScopedImGuiContext& operator=(const ScopedImGuiContext&) = delete;
    ScopedImGuiContext(ScopedImGuiContext&&) = delete;
    ScopedImGuiContext& operator=(ScopedImGuiContext&&) = delete;

private:
    ImGuiContext* m_previous{ nullptr };
};

}   // namespace gp::imgui::detail
