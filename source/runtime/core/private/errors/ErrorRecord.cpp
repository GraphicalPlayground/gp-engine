// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "errors/ErrorRecord.hpp"
#include "errors/ErrorCode.hpp"
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
    return gp::String::format(
        "[{}][{}:0x{:04X}] {}  ({}:{})",
        getSeverityName(severity),
        getDomainName(code.domain()),
        code.code(),
        message,
        location.file_name(),
        location.line()
    );
}

GP_NODISCARD gp::String ErrorRecord::fullReport() const
{
    gp::String out;
    out.reserve(2048);

    out += "╔══ GP ErrorRecord ═══════════════════════════════════════════╗\n";
    out += gp::String::format("║  Severity  : {}\n", getSeverityDisplay(severity));
    out += gp::String::format("║  Code      : [{}] 0x{:04X}\n", getDomainName(code.domain()), code.code());
    out += gp::String::format("║  Message   : {}\n", message);
    out += gp::String::format(
        "║  Location  : {}:{} in {}\n", location.file_name(), location.line(), location.function_name()
    );
    out += gp::String::format(
        "║  Thread    : {} ({})\n",
        threadName.isEmpty() ? "unnamed" : threadName,
        [&]
    {
        auto id = threadId;
        std::hash<std::thread::id> hashThreadId;
        return gp::String::format("{:#x}", hashThreadId(id));
    }()
    );

    auto tt = std::chrono::system_clock::to_time_t(wallTime);
    char tbuf[64]{};
    struct tm tm{};
#if defined(_MSC_VER)
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    strftime(tbuf, sizeof tbuf, "%Y-%m-%d %T UTC", &tm);
    out += gp::String::format("║  Timestamp : {}\n", tbuf);

    if (!metadata.isEmpty())
    {
        out += "║  Meta      :\n";
        for (const auto& m: metadata)
        {
            out += gp::String::format("║    {}  =  {}\n", m.key, m.value);
        }
    }

#if GP_HAS_STACKTRACE
    if (!stacktrace.empty())
    {
        out += "║  Stacktrace:\n";
        for (const auto& frame: stacktrace)
        {
            out += gp::String::format("║    {}\n", std::to_string(frame));
        }
    }
#endif

    if (hasCause())
    {
        out += "║  Caused by :\n";
        const ErrorRecord* c = cause.get();
        int depth = 0;
        while (c && depth < 8)
        {
            out += gp::String::format("║    [{}] {}\n", getSeverityName(c->severity), c->message);
            c = c->cause.get();
            ++depth;
        }
    }

    out += "╚══════════════════════════════════════════════════════════════╝\n";

    return out;
}

}   // namespace gp::error
