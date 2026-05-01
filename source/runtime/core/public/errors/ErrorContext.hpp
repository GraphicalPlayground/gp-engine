// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "container/BasicString.hpp"       // IWYU pragma: keep
#include "container/BasicStringView.hpp"   // IWYU pragma: keep
#include "container/Forward.hpp"
#include "container/Vector.hpp"
#include "CoreMinimal.hpp"
#include "errors/ErrorRecord.hpp"

namespace gp::error
{

/// @brief Represents a single frame in the error context stack.
struct ContextFrame
{
    gp::String subsystem;   //<! e.g. "Renderer", "Audio"
    gp::String operation;   //<! e.g. "LoadTexture", "OpenDevice"
    MetaBag tags;           //<! Extra key/value pairs attached to this scope
};

/// @brief Thread-local context registry for error reporting.  Each thread has its own independent stack of
/// ContextFrames, which can be used to provide rich contextual information when an error occurs.  This is a singleton
/// class; use ErrorContext::current() to access the instance for the current thread.
class ErrorContext
{
private:
    gp::Vector<ContextFrame> m_stack;
    gp::String m_threadName;

private:
    /// @brief Private constructor to enforce singleton pattern. Use ErrorContext::current() to access the instance.
    ErrorContext() = default;

public:
    /// @brief Delete copy constructor and assignment operator to enforce singleton pattern.
    ErrorContext(const ErrorContext&) = delete;
    ErrorContext& operator=(const ErrorContext&) = delete;

public:
    /// @brief Push a new frame onto the stack, describing the current subsystem and operation.
    /// @param[in] subsystem Typically a high-level module name, e.g. "Renderer", "Audio"
    /// @param[in] operation Typically a specific action, e.g. "LoadTexture", "OpenDevice"
    void push(gp::String subsystem, gp::String operation);

    /// @brief Pop the most recent frame from the stack.  No-op if stack is empty.
    void pop() noexcept;

    /// @brief Attach a key/value tag to the *top* frame.  No-op if stack is empty.
    /// @param[in] key  A short identifier, e.g. "filename", "error_code"
    /// @param[in] value A string representation of the value, e.g. "assets/texture.png", "0xDEADBEEF"
    void tag(gp::String key, gp::String value);

    /// @brief Check if the stack is empty (i.e. no context frames).
    /// @return True if the stack is empty, false otherwise.
    GP_NODISCARD bool isEmpty() const noexcept;

    /// @brief Get the current depth of the stack (i.e. number of context frames).
    /// @return The number of frames currently on the stack.
    GP_NODISCARD gp::USize depth() const noexcept;

    /// @brief Get a const reference to the current stack of context frames.
    /// @return A const reference to the vector of context frames.  May be empty if no frames have been pushed.
    GP_NODISCARD const gp::Vector<ContextFrame>& frames() const noexcept;

    /// @brief Get the subsystem of the current (top) frame, or an empty string if the stack is empty.
    /// @return The subsystem of the current frame, or an empty string if the stack is empty.
    GP_NODISCARD gp::StringView currentSubsystem() const noexcept;

    /// @brief Get the flattened context as a MetaBag, suitable for attaching to an ErrorRecord.  This will include all
    /// frames and tags, with keys formatted as "scope[i].subsystem", "scope[i].operation", and "scope[i].tagkey".
    /// @return A MetaBag containing all context information from the stack, flattened into key/value pairs.
    GP_NODISCARD MetaBag flatten() const;

    /// @brief Set the name of the current thread (for debugging purposes).
    /// @note
    /// This is purely informational and does not affect thread behavior.
    /// It can be used to make error reports more readable by indicating which thread they came from.
    /// @param[in] name A human-readable name for the thread, e.g. "MainThread", "WorkerThread1".
    void setThreadName(gp::String name);

    /// @brief Get the name of the current thread.
    /// @return The name of the current thread, or an empty string if no name has been set.
    GP_NODISCARD const gp::String& threadName() const noexcept;

public:
    /// @brief Get the current error context instance.
    /// @note This is a thread-local singleton; each thread has its own independent context stack.
    /// @return A reference to the current error context instance.
    static ErrorContext& current() noexcept
    {
        thread_local ErrorContext instance;
        return instance;
    }
};

/// @brief RAII guard that pushes a new context frame on construction and pops it on destruction.
/// This is intended to be used with the GP_ERROR_SCOPE macro for convenient scoping of error contexts.
class ContextScope
{
public:
    /// @brief Construct a new ContextScope, pushing a new frame onto the error context stack.  The frame will be popped
    /// automatically when this object is destroyed (e.g. at the end of the enclosing block).
    /// @param[in] subsystem The subsystem name for this scope, e.g. "Renderer", "Audio"
    /// @param[in] operation The operation name for this scope, e.g. "LoadTexture", "OpenDevice"
    explicit ContextScope(gp::String subsystem, gp::String operation);

    /// @brief Destructor that pops the context frame from the stack.
    ~ContextScope() noexcept;

    // Non-copyable, non-movable (RAII identity must be stable)
    ContextScope(const ContextScope&) = delete;
    ContextScope& operator=(const ContextScope&) = delete;
    ContextScope(ContextScope&&) = delete;
    ContextScope& operator=(ContextScope&&) = delete;

public:
    /// @brief Attach a key/value tag to this scope.  Chainable.
    /// @param[in] key A short identifier for the tag, e.g. "filename", "error_code"
    /// @param[in] value A string representation of the value, e.g. "assets/texture.png", "0xDEADBEEF"
    /// @return A reference to this ContextScope, allowing for chaining multiple tags.
    ContextScope& tag(gp::String key, gp::String value);
};

}   // namespace gp::error

/// @brief Create a new error scope with the given subsystem and operation.
/// @param[in] subsystem The subsystem name for this scope, e.g. "Renderer", "Audio"
/// @param[in] operation The operation name for this scope, e.g. "LoadTexture", "OpenDevice", etc.
#define GP_ERROR_SCOPE(subsystem_, op_) \
    ::gp::error::ContextScope GP_CONCAT(gp_err_scope_, __LINE__){ (subsystem_), (op_) }

/// @brief Attach a key/value tag to the innermost active scope on this thread.
/// @param[in] key A short identifier for the tag, e.g. "filename", "error_code"
/// @param[in] value A string representation of the value, e.g. "assets/texture.png", "0xDEADBEEF", etc.
#define GP_ERROR_TAG(key_, value_) \
    ::gp::error::ErrorContext::current().tag((key_), gp::String::format("{}", (value_)))

/// @brief Set the name of the current thread (for debugging purposes).
/// @param[in] name A human-readable name for the thread, e.g. "MainThread", "WorkerThread1", etc.
#define GP_THREAD_NAME(name_) \
    ::gp::error::ErrorContext::current().setThreadName(name_)
