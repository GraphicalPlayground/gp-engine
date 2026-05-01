// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

namespace gp::error
{

// Configurations

struct StacktraceConfig;
struct BreakpointConfig;
struct AbortConfig;
struct FilterConfig;
struct ThreadConfig;
struct ErrorSystemConfig;

// Sinks
class Sink;
class AbortSink;
class BreakpointSink;
class ConsoleSink;
class DeferredSink;
class FileSink;
class MultiSink;
class RateLimitedSink;
class TelemetrySink;

// Forward declarations

class ErrorCode;
class ErrorRecord;
class ErrorSystem;
class ErrorContext;
struct ContextFrame;
class ContextScope;

}   // namespace gp::error
