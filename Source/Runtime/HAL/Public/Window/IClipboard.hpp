// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/ContainerFwd.hpp"
#include "Container/Core/Optional.hpp"
#include "Container/String/String.hpp"
#include "Container/Views/StringView.hpp"

namespace GP
{

/// @brief Interface for clipboard operations, allowing applications to interact with the system clipboard for text
///        data.
class IClipboard
{
public:
    virtual ~IClipboard() = default;

public:
    /// @brief Sets the clipboard content to the specified text.
    /// @param text The text to set in the clipboard.
    /// @return True if the operation was successful, false otherwise.
    virtual bool SetText(FStringView text) noexcept = 0;

    /// @brief Retrieves the current text content from the clipboard.
    /// @return An optional string containing the clipboard text if available, or std::nullopt if not.
    virtual TOptional<FString> GetText() const noexcept = 0;

    /// @brief Clears the clipboard content.
    /// @return True if the operation was successful, false otherwise.
    virtual bool Clear() noexcept = 0;

    /// @brief Checks if the clipboard currently has text content.
    /// @return True if the clipboard has text content, false otherwise.
    virtual bool HasText() const noexcept = 0;
};

}   // namespace GP
