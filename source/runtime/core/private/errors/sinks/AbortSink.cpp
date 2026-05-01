// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "errors/sinks/AbortSink.hpp"

namespace gp::error
{

AbortSink::AbortSink(Severity abortAt, Mode mode)
    : m_abortAt(abortAt)
    , m_mode(mode)
{
    setName("AbortSink");
    setMinSeverity(abortAt);
}

void AbortSink::onRecord(const ErrorRecord& record)
{
    if (record.severity >= m_abortAt)
    {
        std::fflush(stderr);
        switch (m_mode)
        {
        case Mode::Abort:
            std::abort();
            break;
        case Mode::Terminate:
            std::terminate();
            break;
        }
    }
}

}   // namespace gp::error
