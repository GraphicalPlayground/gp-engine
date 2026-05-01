// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "errors/ErrorRecord.hpp"
#include "errors/ErrorSeverity.hpp"

namespace gp::error
{

void ErrorRecord::addMetadata(gp::String key, gp::String value)
{
    metadata.pushBack({ std::move(key), std::move(value) });
}

GP_NODISCARD gp::StringView ErrorRecord::getMetadata(gp::StringView key) const noexcept
{
    for (const auto& entry: metadata)
    {
        if (entry.key == key)
        {
            return entry.value;
        }
    }
    return {};
}

GP_NODISCARD bool ErrorRecord::hasCause() const noexcept
{
    return cause != nullptr;
}

GP_NODISCARD gp::USize ErrorRecord::causeDepth() const noexcept
{
    gp::USize depth = 0;
    const ErrorRecord* current = cause.get();
    while (current)
    {
        ++depth;
        current = current->cause.get();
    }
    return depth;
}

GP_NODISCARD gp::String ErrorRecord::summary() const
{
    return gp::String::format("[{}] {}", getSeverityDisplay(severity), message);
}

GP_NODISCARD gp::String ErrorRecord::fullReport() const
{
    // TODO: implement a detailed multi-line report of the error record, including stacktrace and cause chain.
    return {};
}

}   // namespace gp::error
