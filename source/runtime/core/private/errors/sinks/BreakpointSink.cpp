// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "errors/sinks/BreakpointSink.hpp"

namespace gp::error
{

BreakpointSink::BreakpointSink(Severity breakAt)
    : m_breakAt(breakAt)
{
    setName("BreakpointSink");
    setMinSeverity(breakAt);
}

void BreakpointSink::onRecord(const ErrorRecord& record)
{
    if (record.severity >= m_breakAt)
    {
        GP_DEBUGBREAK();
    }
}

}   // namespace gp::error
