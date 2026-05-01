// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "errors/ErrorSink.hpp"

namespace gp::error
{

void Sink::flush()
{}

GP_NODISCARD Severity Sink::minSeverity() const noexcept
{
    return m_minSeverity;
}

void Sink::setMinSeverity(Severity severity) noexcept
{
    m_minSeverity = severity;
}

void Sink::addDomainFilter(Domain domain)
{
    m_domainFilter.pushBack(domain);
}

void Sink::clearDomainFilter()
{
    m_domainFilter.clear();
}

GP_NODISCARD const gp::String& Sink::name() const noexcept
{
    return m_name;
}

void Sink::setName(gp::String name)
{
    m_name = std::move(name);
}

void Sink::dispatch(const ErrorRecord& record)
{
    if (record.severity < m_minSeverity)
    {
        return;
    }
    if (!m_domainFilter.isEmpty())
    {
        bool isAllowed = false;
        for (Domain domain: m_domainFilter)
        {
            if (domain == record.code.domain())
            {
                isAllowed = true;
                break;
            }
        }
        if (!isAllowed)
        {
            return;
        }
    }
    onRecord(record);
}

}   // namespace gp::error
