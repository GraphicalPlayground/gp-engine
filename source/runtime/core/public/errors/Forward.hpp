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
struct CrashReportConfig;

// Sinks
class Sink;
class AbortSink;
class BreakpointSink;
class ConsoleSink;
class CallbackSink;
class DeferredSink;
class FileSink;
class MultiSink;
class RateLimitedSink;
class TelemetrySink;
class FilteredSink;
class SplitterSink;
class BroadcastSink;
class RingBufferSink;
class BinarySink;
class CrashReporter;

// Forward declarations

struct MetaEntry;
class ErrorCode;
class ErrorRecord;
class ErrorContext;
struct ContextFrame;
class ContextScope;
struct ErrorStatistics;
class ErrorSystem;
struct ErrorEntry;
class ErrorRegistry;
class FilterPredicate;
class ErrorScope;
class ErrorGuard;

}   // namespace gp::error
