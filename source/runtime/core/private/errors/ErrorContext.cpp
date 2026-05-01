// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "errors/ErrorContext.hpp"
#include "CoreMinimal.hpp"

namespace gp::error
{

void ErrorContext::push(gp::String subsystem, gp::String operation)
{
    m_stack.pushBack({ std::move(subsystem), std::move(operation), {} });
}

void ErrorContext::pop() noexcept
{
    if (!m_stack.isEmpty())
    {
        m_stack.popBack();
    }
}

void ErrorContext::tag(gp::String key, gp::String value)
{
    if (!m_stack.isEmpty())
    {
        m_stack.back().tags.pushBack({ std::move(key), std::move(value) });
    }
}

GP_NODISCARD bool ErrorContext::isEmpty() const noexcept
{
    return m_stack.isEmpty();
}

GP_NODISCARD gp::USize ErrorContext::depth() const noexcept
{
    return m_stack.size();
}

GP_NODISCARD const gp::Vector<ContextFrame>& ErrorContext::frames() const noexcept
{
    return m_stack;
}

GP_NODISCARD gp::StringView ErrorContext::currentSubsystem() const noexcept
{
    return m_stack.isEmpty() ? gp::StringView{} : m_stack.back().subsystem.asView();
}

GP_NODISCARD MetaBag ErrorContext::flatten() const
{
    MetaBag out;

    for (gp::USize i = 0; i < m_stack.size(); ++i)
    {
        const auto& frame = m_stack[i];
        out.pushBack({ gp::String::format("scope[{}].subsystem", i), frame.subsystem });
        out.pushBack({ gp::String::format("scope[{}].operation", i), frame.operation });
        for (const auto& tag: frame.tags)
        {
            out.pushBack({ gp::String::format("scope[{}].{}", i, tag.key), tag.value });
        }
    }

    return out;
}

void ErrorContext::setThreadName(gp::String name)
{
    m_threadName = std::move(name);
}

GP_NODISCARD const gp::String& ErrorContext::threadName() const noexcept
{
    return m_threadName;
}

ContextScope::ContextScope(gp::String subsystem, gp::String operation)
{
    ErrorContext::current().push(std::move(subsystem), std::move(operation));
}

ContextScope::~ContextScope() noexcept
{
    ErrorContext::current().pop();
}

ContextScope& ContextScope::tag(gp::String key, gp::String value)
{
    ErrorContext::current().tag(std::move(key), std::move(value));
    return *this;
}

}   // namespace gp::error
