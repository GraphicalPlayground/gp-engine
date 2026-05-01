// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "errors/ErrorRegistry.hpp"
#include "container/Optional.hpp"
#include "errors/ErrorCode.hpp"

namespace gp::error
{

ErrorRegistry::ErrorRegistry()
{
    registerBuiltins();
}

void ErrorRegistry::registerCode(ErrorCode code, ErrorEntry entry)
{
    std::lock_guard lock(m_mutex);
    m_table[code.raw()] = std::move(entry);
}

GP_NODISCARD gp::Optional<ErrorEntry> ErrorRegistry::lookup(ErrorCode code) const
{
    std::lock_guard lock(m_mutex);
    auto it = m_table.find(code.raw());
    if (it == m_table.end())
    {
        return gp::nullOpt;
    }
    return it->second;
}

GP_NODISCARD gp::StringView ErrorRegistry::describe(ErrorCode code) const
{
    std::lock_guard lock(m_mutex);
    auto it = m_table.find(code.raw());
    if (it == m_table.end())
    {
        return "<unregistered code>";
    }
    return it->second.description;
}

GP_NODISCARD gp::StringView ErrorRegistry::remediationHint(ErrorCode code) const
{
    std::lock_guard lock(m_mutex);
    auto it = m_table.find(code.raw());
    if (it == m_table.end())
    {
        return {};
    }
    return it->second.remediation;
}

GP_NODISCARD gp::StringView ErrorRegistry::wikiUrl(ErrorCode code) const
{
    std::lock_guard lock(m_mutex);
    auto it = m_table.find(code.raw());
    if (it == m_table.end())
    {
        return {};
    }
    return it->second.wikiUrl;
}

GP_NODISCARD bool ErrorRegistry::isExpected(ErrorCode code) const
{
    std::lock_guard lock(m_mutex);
    auto it = m_table.find(code.raw());
    return it != m_table.end() && it->second.isExpected;
}

GP_NODISCARD bool ErrorRegistry::isAlwaysFatal(ErrorCode code) const
{
    std::lock_guard lock(m_mutex);
    auto it = m_table.find(code.raw());
    return it != m_table.end() && it->second.isAlwaysFatal;
}

GP_NODISCARD gp::USize ErrorRegistry::size() const
{
    std::lock_guard lock(m_mutex);
    return m_table.size();
}

GP_NODISCARD gp::String ErrorRegistry::dumpAll() const
{
    std::lock_guard lock(m_mutex);
    gp::String out;
    out.reserve(m_table.size() * 128);
    out += gp::String::format("ErrorRegistry - {} entries\n", m_table.size());
    out += "--------------------------------\n";
    for (const auto& [raw, entry]: m_table)
    {
        ErrorCode code{ raw };
        out += gp::String::format(
            "  [{:>12}][0x{:04X}]  {}\n", getDomainName(code.domain()), code.code(), entry.description
        );
        if (!entry.remediation.isEmpty())
        {
            out += gp::String::format("    -> {}\n", entry.remediation);
        }
        if (!entry.wikiUrl.isEmpty())
        {
            out += gp::String::format("    See: {}\n", entry.wikiUrl);
        }
        if (entry.isExpected)
        {
            out += "    (expected - not a bug)\n";
        }
        if (entry.isAlwaysFatal)
        {
            out += "    [!] always fatal\n";
        }
    }
    return out;
}

}   // namespace gp::error
