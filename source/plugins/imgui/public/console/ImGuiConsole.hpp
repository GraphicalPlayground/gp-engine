// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "console/ConsoleEnums.hpp"
#include "console/ConsoleForward.hpp"
#include "console/ImGuiConsoleDesc.hpp"
#include "console/ImGuiConsoleEnums.hpp"
#include "console/ImGuiConsoleTypes.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "ImGuiPlugin.hpp"

struct ImGuiInputTextCallbackData;

namespace gp::imgui
{

/// @brief Drop-down developer console for console variables and commands, with Unreal-style auto-completion.
/// @details Mirrors the behavior of Unreal Engine's in-game console (`~`):
///          - Typing filters every registered CVar/command: exact match first, then prefix matches, then names that
///            merely contain the query, each group sorted alphabetically.
///          - Up/Down walks the suggestion list and writes the highlighted name into the input line *without*
///            re-filtering, so the list stays stable while browsing. With no suggestions, Up/Down recalls history.
///          - Tab completes to the highlighted (or first) suggestion plus a trailing space; the list then turns into
///            a hint row showing the current value, default, type and help of that object.
///          - Enter submits through `IConsoleManager::processUserConsoleInput()`; its output lines land in the log.
///
///          The widget splits **state** (query, suggestions, history, log) from **presentation** (`draw()`), so the
///          whole completion/history model is unit-testable headlessly without a GPU or even an ImGui frame.
/// @note Main thread only. The console manager must outlive the widget. Suggestion views point into the manager's
///       registry and are rebuilt on every query change.
class GP_PLUGIN_IMGUI_API ImGuiConsole final
{
public:
    /// @brief Size of the inline UTF-8 input buffer, including the null terminator.
    static constexpr gp::USize kInputCapacity = 512u;

    /// @brief Sentinel for "no suggestion highlighted" / "not browsing history".
    static constexpr gp::Int32 kNoSelection = -1;

public:
    /// @brief Constructs an uninitialized console.
    ImGuiConsole() noexcept = default;

    /// @brief Default destructor (owns only engine containers).
    ~ImGuiConsole() noexcept = default;

    ImGuiConsole(const ImGuiConsole&) = delete;
    ImGuiConsole& operator=(const ImGuiConsole&) = delete;
    ImGuiConsole(ImGuiConsole&&) = delete;
    ImGuiConsole& operator=(ImGuiConsole&&) = delete;

public:
    /// @brief Binds the console to a console manager and reserves its containers.
    /// @param[in] manager Registry to query and execute against; `nullptr` uses `IConsoleManager::get()`.
    /// @param[in] desc Capacities and presentation options.
    void initialize(gp::console::IConsoleManager* manager = nullptr, const ImGuiConsoleDesc& desc = {}) noexcept;

    /// @brief Releases the log, history and suggestion storage and unbinds the manager.
    void shutdown() noexcept;

    /// @brief Draws the console panel (and handles the toggle key) for the current ImGui frame.
    /// @note Call between `ImGuiLayer::beginFrame()` and `endFrame()`. Does nothing before `initialize()`.
    void draw() noexcept;

public:
    /// @brief Opens the panel and focuses its input line on the next `draw()`.
    void open() noexcept;

    /// @brief Closes the panel and clears the pending suggestion state.
    void close() noexcept;

    /// @brief Opens the panel when closed, closes it when open.
    void toggle() noexcept;

    /// @brief Reports whether the panel is visible.
    /// @return True while the console is open.
    [[nodiscard]] bool isOpen() const noexcept
    {
        return m_isOpen;
    }

public:
    /// @brief Replaces the input line as if the user typed it, and re-filters suggestions.
    /// @param[in] text New input; truncated to `kInputCapacity - 1` bytes.
    void setInput(gp::StringView text) noexcept;

    /// @brief Returns the current input line.
    /// @return View into the inline input buffer.
    [[nodiscard]] gp::StringView getInput() const noexcept;

    /// @brief Rebuilds the ranked suggestion list from the current query.
    /// @details When the input is `"<known name> <args...>"` the list collapses to that single object (hint mode).
    void refreshSuggestions() noexcept;

    /// @brief Returns the ranked suggestions for the current query.
    /// @return Suggestions, best match first.
    [[nodiscard]] const gp::Vector<ImGuiConsoleSuggestion>& getSuggestions() const noexcept
    {
        return m_suggestions;
    }

    /// @brief Reports whether the list is the single-object argument hint (`"name "` followed by arguments).
    /// @return True in hint mode.
    [[nodiscard]] bool isShowingArgumentHint() const noexcept
    {
        return m_isArgumentHint;
    }

    /// @brief Returns the highlighted suggestion index.
    /// @return Index into `getSuggestions()`, or `kNoSelection`.
    [[nodiscard]] gp::Int32 getSelectedSuggestion() const noexcept
    {
        return m_selectedSuggestion;
    }

    /// @brief Moves the highlight by @p delta rows (wrapping) and writes that name into the input line.
    /// @param[in] delta `+1` for Down, `-1` for Up.
    /// @return True if a suggestion is now highlighted; false when the list is empty.
    bool moveSuggestionSelection(gp::Int32 delta) noexcept;

    /// @brief Completes the input to the highlighted (or first) suggestion followed by a space.
    /// @return True if the input changed.
    bool acceptSuggestion() noexcept;

    /// @brief Steps through submitted commands, most recent first.
    /// @param[in] delta `-1` for older (Up), `+1` for newer (Down); stepping past the newest restores an empty line.
    /// @return True if the input line changed.
    bool navigateHistory(gp::Int32 delta) noexcept;

    /// @brief Returns submitted commands, oldest first.
    /// @return History entries.
    [[nodiscard]] const gp::Vector<gp::String>& getHistory() const noexcept
    {
        return m_history;
    }

    /// @brief Executes the current input line, records it in history, logs its output and clears the input.
    /// @details `clear` / `cls` are handled locally and wipe the log.
    /// @return Console manager result (`Success` for empty input and local commands).
    gp::console::ConsoleResult submit() noexcept;

public:
    /// @brief Appends one line to the output log, splitting embedded newlines into separate lines.
    /// @param[in] kind Line category.
    /// @param[in] text UTF-8 text.
    void addLine(ImGuiConsoleLineKind kind, gp::StringView text) noexcept;

    /// @brief Empties the output log.
    void clearLog() noexcept;

    /// @brief Returns the number of lines currently held by the log.
    /// @return Line count, at most `ImGuiConsoleDesc::maxOutputLines`.
    [[nodiscard]] gp::USize getLineCount() const noexcept
    {
        return m_lines.size();
    }

    /// @brief Returns a log line in chronological order.
    /// @param[in] index `0` is the oldest line still held; must be `< getLineCount()`.
    /// @return The line.
    [[nodiscard]] const ImGuiConsoleLine& getLine(gp::USize index) const noexcept;

private:
    /// @brief Inserts a candidate into `m_suggestions`, keeping it sorted and bounded by `maxSuggestions`.
    /// @param[in] candidate Candidate to rank.
    void insertRanked(const ImGuiConsoleSuggestion& candidate) noexcept;

    /// @brief Writes @p text into the input buffer without touching the query or suggestions.
    /// @param[in] text Text to copy; truncated to capacity.
    void writeInputBuffer(gp::StringView text) noexcept;

    /// @brief Draws the scrolling output log child window.
    /// @param[in] height Child height in pixels.
    void drawLog(gp::Float32 height) noexcept;

    /// @brief Draws the suggestion overlay just above the input line.
    /// @param[in] inputTopY Screen-space Y of the input line's top edge.
    void drawSuggestions(gp::Float32 inputTopY) noexcept;

    /// @brief Draws one suggestion row (name, value, help).
    /// @param[in] index Row index into `m_suggestions`.
    void drawSuggestionRow(gp::Int32 index) noexcept;

    /// @brief `ImGui::InputText` callback: Tab completion, Up/Down routing, edit re-filtering, toggle-key filtering.
    /// @param[in,out] data ImGui callback payload; `UserData` is the `ImGuiConsole`.
    /// @return `0` to keep the character / event, `1` to discard a filtered character.
    static int inputTextCallback(ImGuiInputTextCallbackData* data) noexcept;

private:
    ImGuiConsoleDesc m_desc{};
    gp::console::IConsoleManager* m_manager{ nullptr };

    /// @brief Chronological ring buffer of log lines; `m_lineHead` is the oldest once the buffer is full.
    gp::Vector<ImGuiConsoleLine> m_lines;
    gp::USize m_lineHead{ 0u };

    gp::Vector<gp::String> m_history;
    gp::Vector<ImGuiConsoleSuggestion> m_suggestions;

    /// @brief Inline input line (ImGui edits it in place).
    char m_input[kInputCapacity]{};

    gp::Int32 m_selectedSuggestion{ kNoSelection };
    gp::Int32 m_historyCursor{ kNoSelection };

    bool m_isInitialized{ false };
    bool m_isOpen{ false };
    bool m_isArgumentHint{ false };
    bool m_shouldFocusInput{ false };
    bool m_shouldScrollToBottom{ false };
    bool m_shouldScrollToSelection{ false };

    /// @brief Set when the input buffer was rewritten outside the ImGui callback; the callback then syncs ImGui's copy.
    bool m_hasPendingBufferSync{ false };
};

}   // namespace gp::imgui
