// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::log
{

enum class LogLevel : gp::UInt8;
enum class LogExecutionMode : gp::UInt8;
enum class LogOverflowPolicy : gp::UInt8;
enum class LogSinkFlags : gp::UInt8;
enum class [[nodiscard]] LogResult : gp::UInt8;

struct LogSourceLocation;
struct LogRecordView;
struct LogEntry;
struct LoggerDesc;
struct ConsoleSinkDesc;
struct FileSinkDesc;

class LogCategory;
class ILogSink;
class ConsoleSink;
class FileSink;
class DebugOutputSink;
class TracySink;
class MemoryRingSink;
class Logger;

}   // namespace gp::log
