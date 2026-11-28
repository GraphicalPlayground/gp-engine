// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "console/ImGuiConsole.hpp"
#include "console/ConsoleTypes.hpp"
#include "console/IConsoleManager.hpp"
#include "console/IConsoleObject.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/strings/CharTraits.hpp"
#include "imgui.h"
#include <cstring>

namespace gp::imgui
{

namespace
{

/// @brief ASCII character traits used for case-insensitive console name matching (names are ASCII by convention).
using Traits = gp::container::CharTraits<char>;

/// @brief Maximum number of suggestion rows visible before the overlay scrolls.
constexpr gp::Int32 kMaxVisibleSuggestionRows = 10;

/// @brief Maximum overlay width in unscaled pixels (multiplied by `style.FontScaleDpi`), so help text does not stretch
/// across ultra-wide viewports.
constexpr gp::Float32 kMaxSuggestionWidth = 980.0f;

/// @brief Grave accent typed by the US-layout toggle key.
constexpr ImWchar kGraveCharacter = static_cast<ImWchar>(0x60);

/// @brief Superscript two typed by the same physical key on AZERTY layouts.
constexpr ImWchar kSuperscriptTwoCharacter = static_cast<ImWchar>(0xB2);

/// @brief Finds @p needle inside @p haystack ignoring ASCII case.
/// @details Naive O(n*m) scan; console names are short (< 64 chars) and the registry holds a few hundred entries,
///          so this is cheaper than building a lowercase copy of every name per keystroke.
/// @param[in] haystack Text to search.
/// @param[in] needle Text to find; empty matches at 0.
/// @return Offset of the first match, or `gp::StringView::npos`.
[[nodiscard]] gp::USize findIgnoreCase(const gp::StringView haystack, const gp::StringView needle) noexcept
{
    if (needle.size() > haystack.size())
    {
        return gp::StringView::npos;
    }

    const gp::USize lastStart = haystack.size() - needle.size();
    for (gp::USize start = 0u; start <= lastStart; ++start)
    {
        gp::USize matched = 0u;
        while (matched < needle.size() &&
               Traits::toLower(haystack.data()[start + matched]) == Traits::toLower(needle.data()[matched]))
        {
            ++matched;
        }
        if (matched == needle.size())
        {
            return start;
        }
    }
    return gp::StringView::npos;
}

/// @brief Three-way ASCII case-insensitive comparison.
/// @param[in] lhs Left operand.
/// @param[in] rhs Right operand.
/// @return Negative, zero or positive like `strcmp`.
[[nodiscard]] gp::Int32 compareIgnoreCase(const gp::StringView lhs, const gp::StringView rhs) noexcept
{
    const gp::USize common = (lhs.size() < rhs.size()) ? lhs.size() : rhs.size();
    for (gp::USize i = 0u; i < common; ++i)
    {
        const gp::Int32 a = static_cast<gp::Int32>(static_cast<unsigned char>(Traits::toLower(lhs.data()[i])));
        const gp::Int32 b = static_cast<gp::Int32>(static_cast<unsigned char>(Traits::toLower(rhs.data()[i])));
        if (a != b)
        {
            return a - b;
        }
    }
    if (lhs.size() == rhs.size())
    {
        return 0;
    }
    return (lhs.size() < rhs.size()) ? -1 : 1;
}

/// @brief Ranking predicate: match quality first, then alphabetical (case-insensitive).
/// @param[in] lhs Left candidate.
/// @param[in] rhs Right candidate.
/// @return True if @p lhs must be listed before @p rhs.
[[nodiscard]] bool isRankedBefore(const ImGuiConsoleSuggestion& lhs, const ImGuiConsoleSuggestion& rhs) noexcept
{
    if (lhs.match != rhs.match)
    {
        return static_cast<gp::UInt8>(lhs.match) < static_cast<gp::UInt8>(rhs.match);
    }
    return compareIgnoreCase(lhs.name, rhs.name) < 0;
}

/// @brief Strips leading and trailing ASCII whitespace.
/// @param[in] text Text to trim.
/// @return Trimmed sub-view.
[[nodiscard]] gp::StringView trimWhitespace(gp::StringView text) noexcept
{
    while (!text.isEmpty() && Traits::isWhitespace(text.data()[0]))
    {
        text.removePrefix(1u);
    }
    while (!text.isEmpty() && Traits::isWhitespace(text.data()[text.size() - 1u]))
    {
        text = text.substr(0u, text.size() - 1u);
    }
    return text;
}

/// @brief Returns the log color for a line category.
/// @param[in] kind Line category.
/// @param[out] outColor Receives the color when the category overrides the default text color.
/// @return True if @p outColor was written.
[[nodiscard]] bool getLineColor(const ImGuiConsoleLineKind kind, ImVec4& outColor) noexcept
{
    switch (kind)
    {
    case ImGuiConsoleLineKind::Input:
        outColor = ImVec4(0.55f, 0.80f, 1.00f, 1.0f);
        return true;
    case ImGuiConsoleLineKind::Info:
        outColor = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        return true;
    case ImGuiConsoleLineKind::Error:
        outColor = ImVec4(1.00f, 0.45f, 0.40f, 1.0f);
        return true;
    case ImGuiConsoleLineKind::Output:
        return false;
    default:
        break;
    }
    return false;
}

}   // namespace

void ImGuiConsole::initialize(gp::console::IConsoleManager* const manager, const ImGuiConsoleDesc& desc) noexcept
{
    m_desc = desc;
    m_manager = (manager != nullptr) ? manager : &gp::console::IConsoleManager::get();

    m_lines.clear();
    m_lineHead = 0u;
    m_history.clear();
    m_suggestions.clear();
    m_lines.reserve(m_desc.maxOutputLines);
    m_history.reserve(m_desc.maxHistoryEntries);
    m_suggestions.reserve(m_desc.maxSuggestions + 1u);

    m_input[0] = '\0';
    m_selectedSuggestion = kNoSelection;
    m_historyCursor = kNoSelection;
    m_isArgumentHint = false;
    m_isInitialized = true;

    addLine(
        ImGuiConsoleLineKind::Info,
        "Console ready. Tab: complete | Up/Down: browse suggestions or history | Enter: run | 'clear': wipe log | " "Esc: close"
    );
}

void ImGuiConsole::shutdown() noexcept
{
    m_lines.clear();
    m_history.clear();
    m_suggestions.clear();
    m_lineHead = 0u;
    m_manager = nullptr;
    m_isOpen = false;
    m_isInitialized = false;
}

void ImGuiConsole::open() noexcept
{
    m_isOpen = true;
    m_shouldFocusInput = true;
    m_shouldScrollToBottom = true;
    refreshSuggestions();
}

void ImGuiConsole::close() noexcept
{
    m_isOpen = false;
    m_suggestions.clear();
    m_selectedSuggestion = kNoSelection;
    m_isArgumentHint = false;
}

void ImGuiConsole::toggle() noexcept
{
    if (m_isOpen)
    {
        close();
    }
    else
    {
        open();
    }
}

void ImGuiConsole::setInput(const gp::StringView text) noexcept
{
    writeInputBuffer(text);
    m_historyCursor = kNoSelection;
    refreshSuggestions();
}

gp::StringView ImGuiConsole::getInput() const noexcept
{
    return gp::StringView(m_input, Traits::length(m_input));
}

void ImGuiConsole::refreshSuggestions() noexcept
{
    m_suggestions.clear();
    m_selectedSuggestion = kNoSelection;
    m_isArgumentHint = false;

    if (m_manager == nullptr)
    {
        return;
    }

    // Leading whitespace is ignored; the first token is the object name, the rest are its arguments.
    gp::StringView input = getInput();
    while (!input.isEmpty() && Traits::isWhitespace(input.data()[0]))
    {
        input.removePrefix(1u);
    }
    if (input.isEmpty())
    {
        return;
    }

    gp::USize tokenEnd = 0u;
    while (tokenEnd < input.size() && !Traits::isWhitespace(input.data()[tokenEnd]))
    {
        ++tokenEnd;
    }
    const gp::StringView query = input.substr(0u, tokenEnd);

    // "name <args>": the user is past the name, so show one argument-hint row for that exact object (if any).
    if (tokenEnd < input.size())
    {
        gp::console::IConsoleObject* const object = m_manager->findConsoleObject(query);
        if (object != nullptr)
        {
            ImGuiConsoleSuggestion hint{};
            hint.object = object;
            hint.name = object->getName();
            hint.kind = (object->asVariable() != nullptr) ? ImGuiConsoleSuggestionKind::Variable
                                                          : ImGuiConsoleSuggestionKind::Command;
            hint.match = ImGuiConsoleMatchKind::Exact;
            m_suggestions.pushBack(hint);
            m_isArgumentHint = true;
        }
        return;
    }

    // Full registry scan with substring matching (Unreal's console matches anywhere in the name, not just the
    // prefix). `autoComplete()` would be cheaper but is prefix-only, so it would miss `TargetFps` -> `app.TargetFps`.
    const gp::console::ConsoleObjectVisitor visitor = gp::console::ConsoleObjectVisitor::createLambda(
        [this, query](gp::console::IConsoleObject* const object) noexcept -> bool
    {
        const gp::StringView name = object->getName();
        const gp::USize position = findIgnoreCase(name, query);
        if (position == gp::StringView::npos)
        {
            return true;
        }

        ImGuiConsoleSuggestion candidate{};
        candidate.object = object;
        candidate.name = name;
        candidate.kind = (object->asVariable() != nullptr) ? ImGuiConsoleSuggestionKind::Variable
                                                           : ImGuiConsoleSuggestionKind::Command;
        if (position != 0u)
        {
            candidate.match = ImGuiConsoleMatchKind::Substring;
        }
        else if (name.size() == query.size())
        {
            candidate.match = ImGuiConsoleMatchKind::Exact;
        }
        else
        {
            candidate.match = ImGuiConsoleMatchKind::Prefix;
        }
        insertRanked(candidate);
        return true;
    }
    );
    m_manager->forEachConsoleObjectStartingWith(visitor, gp::StringView(), m_desc.includeInvisible);
}

bool ImGuiConsole::moveSuggestionSelection(const gp::Int32 delta) noexcept
{
    if (m_suggestions.isEmpty() || delta == 0)
    {
        return false;
    }

    const gp::Int32 count = static_cast<gp::Int32>(m_suggestions.size());
    if (m_selectedSuggestion == kNoSelection)
    {
        m_selectedSuggestion = (delta > 0) ? 0 : (count - 1);
    }
    else
    {
        m_selectedSuggestion = (((m_selectedSuggestion + delta) % count) + count) % count;
    }

    // Unreal behavior: browsing writes the name into the input line but keeps the list filtered by the typed query.
    writeInputBuffer(m_suggestions[static_cast<gp::USize>(m_selectedSuggestion)].name);
    m_shouldScrollToSelection = true;
    return true;
}

bool ImGuiConsole::acceptSuggestion() noexcept
{
    if (m_suggestions.isEmpty() || m_isArgumentHint)
    {
        return false;
    }

    const gp::USize index = (m_selectedSuggestion == kNoSelection) ? 0u : static_cast<gp::USize>(m_selectedSuggestion);
    gp::String completed(m_suggestions[index].name);
    completed.append(gp::StringView(" "));
    setInput(completed.view());
    return true;
}

bool ImGuiConsole::navigateHistory(const gp::Int32 delta) noexcept
{
    if (m_history.isEmpty() || delta == 0)
    {
        return false;
    }

    const gp::Int32 count = static_cast<gp::Int32>(m_history.size());
    if (delta < 0)
    {
        if (m_historyCursor == kNoSelection)
        {
            m_historyCursor = count - 1;
        }
        else if (m_historyCursor > 0)
        {
            --m_historyCursor;
        }
        else
        {
            return false;
        }
    }
    else
    {
        if (m_historyCursor == kNoSelection)
        {
            return false;
        }
        if (m_historyCursor < count - 1)
        {
            ++m_historyCursor;
        }
        else
        {
            // Stepping past the newest entry returns to a fresh, empty line (shell-like).
            m_historyCursor = kNoSelection;
            writeInputBuffer(gp::StringView());
            m_suggestions.clear();
            m_selectedSuggestion = kNoSelection;
            m_isArgumentHint = false;
            return true;
        }
    }

    writeInputBuffer(m_history[static_cast<gp::USize>(m_historyCursor)].view());
    m_suggestions.clear();
    m_selectedSuggestion = kNoSelection;
    m_isArgumentHint = false;
    return true;
}

gp::console::ConsoleResult ImGuiConsole::submit() noexcept
{
    const gp::StringView trimmed = trimWhitespace(getInput());
    if (trimmed.isEmpty())
    {
        writeInputBuffer(gp::StringView());
        return gp::console::ConsoleResult::Success;
    }

    // Copy before clearing: `trimmed` views the input buffer.
    const gp::String command(trimmed);

    if (m_desc.maxHistoryEntries > 0u && (m_history.isEmpty() || m_history.back().view() != command.view()))
    {
        if (m_history.size() >= m_desc.maxHistoryEntries)
        {
            m_history.removeAt(0u);
        }
        m_history.pushBack(command);
    }

    addLine(ImGuiConsoleLineKind::Input, command.view());
    writeInputBuffer(gp::StringView());
    m_suggestions.clear();
    m_selectedSuggestion = kNoSelection;
    m_historyCursor = kNoSelection;
    m_isArgumentHint = false;
    m_shouldScrollToBottom = true;

    if (command.equalsIgnoreCase(gp::StringView("clear")) || command.equalsIgnoreCase(gp::StringView("cls")))
    {
        clearLog();
        return gp::console::ConsoleResult::Success;
    }

    if (m_manager == nullptr)
    {
        addLine(ImGuiConsoleLineKind::Error, "No console manager bound.");
        return gp::console::ConsoleResult::NotFound;
    }

    gp::USize emittedLines = 0u;
    gp::console::ConsoleOutputDelegate output = gp::console::ConsoleOutputDelegate::createLambda(
        [this, &emittedLines](const gp::StringView text) noexcept -> void
    {
        addLine(ImGuiConsoleLineKind::Output, text);
        ++emittedLines;
    }
    );

    const gp::console::ConsoleResult result = m_manager->processUserConsoleInput(command.view(), &output);
    if (result != gp::console::ConsoleResult::Success)
    {
        gp::String message("Error: ");
        message.append(gp::StringView(gp::console::consoleResultToString(result)));
        if (result == gp::console::ConsoleResult::NotFound)
        {
            message.append(gp::StringView(" (no variable or command with that name)"));
        }
        addLine(ImGuiConsoleLineKind::Error, message.view());
    }
    else if (emittedLines == 0u)
    {
        addLine(ImGuiConsoleLineKind::Info, "[ok]");
    }
    return result;
}

void ImGuiConsole::addLine(const ImGuiConsoleLineKind kind, const gp::StringView text) noexcept
{
    if (m_desc.maxOutputLines == 0u)
    {
        return;
    }

    gp::StringView remaining = text;
    do
    {
        gp::USize lineEnd = 0u;
        while (lineEnd < remaining.size() && remaining.data()[lineEnd] != '\n')
        {
            ++lineEnd;
        }

        gp::StringView segment = remaining.substr(0u, lineEnd);
        if (!segment.isEmpty() && segment.data()[segment.size() - 1u] == '\r')
        {
            segment = segment.substr(0u, segment.size() - 1u);
        }

        // Ring buffer: grow until full, then overwrite the oldest slot in place (reusing its string capacity).
        ImGuiConsoleLine* slot = nullptr;
        if (m_lines.size() < m_desc.maxOutputLines)
        {
            slot = &m_lines.pushBackDefaulted();
        }
        else
        {
            slot = &m_lines[m_lineHead];
            m_lineHead = (m_lineHead + 1u) % m_lines.size();
        }

        slot->kind = kind;
        slot->text.clear();
        if (kind == ImGuiConsoleLineKind::Input)
        {
            slot->text.append(gp::StringView("> "));
        }
        slot->text.append(segment);

        if (lineEnd >= remaining.size())
        {
            break;
        }
        remaining.removePrefix(lineEnd + 1u);
    }
    while (!remaining.isEmpty());
}

void ImGuiConsole::clearLog() noexcept
{
    m_lines.clear();
    m_lineHead = 0u;
}

const ImGuiConsoleLine& ImGuiConsole::getLine(const gp::USize index) const noexcept
{
    GP_ASSERT(index < m_lines.size(), "ImGuiConsole::getLine index out of range.");
    return m_lines[(m_lineHead + index) % m_lines.size()];
}

void ImGuiConsole::insertRanked(const ImGuiConsoleSuggestion& candidate) noexcept
{
    const gp::USize capacity = m_desc.maxSuggestions;
    if (capacity == 0u)
    {
        return;
    }

    // Bounded insertion sort: O(n * k) with k = maxSuggestions, no allocation beyond the reserved capacity.
    gp::USize position = m_suggestions.size();
    while (position > 0u && isRankedBefore(candidate, m_suggestions[position - 1u]))
    {
        --position;
    }
    if (position >= capacity)
    {
        return;
    }
    if (m_suggestions.size() >= capacity)
    {
        m_suggestions.removeAt(m_suggestions.size() - 1u);
    }
    m_suggestions.insertAt(position, candidate);
}

void ImGuiConsole::writeInputBuffer(const gp::StringView text) noexcept
{
    const gp::USize length = (text.size() < kInputCapacity - 1u) ? text.size() : (kInputCapacity - 1u);
    if (length > 0u && text.data() != m_input)
    {
        std::memmove(m_input, text.data(), length);
    }
    m_input[length] = '\0';
    m_hasPendingBufferSync = true;
}

void ImGuiConsole::draw() noexcept
{
    if (!m_isInitialized)
    {
        return;
    }

    if (m_desc.handleToggleKey && ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false))
    {
        toggle();
    }
    if (!m_isOpen)
    {
        return;
    }

    const ImGuiViewport* const viewport = ImGui::GetMainViewport();
    const gp::Float32 ratio =
        (m_desc.heightRatio > 0.05f) ? ((m_desc.heightRatio < 1.0f) ? m_desc.heightRatio : 1.0f) : 0.05f;
    const gp::Float32 height = viewport->WorkSize.y * ratio;

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - height));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, height));
    ImGui::SetNextWindowBgAlpha(0.94f);
    if (m_shouldFocusInput)
    {
        ImGui::SetNextWindowFocus();
    }

    const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                         ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings |
                                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (ImGui::Begin("##gp_console", nullptr, windowFlags))
    {
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
            ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        {
            close();
            ImGui::End();
            return;
        }

        const gp::Float32 inputHeight = ImGui::GetFrameHeightWithSpacing();
        drawLog(ImGui::GetContentRegionAvail().y - inputHeight);

        const gp::Float32 inputTopY = ImGui::GetCursorScreenPos().y;
        if (m_shouldFocusInput)
        {
            ImGui::SetKeyboardFocusHere();
            m_shouldFocusInput = false;
        }

        const ImGuiInputTextFlags inputFlags =
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion |
            ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackEdit |
            ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CallbackCharFilter;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputTextWithHint(
                "##gp_console_input",
                "Console variable or command (Tab: complete, Up/Down: browse)",
                m_input,
                kInputCapacity,
                inputFlags,
                &ImGuiConsole::inputTextCallback,
                this
            ))
        {
            // Enter deactivates the widget; re-focus next frame so commands can be chained like in a shell.
            (void)submit();
            m_shouldFocusInput = true;
        }

        drawSuggestions(inputTopY);
    }
    ImGui::End();
}

void ImGuiConsole::drawLog(const gp::Float32 height) noexcept
{
    if (!ImGui::BeginChild(
            "##gp_console_log", ImVec2(0.0f, height), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar
        ))
    {
        ImGui::EndChild();
        return;
    }

    // Stick to the bottom unless the user scrolled up to read older output.
    const bool wasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;

    const gp::Float32 dpiScale = ImGui::GetStyle().FontScaleDpi;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * dpiScale, 1.0f * dpiScale));
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(m_lines.size()));
    while (clipper.Step())
    {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
        {
            const ImGuiConsoleLine& line = getLine(static_cast<gp::USize>(row));
            ImVec4 color{};
            const bool hasColor = getLineColor(line.kind, color);
            if (hasColor)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, color);
            }
            ImGui::TextUnformatted(line.text.data(), line.text.data() + line.text.size());
            if (hasColor)
            {
                ImGui::PopStyleColor();
            }
        }
    }
    ImGui::PopStyleVar();

    if (m_shouldScrollToBottom || wasAtBottom)
    {
        ImGui::SetScrollHereY(1.0f);
        m_shouldScrollToBottom = false;
    }
    ImGui::EndChild();
}

void ImGuiConsole::drawSuggestions(const gp::Float32 inputTopY) noexcept
{
    if (m_suggestions.isEmpty())
    {
        return;
    }

    const ImGuiStyle& style = ImGui::GetStyle();
    const gp::Int32 count = static_cast<gp::Int32>(m_suggestions.size());
    const gp::Int32 visibleRows = (count < kMaxVisibleSuggestionRows) ? count : kMaxVisibleSuggestionRows;
    // Table rows are one text line plus cell padding; the argument hint adds a detail line (type / default / set-by).
    const gp::Float32 contentHeight =
        m_isArgumentHint ? (ImGui::GetTextLineHeight() * 2.0f + style.ItemSpacing.y) : ImGui::GetTextLineHeight();
    const gp::Float32 rowHeight = contentHeight + style.CellPadding.y * 2.0f;
    const gp::Float32 overlayHeight = static_cast<gp::Float32>(visibleRows) * rowHeight + style.WindowPadding.y * 2.0f;
    const gp::Float32 availableWidth = ImGui::GetContentRegionAvail().x;
    const gp::Float32 maxWidth = kMaxSuggestionWidth * style.FontScaleDpi;
    const gp::Float32 overlayWidth = (availableWidth < maxWidth) ? availableWidth : maxWidth;

    // Overlay the log: this child is submitted after the log child, so it is drawn (and hit-tested) on top of it.
    ImGui::SetCursorScreenPos(
        ImVec2(ImGui::GetWindowPos().x + style.WindowPadding.x, inputTopY - overlayHeight - style.ItemSpacing.y)
    );
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_PopupBg));
    if (ImGui::BeginChild(
            "##gp_console_suggestions",
            ImVec2(overlayWidth, overlayHeight),
            ImGuiChildFlags_Borders,
            ImGuiWindowFlags_NoFocusOnAppearing
        ))
    {
        const ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg;
        if (ImGui::BeginTable("##gp_console_suggestion_table", 3, tableFlags))
        {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.34f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.16f);
            ImGui::TableSetupColumn("Help", ImGuiTableColumnFlags_WidthStretch, 0.50f);
            for (gp::Int32 index = 0; index < count; ++index)
            {
                drawSuggestionRow(index);
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void ImGuiConsole::drawSuggestionRow(const gp::Int32 index) noexcept
{
    const ImGuiConsoleSuggestion& suggestion = m_suggestions[static_cast<gp::USize>(index)];
    const bool isSelected = (index == m_selectedSuggestion);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::PushID(index);

    // Registry names are views, not guaranteed to be null-terminated; ImGui labels need a C string.
    char label[128]{};
    const gp::USize labelLength =
        (suggestion.name.size() < sizeof(label) - 1u) ? suggestion.name.size() : (sizeof(label) - 1u);
    std::memcpy(label, suggestion.name.data(), labelLength);
    label[labelLength] = '\0';

    const bool isCommand = suggestion.kind == ImGuiConsoleSuggestionKind::Command;
    if (isCommand)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.80f, 0.45f, 1.0f));
    }
    if (ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_SpanAllColumns))
    {
        m_selectedSuggestion = index;
        (void)acceptSuggestion();
        m_shouldFocusInput = true;
    }
    if (isCommand)
    {
        ImGui::PopStyleColor();
    }
    if (isSelected && m_shouldScrollToSelection)
    {
        ImGui::SetScrollHereY(0.5f);
        m_shouldScrollToSelection = false;
    }

    ImGui::TableNextColumn();
    const gp::console::IConsoleVariable* const variable =
        (suggestion.object != nullptr) ? suggestion.object->asVariable() : nullptr;
    if (variable != nullptr)
    {
        const gp::console::ConsoleString value = variable->getString();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.85f, 0.55f, 1.0f));
        ImGui::TextUnformatted(value.data(), value.data() + value.size());
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::TextDisabled("command");
    }

    ImGui::TableNextColumn();
    const gp::StringView help = (suggestion.object != nullptr) ? suggestion.object->getHelp() : gp::StringView();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextUnformatted(help.data(), help.data() + help.size());
    ImGui::PopStyleColor();

    if (m_isArgumentHint && variable != nullptr)
    {
        const gp::console::ConsoleString defaultValue = variable->getDefaultString();
        ImGui::TextDisabled(
            "%s | default %.*s | set by %s",
            gp::console::varTypeToString(variable->getType()),
            static_cast<int>(defaultValue.size()),
            defaultValue.data(),
            gp::console::setByToString(variable->getSetBy())
        );
    }

    ImGui::PopID();
}

int ImGuiConsole::inputTextCallback(ImGuiInputTextCallbackData* const data) noexcept
{
    ImGuiConsole* const self = static_cast<ImGuiConsole*>(data->UserData);

    switch (data->EventFlag)
    {
    case ImGuiInputTextFlags_CallbackCharFilter:
        // The toggle key also types a character; drop it so closing/opening never leaves a stray '`' / '²'.
        if (self->m_desc.handleToggleKey &&
            (data->EventChar == kGraveCharacter || data->EventChar == kSuperscriptTwoCharacter))
        {
            return 1;
        }
        return 0;
    case ImGuiInputTextFlags_CallbackEdit:
    {
        // User typed: `data->Buf` is ImGui's live edit copy. Mirror it and re-filter (no sync back needed).
        const gp::USize length = (static_cast<gp::USize>(data->BufTextLen) < kInputCapacity - 1u)
                                     ? static_cast<gp::USize>(data->BufTextLen)
                                     : (kInputCapacity - 1u);
        std::memmove(self->m_input, data->Buf, length);
        self->m_input[length] = '\0';
        self->m_historyCursor = kNoSelection;
        self->refreshSuggestions();
        return 0;
    }
    case ImGuiInputTextFlags_CallbackCompletion:
        (void)self->acceptSuggestion();
        break;
    case ImGuiInputTextFlags_CallbackHistory:
    {
        const gp::Int32 delta = (data->EventKey == ImGuiKey_UpArrow) ? -1 : 1;
        if (!self->m_isArgumentHint && !self->m_suggestions.isEmpty())
        {
            (void)self->moveSuggestionSelection(delta);
        }
        else
        {
            (void)self->navigateHistory(delta);
        }
        break;
    }
    default:
        break;
    }

    // While the widget is active ImGui edits a private copy of the text, so programmatic changes (completion,
    // browsing, history, clicks) must be pushed into that copy explicitly.
    if (self->m_hasPendingBufferSync)
    {
        char snapshot[kInputCapacity];
        const gp::USize length = Traits::length(self->m_input);
        std::memcpy(snapshot, self->m_input, length + 1u);

        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, snapshot, snapshot + length);
        data->CursorPos = static_cast<int>(length);
        data->SelectionStart = data->CursorPos;
        data->SelectionEnd = data->CursorPos;
        self->m_hasPendingBufferSync = false;
    }
    return 0;
}

}   // namespace gp::imgui
