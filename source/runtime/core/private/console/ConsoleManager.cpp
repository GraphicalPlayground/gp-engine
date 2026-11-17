// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "console/ConsoleManager.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleTypes.hpp"
#include "console/IConsoleManager.hpp"
#include "containers/strings/StringEnums.hpp"
#include "hashing/Fnv1a.hpp"
#include "hashing/HashingEnums.hpp"
#include "logging/Logger.hpp"   // IWYU pragma: keep
#include "logging/LogMacros.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "platforms/base/system/Thread.hpp"
#include "profiling/Profiler.hpp"
#include "profiling/ProfilingControl.hpp"
#include "templates/ScopeExit.hpp"
#include <atomic>
#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>
#include <system_error>
#include <utility>

namespace gp::console
{

namespace
{

GP_DEFINE_LOG_CATEGORY_INLINE(LogConsole, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);

constexpr gp::UInt64 kDeferredSinkBit = 0x8000000000000000ULL;

struct DeferredObjectQueue final
{
    gp::thread::AtomicFlag lock{};
    internal::ConsoleObjectBase* head{ nullptr };
};

struct DeferredSinkEntry final
{
    ConsoleSinkDelegate callback{};
    gp::DelegateHandle externalHandle{};
    gp::DelegateHandle liveHandle{};
    gp::UInt32 generation{ 1u };
    bool active{ false };
    bool migrated{ false };
};

struct DeferredSinkRegistry final
{
    gp::thread::AtomicFlag lock{};
    DeferredSinkEntry entries[kMaxConsoleSinks]{};
};

// Constant-initialized in .bss before any dynamic static-init constructor runs across translation units.
constinit DeferredObjectQueue g_deferredObjects{};
constinit DeferredSinkRegistry g_deferredSinks{};
constinit gp::thread::Atomic<bool> g_globalInitialized{ false };
constinit gp::thread::Atomic<bool> g_globalShuttingDown{ false };

using GlobalConsoleManagerStorage = BasicConsoleManager<
    kDefaultConsoleHashCapacity,
    kMaxDynamicConsoleVariables,
    kMaxDynamicConsoleCommands,
    kMaxConsoleSinks>;

GlobalConsoleManagerStorage& globalConsoleManagerInstance() noexcept
{
    static GlobalConsoleManagerStorage s_instance{};
    return s_instance;
}

void acquireFlagLock(gp::thread::AtomicFlag& flag) noexcept
{
    while (flag.testAndSet(gp::MemoryOrder::Acquire))
    {
        gp::platform::Thread::spinWait();
    }
}

void releaseFlagLock(gp::thread::AtomicFlag& flag) noexcept
{
    flag.clear(gp::MemoryOrder::Release);
}

void emitConsoleTraceBookmark(
    const gp::StringView prefix, const gp::StringView name, const gp::StringView valueText = {}
) noexcept
{
    if (!gp::profiling::ProfilingControl::isConsoleTraceEnabled())
    {
        return;
    }

    gp::container::BasicFixedString<char, 255u> msg{};
    msg.append(prefix);
    const gp::USize remName = (msg.capacity() > msg.size()) ? (msg.capacity() - msg.size()) : 0u;
    msg.append(name.substr(0u, remName));
    if (!valueText.isEmpty() && msg.size() + 3u < msg.capacity())
    {
        msg.append(" = ");
        const gp::USize remVal = msg.capacity() - msg.size();
        msg.append(valueText.substr(0u, remVal));
    }
    gp::profiling::ProfilingControl::emitBookmark(msg.view(), GP_U32_COLOR_YELLOW);
}

[[nodiscard]] bool isWhitespaceChar(const char ch) noexcept
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

[[nodiscard]] gp::StringView stripOuterQuotes(const gp::StringView value) noexcept
{
    if (value.size() >= 2u)
    {
        const char first = value[0u];
        const char last = value[value.size() - 1u];
        if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
        {
            return value.substr(1u, value.size() - 2u);
        }
    }
    return value;
}

[[nodiscard]] bool
    splitKeyValueToken(const gp::StringView token, gp::StringView& outKey, gp::StringView& outValue) noexcept
{
    const gp::USize eqPos = token.find('=');
    const gp::USize colonPos = token.find(':');
    gp::USize sepPos = gp::StringView::npos;

    if (eqPos != gp::StringView::npos && colonPos != gp::StringView::npos)
    {
        sepPos = (eqPos < colonPos) ? eqPos : colonPos;
    }
    else if (eqPos != gp::StringView::npos)
    {
        sepPos = eqPos;
    }
    else if (colonPos != gp::StringView::npos)
    {
        sepPos = colonPos;
    }

    if (sepPos == gp::StringView::npos || sepPos == 0u)
    {
        return false;
    }

    outKey = token.substr(0u, sepPos);
    outValue = stripOuterQuotes(token.substr(sepPos + 1u));
    return true;
}

[[nodiscard]] bool payloadToBool(const internal::ConsoleVarPayload& payload) noexcept
{
    switch (payload.type)
    {
    case ConsoleVarType::Bool:
        return payload.boolValue;
    case ConsoleVarType::Int32:
        return payload.intValue != 0;
    case ConsoleVarType::UInt32:
        return payload.uintValue != 0u;
    case ConsoleVarType::Float32:
        return payload.floatValue != 0.0f;
    case ConsoleVarType::Float64:
        return payload.doubleValue != 0.0;
    case ConsoleVarType::String:
        return parseConsoleBool(payload.stringValue.view()).valueOr(false);
    default:
        return false;
    }
}

[[nodiscard]] gp::Int32 payloadToInt32(const internal::ConsoleVarPayload& payload) noexcept
{
    switch (payload.type)
    {
    case ConsoleVarType::Bool:
        return payload.boolValue ? 1 : 0;
    case ConsoleVarType::Int32:
        return payload.intValue;
    case ConsoleVarType::UInt32:
    {
        constexpr gp::UInt32 kMaxInt32 = static_cast<gp::UInt32>(std::numeric_limits<gp::Int32>::max());
        return (payload.uintValue > kMaxInt32) ? std::numeric_limits<gp::Int32>::max()
                                               : static_cast<gp::Int32>(payload.uintValue);
    }
    case ConsoleVarType::Float32:
        return static_cast<gp::Int32>(payload.floatValue);
    case ConsoleVarType::Float64:
        return static_cast<gp::Int32>(payload.doubleValue);
    case ConsoleVarType::String:
        return parseConsoleInt32(payload.stringValue.view()).valueOr(0);
    default:
        return 0;
    }
}

[[nodiscard]] gp::UInt32 payloadToUInt32(const internal::ConsoleVarPayload& payload) noexcept
{
    switch (payload.type)
    {
    case ConsoleVarType::Bool:
        return payload.boolValue ? 1u : 0u;
    case ConsoleVarType::Int32:
        return payload.intValue >= 0 ? static_cast<gp::UInt32>(payload.intValue) : 0u;
    case ConsoleVarType::UInt32:
        return payload.uintValue;
    case ConsoleVarType::Float32:
        return payload.floatValue >= 0.0f ? static_cast<gp::UInt32>(payload.floatValue) : 0u;
    case ConsoleVarType::Float64:
        return payload.doubleValue >= 0.0 ? static_cast<gp::UInt32>(payload.doubleValue) : 0u;
    case ConsoleVarType::String:
        return parseConsoleUInt32(payload.stringValue.view()).valueOr(0u);
    default:
        return 0u;
    }
}

[[nodiscard]] gp::Float32 payloadToFloat32(const internal::ConsoleVarPayload& payload) noexcept
{
    switch (payload.type)
    {
    case ConsoleVarType::Bool:
        return payload.boolValue ? 1.0f : 0.0f;
    case ConsoleVarType::Int32:
        return static_cast<gp::Float32>(payload.intValue);
    case ConsoleVarType::UInt32:
        return static_cast<gp::Float32>(payload.uintValue);
    case ConsoleVarType::Float32:
        return payload.floatValue;
    case ConsoleVarType::Float64:
        return static_cast<gp::Float32>(payload.doubleValue);
    case ConsoleVarType::String:
        return parseConsoleFloat32(payload.stringValue.view()).valueOr(0.0f);
    default:
        return 0.0f;
    }
}

[[nodiscard]] gp::Float64 payloadToFloat64(const internal::ConsoleVarPayload& payload) noexcept
{
    switch (payload.type)
    {
    case ConsoleVarType::Bool:
        return payload.boolValue ? 1.0 : 0.0;
    case ConsoleVarType::Int32:
        return static_cast<gp::Float64>(payload.intValue);
    case ConsoleVarType::UInt32:
        return static_cast<gp::Float64>(payload.uintValue);
    case ConsoleVarType::Float32:
        return static_cast<gp::Float64>(payload.floatValue);
    case ConsoleVarType::Float64:
        return payload.doubleValue;
    case ConsoleVarType::String:
        return parseConsoleFloat64(payload.stringValue.view()).valueOr(0.0);
    default:
        return 0.0;
    }
}

[[nodiscard]] ConsoleString payloadToString(const internal::ConsoleVarPayload& payload) noexcept
{
    switch (payload.type)
    {
    case ConsoleVarType::Bool:
        return formatConsoleBool(payload.boolValue);
    case ConsoleVarType::Int32:
        return formatConsoleInt32(payload.intValue);
    case ConsoleVarType::UInt32:
        return formatConsoleUInt32(payload.uintValue);
    case ConsoleVarType::Float32:
        return formatConsoleFloat32(payload.floatValue);
    case ConsoleVarType::Float64:
        return formatConsoleFloat64(payload.doubleValue);
    case ConsoleVarType::String:
        return payload.stringValue;
    default:
        return ConsoleString{};
    }
}

[[nodiscard]] bool
    payloadsEqual(const internal::ConsoleVarPayload& lhs, const internal::ConsoleVarPayload& rhs) noexcept
{
    if (lhs.type != rhs.type)
    {
        return false;
    }

    switch (lhs.type)
    {
    case ConsoleVarType::Bool:
        return lhs.boolValue == rhs.boolValue;
    case ConsoleVarType::Int32:
        return lhs.intValue == rhs.intValue;
    case ConsoleVarType::UInt32:
        return lhs.uintValue == rhs.uintValue;
    case ConsoleVarType::Float32:
        return lhs.floatValue == rhs.floatValue;
    case ConsoleVarType::Float64:
        return lhs.doubleValue == rhs.doubleValue;
    case ConsoleVarType::String:
        return lhs.stringValue.view() == rhs.stringValue.view();
    default:
        return false;
    }
}

void emitConsoleLine(ConsoleOutputDelegate* const output, const gp::StringView line) noexcept
{
    if (output != nullptr && output->isBound())
    {
        output->execute(line);
        return;
    }
    GP_LOG(LogConsole, Info, "{}", line);
}

void formatVariableSummary(
    const IConsoleVariable& variable, gp::container::BasicFixedString<char, 511u>& outLine
) noexcept
{
    const ConsoleString currentVal = variable.getString();
    const ConsoleString defaultVal = variable.getDefaultString();
    outLine.clear();
    outLine.append(variable.getName());
    outLine.append(" = ");
    if (variable.getType() == ConsoleVarType::String)
    {
        outLine.pushBack('"');
        outLine.append(currentVal.view());
        outLine.pushBack('"');
    }
    else
    {
        outLine.append(currentVal.view());
    }
    outLine.append(" [");
    outLine.append(varTypeToString(variable.getType()));
    outLine.append(", default=");
    outLine.append(defaultVal.view());
    outLine.append(", setBy=");
    outLine.append(setByToString(variable.getSetBy()));
    outLine.pushBack(']');
    if (!variable.getHelp().isEmpty())
    {
        outLine.append(" : ");
        const gp::USize rem = (outLine.capacity() > outLine.size()) ? (outLine.capacity() - outLine.size()) : 0u;
        const gp::StringView help = variable.getHelp();
        outLine.append(help.substr(0u, (help.size() < rem) ? help.size() : rem));
    }
}

void enqueueUniqueVariableLocked(
    IConsoleVariable** const queue,
    gp::USize& count,
    const gp::USize capacity,
    bool& overflow,
    IConsoleVariable* const variable
) noexcept
{
    if (overflow)
    {
        return;
    }

    for (gp::USize i = 0u; i < count; ++i)
    {
        if (queue[i] == variable)
        {
            return;
        }
    }

    if (count < capacity)
    {
        queue[count++] = variable;
    }
    else
    {
        overflow = true;
    }
}

}   // namespace

/// @section ConsoleTypes Utilities

ConsoleCommandArgs::ConsoleCommandArgs(
    const gp::StringView rawCommandLine, ConsoleOutputDelegate* const output
) noexcept
{
    (void)parse(rawCommandLine, *this, output);
}

bool ConsoleCommandArgs::parse(
    const gp::StringView rawCommandLine, ConsoleCommandArgs& outArgs, ConsoleOutputDelegate* const output
) noexcept
{
    outArgs.m_commandName = gp::StringView{};
    outArgs.m_rawArgs = gp::StringView{};
    outArgs.m_argCount = 0u;
    outArgs.m_output = output;
    outArgs.m_storage[0] = '\0';
    outArgs.m_rawTail[0] = '\0';

    const gp::StringView trimmed = trimConsoleWhitespace(rawCommandLine);
    if (trimmed.isEmpty() || trimmed.size() > kMaxConsoleCommandLineLength)
    {
        return false;
    }

    gp::USize cursor = 0u;
    gp::USize writePos = 0u;
    gp::USize tokenCount = 0u;
    gp::USize tailStartOffset = trimmed.size();
    bool commandNameDone = false;

    while (cursor < trimmed.size())
    {
        while (cursor < trimmed.size() && isWhitespaceChar(trimmed[cursor]))
        {
            ++cursor;
        }
        if (cursor >= trimmed.size())
        {
            break;
        }

        if (tokenCount >= kMaxConsoleCommandArgs + 1u)
        {
            return false;
        }

        const gp::USize tokenStart = writePos;
        const char quoteChar = (trimmed[cursor] == '"' || trimmed[cursor] == '\'') ? trimmed[cursor] : '\0';
        const bool isQuoted = (quoteChar != '\0');
        if (isQuoted)
        {
            ++cursor;
        }

        bool closedQuote = !isQuoted;
        while (cursor < trimmed.size())
        {
            const char ch = trimmed[cursor];
            if (isQuoted)
            {
                if (ch == '\\' && cursor + 1u < trimmed.size())
                {
                    const char nextCh = trimmed[cursor + 1u];
                    if (nextCh == quoteChar || nextCh == '\\')
                    {
                        if (writePos >= kMaxConsoleCommandLineLength)
                        {
                            return false;
                        }
                        outArgs.m_storage[writePos++] = nextCh;
                        cursor += 2u;
                        continue;
                    }
                }
                if (ch == quoteChar)
                {
                    ++cursor;
                    closedQuote = true;
                    break;
                }
            }
            else if (isWhitespaceChar(ch))
            {
                break;
            }

            if (writePos >= kMaxConsoleCommandLineLength)
            {
                return false;
            }
            outArgs.m_storage[writePos++] = ch;
            ++cursor;
        }

        if (!closedQuote)
        {
            return false;
        }

        const gp::USize tokenLength = writePos - tokenStart;
        if (writePos >= kMaxConsoleCommandLineLength + 1u)
        {
            return false;
        }
        outArgs.m_storage[writePos++] = '\0';

        const gp::StringView tokenView(outArgs.m_storage + tokenStart, tokenLength);
        if (!commandNameDone)
        {
            outArgs.m_commandName = tokenView;
            commandNameDone = true;
            gp::USize afterCmd = cursor;
            while (afterCmd < trimmed.size() && isWhitespaceChar(trimmed[afterCmd]))
            {
                ++afterCmd;
            }
            tailStartOffset = afterCmd;
        }
        else
        {
            outArgs.m_args[outArgs.m_argCount++] = tokenView;
        }
        ++tokenCount;
    }

    if (!commandNameDone || outArgs.m_commandName.isEmpty())
    {
        return false;
    }

    if (tailStartOffset < trimmed.size())
    {
        const gp::USize tailLen = trimmed.size() - tailStartOffset;
        std::memcpy(outArgs.m_rawTail, trimmed.data() + tailStartOffset, tailLen);
        outArgs.m_rawTail[tailLen] = '\0';
        outArgs.m_rawArgs = gp::StringView(outArgs.m_rawTail, tailLen);
    }

    return true;
}

gp::StringView ConsoleCommandArgs::getArg(const gp::USize index) const noexcept
{
    if (index >= m_argCount)
    {
        return gp::StringView{};
    }
    return m_args[index];
}

gp::Optional<bool> ConsoleCommandArgs::tryGetBool(const gp::USize index) const noexcept
{
    if (index >= m_argCount)
    {
        return gp::Optional<bool>{};
    }
    return parseConsoleBool(m_args[index]);
}

gp::Optional<gp::Int32> ConsoleCommandArgs::tryGetInt32(const gp::USize index) const noexcept
{
    if (index >= m_argCount)
    {
        return gp::Optional<gp::Int32>{};
    }
    return parseConsoleInt32(m_args[index]);
}

gp::Optional<gp::UInt32> ConsoleCommandArgs::tryGetUInt32(const gp::USize index) const noexcept
{
    if (index >= m_argCount)
    {
        return gp::Optional<gp::UInt32>{};
    }
    return parseConsoleUInt32(m_args[index]);
}

gp::Optional<gp::Float32> ConsoleCommandArgs::tryGetFloat32(const gp::USize index) const noexcept
{
    if (index >= m_argCount)
    {
        return gp::Optional<gp::Float32>{};
    }
    return parseConsoleFloat32(m_args[index]);
}

gp::Optional<gp::Float64> ConsoleCommandArgs::tryGetFloat64(const gp::USize index) const noexcept
{
    if (index >= m_argCount)
    {
        return gp::Optional<gp::Float64>{};
    }
    return parseConsoleFloat64(m_args[index]);
}

void ConsoleCommandArgs::writeLine(const gp::StringView line) const noexcept
{
    emitConsoleLine(m_output, line);
}

gp::StringView trimConsoleWhitespace(const gp::StringView view) noexcept
{
    return view.trim();
}

gp::UInt64 hashConsoleName(const gp::StringView name) noexcept
{
    if (name.isEmpty())
    {
        return 0ULL;
    }
    return gp::hashing::Fnv1a64::hash(name, gp::hashing::HashCase::IgnoreCase);
}

gp::Optional<bool> parseConsoleBool(const gp::StringView text) noexcept
{
    const gp::StringView trimmed = trimConsoleWhitespace(text);
    if (trimmed.isEmpty())
    {
        return gp::Optional<bool>{};
    }
    if (trimmed.equalsIgnoreCase("true") || trimmed == "1" || trimmed.equalsIgnoreCase("on") ||
        trimmed.equalsIgnoreCase("yes"))
    {
        return gp::Optional<bool>(true);
    }
    if (trimmed.equalsIgnoreCase("false") || trimmed == "0" || trimmed.equalsIgnoreCase("off") ||
        trimmed.equalsIgnoreCase("no"))
    {
        return gp::Optional<bool>(false);
    }
    return gp::Optional<bool>{};
}

gp::Optional<gp::Int32> parseConsoleInt32(const gp::StringView text) noexcept
{
    const gp::StringView trimmed = trimConsoleWhitespace(text);
    if (trimmed.isEmpty())
    {
        return gp::Optional<gp::Int32>{};
    }

    if (trimmed.equalsIgnoreCase("true"))
    {
        return gp::Optional<gp::Int32>(1);
    }
    if (trimmed.equalsIgnoreCase("false"))
    {
        return gp::Optional<gp::Int32>(0);
    }

    bool negative = false;
    gp::StringView body = trimmed;
    if (body[0u] == '+' || body[0u] == '-')
    {
        negative = (body[0u] == '-');
        body.removePrefix(1u);
        if (body.isEmpty() || body[0u] == '+' || body[0u] == '-')
        {
            return gp::Optional<gp::Int32>{};
        }
    }

    if (body.size() > 2u && body[0u] == '0' && (body[1u] == 'x' || body[1u] == 'X'))
    {
        gp::UInt32 hexVal = 0u;
        const char* const begin = body.data() + 2u;
        const char* const end = body.data() + body.size();
        const std::from_chars_result res = std::from_chars(begin, end, hexVal, 16);
        if (res.ec != std::errc{} || res.ptr != end)
        {
            return gp::Optional<gp::Int32>{};
        }
        if (!negative)
        {
            if (hexVal <= static_cast<gp::UInt32>(std::numeric_limits<gp::Int32>::max()))
            {
                return gp::Optional<gp::Int32>(static_cast<gp::Int32>(hexVal));
            }
            return gp::Optional<gp::Int32>{};
        }
        if (hexVal <= static_cast<gp::UInt32>(std::numeric_limits<gp::Int32>::max()))
        {
            return gp::Optional<gp::Int32>(-static_cast<gp::Int32>(hexVal));
        }
        if (hexVal == 0x80000000u)
        {
            return gp::Optional<gp::Int32>(std::numeric_limits<gp::Int32>::min());
        }
        return gp::Optional<gp::Int32>{};
    }

    const gp::StringView decimalSlice = negative ? trimmed : body;
    gp::Int32 value = 0;
    const char* const begin = decimalSlice.data();
    const char* const end = decimalSlice.data() + decimalSlice.size();
    const std::from_chars_result res = std::from_chars(begin, end, value, 10);
    if (res.ec == std::errc{} && res.ptr == end)
    {
        return gp::Optional<gp::Int32>(value);
    }
    return gp::Optional<gp::Int32>{};
}

gp::Optional<gp::UInt32> parseConsoleUInt32(const gp::StringView text) noexcept
{
    gp::StringView trimmed = trimConsoleWhitespace(text);
    if (trimmed.isEmpty() || trimmed[0u] == '-')
    {
        return gp::Optional<gp::UInt32>{};
    }

    if (trimmed.equalsIgnoreCase("true"))
    {
        return gp::Optional<gp::UInt32>(1u);
    }
    if (trimmed.equalsIgnoreCase("false"))
    {
        return gp::Optional<gp::UInt32>(0u);
    }

    if (trimmed[0u] == '+')
    {
        trimmed.removePrefix(1u);
        if (trimmed.isEmpty() || trimmed[0u] == '-' || trimmed[0u] == '+')
        {
            return gp::Optional<gp::UInt32>{};
        }
    }

    gp::Int32 base = 10;
    if (trimmed.size() > 2u && trimmed[0u] == '0' && (trimmed[1u] == 'x' || trimmed[1u] == 'X'))
    {
        base = 16;
        trimmed.removePrefix(2u);
        if (trimmed.isEmpty())
        {
            return gp::Optional<gp::UInt32>{};
        }
    }

    gp::UInt32 value = 0u;
    const char* const begin = trimmed.data();
    const char* const end = trimmed.data() + trimmed.size();
    const std::from_chars_result res = std::from_chars(begin, end, value, base);
    if (res.ec == std::errc{} && res.ptr == end)
    {
        return gp::Optional<gp::UInt32>(value);
    }
    return gp::Optional<gp::UInt32>{};
}

gp::Optional<gp::Float32> parseConsoleFloat32(const gp::StringView text) noexcept
{
    gp::StringView trimmed = trimConsoleWhitespace(text);
    if (trimmed.isEmpty())
    {
        return gp::Optional<gp::Float32>{};
    }

    if (trimmed[0u] == '+')
    {
        trimmed.removePrefix(1u);
        if (trimmed.isEmpty())
        {
            return gp::Optional<gp::Float32>{};
        }
    }

    if (trimmed.size() > 1u && (trimmed.back() == 'f' || trimmed.back() == 'F'))
    {
        trimmed.removeSuffix(1u);
    }

    gp::Float32 value = 0.0f;
    const char* const begin = trimmed.data();
    const char* const end = trimmed.data() + trimmed.size();
    const std::from_chars_result res = std::from_chars(begin, end, value);
    if (res.ec == std::errc{} && res.ptr == end && std::isfinite(value))
    {
        return gp::Optional<gp::Float32>(value);
    }
    return gp::Optional<gp::Float32>{};
}

gp::Optional<gp::Float64> parseConsoleFloat64(const gp::StringView text) noexcept
{
    gp::StringView trimmed = trimConsoleWhitespace(text);
    if (trimmed.isEmpty())
    {
        return gp::Optional<gp::Float64>{};
    }

    if (trimmed[0u] == '+')
    {
        trimmed.removePrefix(1u);
        if (trimmed.isEmpty())
        {
            return gp::Optional<gp::Float64>{};
        }
    }

    gp::Float64 value = 0.0;
    const char* const begin = trimmed.data();
    const char* const end = trimmed.data() + trimmed.size();
    const std::from_chars_result res = std::from_chars(begin, end, value);
    if (res.ec == std::errc{} && res.ptr == end && std::isfinite(value))
    {
        return gp::Optional<gp::Float64>(value);
    }
    return gp::Optional<gp::Float64>{};
}

ConsoleString formatConsoleBool(const bool value) noexcept
{
    return ConsoleString(value ? "true" : "false");
}

ConsoleString formatConsoleInt32(const gp::Int32 value) noexcept
{
    char buf[32]{};
    const std::to_chars_result res = std::to_chars(buf, buf + sizeof(buf), value, 10);
    if (res.ec == std::errc{})
    {
        return ConsoleString(buf, static_cast<gp::USize>(res.ptr - buf));
    }
    return ConsoleString("0");
}

ConsoleString formatConsoleUInt32(const gp::UInt32 value) noexcept
{
    char buf[32]{};
    const std::to_chars_result res = std::to_chars(buf, buf + sizeof(buf), value, 10);
    if (res.ec == std::errc{})
    {
        return ConsoleString(buf, static_cast<gp::USize>(res.ptr - buf));
    }
    return ConsoleString("0");
}

ConsoleString formatConsoleFloat32(const gp::Float32 value) noexcept
{
    if (!std::isfinite(value))
    {
        return ConsoleString("0");
    }
    char buf[64]{};
    const std::to_chars_result res = std::to_chars(buf, buf + sizeof(buf), value);
    if (res.ec == std::errc{})
    {
        return ConsoleString(buf, static_cast<gp::USize>(res.ptr - buf));
    }
    return ConsoleString("0");
}

ConsoleString formatConsoleFloat64(const gp::Float64 value) noexcept
{
    if (!std::isfinite(value))
    {
        return ConsoleString("0");
    }
    char buf[64]{};
    const std::to_chars_result res = std::to_chars(buf, buf + sizeof(buf), value);
    if (res.ec == std::errc{})
    {
        return ConsoleString(buf, static_cast<gp::USize>(res.ptr - buf));
    }
    return ConsoleString("0");
}

namespace internal
{

/// @section ConsoleObjectBase

ConsoleObjectBase::ConsoleObjectBase(
    const gp::StringView name, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
    : m_name(name.size() > kMaxConsoleNameLength ? name.substr(0u, kMaxConsoleNameLength) : name)
    , m_help(help.size() > kMaxConsoleHelpLength ? help.substr(0u, kMaxConsoleHelpLength) : help)
    , m_nameHash(hashConsoleName(m_name.view()))
    , m_flags(flags)
{}

void ConsoleObjectBase::addObjectFlags(const ConsoleVarFlags flags) noexcept
{
    ConsoleVarFlags expected = m_flags.load(gp::MemoryOrder::Relaxed);
    while (!m_flags.compareExchangeWeak(expected, expected | flags, gp::MemoryOrder::AcqRel, gp::MemoryOrder::Relaxed))
    {
    }
}

void ConsoleObjectBase::clearObjectFlags(const ConsoleVarFlags flags) noexcept
{
    const ConsoleVarFlags mask = static_cast<ConsoleVarFlags>(~static_cast<gp::UInt32>(flags));
    ConsoleVarFlags expected = m_flags.load(gp::MemoryOrder::Relaxed);
    while (!m_flags.compareExchangeWeak(expected, expected & mask, gp::MemoryOrder::AcqRel, gp::MemoryOrder::Relaxed))
    {
    }
}

void ConsoleObjectBase::reinitializeObject(
    const gp::StringView name, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
{
    m_name.assign(name.size() > kMaxConsoleNameLength ? name.substr(0u, kMaxConsoleNameLength) : name);
    m_help.assign(help.size() > kMaxConsoleHelpLength ? help.substr(0u, kMaxConsoleHelpLength) : help);
    m_nameHash = hashConsoleName(m_name.view());
    m_flags.store(flags, gp::MemoryOrder::Release);
    m_ownerManager.store(nullptr, gp::MemoryOrder::Release);
    m_deferredNext = nullptr;
    m_isInDeferredList = false;
}

/// @section ConsoleVariableBase

ConsoleVariableBase::ConsoleVariableBase(
    const ConsoleVarType type, const gp::StringView name, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
    : ConsoleObjectBase(name, help, flags)
    , m_type(type)
{}

void ConsoleVariableBase::reinitializeVariable(
    const ConsoleVarType type, const gp::StringView name, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
{
    reinitializeObject(name, help, flags);
    m_type = type;
    m_setBy.store(ConsoleVarSetBy::Constructor, gp::MemoryOrder::Release);
    m_pendingLatchedSetBy.store(ConsoleVarSetBy::Constructor, gp::MemoryOrder::Release);
    m_hasPendingLatched.store(false, gp::MemoryOrder::Release);
    m_hasPendingRenderSync.store(false, gp::MemoryOrder::Release);
    acquireFlagLock(m_callbackLock);
    m_primaryCallback.unbind();
    m_multicastCallbacks.clear();
    releaseFlagLock(m_callbackLock);
}

bool ConsoleVariableBase::getBool() const noexcept
{
    ConsoleVarPayload payload{};
    readSlot(ConsoleSlotKind::Game, payload);
    return payloadToBool(payload);
}

gp::Int32 ConsoleVariableBase::getInt() const noexcept
{
    ConsoleVarPayload payload{};
    readSlot(ConsoleSlotKind::Game, payload);
    return payloadToInt32(payload);
}

gp::UInt32 ConsoleVariableBase::getUInt() const noexcept
{
    ConsoleVarPayload payload{};
    readSlot(ConsoleSlotKind::Game, payload);
    return payloadToUInt32(payload);
}

gp::Float32 ConsoleVariableBase::getFloat() const noexcept
{
    ConsoleVarPayload payload{};
    readSlot(ConsoleSlotKind::Game, payload);
    return payloadToFloat32(payload);
}

gp::Float64 ConsoleVariableBase::getFloat64() const noexcept
{
    ConsoleVarPayload payload{};
    readSlot(ConsoleSlotKind::Game, payload);
    return payloadToFloat64(payload);
}

ConsoleString ConsoleVariableBase::getString() const noexcept
{
    ConsoleVarPayload payload{};
    readSlot(ConsoleSlotKind::Game, payload);
    return payloadToString(payload);
}

ConsoleString ConsoleVariableBase::getDefaultString() const noexcept
{
    ConsoleVarPayload payload{};
    readSlot(ConsoleSlotKind::Default, payload);
    return payloadToString(payload);
}

bool ConsoleVariableBase::getBoolOnRenderThread() const noexcept
{
    ConsoleVarPayload payload{};
    readSlot(ConsoleSlotKind::Render, payload);
    return payloadToBool(payload);
}

gp::Int32 ConsoleVariableBase::getIntOnRenderThread() const noexcept
{
    ConsoleVarPayload payload{};
    readSlot(ConsoleSlotKind::Render, payload);
    return payloadToInt32(payload);
}

gp::UInt32 ConsoleVariableBase::getUIntOnRenderThread() const noexcept
{
    ConsoleVarPayload payload{};
    readSlot(ConsoleSlotKind::Render, payload);
    return payloadToUInt32(payload);
}

gp::Float32 ConsoleVariableBase::getFloatOnRenderThread() const noexcept
{
    ConsoleVarPayload payload{};
    readSlot(ConsoleSlotKind::Render, payload);
    return payloadToFloat32(payload);
}

gp::Float64 ConsoleVariableBase::getFloat64OnRenderThread() const noexcept
{
    ConsoleVarPayload payload{};
    readSlot(ConsoleSlotKind::Render, payload);
    return payloadToFloat64(payload);
}

ConsoleString ConsoleVariableBase::getStringOnRenderThread() const noexcept
{
    ConsoleVarPayload payload{};
    readSlot(ConsoleSlotKind::Render, payload);
    return payloadToString(payload);
}

bool ConsoleVariableBase::getBoolOnAnyThread() const noexcept
{
    ConsoleVarPayload payload{};
    readSlot(ConsoleSlotKind::AnyThread, payload);
    return payloadToBool(payload);
}

gp::Int32 ConsoleVariableBase::getIntOnAnyThread() const noexcept
{
    ConsoleVarPayload payload{};
    readSlot(ConsoleSlotKind::AnyThread, payload);
    return payloadToInt32(payload);
}

gp::UInt32 ConsoleVariableBase::getUIntOnAnyThread() const noexcept
{
    ConsoleVarPayload payload{};
    readSlot(ConsoleSlotKind::AnyThread, payload);
    return payloadToUInt32(payload);
}

gp::Float32 ConsoleVariableBase::getFloatOnAnyThread() const noexcept
{
    ConsoleVarPayload payload{};
    readSlot(ConsoleSlotKind::AnyThread, payload);
    return payloadToFloat32(payload);
}

gp::Float64 ConsoleVariableBase::getFloat64OnAnyThread() const noexcept
{
    ConsoleVarPayload payload{};
    readSlot(ConsoleSlotKind::AnyThread, payload);
    return payloadToFloat64(payload);
}

ConsoleString ConsoleVariableBase::getStringOnAnyThread() const noexcept
{
    ConsoleVarPayload payload{};
    readSlot(ConsoleSlotKind::AnyThread, payload);
    return payloadToString(payload);
}

ConsoleResult ConsoleVariableBase::validateWritePolicy(const ConsoleVarSetBy setBy) const noexcept
{
    if (hasObjectFlags(ConsoleVarFlags::ReadOnly) && setBy != ConsoleVarSetBy::Constructor)
    {
        return ConsoleResult::ReadOnly;
    }

    if (hasObjectFlags(ConsoleVarFlags::Cheat))
    {
        const IConsoleManager* const owner = getOwnerManager();
        if (owner != nullptr && !owner->areCheatsEnabled())
        {
            return ConsoleResult::CheatBlocked;
        }
    }

    const ConsoleVarSetBy currentSetBy = m_setBy.load(gp::MemoryOrder::Acquire);
    if (static_cast<gp::UInt8>(setBy) < static_cast<gp::UInt8>(currentSetBy))
    {
        return ConsoleResult::PriorityTooLow;
    }

    return ConsoleResult::Success;
}

ConsoleResult ConsoleVariableBase::set(const bool value, const ConsoleVarSetBy setBy) noexcept
{
    ConsoleVarPayload payload{};
    payload.type = m_type;
    switch (m_type)
    {
    case ConsoleVarType::Bool:
        payload.boolValue = value;
        break;
    case ConsoleVarType::Int32:
        payload.intValue = value ? 1 : 0;
        break;
    case ConsoleVarType::UInt32:
        payload.uintValue = value ? 1u : 0u;
        break;
    case ConsoleVarType::Float32:
        payload.floatValue = value ? 1.0f : 0.0f;
        break;
    case ConsoleVarType::Float64:
        payload.doubleValue = value ? 1.0 : 0.0;
        break;
    case ConsoleVarType::String:
        payload.stringValue = formatConsoleBool(value);
        break;
    default:
        return ConsoleResult::TypeMismatch;
    }
    return applyTypedWrite(payload, setBy);
}

ConsoleResult ConsoleVariableBase::set(const gp::Int32 value, const ConsoleVarSetBy setBy) noexcept
{
    ConsoleVarPayload payload{};
    payload.type = m_type;
    switch (m_type)
    {
    case ConsoleVarType::Bool:
        payload.boolValue = (value != 0);
        break;
    case ConsoleVarType::Int32:
        payload.intValue = value;
        break;
    case ConsoleVarType::UInt32:
        if (value < 0)
        {
            return ConsoleResult::InvalidArguments;
        }
        payload.uintValue = static_cast<gp::UInt32>(value);
        break;
    case ConsoleVarType::Float32:
        payload.floatValue = static_cast<gp::Float32>(value);
        break;
    case ConsoleVarType::Float64:
        payload.doubleValue = static_cast<gp::Float64>(value);
        break;
    case ConsoleVarType::String:
        payload.stringValue = formatConsoleInt32(value);
        break;
    default:
        return ConsoleResult::TypeMismatch;
    }
    return applyTypedWrite(payload, setBy);
}

ConsoleResult ConsoleVariableBase::set(const gp::UInt32 value, const ConsoleVarSetBy setBy) noexcept
{
    ConsoleVarPayload payload{};
    payload.type = m_type;
    switch (m_type)
    {
    case ConsoleVarType::Bool:
        payload.boolValue = (value != 0u);
        break;
    case ConsoleVarType::Int32:
        if (value > static_cast<gp::UInt32>(std::numeric_limits<gp::Int32>::max()))
        {
            return ConsoleResult::InvalidArguments;
        }
        payload.intValue = static_cast<gp::Int32>(value);
        break;
    case ConsoleVarType::UInt32:
        payload.uintValue = value;
        break;
    case ConsoleVarType::Float32:
        payload.floatValue = static_cast<gp::Float32>(value);
        break;
    case ConsoleVarType::Float64:
        payload.doubleValue = static_cast<gp::Float64>(value);
        break;
    case ConsoleVarType::String:
        payload.stringValue = formatConsoleUInt32(value);
        break;
    default:
        return ConsoleResult::TypeMismatch;
    }
    return applyTypedWrite(payload, setBy);
}

ConsoleResult ConsoleVariableBase::set(const gp::Float32 value, const ConsoleVarSetBy setBy) noexcept
{
    if (!std::isfinite(value))
    {
        return ConsoleResult::InvalidArguments;
    }

    ConsoleVarPayload payload{};
    payload.type = m_type;
    switch (m_type)
    {
    case ConsoleVarType::Bool:
        payload.boolValue = (value != 0.0f);
        break;
    case ConsoleVarType::Int32:
        payload.intValue = static_cast<gp::Int32>(value);
        break;
    case ConsoleVarType::UInt32:
        if (value < 0.0f)
        {
            return ConsoleResult::InvalidArguments;
        }
        payload.uintValue = static_cast<gp::UInt32>(value);
        break;
    case ConsoleVarType::Float32:
        payload.floatValue = value;
        break;
    case ConsoleVarType::Float64:
        payload.doubleValue = static_cast<gp::Float64>(value);
        break;
    case ConsoleVarType::String:
        payload.stringValue = formatConsoleFloat32(value);
        break;
    default:
        return ConsoleResult::TypeMismatch;
    }
    return applyTypedWrite(payload, setBy);
}

ConsoleResult ConsoleVariableBase::set(const gp::Float64 value, const ConsoleVarSetBy setBy) noexcept
{
    if (!std::isfinite(value))
    {
        return ConsoleResult::InvalidArguments;
    }

    ConsoleVarPayload payload{};
    payload.type = m_type;
    switch (m_type)
    {
    case ConsoleVarType::Bool:
        payload.boolValue = (value != 0.0);
        break;
    case ConsoleVarType::Int32:
        payload.intValue = static_cast<gp::Int32>(value);
        break;
    case ConsoleVarType::UInt32:
        if (value < 0.0)
        {
            return ConsoleResult::InvalidArguments;
        }
        payload.uintValue = static_cast<gp::UInt32>(value);
        break;
    case ConsoleVarType::Float32:
        payload.floatValue = static_cast<gp::Float32>(value);
        break;
    case ConsoleVarType::Float64:
        payload.doubleValue = value;
        break;
    case ConsoleVarType::String:
        payload.stringValue = formatConsoleFloat64(value);
        break;
    default:
        return ConsoleResult::TypeMismatch;
    }
    return applyTypedWrite(payload, setBy);
}

ConsoleResult ConsoleVariableBase::setFromString(const gp::StringView value, const ConsoleVarSetBy setBy) noexcept
{
    ConsoleVarPayload payload{};
    payload.type = m_type;

    switch (m_type)
    {
    case ConsoleVarType::Bool:
    {
        const gp::Optional<bool> parsed = parseConsoleBool(value);
        if (!parsed.hasValue())
        {
            return ConsoleResult::ParseFailed;
        }
        payload.boolValue = *parsed;
        break;
    }
    case ConsoleVarType::Int32:
    {
        const gp::Optional<gp::Int32> parsed = parseConsoleInt32(value);
        if (!parsed.hasValue())
        {
            return ConsoleResult::ParseFailed;
        }
        payload.intValue = *parsed;
        break;
    }
    case ConsoleVarType::UInt32:
    {
        const gp::Optional<gp::UInt32> parsed = parseConsoleUInt32(value);
        if (!parsed.hasValue())
        {
            return ConsoleResult::ParseFailed;
        }
        payload.uintValue = *parsed;
        break;
    }
    case ConsoleVarType::Float32:
    {
        const gp::Optional<gp::Float32> parsed = parseConsoleFloat32(value);
        if (!parsed.hasValue())
        {
            return ConsoleResult::ParseFailed;
        }
        payload.floatValue = *parsed;
        break;
    }
    case ConsoleVarType::Float64:
    {
        const gp::Optional<gp::Float64> parsed = parseConsoleFloat64(value);
        if (!parsed.hasValue())
        {
            return ConsoleResult::ParseFailed;
        }
        payload.doubleValue = *parsed;
        break;
    }
    case ConsoleVarType::String:
    {
        const gp::StringView clamped =
            (value.size() > kMaxConsoleStringLength) ? value.substr(0u, kMaxConsoleStringLength) : value;
        payload.stringValue.assign(clamped);
        break;
    }
    default:
        return ConsoleResult::TypeMismatch;
    }

    return applyTypedWrite(payload, setBy);
}

ConsoleResult ConsoleVariableBase::reset(const ConsoleVarSetBy setBy) noexcept
{
    const ConsoleResult policy = validateWritePolicy(setBy);
    if (policy != ConsoleResult::Success)
    {
        return policy;
    }

    if (m_hasPendingLatched.load(gp::MemoryOrder::Acquire) &&
        static_cast<gp::UInt8>(setBy) < static_cast<gp::UInt8>(m_pendingLatchedSetBy.load(gp::MemoryOrder::Acquire)))
    {
        return ConsoleResult::PriorityTooLow;
    }

    ConsoleVarPayload defaultPayload{};
    readSlot(ConsoleSlotKind::Default, defaultPayload);

    ConsoleVarPayload currentPayload{};
    readSlot(ConsoleSlotKind::Game, currentPayload);
    const bool valueChanged = !payloadsEqual(currentPayload, defaultPayload);

    writeSlot(ConsoleSlotKind::Latched, defaultPayload);
    writeSlot(ConsoleSlotKind::Game, defaultPayload);
    resetPolicyMetadata();

    if (valueChanged)
    {
        m_hasPendingRenderSync.store(true, gp::MemoryOrder::Release);
        dispatchChangedNotifications();
    }

    return ConsoleResult::Success;
}

ConsoleResult
    ConsoleVariableBase::applyTypedWrite(const ConsoleVarPayload& payload, const ConsoleVarSetBy setBy) noexcept
{
    const ConsoleResult policy = validateWritePolicy(setBy);
    if (policy != ConsoleResult::Success)
    {
        return policy;
    }

    if (isLatched() && setBy != ConsoleVarSetBy::Constructor)
    {
        const ConsoleVarSetBy pendingPriority = m_pendingLatchedSetBy.load(gp::MemoryOrder::Acquire);
        if (m_hasPendingLatched.load(gp::MemoryOrder::Acquire) &&
            static_cast<gp::UInt8>(setBy) < static_cast<gp::UInt8>(pendingPriority))
        {
            return ConsoleResult::PriorityTooLow;
        }

        writeSlot(ConsoleSlotKind::Latched, payload);
        m_pendingLatchedSetBy.store(setBy, gp::MemoryOrder::Release);
        m_hasPendingLatched.store(true, gp::MemoryOrder::Release);
        if (IConsoleManager* const owner = getOwnerManager(); owner != nullptr)
        {
            owner->notifyVariableLatched(this);
        }
        return ConsoleResult::Success;
    }

    ConsoleVarPayload currentPayload{};
    readSlot(ConsoleSlotKind::Game, currentPayload);
    const bool valueChanged = !payloadsEqual(currentPayload, payload);

    m_setBy.store(setBy, gp::MemoryOrder::Release);
    if (equalsDefault(payload))
    {
        clearObjectFlags(ConsoleVarFlags::Modified);
    }
    else
    {
        addObjectFlags(ConsoleVarFlags::Modified);
    }

    if (valueChanged)
    {
        writeSlot(ConsoleSlotKind::Game, payload);
        m_hasPendingRenderSync.store(true, gp::MemoryOrder::Release);
        dispatchChangedNotifications();
    }

    return ConsoleResult::Success;
}

void ConsoleVariableBase::setOnChangedCallback(const ConsoleVarChangedDelegate& callback) noexcept
{
    acquireFlagLock(m_callbackLock);
    m_primaryCallback = callback;
    releaseFlagLock(m_callbackLock);
}

gp::DelegateHandle ConsoleVariableBase::addOnChangedCallback(ConsoleVarChangedDelegate callback) noexcept
{
    acquireFlagLock(m_callbackLock);
    const gp::DelegateHandle handle = m_multicastCallbacks.add(std::move(callback));
    releaseFlagLock(m_callbackLock);
    return handle;
}

bool ConsoleVariableBase::removeOnChangedCallback(const gp::DelegateHandle handle) noexcept
{
    acquireFlagLock(m_callbackLock);
    const bool removed = m_multicastCallbacks.remove(handle);
    releaseFlagLock(m_callbackLock);
    return removed;
}

void ConsoleVariableBase::clearOnChangedCallbacks() noexcept
{
    acquireFlagLock(m_callbackLock);
    m_multicastCallbacks.clear();
    releaseFlagLock(m_callbackLock);
}

void ConsoleVariableBase::syncRenderThreadValue() noexcept
{
    ConsoleVarPayload gamePayload{};
    readSlot(ConsoleSlotKind::Game, gamePayload);
    writeSlot(ConsoleSlotKind::Render, gamePayload);
    m_hasPendingRenderSync.store(false, gp::MemoryOrder::Release);
}

bool ConsoleVariableBase::applyLatchedValue() noexcept
{
    if (!m_hasPendingLatched.load(gp::MemoryOrder::Acquire))
    {
        return false;
    }

    ConsoleVarPayload latchedPayload{};
    readSlot(ConsoleSlotKind::Latched, latchedPayload);

    ConsoleVarPayload currentPayload{};
    readSlot(ConsoleSlotKind::Game, currentPayload);
    const bool valueChanged = !payloadsEqual(currentPayload, latchedPayload);

    const ConsoleVarSetBy latchedSetBy = m_pendingLatchedSetBy.load(gp::MemoryOrder::Acquire);
    const ConsoleVarSetBy currentSetBy = m_setBy.load(gp::MemoryOrder::Acquire);
    if (static_cast<gp::UInt8>(latchedSetBy) > static_cast<gp::UInt8>(currentSetBy))
    {
        m_setBy.store(latchedSetBy, gp::MemoryOrder::Release);
    }

    m_hasPendingLatched.store(false, gp::MemoryOrder::Release);
    m_pendingLatchedSetBy.store(ConsoleVarSetBy::Constructor, gp::MemoryOrder::Release);

    if (equalsDefault(latchedPayload))
    {
        clearObjectFlags(ConsoleVarFlags::Modified);
    }
    else
    {
        addObjectFlags(ConsoleVarFlags::Modified);
    }

    if (valueChanged)
    {
        writeSlot(ConsoleSlotKind::Game, latchedPayload);
        m_hasPendingRenderSync.store(true, gp::MemoryOrder::Release);
        dispatchChangedNotifications();
    }

    return true;
}

void ConsoleVariableBase::dispatchChangedNotifications() noexcept
{
    ConsoleVarChangedDelegate primarySnapshot{};
    gp::FixedMulticastDelegate<kMaxConsoleCallbacksPerVariable, IConsoleVariable*> multicastSnapshot{};

    acquireFlagLock(m_callbackLock);
    primarySnapshot = m_primaryCallback;
    multicastSnapshot = m_multicastCallbacks;
    releaseFlagLock(m_callbackLock);

    if (primarySnapshot.isBound())
    {
        primarySnapshot.execute(this);
    }
    multicastSnapshot.broadcast(this);

    if (IConsoleManager* const owner = getOwnerManager(); owner != nullptr)
    {
        owner->notifyVariableChanged(this);
    }
}

void ConsoleVariableBase::resetPolicyMetadata() noexcept
{
    m_setBy.store(ConsoleVarSetBy::Constructor, gp::MemoryOrder::Release);
    m_pendingLatchedSetBy.store(ConsoleVarSetBy::Constructor, gp::MemoryOrder::Release);
    m_hasPendingLatched.store(false, gp::MemoryOrder::Release);
    clearObjectFlags(ConsoleVarFlags::Modified);
}

/// @section ConsoleCommandBase

ConsoleCommandBase::ConsoleCommandBase(
    const gp::StringView name, const gp::StringView help, ConsoleCommandDelegate callback, const ConsoleVarFlags flags
) noexcept
    : ConsoleObjectBase(name, help, flags)
    , m_voidCallback(std::move(callback))
    , m_takesArgs(false)
{}

ConsoleCommandBase::ConsoleCommandBase(
    const gp::StringView name,
    const gp::StringView help,
    ConsoleCommandWithArgsDelegate callback,
    const ConsoleVarFlags flags
) noexcept
    : ConsoleObjectBase(name, help, flags)
    , m_argsCallback(std::move(callback))
    , m_takesArgs(true)
{}

ConsoleResult ConsoleCommandBase::execute(const ConsoleCommandArgs& args) noexcept
{
    if (!isEnabled())
    {
        const IConsoleManager* const owner = getOwnerManager();
        if (owner != nullptr && !owner->isConsoleEnabled())
        {
            return ConsoleResult::ConsoleDisabled;
        }
        return ConsoleResult::CheatBlocked;
    }

    if (m_takesArgs)
    {
        if (!m_argsCallback.isBound())
        {
            return ConsoleResult::InvalidArguments;
        }
        m_argsCallback.execute(args);
        return ConsoleResult::Success;
    }

    if (!m_voidCallback.isBound())
    {
        return ConsoleResult::InvalidArguments;
    }
    m_voidCallback.execute();
    return ConsoleResult::Success;
}

bool ConsoleCommandBase::isEnabled() const noexcept
{
    const IConsoleManager* const owner = getOwnerManager();
    if (owner != nullptr)
    {
        if (!owner->isConsoleEnabled())
        {
            return false;
        }
        if (hasObjectFlags(ConsoleVarFlags::Cheat) && !owner->areCheatsEnabled())
        {
            return false;
        }
    }
    return true;
}

void ConsoleCommandBase::rebind(
    const gp::StringView name, const gp::StringView help, ConsoleCommandDelegate callback, const ConsoleVarFlags flags
) noexcept
{
    reinitializeObject(name, help, flags);
    m_argsCallback.unbind();
    m_voidCallback = std::move(callback);
    m_takesArgs = false;
}

void ConsoleCommandBase::rebind(
    const gp::StringView name,
    const gp::StringView help,
    ConsoleCommandWithArgsDelegate callback,
    const ConsoleVarFlags flags
) noexcept
{
    reinitializeObject(name, help, flags);
    m_voidCallback.unbind();
    m_argsCallback = std::move(callback);
    m_takesArgs = true;
}

void ConsoleCommandBase::resetCommandSlot() noexcept
{
    m_voidCallback.unbind();
    m_argsCallback.unbind();
    m_takesArgs = false;
    setOwnerManager(nullptr);
}

/// @section Shared Scalar, Seqlock & External Reference Helpers

gp::UInt64 scalarPayloadToBits(const ConsoleVarType type, const ConsoleVarPayload& payload) noexcept
{
    gp::UInt64 bits = 0ULL;
    switch (type)
    {
    case ConsoleVarType::Bool:
        bits = payload.boolValue ? 1ULL : 0ULL;
        break;
    case ConsoleVarType::Int32:
    {
        gp::UInt32 raw = 0u;
        std::memcpy(&raw, &payload.intValue, sizeof(gp::Int32));
        bits = static_cast<gp::UInt64>(raw);
        break;
    }
    case ConsoleVarType::UInt32:
        bits = static_cast<gp::UInt64>(payload.uintValue);
        break;
    case ConsoleVarType::Float32:
    {
        gp::UInt32 raw = 0u;
        std::memcpy(&raw, &payload.floatValue, sizeof(gp::Float32));
        bits = static_cast<gp::UInt64>(raw);
        break;
    }
    case ConsoleVarType::Float64:
        std::memcpy(&bits, &payload.doubleValue, sizeof(gp::Float64));
        break;
    case ConsoleVarType::String:
    default:
        break;
    }
    return bits;
}

void bitsToScalarPayload(const ConsoleVarType type, const gp::UInt64 bits, ConsoleVarPayload& outPayload) noexcept
{
    outPayload.type = type;
    switch (type)
    {
    case ConsoleVarType::Bool:
        outPayload.boolValue = (bits != 0ULL);
        break;
    case ConsoleVarType::Int32:
    {
        const gp::UInt32 raw = static_cast<gp::UInt32>(bits & 0xFFFFFFFFULL);
        std::memcpy(&outPayload.intValue, &raw, sizeof(gp::Int32));
        break;
    }
    case ConsoleVarType::UInt32:
        outPayload.uintValue = static_cast<gp::UInt32>(bits & 0xFFFFFFFFULL);
        break;
    case ConsoleVarType::Float32:
    {
        const gp::UInt32 raw = static_cast<gp::UInt32>(bits & 0xFFFFFFFFULL);
        std::memcpy(&outPayload.floatValue, &raw, sizeof(gp::Float32));
        break;
    }
    case ConsoleVarType::Float64:
        std::memcpy(&outPayload.doubleValue, &bits, sizeof(gp::Float64));
        break;
    case ConsoleVarType::String:
    default:
        break;
    }
}

void readConsoleStringSeqlock(
    const char* const buffer, const gp::thread::Atomic<gp::UInt32>& seq, ConsoleString& outStr
) noexcept
{
    while (true)
    {
        const gp::UInt32 before = seq.load(gp::MemoryOrder::Acquire);
        if ((before & 1u) != 0u)
        {
            gp::platform::Thread::spinWait();
            continue;
        }
        char localBuf[kMaxConsoleStringLength + 1u]{};
        std::memcpy(localBuf, buffer, kMaxConsoleStringLength + 1u);
        localBuf[kMaxConsoleStringLength] = '\0';
        std::atomic_thread_fence(std::memory_order_acquire);
        const gp::UInt32 after = seq.load(gp::MemoryOrder::Relaxed);
        if (before == after)
        {
            outStr.assign(gp::StringView(localBuf));
            return;
        }
    }
}

void writeConsoleStringSeqlock(
    char* const buffer, gp::thread::Atomic<gp::UInt32>& seq, const gp::StringView str
) noexcept
{
    const gp::UInt32 current = seq.load(gp::MemoryOrder::Relaxed);
    seq.store(current + 1u, gp::MemoryOrder::Relaxed);
    std::atomic_thread_fence(std::memory_order_acq_rel);
    const gp::USize copyLen = (str.size() > kMaxConsoleStringLength) ? kMaxConsoleStringLength : str.size();
    if (copyLen > 0u)
    {
        std::memcpy(buffer, str.data(), copyLen);
    }
    buffer[copyLen] = '\0';
    seq.store(current + 2u, gp::MemoryOrder::Release);
}

void readExternalRefValue(
    const ConsoleVarType type, const void* const refPtr, const bool isGpString, ConsoleVarPayload& outValue
) noexcept
{
    outValue.type = type;
    if (refPtr == nullptr)
    {
        return;
    }

    switch (type)
    {
    case ConsoleVarType::Bool:
        outValue.boolValue = *static_cast<const bool*>(refPtr);
        break;
    case ConsoleVarType::Int32:
        outValue.intValue = *static_cast<const gp::Int32*>(refPtr);
        break;
    case ConsoleVarType::UInt32:
        outValue.uintValue = *static_cast<const gp::UInt32*>(refPtr);
        break;
    case ConsoleVarType::Float32:
        outValue.floatValue = *static_cast<const gp::Float32*>(refPtr);
        break;
    case ConsoleVarType::Float64:
        outValue.doubleValue = *static_cast<const gp::Float64*>(refPtr);
        break;
    case ConsoleVarType::String:
        if (isGpString)
        {
            outValue.stringValue.assign(static_cast<const gp::String*>(refPtr)->view());
        }
        else
        {
            outValue.stringValue = *static_cast<const ConsoleString*>(refPtr);
        }
        break;
    default:
        break;
    }
}

void writeExternalRefValue(
    const ConsoleVarType type, void* const refPtr, const bool isGpString, const ConsoleVarPayload& value
) noexcept
{
    if (refPtr == nullptr)
    {
        return;
    }

    switch (type)
    {
    case ConsoleVarType::Bool:
        *static_cast<bool*>(refPtr) = value.boolValue;
        break;
    case ConsoleVarType::Int32:
        *static_cast<gp::Int32*>(refPtr) = value.intValue;
        break;
    case ConsoleVarType::UInt32:
        *static_cast<gp::UInt32*>(refPtr) = value.uintValue;
        break;
    case ConsoleVarType::Float32:
        *static_cast<gp::Float32*>(refPtr) = value.floatValue;
        break;
    case ConsoleVarType::Float64:
        *static_cast<gp::Float64*>(refPtr) = value.doubleValue;
        break;
    case ConsoleVarType::String:
        if (isGpString)
        {
            static_cast<gp::String*>(refPtr)->assign(value.stringValue.view());
        }
        else
        {
            *static_cast<ConsoleString*>(refPtr) = value.stringValue;
        }
        break;
    default:
        break;
    }
}

/// @section ConsoleDynamicVariable

ConsoleDynamicVariable::ConsoleDynamicVariable() noexcept
    : ConsoleVariableBase(ConsoleVarType::Int32, gp::StringView{}, gp::StringView{}, ConsoleVarFlags::Unregistered)
{}

void ConsoleDynamicVariable::initializeOwned(
    const ConsoleVarType type,
    const gp::StringView name,
    const ConsoleVarPayload& defaultValue,
    const gp::StringView help,
    const ConsoleVarFlags flags
) noexcept
{
    reinitializeVariable(type, name, help, flags | ConsoleVarFlags::RenderThreadSafe);

    m_refPtr = nullptr;
    m_isGpStringRef = false;
    m_inUse = true;

    if (type == ConsoleVarType::String)
    {
        m_defaultBits = 0ULL;
        m_gameBits.store(0ULL, gp::MemoryOrder::Relaxed);
        m_renderBits.store(0ULL, gp::MemoryOrder::Relaxed);
        m_latchedBits = 0ULL;
        m_defaultString = defaultValue.stringValue;
        m_latchedString = defaultValue.stringValue;
        writeConsoleStringSeqlock(m_gameString, m_gameStringSeq, defaultValue.stringValue.view());
        writeConsoleStringSeqlock(m_renderString, m_renderStringSeq, defaultValue.stringValue.view());
    }
    else
    {
        m_defaultString.clear();
        m_latchedString.clear();
        m_defaultBits = scalarPayloadToBits(type, defaultValue);
        m_gameBits.store(m_defaultBits, gp::MemoryOrder::Relaxed);
        m_renderBits.store(m_defaultBits, gp::MemoryOrder::Relaxed);
        m_latchedBits = m_defaultBits;
    }
}

void ConsoleDynamicVariable::initializeRef(
    const ConsoleVarType type,
    const gp::StringView name,
    void* const refPtr,
    const bool isGpStringRef,
    const gp::StringView help,
    const ConsoleVarFlags flags
) noexcept
{
    reinitializeVariable(type, name, help, flags);

    m_refPtr = refPtr;
    m_isGpStringRef = isGpStringRef;
    m_inUse = true;

    ConsoleVarPayload initial{};
    readSlot(ConsoleSlotKind::Game, initial);
    if (type == ConsoleVarType::String)
    {
        m_defaultBits = 0ULL;
        m_latchedBits = 0ULL;
        m_defaultString = initial.stringValue;
        m_latchedString = initial.stringValue;
        writeConsoleStringSeqlock(m_renderString, m_renderStringSeq, initial.stringValue.view());
    }
    else
    {
        m_defaultString.clear();
        m_latchedString.clear();
        m_defaultBits = scalarPayloadToBits(type, initial);
        m_renderBits.store(m_defaultBits, gp::MemoryOrder::Relaxed);
        m_latchedBits = m_defaultBits;
    }
}

void ConsoleDynamicVariable::releaseSlot() noexcept
{
    setOnChangedCallback(ConsoleVarChangedDelegate{});
    clearOnChangedCallbacks();
    setOwnerManager(nullptr);
    m_refPtr = nullptr;
    m_isGpStringRef = false;
    m_inUse = false;
}

void ConsoleDynamicVariable::readSlot(const ConsoleSlotKind slot, ConsoleVarPayload& outValue) const noexcept
{
    const ConsoleVarType type = getType();
    outValue.type = type;

    if (m_refPtr != nullptr)
    {
        if (type == ConsoleVarType::String)
        {
            if (slot == ConsoleSlotKind::Default)
            {
                outValue.stringValue = m_defaultString;
            }
            else if (slot == ConsoleSlotKind::Render)
            {
                readConsoleStringSeqlock(m_renderString, m_renderStringSeq, outValue.stringValue);
            }
            else if (slot == ConsoleSlotKind::Latched)
            {
                outValue.stringValue = m_latchedString;
            }
            else
            {
                readExternalRefValue(type, m_refPtr, m_isGpStringRef, outValue);
            }
            return;
        }

        if (slot == ConsoleSlotKind::Default)
        {
            bitsToScalarPayload(type, m_defaultBits, outValue);
        }
        else if (slot == ConsoleSlotKind::Render)
        {
            bitsToScalarPayload(type, m_renderBits.load(gp::MemoryOrder::Acquire), outValue);
        }
        else if (slot == ConsoleSlotKind::Latched)
        {
            bitsToScalarPayload(type, m_latchedBits, outValue);
        }
        else
        {
            readExternalRefValue(type, m_refPtr, m_isGpStringRef, outValue);
        }
        return;
    }

    if (type == ConsoleVarType::String)
    {
        switch (slot)
        {
        case ConsoleSlotKind::Default:
            outValue.stringValue = m_defaultString;
            break;
        case ConsoleSlotKind::Game:
            outValue.stringValue.assign(gp::StringView(m_gameString));
            break;
        case ConsoleSlotKind::Render:
            readConsoleStringSeqlock(m_renderString, m_renderStringSeq, outValue.stringValue);
            break;
        case ConsoleSlotKind::AnyThread:
            readConsoleStringSeqlock(m_gameString, m_gameStringSeq, outValue.stringValue);
            break;
        case ConsoleSlotKind::Latched:
            outValue.stringValue = m_latchedString;
            break;
        default:
            break;
        }
        return;
    }

    switch (slot)
    {
    case ConsoleSlotKind::Default:
        bitsToScalarPayload(type, m_defaultBits, outValue);
        break;
    case ConsoleSlotKind::Game:
        bitsToScalarPayload(type, m_gameBits.load(gp::MemoryOrder::Relaxed), outValue);
        break;
    case ConsoleSlotKind::Render:
        bitsToScalarPayload(type, m_renderBits.load(gp::MemoryOrder::Acquire), outValue);
        break;
    case ConsoleSlotKind::AnyThread:
        bitsToScalarPayload(type, m_gameBits.load(gp::MemoryOrder::Acquire), outValue);
        break;
    case ConsoleSlotKind::Latched:
        bitsToScalarPayload(type, m_latchedBits, outValue);
        break;
    default:
        break;
    }
}

void ConsoleDynamicVariable::writeSlot(const ConsoleSlotKind slot, const ConsoleVarPayload& value) noexcept
{
    const ConsoleVarType type = getType();
    if (m_refPtr != nullptr)
    {
        if (type == ConsoleVarType::String)
        {
            if (slot == ConsoleSlotKind::Game)
            {
                writeExternalRefValue(type, m_refPtr, m_isGpStringRef, value);
            }
            else if (slot == ConsoleSlotKind::Render)
            {
                writeConsoleStringSeqlock(m_renderString, m_renderStringSeq, value.stringValue.view());
            }
            else if (slot == ConsoleSlotKind::Latched)
            {
                m_latchedString = value.stringValue;
            }
            return;
        }

        if (slot == ConsoleSlotKind::Game)
        {
            writeExternalRefValue(type, m_refPtr, m_isGpStringRef, value);
        }
        else if (slot == ConsoleSlotKind::Render)
        {
            m_renderBits.store(scalarPayloadToBits(type, value), gp::MemoryOrder::Release);
        }
        else if (slot == ConsoleSlotKind::Latched)
        {
            m_latchedBits = scalarPayloadToBits(type, value);
        }
        return;
    }

    if (type == ConsoleVarType::String)
    {
        if (slot == ConsoleSlotKind::Game)
        {
            writeConsoleStringSeqlock(m_gameString, m_gameStringSeq, value.stringValue.view());
        }
        else if (slot == ConsoleSlotKind::Render)
        {
            writeConsoleStringSeqlock(m_renderString, m_renderStringSeq, value.stringValue.view());
        }
        else if (slot == ConsoleSlotKind::Latched)
        {
            m_latchedString = value.stringValue;
        }
        return;
    }

    const gp::UInt64 bits = scalarPayloadToBits(type, value);
    if (slot == ConsoleSlotKind::Game)
    {
        m_gameBits.store(bits, gp::MemoryOrder::Release);
    }
    else if (slot == ConsoleSlotKind::Render)
    {
        m_renderBits.store(bits, gp::MemoryOrder::Release);
    }
    else if (slot == ConsoleSlotKind::Latched)
    {
        m_latchedBits = bits;
    }
}

bool ConsoleDynamicVariable::equalsDefault(const ConsoleVarPayload& value) const noexcept
{
    if (getType() == ConsoleVarType::String)
    {
        return value.stringValue.view() == m_defaultString.view();
    }
    return scalarPayloadToBits(getType(), value) == m_defaultBits;
}

/// @section ConsoleDynamicCommand

ConsoleDynamicCommand::ConsoleDynamicCommand() noexcept
    : ConsoleCommandBase(gp::StringView{}, gp::StringView{}, ConsoleCommandDelegate{}, ConsoleVarFlags::Unregistered)
{}

void ConsoleDynamicCommand::initialize(
    const gp::StringView name, const gp::StringView help, ConsoleCommandDelegate callback, const ConsoleVarFlags flags
) noexcept
{
    rebind(name, help, std::move(callback), flags);
    m_inUse = true;
}

void ConsoleDynamicCommand::initialize(
    const gp::StringView name,
    const gp::StringView help,
    ConsoleCommandWithArgsDelegate callback,
    const ConsoleVarFlags flags
) noexcept
{
    rebind(name, help, std::move(callback), flags);
    m_inUse = true;
}

void ConsoleDynamicCommand::releaseSlot() noexcept
{
    resetCommandSlot();
    m_inUse = false;
}

/// @section Deferred Global Registration Helpers

void enqueueDeferredConsoleObject(ConsoleObjectBase* const object) noexcept
{
    if (object == nullptr)
    {
        return;
    }

    if (g_globalInitialized.load(gp::MemoryOrder::Acquire) && !g_globalShuttingDown.load(gp::MemoryOrder::Acquire))
    {
        (void)globalConsoleManagerInstance().registerExternalObject(object->asConsoleObject());
        return;
    }

    acquireFlagLock(g_deferredObjects.lock);
    if (g_globalInitialized.load(gp::MemoryOrder::Acquire) && !g_globalShuttingDown.load(gp::MemoryOrder::Acquire))
    {
        releaseFlagLock(g_deferredObjects.lock);
        (void)globalConsoleManagerInstance().registerExternalObject(object->asConsoleObject());
        return;
    }
    object->m_deferredNext = g_deferredObjects.head;
    object->m_isInDeferredList = true;
    g_deferredObjects.head = object;
    releaseFlagLock(g_deferredObjects.lock);
}

void unregisterAutoConsoleObject(ConsoleObjectBase* const object) noexcept
{
    if (object == nullptr)
    {
        return;
    }

    acquireFlagLock(g_deferredObjects.lock);
    if (object->m_isInDeferredList)
    {
        ConsoleObjectBase* prev = nullptr;
        ConsoleObjectBase* curr = g_deferredObjects.head;
        while (curr != nullptr)
        {
            if (curr == object)
            {
                if (prev != nullptr)
                {
                    prev->m_deferredNext = curr->m_deferredNext;
                }
                else
                {
                    g_deferredObjects.head = curr->m_deferredNext;
                }
                curr->m_deferredNext = nullptr;
                curr->m_isInDeferredList = false;
                break;
            }
            prev = curr;
            curr = curr->m_deferredNext;
        }
    }
    releaseFlagLock(g_deferredObjects.lock);

    if (IConsoleManager* const owner = object->getOwnerManager(); owner != nullptr)
    {
        (void)owner->unregisterConsoleObject(object->asConsoleObject());
    }
}

gp::DelegateHandle registerGlobalOrDeferredSink(ConsoleSinkDelegate callback, bool& outInDeferredList) noexcept
{
    outInDeferredList = false;
    if (!callback.isBound())
    {
        return gp::DelegateHandle{};
    }

    if (g_globalInitialized.load(gp::MemoryOrder::Acquire) && !g_globalShuttingDown.load(gp::MemoryOrder::Acquire))
    {
        return IConsoleManager::get().registerConsoleVariableSink(std::move(callback));
    }

    acquireFlagLock(g_deferredSinks.lock);
    if (g_globalInitialized.load(gp::MemoryOrder::Acquire) && !g_globalShuttingDown.load(gp::MemoryOrder::Acquire))
    {
        releaseFlagLock(g_deferredSinks.lock);
        return IConsoleManager::get().registerConsoleVariableSink(std::move(callback));
    }
    for (gp::USize i = 0u; i < kMaxConsoleSinks; ++i)
    {
        DeferredSinkEntry& entry = g_deferredSinks.entries[i];
        if (!entry.active)
        {
            const gp::UInt64 rawId =
                kDeferredSinkBit | (static_cast<gp::UInt64>(entry.generation) << 32u) | static_cast<gp::UInt64>(i + 1u);
            entry.callback = std::move(callback);
            entry.externalHandle = gp::DelegateHandle(rawId);
            entry.liveHandle.reset();
            entry.active = true;
            entry.migrated = false;
            outInDeferredList = true;
            releaseFlagLock(g_deferredSinks.lock);
            return entry.externalHandle;
        }
    }
    releaseFlagLock(g_deferredSinks.lock);
    return gp::DelegateHandle{};
}

void unregisterGlobalOrDeferredSink(const gp::DelegateHandle handle) noexcept
{
    if (!handle.isValid())
    {
        return;
    }

    if ((handle.rawValue() & kDeferredSinkBit) == 0ULL)
    {
        if (g_globalInitialized.load(gp::MemoryOrder::Acquire) && !g_globalShuttingDown.load(gp::MemoryOrder::Acquire))
        {
            (void)IConsoleManager::get().unregisterConsoleVariableSink(handle);
        }
        return;
    }

    const gp::USize idxPlusOne = static_cast<gp::USize>(handle.rawValue() & 0xFFFFFFFFULL);
    if (idxPlusOne == 0u || idxPlusOne > kMaxConsoleSinks)
    {
        return;
    }

    gp::DelegateHandle liveHandle{};
    bool shouldUnregisterLive = false;

    acquireFlagLock(g_deferredSinks.lock);
    DeferredSinkEntry& entry = g_deferredSinks.entries[idxPlusOne - 1u];
    if (entry.active && entry.externalHandle == handle)
    {
        if (entry.migrated && entry.liveHandle.isValid())
        {
            liveHandle = entry.liveHandle;
            shouldUnregisterLive = true;
        }
        entry.callback.unbind();
        entry.externalHandle.reset();
        entry.liveHandle.reset();
        entry.active = false;
        entry.migrated = false;
        ++entry.generation;
        if (entry.generation == 0u)
        {
            entry.generation = 1u;
        }
    }
    releaseFlagLock(g_deferredSinks.lock);

    if (shouldUnregisterLive && g_globalInitialized.load(gp::MemoryOrder::Acquire) &&
        !g_globalShuttingDown.load(gp::MemoryOrder::Acquire))
    {
        (void)IConsoleManager::get().unregisterConsoleVariableSink(liveHandle);
    }
}

}   // namespace internal

/// @section ConsoleManagerBase

ConsoleManagerBase::ConsoleManagerBase(const internal::ConsoleManagerStorageView& storage) noexcept
    : m_storage(storage)
    , m_hashMask(storage.hashSlotCapacity > 0u ? (storage.hashSlotCapacity - 1u) : 0u)
{}

ConsoleManagerBase::~ConsoleManagerBase() noexcept
{
    if (m_isGlobalSingleton)
    {
        g_globalShuttingDown.store(true, gp::MemoryOrder::Release);
    }
    shutdown();
}

ConsoleResult ConsoleManagerBase::initialize(const ConsoleManagerDesc& desc) noexcept
{
    if (m_isInitialized.load(gp::MemoryOrder::Acquire))
    {
        return ConsoleResult::AlreadyExists;
    }

    if (m_storage.hashSlots == nullptr || m_storage.hashSlotCapacity < 16u ||
        (m_storage.hashSlotCapacity & (m_storage.hashSlotCapacity - 1u)) != 0u)
    {
        return ConsoleResult::InvalidArguments;
    }

    acquireLock();
    for (gp::USize i = 0u; i < m_storage.hashSlotCapacity; ++i)
    {
        m_storage.hashSlots[i] = internal::ConsoleHashSlot{};
    }
    for (gp::USize i = 0u; i < m_storage.sinkCapacity; ++i)
    {
        m_storage.sinkSlots[i] = internal::ConsoleSinkSlot{};
    }
    m_objectCount = 0u;
    m_dirtyRenderCount = 0u;
    m_dirtyLatchedCount = 0u;
    m_refVariableCount = 0u;
    m_dirtyRenderOverflow = false;
    m_dirtyLatchedOverflow = false;
    m_sinksDirty.store(false, gp::MemoryOrder::Release);
    m_applyCliOnRegister = desc.applyCommandLineOverrides;
    m_ownsBuiltInCommands = desc.registerBuiltInCommands;
    m_cliOverrideSetBy = ConsoleVarSetBy::CommandLine;

    m_variableCount.store(0u, gp::MemoryOrder::Release);
    m_commandCount.store(0u, gp::MemoryOrder::Release);
    m_sinkCount.store(0u, gp::MemoryOrder::Release);
    m_consoleEnabled.store(desc.enableConsole, gp::MemoryOrder::Release);
    m_cheatsEnabled.store(desc.enableCheats, gp::MemoryOrder::Release);
    m_isInitialized.store(true, gp::MemoryOrder::Release);
    releaseLock();

    if (desc.registerBuiltInCommands)
    {
        registerBuiltInDiagnostics();
    }

    if (desc.registerDeferredStaticObjects)
    {
        drainGlobalDeferredQueue();
    }

    if (desc.applyCommandLineOverrides)
    {
        (void)applyCommandLineOverrides(ConsoleVarSetBy::CommandLine);
    }

    return ConsoleResult::Success;
}

void ConsoleManagerBase::shutdown() noexcept
{
    if (!m_isInitialized.load(gp::MemoryOrder::Acquire))
    {
        return;
    }

    constexpr gp::USize kDetachBatchSize = 256u;
    IConsoleObject* detached[kDetachBatchSize]{};
    gp::USize detachedCount = 0u;

    do
    {
        detachedCount = 0u;
        acquireLock();
        for (gp::USize i = 0u; i < m_storage.hashSlotCapacity && detachedCount < kDetachBatchSize; ++i)
        {
            if (m_storage.hashSlots[i].object != nullptr)
            {
                IConsoleObject* const obj = m_storage.hashSlots[i].object;
                if (removeObjectLocked(obj))
                {
                    detached[detachedCount++] = obj;
                }
            }
        }
        releaseLock();

        for (gp::USize i = 0u; i < detachedCount; ++i)
        {
            detached[i]->onUnregistered();
        }
    }
    while (detachedCount > 0u);

    acquireLock();
    for (gp::USize i = 0u; i < m_storage.dynamicVariableCapacity; ++i)
    {
        if (m_storage.dynamicVariables[i].isInUse())
        {
            m_storage.dynamicVariables[i].releaseSlot();
        }
    }
    for (gp::USize i = 0u; i < m_storage.dynamicCommandCapacity; ++i)
    {
        if (m_storage.dynamicCommands[i].isInUse())
        {
            m_storage.dynamicCommands[i].releaseSlot();
        }
    }
    for (gp::USize i = 0u; i < m_storage.sinkCapacity; ++i)
    {
        internal::ConsoleSinkSlot& slot = m_storage.sinkSlots[i];
        slot.callback.unbind();
        slot.handle.reset();
        slot.isActive = false;
    }

    m_objectCount = 0u;
    m_dirtyRenderCount = 0u;
    m_dirtyLatchedCount = 0u;
    m_refVariableCount = 0u;
    m_dirtyRenderOverflow = false;
    m_dirtyLatchedOverflow = false;
    m_sinksDirty.store(false, gp::MemoryOrder::Release);
    m_applyCliOnRegister = false;
    m_variableCount.store(0u, gp::MemoryOrder::Release);
    m_commandCount.store(0u, gp::MemoryOrder::Release);
    m_sinkCount.store(0u, gp::MemoryOrder::Release);
    m_isInitialized.store(false, gp::MemoryOrder::Release);
    releaseLock();

    if (m_isGlobalSingleton)
    {
        g_globalInitialized.store(false, gp::MemoryOrder::Release);
    }
}

void ConsoleManagerBase::drainGlobalDeferredQueue() noexcept
{
    GP_PROFILE_SCOPE_NC_ACTIVE(
        "Console::DrainDeferredQueue", GP_U32_COLOR_CYAN, gp::profiling::ProfilingControl::isZonesEnabled()
    );

    while (true)
    {
        internal::ConsoleObjectBase* batchHead = nullptr;
        acquireFlagLock(g_deferredObjects.lock);
        batchHead = g_deferredObjects.head;
        g_deferredObjects.head = nullptr;
        releaseFlagLock(g_deferredObjects.lock);

        if (batchHead == nullptr)
        {
            break;
        }

        while (batchHead != nullptr)
        {
            internal::ConsoleObjectBase* const next = batchHead->m_deferredNext;
            batchHead->m_deferredNext = nullptr;
            batchHead->m_isInDeferredList = false;
            (void)registerExternalObject(batchHead->asConsoleObject());
            batchHead = next;
        }
    }

    acquireFlagLock(g_deferredSinks.lock);
    for (gp::USize i = 0u; i < kMaxConsoleSinks; ++i)
    {
        DeferredSinkEntry& entry = g_deferredSinks.entries[i];
        if (entry.active && !entry.migrated && entry.callback.isBound())
        {
            const gp::DelegateHandle live = registerConsoleVariableSink(entry.callback);
            if (live.isValid())
            {
                entry.liveHandle = live;
                entry.migrated = true;
            }
        }
    }
    releaseFlagLock(g_deferredSinks.lock);
}

void ConsoleManagerBase::registerBuiltInDiagnostics() noexcept
{
    (void)registerConsoleCommand(
        "help",
        "Lists visible console variables and commands matching an optional prefix.",
        ConsoleCommandWithArgsDelegate::createLambda(
            [this](const ConsoleCommandArgs& args) noexcept -> void
    {
        const gp::StringView prefix = args.getArgCount() > 0u ? args.getArg(0u) : gp::StringView{};
        forEachConsoleObjectStartingWith(
            ConsoleObjectVisitor::createLambda(
                [&args](IConsoleObject* const obj) noexcept -> bool
        {
            gp::container::BasicFixedString<char, 511u> line{};
            if (const IConsoleVariable* const var = obj->asVariable())
            {
                formatVariableSummary(*var, line);
            }
            else
            {
                line.append(obj->getName());
                line.append(" [Command]");
                if (!obj->getHelp().isEmpty())
                {
                    line.append(" : ");
                    const gp::USize rem = (line.capacity() > line.size()) ? (line.capacity() - line.size()) : 0u;
                    line.append(obj->getHelp().substr(0u, rem));
                }
            }
            args.writeLine(line.view());
            return true;
        }
            ),
            prefix,
            false
        );
    }
        ),
        ConsoleVarFlags::None
    );

    (void)registerConsoleCommand(
        "cvar.list",
        "Lists registered console variables matching an optional prefix.",
        ConsoleCommandWithArgsDelegate::createLambda(
            [this](const ConsoleCommandArgs& args) noexcept -> void
    {
        const gp::StringView prefix = args.getArgCount() > 0u ? args.getArg(0u) : gp::StringView{};
        forEachConsoleObjectStartingWith(
            ConsoleObjectVisitor::createLambda(
                [&args](IConsoleObject* const obj) noexcept -> bool
        {
            if (const IConsoleVariable* const var = obj->asVariable())
            {
                gp::container::BasicFixedString<char, 511u> line{};
                formatVariableSummary(*var, line);
                args.writeLine(line.view());
            }
            return true;
        }
            ),
            prefix,
            false
        );
    }
        ),
        ConsoleVarFlags::None
    );

    (void)registerConsoleCommand(
        "cvar.dump",
        "Dumps modified console variables matching an optional prefix.",
        ConsoleCommandWithArgsDelegate::createLambda(
            [this](const ConsoleCommandArgs& args) noexcept -> void
    {
        const gp::StringView prefix = args.getArgCount() > 0u ? args.getArg(0u) : gp::StringView{};
        forEachConsoleObjectStartingWith(
            ConsoleObjectVisitor::createLambda(
                [&args](IConsoleObject* const obj) noexcept -> bool
        {
            if (const IConsoleVariable* const var = obj->asVariable(); var != nullptr && var->isModified())
            {
                gp::container::BasicFixedString<char, 511u> line{};
                formatVariableSummary(*var, line);
                args.writeLine(line.view());
            }
            return true;
        }
            ),
            prefix,
            false
        );
    }
        ),
        ConsoleVarFlags::None
    );

    (void)registerConsoleCommand(
        "cvar.reset",
        "Resets a named console variable or 'all' back to constructor defaults.",
        ConsoleCommandWithArgsDelegate::createLambda(
            [this](const ConsoleCommandArgs& args) noexcept -> void
    {
        if (args.isEmpty())
        {
            args.writeLine("Usage: cvar.reset <name|all>");
            return;
        }

        const gp::StringView target = args.getArg(0u);
        if (target.equalsIgnoreCase("all"))
        {
            (void)resetAllVariables(ConsoleVarSetBy::Console);
            args.writeLine("Reset all console variables.");
            return;
        }

        if (IConsoleVariable* const var = findConsoleVariable(target))
        {
            const ConsoleResult res = var->reset(ConsoleVarSetBy::Console);
            args.writeLine(consoleResultToString(res));
        }
        else
        {
            args.writeLine("Variable not found.");
        }
    }
        ),
        ConsoleVarFlags::None
    );
}

IConsoleVariable* ConsoleManagerBase::registerConsoleVariable(
    const gp::StringView name, const bool defaultValue, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
{
    internal::ConsoleVarPayload payload{};
    payload.type = ConsoleVarType::Bool;
    payload.boolValue = defaultValue;
    return registerDynamicVariableImpl(ConsoleVarType::Bool, name, payload, help, flags);
}

IConsoleVariable* ConsoleManagerBase::registerConsoleVariable(
    const gp::StringView name, const gp::Int32 defaultValue, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
{
    internal::ConsoleVarPayload payload{};
    payload.type = ConsoleVarType::Int32;
    payload.intValue = defaultValue;
    return registerDynamicVariableImpl(ConsoleVarType::Int32, name, payload, help, flags);
}

IConsoleVariable* ConsoleManagerBase::registerConsoleVariable(
    const gp::StringView name, const gp::UInt32 defaultValue, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
{
    internal::ConsoleVarPayload payload{};
    payload.type = ConsoleVarType::UInt32;
    payload.uintValue = defaultValue;
    return registerDynamicVariableImpl(ConsoleVarType::UInt32, name, payload, help, flags);
}

IConsoleVariable* ConsoleManagerBase::registerConsoleVariable(
    const gp::StringView name, const gp::Float32 defaultValue, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
{
    internal::ConsoleVarPayload payload{};
    payload.type = ConsoleVarType::Float32;
    payload.floatValue = defaultValue;
    return registerDynamicVariableImpl(ConsoleVarType::Float32, name, payload, help, flags);
}

IConsoleVariable* ConsoleManagerBase::registerConsoleVariable(
    const gp::StringView name, const gp::Float64 defaultValue, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
{
    internal::ConsoleVarPayload payload{};
    payload.type = ConsoleVarType::Float64;
    payload.doubleValue = defaultValue;
    return registerDynamicVariableImpl(ConsoleVarType::Float64, name, payload, help, flags);
}

IConsoleVariable* ConsoleManagerBase::registerConsoleVariable(
    const gp::StringView name, const gp::StringView defaultValue, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
{
    internal::ConsoleVarPayload payload{};
    payload.type = ConsoleVarType::String;
    const gp::StringView clamped =
        defaultValue.size() > kMaxConsoleStringLength ? defaultValue.substr(0u, kMaxConsoleStringLength) : defaultValue;
    payload.stringValue.assign(clamped);
    return registerDynamicVariableImpl(ConsoleVarType::String, name, payload, help, flags);
}

IConsoleVariable* ConsoleManagerBase::registerConsoleVariableRef(
    const gp::StringView name, bool& refValue, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
{
    return registerDynamicVariableRefImpl(ConsoleVarType::Bool, name, &refValue, false, help, flags);
}

IConsoleVariable* ConsoleManagerBase::registerConsoleVariableRef(
    const gp::StringView name, gp::Int32& refValue, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
{
    return registerDynamicVariableRefImpl(ConsoleVarType::Int32, name, &refValue, false, help, flags);
}

IConsoleVariable* ConsoleManagerBase::registerConsoleVariableRef(
    const gp::StringView name, gp::UInt32& refValue, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
{
    return registerDynamicVariableRefImpl(ConsoleVarType::UInt32, name, &refValue, false, help, flags);
}

IConsoleVariable* ConsoleManagerBase::registerConsoleVariableRef(
    const gp::StringView name, gp::Float32& refValue, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
{
    return registerDynamicVariableRefImpl(ConsoleVarType::Float32, name, &refValue, false, help, flags);
}

IConsoleVariable* ConsoleManagerBase::registerConsoleVariableRef(
    const gp::StringView name, gp::Float64& refValue, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
{
    return registerDynamicVariableRefImpl(ConsoleVarType::Float64, name, &refValue, false, help, flags);
}

IConsoleVariable* ConsoleManagerBase::registerConsoleVariableRef(
    const gp::StringView name, ConsoleString& refValue, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
{
    return registerDynamicVariableRefImpl(ConsoleVarType::String, name, &refValue, false, help, flags);
}

IConsoleVariable* ConsoleManagerBase::registerConsoleVariableRef(
    const gp::StringView name, gp::String& refValue, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
{
    return registerDynamicVariableRefImpl(ConsoleVarType::String, name, &refValue, true, help, flags);
}

internal::ConsoleDynamicVariable*
    ConsoleManagerBase::acquireDynamicVariableSlotLocked(const gp::StringView name) noexcept
{
    const gp::UInt64 nameHash = hashConsoleName(name);
    if (findObjectLocked(name, nameHash) != nullptr)
    {
        return nullptr;
    }

    for (gp::USize i = 0u; i < m_storage.dynamicVariableCapacity; ++i)
    {
        if (!m_storage.dynamicVariables[i].isInUse())
        {
            return &m_storage.dynamicVariables[i];
        }
    }
    return nullptr;
}

internal::ConsoleDynamicCommand* ConsoleManagerBase::acquireDynamicCommandSlotLocked(const gp::StringView name) noexcept
{
    const gp::UInt64 nameHash = hashConsoleName(name);
    if (findObjectLocked(name, nameHash) != nullptr)
    {
        return nullptr;
    }

    for (gp::USize i = 0u; i < m_storage.dynamicCommandCapacity; ++i)
    {
        if (!m_storage.dynamicCommands[i].isInUse())
        {
            return &m_storage.dynamicCommands[i];
        }
    }
    return nullptr;
}

IConsoleVariable* ConsoleManagerBase::registerDynamicVariableImpl(
    const ConsoleVarType type,
    const gp::StringView name,
    const internal::ConsoleVarPayload& defaultValue,
    const gp::StringView help,
    const ConsoleVarFlags flags
) noexcept
{
    if (!m_isInitialized.load(gp::MemoryOrder::Acquire) || name.isEmpty() || name.size() > kMaxConsoleNameLength)
    {
        return nullptr;
    }

    acquireLock();
    internal::ConsoleDynamicVariable* const slot = acquireDynamicVariableSlotLocked(name);
    if (slot == nullptr)
    {
        releaseLock();
        return nullptr;
    }

    slot->initializeOwned(type, name, defaultValue, help, flags);
    if (insertObjectLocked(slot) != ConsoleResult::Success)
    {
        slot->releaseSlot();
        releaseLock();
        return nullptr;
    }

    const bool applyCli = m_applyCliOnRegister;
    const ConsoleVarSetBy cliSetBy = m_cliOverrideSetBy;
    releaseLock();

    slot->onRegistered(this);
    if (applyCli)
    {
        applySingleCommandLineOverride(slot, cliSetBy);
    }
    return slot;
}

IConsoleVariable* ConsoleManagerBase::registerDynamicVariableRefImpl(
    const ConsoleVarType type,
    const gp::StringView name,
    void* const refPtr,
    const bool isGpStringRef,
    const gp::StringView help,
    const ConsoleVarFlags flags
) noexcept
{
    if (!m_isInitialized.load(gp::MemoryOrder::Acquire) || name.isEmpty() || name.size() > kMaxConsoleNameLength ||
        refPtr == nullptr)
    {
        return nullptr;
    }

    acquireLock();
    internal::ConsoleDynamicVariable* const slot = acquireDynamicVariableSlotLocked(name);
    if (slot == nullptr)
    {
        releaseLock();
        return nullptr;
    }

    slot->initializeRef(type, name, refPtr, isGpStringRef, help, flags);
    if (insertObjectLocked(slot) != ConsoleResult::Success)
    {
        slot->releaseSlot();
        releaseLock();
        return nullptr;
    }

    const bool applyCli = m_applyCliOnRegister;
    const ConsoleVarSetBy cliSetBy = m_cliOverrideSetBy;
    releaseLock();

    slot->onRegistered(this);
    if (applyCli)
    {
        applySingleCommandLineOverride(slot, cliSetBy);
    }
    return slot;
}

IConsoleCommand* ConsoleManagerBase::registerConsoleCommand(
    const gp::StringView name, const gp::StringView help, ConsoleCommandDelegate command, const ConsoleVarFlags flags
) noexcept
{
    if (!m_isInitialized.load(gp::MemoryOrder::Acquire) || name.isEmpty() || name.size() > kMaxConsoleNameLength ||
        !command.isBound())
    {
        return nullptr;
    }

    acquireLock();
    internal::ConsoleDynamicCommand* const slot = acquireDynamicCommandSlotLocked(name);
    if (slot == nullptr)
    {
        releaseLock();
        return nullptr;
    }

    slot->initialize(name, help, std::move(command), flags);
    if (insertObjectLocked(slot) != ConsoleResult::Success)
    {
        slot->releaseSlot();
        releaseLock();
        return nullptr;
    }
    releaseLock();

    slot->onRegistered(this);
    return slot;
}

IConsoleCommand* ConsoleManagerBase::registerConsoleCommand(
    const gp::StringView name,
    const gp::StringView help,
    ConsoleCommandWithArgsDelegate command,
    const ConsoleVarFlags flags
) noexcept
{
    if (!m_isInitialized.load(gp::MemoryOrder::Acquire) || name.isEmpty() || name.size() > kMaxConsoleNameLength ||
        !command.isBound())
    {
        return nullptr;
    }

    acquireLock();
    internal::ConsoleDynamicCommand* const slot = acquireDynamicCommandSlotLocked(name);
    if (slot == nullptr)
    {
        releaseLock();
        return nullptr;
    }

    slot->initialize(name, help, std::move(command), flags);
    if (insertObjectLocked(slot) != ConsoleResult::Success)
    {
        slot->releaseSlot();
        releaseLock();
        return nullptr;
    }
    releaseLock();

    slot->onRegistered(this);
    return slot;
}

ConsoleResult ConsoleManagerBase::registerExternalObject(IConsoleObject* const object) noexcept
{
    if (!m_isInitialized.load(gp::MemoryOrder::Acquire) || object == nullptr || object->getName().isEmpty())
    {
        return ConsoleResult::InvalidArguments;
    }

    bool applyCli = false;
    ConsoleVarSetBy cliSetBy = ConsoleVarSetBy::CommandLine;

    acquireLock();
    const ConsoleResult res = insertObjectLocked(object);
    applyCli = m_applyCliOnRegister;
    cliSetBy = m_cliOverrideSetBy;
    releaseLock();

    if (res == ConsoleResult::Success)
    {
        object->onRegistered(this);
        if (applyCli)
        {
            if (IConsoleVariable* const var = object->asVariable())
            {
                applySingleCommandLineOverride(var, cliSetBy);
            }
        }
    }
    return res;
}

bool ConsoleManagerBase::unregisterConsoleObject(const gp::StringView name) noexcept
{
    if (!m_isInitialized.load(gp::MemoryOrder::Acquire) || name.isEmpty())
    {
        return false;
    }

    IConsoleObject* removed = nullptr;
    acquireLock();
    const gp::UInt64 nameHash = hashConsoleName(name);
    removed = findObjectLocked(name, nameHash);
    if (removed != nullptr)
    {
        (void)removeObjectLocked(removed);
    }
    releaseLock();

    if (removed != nullptr)
    {
        removed->onUnregistered();
        return true;
    }
    return false;
}

bool ConsoleManagerBase::unregisterConsoleObject(IConsoleObject* const object) noexcept
{
    if (!m_isInitialized.load(gp::MemoryOrder::Acquire) || object == nullptr)
    {
        return false;
    }

    bool removed = false;
    acquireLock();
    removed = removeObjectLocked(object);
    releaseLock();

    if (removed)
    {
        object->onUnregistered();
    }
    return removed;
}

gp::DelegateHandle ConsoleManagerBase::registerConsoleVariableSink(ConsoleSinkDelegate sink) noexcept
{
    if (!m_isInitialized.load(gp::MemoryOrder::Acquire) || !sink.isBound())
    {
        return gp::DelegateHandle{};
    }

    acquireLock();
    for (gp::USize i = 0u; i < m_storage.sinkCapacity; ++i)
    {
        internal::ConsoleSinkSlot& slot = m_storage.sinkSlots[i];
        if (!slot.isActive)
        {
            const gp::UInt64 rawId =
                (static_cast<gp::UInt64>(slot.generation) << 32u) | static_cast<gp::UInt64>(i + 1u);
            slot.callback = std::move(sink);
            slot.handle = gp::DelegateHandle(rawId);
            slot.isActive = true;
            m_sinkCount.fetchAdd(1u, gp::MemoryOrder::AcqRel);
            releaseLock();
            return slot.handle;
        }
    }
    releaseLock();
    return gp::DelegateHandle{};
}

bool ConsoleManagerBase::unregisterConsoleVariableSink(const gp::DelegateHandle handle) noexcept
{
    if (!m_isInitialized.load(gp::MemoryOrder::Acquire) || !handle.isValid())
    {
        return false;
    }

    const gp::USize idxPlusOne = static_cast<gp::USize>(handle.rawValue() & 0xFFFFFFFFULL);
    if (idxPlusOne == 0u || idxPlusOne > m_storage.sinkCapacity)
    {
        return false;
    }

    acquireLock();
    internal::ConsoleSinkSlot& slot = m_storage.sinkSlots[idxPlusOne - 1u];
    if (!slot.isActive || slot.handle != handle)
    {
        releaseLock();
        return false;
    }

    slot.callback.unbind();
    slot.handle.reset();
    slot.isActive = false;
    ++slot.generation;
    if (slot.generation == 0u)
    {
        slot.generation = 1u;
    }
    m_sinkCount.fetchSub(1u, gp::MemoryOrder::AcqRel);
    releaseLock();
    return true;
}

void ConsoleManagerBase::notifyVariableChanged(IConsoleVariable* const variable) noexcept
{
    if (variable == nullptr || !m_isInitialized.load(gp::MemoryOrder::Acquire))
    {
        return;
    }

    m_sinksDirty.store(true, gp::MemoryOrder::Release);

    acquireLock();
    enqueueUniqueVariableLocked(
        m_storage.dirtyRenderQueue, m_dirtyRenderCount, m_storage.dirtyRenderCapacity, m_dirtyRenderOverflow, variable
    );
    releaseLock();
}

void ConsoleManagerBase::notifyVariableLatched(IConsoleVariable* const variable) noexcept
{
    if (variable == nullptr || !m_isInitialized.load(gp::MemoryOrder::Acquire))
    {
        return;
    }

    acquireLock();
    enqueueUniqueVariableLocked(
        m_storage.dirtyLatchedQueue,
        m_dirtyLatchedCount,
        m_storage.dirtyLatchedCapacity,
        m_dirtyLatchedOverflow,
        variable
    );
    releaseLock();
}

IConsoleObject* ConsoleManagerBase::findConsoleObject(const gp::StringView name) const noexcept
{
    if (!m_isInitialized.load(gp::MemoryOrder::Acquire))
    {
        return nullptr;
    }

    if (m_isGlobalSingleton)
    {
        const_cast<ConsoleManagerBase*>(this)->drainGlobalDeferredQueue();
    }

    if (name.isEmpty())
    {
        return nullptr;
    }

    const gp::UInt64 nameHash = hashConsoleName(name);
    acquireLock();
    IConsoleObject* const found = findObjectLocked(name, nameHash);
    releaseLock();
    return found;
}

IConsoleVariable* ConsoleManagerBase::findConsoleVariable(const gp::StringView name) const noexcept
{
    IConsoleObject* const obj = findConsoleObject(name);
    return (obj != nullptr) ? obj->asVariable() : nullptr;
}

IConsoleCommand* ConsoleManagerBase::findConsoleCommand(const gp::StringView name) const noexcept
{
    IConsoleObject* const obj = findConsoleObject(name);
    return (obj != nullptr) ? obj->asCommand() : nullptr;
}

void ConsoleManagerBase::forEachConsoleObjectStartingWith(
    const ConsoleObjectVisitor& visitor, const gp::StringView prefix, const bool includeInvisible
) const noexcept
{
    if (!m_isInitialized.load(gp::MemoryOrder::Acquire) || !visitor.isBound())
    {
        return;
    }

    if (m_isGlobalSingleton)
    {
        const_cast<ConsoleManagerBase*>(this)->drainGlobalDeferredQueue();
    }

    constexpr gp::USize kChunkCap = 256u;
    IConsoleObject* snapshot[kChunkCap]{};
    gp::USize slotCursor = 0u;

    while (slotCursor < m_storage.hashSlotCapacity)
    {
        gp::USize snapshotCount = 0u;
        acquireLock();
        while (slotCursor < m_storage.hashSlotCapacity && snapshotCount < kChunkCap)
        {
            IConsoleObject* const obj = m_storage.hashSlots[slotCursor++].object;
            if (obj == nullptr)
            {
                continue;
            }
            if (!includeInvisible && obj->hasFlags(ConsoleVarFlags::Invisible))
            {
                continue;
            }
            if (!prefix.isEmpty() && !obj->getName().startsWith(prefix, gp::SearchCase::IgnoreCase))
            {
                continue;
            }
            snapshot[snapshotCount++] = obj;
        }
        releaseLock();

        for (gp::USize i = 0u; i < snapshotCount; ++i)
        {
            if (!visitor.execute(snapshot[i]))
            {
                return;
            }
        }
    }
}

gp::USize ConsoleManagerBase::autoComplete(
    const gp::StringView prefix, gp::StringView* const outNames, const gp::USize maxResults, const bool includeInvisible
) const noexcept
{
    if (!m_isInitialized.load(gp::MemoryOrder::Acquire) || outNames == nullptr || maxResults == 0u)
    {
        return 0u;
    }

    if (m_isGlobalSingleton)
    {
        const_cast<ConsoleManagerBase*>(this)->drainGlobalDeferredQueue();
    }

    gp::USize count = 0u;
    acquireLock();
    for (gp::USize i = 0u; i < m_storage.hashSlotCapacity; ++i)
    {
        const IConsoleObject* const obj = m_storage.hashSlots[i].object;
        if (obj == nullptr)
        {
            continue;
        }
        if (!includeInvisible && obj->hasFlags(ConsoleVarFlags::Invisible))
        {
            continue;
        }

        const gp::StringView name = obj->getName();
        if (!prefix.isEmpty() && !name.startsWith(prefix, gp::SearchCase::IgnoreCase))
        {
            continue;
        }

        if (count == maxResults && name.compareIgnoreCase(outNames[count - 1u]) >= 0)
        {
            continue;
        }

        gp::USize insertPos = 0u;
        while (insertPos < count && outNames[insertPos].compareIgnoreCase(name) <= 0)
        {
            ++insertPos;
        }

        const gp::USize newCount = (count < maxResults) ? (count + 1u) : maxResults;
        for (gp::USize shift = newCount - 1u; shift > insertPos; --shift)
        {
            outNames[shift] = outNames[shift - 1u];
        }
        outNames[insertPos] = name;
        count = newCount;
    }
    releaseLock();

    return count;
}

ConsoleResult ConsoleManagerBase::processUserConsoleInput(
    const gp::StringView input, ConsoleOutputDelegate* const output
) noexcept
{
    GP_PROFILE_SCOPE_NC_ACTIVE(
        "Console::ProcessInput", GP_U32_COLOR_CYAN, gp::profiling::ProfilingControl::isZonesEnabled()
    );

    if (!m_isInitialized.load(gp::MemoryOrder::Acquire))
    {
        return ConsoleResult::NotFound;
    }

    if (!m_consoleEnabled.load(gp::MemoryOrder::Acquire))
    {
        emitConsoleLine(output, "Console is disabled.");
        return ConsoleResult::ConsoleDisabled;
    }

    const gp::StringView trimmed = trimConsoleWhitespace(input);
    if (trimmed.isEmpty())
    {
        return ConsoleResult::InvalidArguments;
    }

    // Handle `cvar=val`, `cvar = val`, `cvar =val`, and `cvar= val` uniformly before general argument splitting.
    gp::USize identEnd = 0u;
    while (identEnd < trimmed.size() && !isWhitespaceChar(trimmed[identEnd]) && trimmed[identEnd] != '=' &&
           trimmed[identEnd] != '"' && trimmed[identEnd] != '\'')
    {
        ++identEnd;
    }
    gp::USize eqProbe = identEnd;
    while (eqProbe < trimmed.size() && isWhitespaceChar(trimmed[eqProbe]))
    {
        ++eqProbe;
    }
    if (identEnd > 0u && eqProbe < trimmed.size() && trimmed[eqProbe] == '=')
    {
        const gp::StringView lhs = trimmed.substr(0u, identEnd);
        IConsoleObject* const inlineObj = findConsoleObject(lhs);
        if (inlineObj == nullptr)
        {
            emitConsoleLine(output, "Unknown console command or variable.");
            return ConsoleResult::NotFound;
        }
        if (IConsoleVariable* const inlineVar = inlineObj->asVariable())
        {
            const gp::StringView rhsRaw = trimConsoleWhitespace(trimmed.substr(eqProbe + 1u));
            if (rhsRaw.isEmpty())
            {
                emitConsoleLine(output, consoleResultToString(ConsoleResult::InvalidArguments));
                return ConsoleResult::InvalidArguments;
            }
            const gp::StringView rhs = stripOuterQuotes(rhsRaw);
            const ConsoleResult inlineRes = inlineVar->setFromString(rhs, ConsoleVarSetBy::Console);
            if (inlineRes == ConsoleResult::Success)
            {
                emitConsoleTraceBookmark("[CVar] ", inlineVar->getName(), rhs);
            }
            else if (output != nullptr && output->isBound())
            {
                emitConsoleLine(output, consoleResultToString(inlineRes));
            }
            return inlineRes;
        }
    }

    ConsoleCommandArgs parsedArgs{};
    if (!ConsoleCommandArgs::parse(trimmed, parsedArgs, output))
    {
        return ConsoleResult::ParseFailed;
    }

    IConsoleObject* const obj = findConsoleObject(parsedArgs.getCommandName());
    if (obj == nullptr)
    {
        emitConsoleLine(output, "Unknown console command or variable.");
        return ConsoleResult::NotFound;
    }

    if (IConsoleCommand* const cmd = obj->asCommand())
    {
        const ConsoleResult cmdRes = cmd->execute(parsedArgs);
        if (cmdRes == ConsoleResult::Success)
        {
            emitConsoleTraceBookmark("[Cmd] ", cmd->getName());
        }
        return cmdRes;
    }

    IConsoleVariable* const var = obj->asVariable();
    if (var == nullptr)
    {
        return ConsoleResult::NotFound;
    }

    if (parsedArgs.isEmpty() || parsedArgs.getArg(0u) == "?")
    {
        gp::container::BasicFixedString<char, 511u> summary{};
        formatVariableSummary(*var, summary);
        emitConsoleLine(output, summary.view());
        return ConsoleResult::Success;
    }

    const gp::StringView valueText = parsedArgs.getArg(0u);
    const ConsoleResult res = var->setFromString(valueText, ConsoleVarSetBy::Console);
    if (res == ConsoleResult::Success)
    {
        emitConsoleTraceBookmark("[CVar] ", var->getName(), valueText);
    }
    else if (output != nullptr && output->isBound())
    {
        emitConsoleLine(output, consoleResultToString(res));
    }
    return res;
}

gp::USize ConsoleManagerBase::applyCommandLineOverrides(const ConsoleVarSetBy setBy) noexcept
{
    GP_PROFILE_SCOPE_NC_ACTIVE(
        "Console::ApplyCliOverrides", GP_U32_COLOR_CYAN, gp::profiling::ProfilingControl::isZonesEnabled()
    );

    if (!m_isInitialized.load(gp::MemoryOrder::Acquire) || !gp::CommandLine::isInitialized())
    {
        return 0u;
    }

    acquireLock();
    m_applyCliOnRegister = true;
    m_cliOverrideSetBy = setBy;
    releaseLock();

    gp::USize appliedCount = 0u;
    const gp::UInt32 argCount = gp::CommandLine::getArgumentCount();

    for (gp::UInt32 i = 1u; i < argCount; ++i)
    {
        const gp::StringView token = gp::CommandLine::getArgument(i);
        if (token.isEmpty())
        {
            continue;
        }

        if (token[0u] == '+')
        {
            const gp::StringView body = token.substr(1u);
            if (body.isEmpty())
            {
                continue;
            }

            gp::StringView key{};
            gp::StringView value{};
            bool hasValue = splitKeyValueToken(body, key, value);
            if (!hasValue)
            {
                key = body;
                if (i + 1u < argCount)
                {
                    const gp::StringView nextTok = gp::CommandLine::getArgument(i + 1u);
                    if (!nextTok.isEmpty() && nextTok[0u] != '+' && nextTok[0u] != '-')
                    {
                        value = stripOuterQuotes(nextTok);
                        hasValue = true;
                        ++i;
                    }
                }
            }

            if (IConsoleVariable* const var = findConsoleVariable(key))
            {
                if (!hasValue && var->getType() == ConsoleVarType::Bool)
                {
                    if (var->set(true, setBy) == ConsoleResult::Success)
                    {
                        ++appliedCount;
                    }
                }
                else if (hasValue && var->setFromString(value, setBy) == ConsoleResult::Success)
                {
                    ++appliedCount;
                }
            }
            else if (IConsoleCommand* const cmd = findConsoleCommand(key))
            {
                ConsoleCommandArgs cmdArgs{};
                if (hasValue)
                {
                    gp::container::BasicFixedString<char, kMaxConsoleCommandLineLength> rebuilt{};
                    rebuilt.append(key);
                    rebuilt.pushBack(' ');
                    rebuilt.append(value);
                    (void)ConsoleCommandArgs::parse(rebuilt.view(), cmdArgs, nullptr);
                }
                else
                {
                    (void)ConsoleCommandArgs::parse(key, cmdArgs, nullptr);
                }
                if (cmd->execute(cmdArgs) == ConsoleResult::Success)
                {
                    ++appliedCount;
                }
            }
        }
        else if (token[0u] == '-')
        {
            gp::StringView body = token.substr(1u);
            if (!body.isEmpty() && body[0u] == '-')
            {
                body = body.substr(1u);
            }

            gp::StringView key{};
            gp::StringView value{};
            if (!splitKeyValueToken(body, key, value))
            {
                if (body.equalsIgnoreCase("ExecCmds") && i + 1u < argCount)
                {
                    key = body;
                    value = stripOuterQuotes(gp::CommandLine::getArgument(++i));
                }
                else
                {
                    continue;
                }
            }

            if (key.equalsIgnoreCase("ExecCmds"))
            {
                gp::USize start = 0u;
                while (start < value.size())
                {
                    const gp::USize semi = value.find(';', start);
                    const gp::USize len = (semi == gp::StringView::npos) ? (value.size() - start) : (semi - start);
                    const gp::StringView subCmd = trimConsoleWhitespace(value.substr(start, len));
                    if (!subCmd.isEmpty() && processUserConsoleInput(subCmd, nullptr) == ConsoleResult::Success)
                    {
                        ++appliedCount;
                    }
                    if (semi == gp::StringView::npos)
                    {
                        break;
                    }
                    start = semi + 1u;
                }
                continue;
            }

            if (IConsoleVariable* const var = findConsoleVariable(key))
            {
                if (var->setFromString(value, setBy) == ConsoleResult::Success)
                {
                    ++appliedCount;
                }
            }
        }
    }

    return appliedCount;
}

void ConsoleManagerBase::applySingleCommandLineOverride(
    IConsoleVariable* const variable, const ConsoleVarSetBy setBy
) noexcept
{
    if (variable == nullptr || !gp::CommandLine::isInitialized())
    {
        return;
    }

    const gp::StringView targetName = variable->getName();
    const gp::UInt32 argCount = gp::CommandLine::getArgumentCount();

    for (gp::UInt32 i = 1u; i < argCount; ++i)
    {
        const gp::StringView token = gp::CommandLine::getArgument(i);
        if (token.isEmpty())
        {
            continue;
        }

        if (token[0u] == '+')
        {
            const gp::StringView body = token.substr(1u);
            gp::StringView key{};
            gp::StringView value{};
            bool hasValue = splitKeyValueToken(body, key, value);
            if (!hasValue)
            {
                key = body;
                if (i + 1u < argCount)
                {
                    const gp::StringView nextTok = gp::CommandLine::getArgument(i + 1u);
                    if (!nextTok.isEmpty() && nextTok[0u] != '+' && nextTok[0u] != '-')
                    {
                        value = stripOuterQuotes(nextTok);
                        hasValue = true;
                        ++i;
                    }
                }
            }

            if (key.equalsIgnoreCase(targetName))
            {
                if (!hasValue && variable->getType() == ConsoleVarType::Bool)
                {
                    (void)variable->set(true, setBy);
                }
                else if (hasValue)
                {
                    (void)variable->setFromString(value, setBy);
                }
            }
        }
        else if (token[0u] == '-')
        {
            gp::StringView body = token.substr(1u);
            if (!body.isEmpty() && body[0u] == '-')
            {
                body = body.substr(1u);
            }
            gp::StringView key{};
            gp::StringView value{};
            if (splitKeyValueToken(body, key, value) && key.equalsIgnoreCase(targetName))
            {
                (void)variable->setFromString(value, setBy);
            }
        }
    }
}

gp::USize ConsoleManagerBase::syncRenderThreadValues() noexcept
{
    if (!m_isInitialized.load(gp::MemoryOrder::Acquire))
    {
        return 0u;
    }

    GP_PROFILE_SCOPE_NC_ACTIVE(
        "Console::SyncRenderThreadValues", GP_U32_COLOR_CYAN, gp::profiling::ProfilingControl::isZonesEnabled()
    );

    constexpr gp::USize kBatchCap = 256u;
    IConsoleVariable* toSync[kBatchCap]{};
    gp::USize actualSynced = 0u;
    const gp::ScopeExit plotTelemetry(
        [this, &actualSynced]() noexcept -> void
    {
        const bool shouldPlot = m_isGlobalSingleton && gp::profiling::ProfilingControl::isPlotsEnabled();
        GP_PLOT_IF(shouldPlot, "Console::SyncedRenderVars", static_cast<gp::Int64>(actualSynced));
        GP_PLOT_IF(
            shouldPlot, "Console::Variables", static_cast<gp::Int64>(m_variableCount.load(gp::MemoryOrder::Relaxed))
        );
    }
    );

    gp::USize refCursor = 0u;
    gp::USize firstBatchCount = 0u;
    bool overflowScan = false;
    bool morePending = false;

    acquireLock();
    overflowScan = m_dirtyRenderOverflow;
    m_dirtyRenderOverflow = false;
    if (overflowScan)
    {
        m_dirtyRenderCount = 0u;
    }
    else
    {
        while (refCursor < m_refVariableCount && firstBatchCount < kBatchCap)
        {
            toSync[firstBatchCount++] = m_storage.refVariables[refCursor++];
        }
        while (m_dirtyRenderCount > 0u && firstBatchCount < kBatchCap)
        {
            IConsoleVariable* const var = m_storage.dirtyRenderQueue[--m_dirtyRenderCount];
            m_storage.dirtyRenderQueue[m_dirtyRenderCount] = nullptr;
            if (var != nullptr && !var->isExternalReference() && var->hasPendingRenderSync())
            {
                toSync[firstBatchCount++] = var;
            }
        }
        morePending = (refCursor < m_refVariableCount) || (m_dirtyRenderCount > 0u);
    }
    releaseLock();

    if (overflowScan)
    {
        gp::USize slotCursor = 0u;
        while (slotCursor < m_storage.hashSlotCapacity)
        {
            gp::USize batchCount = 0u;
            acquireLock();
            while (slotCursor < m_storage.hashSlotCapacity && batchCount < kBatchCap)
            {
                IConsoleObject* const obj = m_storage.hashSlots[slotCursor++].object;
                if (obj == nullptr)
                {
                    continue;
                }
                if (IConsoleVariable* const var = obj->asVariable();
                    var != nullptr && (var->hasPendingRenderSync() || var->isExternalReference()))
                {
                    toSync[batchCount++] = var;
                }
            }
            releaseLock();

            for (gp::USize i = 0u; i < batchCount; ++i)
            {
                toSync[i]->syncRenderThreadValue();
                ++actualSynced;
            }
        }
        return actualSynced;
    }

    for (gp::USize i = 0u; i < firstBatchCount; ++i)
    {
        toSync[i]->syncRenderThreadValue();
        ++actualSynced;
    }

    if (!morePending)
    {
        return actualSynced;
    }

    while (true)
    {
        gp::USize refBatchCount = 0u;
        acquireLock();
        while (refCursor < m_refVariableCount && refBatchCount < kBatchCap)
        {
            toSync[refBatchCount++] = m_storage.refVariables[refCursor++];
        }
        releaseLock();

        if (refBatchCount == 0u)
        {
            break;
        }

        for (gp::USize i = 0u; i < refBatchCount; ++i)
        {
            toSync[i]->syncRenderThreadValue();
            ++actualSynced;
        }
    }

    while (true)
    {
        gp::USize queueBatchCount = 0u;
        acquireLock();
        while (m_dirtyRenderCount > 0u && queueBatchCount < kBatchCap)
        {
            IConsoleVariable* const var = m_storage.dirtyRenderQueue[--m_dirtyRenderCount];
            m_storage.dirtyRenderQueue[m_dirtyRenderCount] = nullptr;
            if (var != nullptr && !var->isExternalReference() && var->hasPendingRenderSync())
            {
                toSync[queueBatchCount++] = var;
            }
        }
        releaseLock();

        if (queueBatchCount == 0u)
        {
            break;
        }

        for (gp::USize i = 0u; i < queueBatchCount; ++i)
        {
            if (toSync[i]->hasPendingRenderSync())
            {
                toSync[i]->syncRenderThreadValue();
                ++actualSynced;
            }
        }
    }

    return actualSynced;
}

gp::USize ConsoleManagerBase::flushLatchedVariables() noexcept
{
    if (!m_isInitialized.load(gp::MemoryOrder::Acquire))
    {
        return 0u;
    }

    GP_PROFILE_SCOPE_NC_ACTIVE(
        "Console::FlushLatched", GP_U32_COLOR_CYAN, gp::profiling::ProfilingControl::isZonesEnabled()
    );

    constexpr gp::USize kBatchCap = 256u;
    IConsoleVariable* toFlush[kBatchCap]{};
    gp::USize applied = 0u;
    const gp::ScopeExit plotTelemetry(
        [this, &applied]() noexcept -> void
    {
        const bool shouldPlot = m_isGlobalSingleton && gp::profiling::ProfilingControl::isPlotsEnabled();
        GP_PLOT_IF(shouldPlot, "Console::FlushedLatched", static_cast<gp::Int64>(applied));
    }
    );

    gp::USize firstBatchCount = 0u;
    bool overflowScan = false;
    bool morePending = false;

    acquireLock();
    overflowScan = m_dirtyLatchedOverflow;
    m_dirtyLatchedOverflow = false;
    if (overflowScan)
    {
        m_dirtyLatchedCount = 0u;
    }
    else
    {
        while (m_dirtyLatchedCount > 0u && firstBatchCount < kBatchCap)
        {
            IConsoleVariable* const var = m_storage.dirtyLatchedQueue[--m_dirtyLatchedCount];
            m_storage.dirtyLatchedQueue[m_dirtyLatchedCount] = nullptr;
            if (var != nullptr)
            {
                toFlush[firstBatchCount++] = var;
            }
        }
        morePending = (m_dirtyLatchedCount > 0u);
    }
    releaseLock();

    if (overflowScan)
    {
        gp::USize slotCursor = 0u;
        while (slotCursor < m_storage.hashSlotCapacity)
        {
            gp::USize batchCount = 0u;
            acquireLock();
            while (slotCursor < m_storage.hashSlotCapacity && batchCount < kBatchCap)
            {
                IConsoleObject* const obj = m_storage.hashSlots[slotCursor++].object;
                if (obj == nullptr)
                {
                    continue;
                }
                if (IConsoleVariable* const var = obj->asVariable(); var != nullptr && var->hasPendingLatchedValue())
                {
                    toFlush[batchCount++] = var;
                }
            }
            releaseLock();

            for (gp::USize i = 0u; i < batchCount; ++i)
            {
                if (toFlush[i]->applyLatchedValue())
                {
                    ++applied;
                }
            }
        }
        return applied;
    }

    for (gp::USize i = 0u; i < firstBatchCount; ++i)
    {
        if (toFlush[i]->applyLatchedValue())
        {
            ++applied;
        }
    }

    if (!morePending)
    {
        return applied;
    }

    while (true)
    {
        gp::USize batchCount = 0u;
        acquireLock();
        while (m_dirtyLatchedCount > 0u && batchCount < kBatchCap)
        {
            IConsoleVariable* const var = m_storage.dirtyLatchedQueue[--m_dirtyLatchedCount];
            m_storage.dirtyLatchedQueue[m_dirtyLatchedCount] = nullptr;
            if (var != nullptr)
            {
                toFlush[batchCount++] = var;
            }
        }
        releaseLock();

        if (batchCount == 0u)
        {
            break;
        }

        for (gp::USize i = 0u; i < batchCount; ++i)
        {
            if (toFlush[i]->applyLatchedValue())
            {
                ++applied;
            }
        }
    }

    return applied;
}

gp::USize ConsoleManagerBase::callAllConsoleVariableSinks(const bool force) noexcept
{
    if (!m_isInitialized.load(gp::MemoryOrder::Acquire))
    {
        return 0u;
    }

    GP_PROFILE_SCOPE_NC_ACTIVE(
        "Console::CallSinks", GP_U32_COLOR_CYAN, gp::profiling::ProfilingControl::isZonesEnabled()
    );

    if (m_isGlobalSingleton)
    {
        drainGlobalDeferredQueue();
    }

    if (!force && !m_sinksDirty.exchange(false, gp::MemoryOrder::AcqRel))
    {
        return 0u;
    }
    if (force)
    {
        m_sinksDirty.store(false, gp::MemoryOrder::Release);
    }

    constexpr gp::USize kChunkCap = 128u;
    gp::DelegateHandle handles[kChunkCap]{};
    ConsoleSinkDelegate callbacks[kChunkCap]{};
    gp::USize slotCursor = 0u;
    gp::USize invoked = 0u;
    const gp::ScopeExit plotTelemetry(
        [this, &invoked]() noexcept -> void
    {
        const bool shouldPlot = m_isGlobalSingleton && gp::profiling::ProfilingControl::isPlotsEnabled();
        GP_PLOT_IF(shouldPlot, "Console::SinksExecuted", static_cast<gp::Int64>(invoked));
    }
    );

    while (slotCursor < m_storage.sinkCapacity)
    {
        gp::USize chunkCount = 0u;
        acquireLock();
        while (slotCursor < m_storage.sinkCapacity && chunkCount < kChunkCap)
        {
            const internal::ConsoleSinkSlot& slot = m_storage.sinkSlots[slotCursor++];
            if (slot.isActive && slot.callback.isBound())
            {
                handles[chunkCount] = slot.handle;
                callbacks[chunkCount] = slot.callback;
                ++chunkCount;
            }
        }
        releaseLock();

        for (gp::USize i = 0u; i < chunkCount; ++i)
        {
            const gp::USize slotIdx = static_cast<gp::USize>(handles[i].rawValue() & 0xFFFFFFFFULL) - 1u;
            bool stillLive = false;
            acquireLock();
            if (slotIdx < m_storage.sinkCapacity)
            {
                const internal::ConsoleSinkSlot& currentSlot = m_storage.sinkSlots[slotIdx];
                stillLive = currentSlot.isActive && currentSlot.handle == handles[i];
            }
            releaseLock();

            if (stillLive)
            {
                callbacks[i].execute();
                ++invoked;
            }
        }
    }

    return invoked;
}

gp::USize ConsoleManagerBase::resetAllVariables(const ConsoleVarSetBy maxPriority) noexcept
{
    if (!m_isInitialized.load(gp::MemoryOrder::Acquire))
    {
        return 0u;
    }

    constexpr gp::USize kBatchCap = 256u;
    IConsoleVariable* vars[kBatchCap]{};
    gp::USize slotCursor = 0u;
    gp::USize resetCount = 0u;

    while (slotCursor < m_storage.hashSlotCapacity)
    {
        gp::USize batchCount = 0u;
        acquireLock();
        while (slotCursor < m_storage.hashSlotCapacity && batchCount < kBatchCap)
        {
            IConsoleObject* const obj = m_storage.hashSlots[slotCursor++].object;
            if (obj == nullptr)
            {
                continue;
            }
            if (IConsoleVariable* const var = obj->asVariable())
            {
                vars[batchCount++] = var;
            }
        }
        releaseLock();

        for (gp::USize i = 0u; i < batchCount; ++i)
        {
            if (static_cast<gp::UInt8>(vars[i]->getSetBy()) <= static_cast<gp::UInt8>(maxPriority))
            {
                if (vars[i]->reset(maxPriority) == ConsoleResult::Success)
                {
                    ++resetCount;
                }
            }
        }
    }

    return resetCount;
}

IConsoleObject*
    ConsoleManagerBase::findObjectLocked(const gp::StringView name, const gp::UInt64 nameHash) const noexcept
{
    gp::USize idx = static_cast<gp::USize>(nameHash) & m_hashMask;
    for (gp::USize probe = 0u; probe < m_storage.hashSlotCapacity; ++probe)
    {
        const internal::ConsoleHashSlot& slot = m_storage.hashSlots[idx];
        if (slot.object == nullptr)
        {
            return nullptr;
        }
        if (slot.nameHash == nameHash && slot.object->getName().equalsIgnoreCase(name))
        {
            return slot.object;
        }
        idx = (idx + 1u) & m_hashMask;
    }
    return nullptr;
}

ConsoleResult ConsoleManagerBase::insertObjectLocked(IConsoleObject* const object) noexcept
{
    if (object == nullptr || object->getName().isEmpty())
    {
        return ConsoleResult::InvalidArguments;
    }

    // Reserve at least one empty slot so linear probing always terminates cleanly.
    if (m_objectCount + 1u >= m_storage.hashSlotCapacity)
    {
        return ConsoleResult::CapacityExceeded;
    }

    const gp::StringView name = object->getName();
    const gp::UInt64 nameHash = object->getNameHash();
    gp::USize idx = static_cast<gp::USize>(nameHash) & m_hashMask;

    for (gp::USize probe = 0u; probe < m_storage.hashSlotCapacity; ++probe)
    {
        internal::ConsoleHashSlot& slot = m_storage.hashSlots[idx];
        if (slot.object == nullptr)
        {
            slot.nameHash = nameHash;
            slot.object = object;
            ++m_objectCount;

            if (IConsoleVariable* const var = object->asVariable())
            {
                m_variableCount.fetchAdd(1u, gp::MemoryOrder::AcqRel);
                if (var->isExternalReference() && m_refVariableCount < m_storage.refVariableCapacity)
                {
                    m_storage.refVariables[m_refVariableCount++] = var;
                }
            }
            else if (object->asCommand() != nullptr)
            {
                m_commandCount.fetchAdd(1u, gp::MemoryOrder::AcqRel);
            }
            return ConsoleResult::Success;
        }

        if (slot.nameHash == nameHash && slot.object->getName().equalsIgnoreCase(name))
        {
            return ConsoleResult::AlreadyExists;
        }

        idx = (idx + 1u) & m_hashMask;
    }

    return ConsoleResult::CapacityExceeded;
}

bool ConsoleManagerBase::removeObjectLocked(IConsoleObject* const object) noexcept
{
    if (object == nullptr)
    {
        return false;
    }

    const gp::UInt64 nameHash = object->getNameHash();
    gp::USize idx = static_cast<gp::USize>(nameHash) & m_hashMask;
    gp::USize foundIdx = m_storage.hashSlotCapacity;

    for (gp::USize probe = 0u; probe < m_storage.hashSlotCapacity; ++probe)
    {
        const internal::ConsoleHashSlot& slot = m_storage.hashSlots[idx];
        if (slot.object == nullptr)
        {
            return false;
        }
        if (slot.object == object)
        {
            foundIdx = idx;
            break;
        }
        idx = (idx + 1u) & m_hashMask;
    }

    if (foundIdx >= m_storage.hashSlotCapacity)
    {
        return false;
    }

    if (IConsoleVariable* const var = object->asVariable())
    {
        m_variableCount.fetchSub(1u, gp::MemoryOrder::AcqRel);
        removeFromQueueLocked(m_storage.dirtyRenderQueue, m_dirtyRenderCount, var);
        removeFromQueueLocked(m_storage.dirtyLatchedQueue, m_dirtyLatchedCount, var);
        removeFromQueueLocked(m_storage.refVariables, m_refVariableCount, var);
    }
    else if (object->asCommand() != nullptr)
    {
        m_commandCount.fetchSub(1u, gp::MemoryOrder::AcqRel);
    }

    // Backward-shift subsequent entries whose probe path passed through `foundIdx`.
    gp::USize holeIdx = foundIdx;
    gp::USize scanIdx = (holeIdx + 1u) & m_hashMask;

    while (m_storage.hashSlots[scanIdx].object != nullptr)
    {
        const gp::UInt64 homeIdx = m_storage.hashSlots[scanIdx].nameHash & m_hashMask;
        const gp::USize holeDist = (holeIdx - homeIdx) & m_hashMask;
        const gp::USize scanDist = (scanIdx - homeIdx) & m_hashMask;
        if (holeDist < scanDist)
        {
            m_storage.hashSlots[holeIdx] = m_storage.hashSlots[scanIdx];
            holeIdx = scanIdx;
        }
        scanIdx = (scanIdx + 1u) & m_hashMask;
    }

    m_storage.hashSlots[holeIdx] = internal::ConsoleHashSlot{};
    --m_objectCount;

    for (gp::USize i = 0u; i < m_storage.dynamicVariableCapacity; ++i)
    {
        if (&m_storage.dynamicVariables[i] == object)
        {
            m_storage.dynamicVariables[i].releaseSlot();
            break;
        }
    }
    for (gp::USize i = 0u; i < m_storage.dynamicCommandCapacity; ++i)
    {
        if (&m_storage.dynamicCommands[i] == object)
        {
            m_storage.dynamicCommands[i].releaseSlot();
            break;
        }
    }

    return true;
}

void ConsoleManagerBase::removeFromQueueLocked(
    IConsoleVariable** const queue, gp::USize& count, IConsoleVariable* const target
) noexcept
{
    for (gp::USize i = 0u; i < count; ++i)
    {
        if (queue[i] == target)
        {
            queue[i] = queue[count - 1u];
            queue[count - 1u] = nullptr;
            --count;
            return;
        }
    }
}

void ConsoleManagerBase::acquireLock() const noexcept
{
    acquireFlagLock(m_lock);
}

void ConsoleManagerBase::releaseLock() const noexcept
{
    releaseFlagLock(m_lock);
}

/// @section AutoConsoleVariableRef

AutoConsoleVariableRef::AutoConsoleVariableRef(
    const gp::StringView name, bool& refValue, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
    : ConsoleVariableBase(ConsoleVarType::Bool, name, help, flags)
    , m_refPtr(&refValue)
{
    captureInitialSnapshot();
    internal::enqueueDeferredConsoleObject(this);
}

AutoConsoleVariableRef::AutoConsoleVariableRef(
    const gp::StringView name, gp::Int32& refValue, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
    : ConsoleVariableBase(ConsoleVarType::Int32, name, help, flags)
    , m_refPtr(&refValue)
{
    captureInitialSnapshot();
    internal::enqueueDeferredConsoleObject(this);
}

AutoConsoleVariableRef::AutoConsoleVariableRef(
    const gp::StringView name, gp::UInt32& refValue, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
    : ConsoleVariableBase(ConsoleVarType::UInt32, name, help, flags)
    , m_refPtr(&refValue)
{
    captureInitialSnapshot();
    internal::enqueueDeferredConsoleObject(this);
}

AutoConsoleVariableRef::AutoConsoleVariableRef(
    const gp::StringView name, gp::Float32& refValue, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
    : ConsoleVariableBase(ConsoleVarType::Float32, name, help, flags)
    , m_refPtr(&refValue)
{
    captureInitialSnapshot();
    internal::enqueueDeferredConsoleObject(this);
}

AutoConsoleVariableRef::AutoConsoleVariableRef(
    const gp::StringView name, gp::Float64& refValue, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
    : ConsoleVariableBase(ConsoleVarType::Float64, name, help, flags)
    , m_refPtr(&refValue)
{
    captureInitialSnapshot();
    internal::enqueueDeferredConsoleObject(this);
}

AutoConsoleVariableRef::AutoConsoleVariableRef(
    const gp::StringView name, ConsoleString& refValue, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
    : ConsoleVariableBase(ConsoleVarType::String, name, help, flags)
    , m_refPtr(&refValue)
    , m_isGpString(false)
{
    captureInitialSnapshot();
    internal::enqueueDeferredConsoleObject(this);
}

AutoConsoleVariableRef::AutoConsoleVariableRef(
    const gp::StringView name, gp::String& refValue, const gp::StringView help, const ConsoleVarFlags flags
) noexcept
    : ConsoleVariableBase(ConsoleVarType::String, name, help, flags)
    , m_refPtr(&refValue)
    , m_isGpString(true)
{
    captureInitialSnapshot();
    internal::enqueueDeferredConsoleObject(this);
}

AutoConsoleVariableRef::AutoConsoleVariableRef(
    IConsoleManager& manager,
    const gp::StringView name,
    bool& refValue,
    const gp::StringView help,
    const ConsoleVarFlags flags
) noexcept
    : ConsoleVariableBase(ConsoleVarType::Bool, name, help, flags)
    , m_refPtr(&refValue)
{
    captureInitialSnapshot();
    (void)manager.registerExternalObject(this);
}

AutoConsoleVariableRef::AutoConsoleVariableRef(
    IConsoleManager& manager,
    const gp::StringView name,
    gp::Int32& refValue,
    const gp::StringView help,
    const ConsoleVarFlags flags
) noexcept
    : ConsoleVariableBase(ConsoleVarType::Int32, name, help, flags)
    , m_refPtr(&refValue)
{
    captureInitialSnapshot();
    (void)manager.registerExternalObject(this);
}

AutoConsoleVariableRef::AutoConsoleVariableRef(
    IConsoleManager& manager,
    const gp::StringView name,
    gp::UInt32& refValue,
    const gp::StringView help,
    const ConsoleVarFlags flags
) noexcept
    : ConsoleVariableBase(ConsoleVarType::UInt32, name, help, flags)
    , m_refPtr(&refValue)
{
    captureInitialSnapshot();
    (void)manager.registerExternalObject(this);
}

AutoConsoleVariableRef::AutoConsoleVariableRef(
    IConsoleManager& manager,
    const gp::StringView name,
    gp::Float32& refValue,
    const gp::StringView help,
    const ConsoleVarFlags flags
) noexcept
    : ConsoleVariableBase(ConsoleVarType::Float32, name, help, flags)
    , m_refPtr(&refValue)
{
    captureInitialSnapshot();
    (void)manager.registerExternalObject(this);
}

AutoConsoleVariableRef::AutoConsoleVariableRef(
    IConsoleManager& manager,
    const gp::StringView name,
    gp::Float64& refValue,
    const gp::StringView help,
    const ConsoleVarFlags flags
) noexcept
    : ConsoleVariableBase(ConsoleVarType::Float64, name, help, flags)
    , m_refPtr(&refValue)
{
    captureInitialSnapshot();
    (void)manager.registerExternalObject(this);
}

AutoConsoleVariableRef::AutoConsoleVariableRef(
    IConsoleManager& manager,
    const gp::StringView name,
    ConsoleString& refValue,
    const gp::StringView help,
    const ConsoleVarFlags flags
) noexcept
    : ConsoleVariableBase(ConsoleVarType::String, name, help, flags)
    , m_refPtr(&refValue)
    , m_isGpString(false)
{
    captureInitialSnapshot();
    (void)manager.registerExternalObject(this);
}

AutoConsoleVariableRef::AutoConsoleVariableRef(
    IConsoleManager& manager,
    const gp::StringView name,
    gp::String& refValue,
    const gp::StringView help,
    const ConsoleVarFlags flags
) noexcept
    : ConsoleVariableBase(ConsoleVarType::String, name, help, flags)
    , m_refPtr(&refValue)
    , m_isGpString(true)
{
    captureInitialSnapshot();
    (void)manager.registerExternalObject(this);
}

AutoConsoleVariableRef::~AutoConsoleVariableRef() noexcept
{
    internal::unregisterAutoConsoleObject(this);
}

void AutoConsoleVariableRef::captureInitialSnapshot() noexcept
{
    internal::ConsoleVarPayload initial{};
    readSlot(internal::ConsoleSlotKind::Game, initial);
    if (getType() == ConsoleVarType::String)
    {
        m_defaultString = initial.stringValue;
        m_latchedString = initial.stringValue;
        internal::writeConsoleStringSeqlock(m_renderString, m_renderStringSeq, initial.stringValue.view());
        return;
    }

    const gp::UInt64 bits = internal::scalarPayloadToBits(getType(), initial);
    m_defaultBits = bits;
    m_renderBits.store(bits, gp::MemoryOrder::Relaxed);
    m_latchedBits = bits;
}

void AutoConsoleVariableRef::readSlot(
    const internal::ConsoleSlotKind slot, internal::ConsoleVarPayload& outValue
) const noexcept
{
    const ConsoleVarType type = getType();
    outValue.type = type;

    if (type == ConsoleVarType::String)
    {
        if (slot == internal::ConsoleSlotKind::Default)
        {
            outValue.stringValue = m_defaultString;
        }
        else if (slot == internal::ConsoleSlotKind::Render)
        {
            internal::readConsoleStringSeqlock(m_renderString, m_renderStringSeq, outValue.stringValue);
        }
        else if (slot == internal::ConsoleSlotKind::Latched)
        {
            outValue.stringValue = m_latchedString;
        }
        else
        {
            internal::readExternalRefValue(type, m_refPtr, m_isGpString, outValue);
        }
        return;
    }

    if (slot == internal::ConsoleSlotKind::Default)
    {
        internal::bitsToScalarPayload(type, m_defaultBits, outValue);
    }
    else if (slot == internal::ConsoleSlotKind::Render)
    {
        internal::bitsToScalarPayload(type, m_renderBits.load(gp::MemoryOrder::Acquire), outValue);
    }
    else if (slot == internal::ConsoleSlotKind::Latched)
    {
        internal::bitsToScalarPayload(type, m_latchedBits, outValue);
    }
    else
    {
        internal::readExternalRefValue(type, m_refPtr, m_isGpString, outValue);
    }
}

void AutoConsoleVariableRef::writeSlot(
    const internal::ConsoleSlotKind slot, const internal::ConsoleVarPayload& value
) noexcept
{
    const ConsoleVarType type = getType();

    if (type == ConsoleVarType::String)
    {
        if (slot == internal::ConsoleSlotKind::Game)
        {
            internal::writeExternalRefValue(type, m_refPtr, m_isGpString, value);
        }
        else if (slot == internal::ConsoleSlotKind::Render)
        {
            internal::writeConsoleStringSeqlock(m_renderString, m_renderStringSeq, value.stringValue.view());
        }
        else if (slot == internal::ConsoleSlotKind::Latched)
        {
            m_latchedString = value.stringValue;
        }
        return;
    }

    if (slot == internal::ConsoleSlotKind::Game)
    {
        internal::writeExternalRefValue(type, m_refPtr, m_isGpString, value);
        return;
    }

    const gp::UInt64 bits = internal::scalarPayloadToBits(type, value);
    if (slot == internal::ConsoleSlotKind::Render)
    {
        m_renderBits.store(bits, gp::MemoryOrder::Release);
    }
    else if (slot == internal::ConsoleSlotKind::Latched)
    {
        m_latchedBits = bits;
    }
}

bool AutoConsoleVariableRef::equalsDefault(const internal::ConsoleVarPayload& value) const noexcept
{
    if (getType() == ConsoleVarType::String)
    {
        return value.stringValue.view() == m_defaultString.view();
    }
    return internal::scalarPayloadToBits(getType(), value) == m_defaultBits;
}

/// @section AutoConsoleVariableSink

AutoConsoleVariableSink::AutoConsoleVariableSink(ConsoleSinkDelegate callback) noexcept
    : m_registeredGlobally(true)
{
    bool inDeferred = false;
    m_handle = internal::registerGlobalOrDeferredSink(std::move(callback), inDeferred);
    if (!inDeferred && m_handle.isValid())
    {
        m_ownerManager = &IConsoleManager::get();
    }
}

AutoConsoleVariableSink::AutoConsoleVariableSink(IConsoleManager& manager, ConsoleSinkDelegate callback) noexcept
    : m_ownerManager(&manager)
    , m_registeredGlobally(false)
{
    m_handle = manager.registerConsoleVariableSink(std::move(callback));
}

AutoConsoleVariableSink::~AutoConsoleVariableSink() noexcept
{
    if (m_registeredGlobally)
    {
        internal::unregisterGlobalOrDeferredSink(m_handle);
    }
    else if (m_ownerManager != nullptr && m_handle.isValid())
    {
        (void)m_ownerManager->unregisterConsoleVariableSink(m_handle);
    }
}

/// @section AutoConsoleCommand

AutoConsoleCommand::AutoConsoleCommand(
    const gp::StringView name, const gp::StringView help, ConsoleCommandDelegate callback, const ConsoleVarFlags flags
) noexcept
    : ConsoleCommandBase(name, help, std::move(callback), flags)
{
    internal::enqueueDeferredConsoleObject(this);
}

AutoConsoleCommand::AutoConsoleCommand(
    const gp::StringView name,
    const gp::StringView help,
    ConsoleCommandWithArgsDelegate callback,
    const ConsoleVarFlags flags
) noexcept
    : ConsoleCommandBase(name, help, std::move(callback), flags)
{
    internal::enqueueDeferredConsoleObject(this);
}

AutoConsoleCommand::AutoConsoleCommand(
    IConsoleManager& manager,
    const gp::StringView name,
    const gp::StringView help,
    ConsoleCommandDelegate callback,
    const ConsoleVarFlags flags
) noexcept
    : ConsoleCommandBase(name, help, std::move(callback), flags)
{
    (void)manager.registerExternalObject(this);
}

AutoConsoleCommand::AutoConsoleCommand(
    IConsoleManager& manager,
    const gp::StringView name,
    const gp::StringView help,
    ConsoleCommandWithArgsDelegate callback,
    const ConsoleVarFlags flags
) noexcept
    : ConsoleCommandBase(name, help, std::move(callback), flags)
{
    (void)manager.registerExternalObject(this);
}

AutoConsoleCommand::~AutoConsoleCommand() noexcept
{
    internal::unregisterAutoConsoleObject(this);
}

/// @section IConsoleManager Singleton Accessors

IConsoleManager& IConsoleManager::get() noexcept
{
    static gp::thread::AtomicFlag s_initLock{};
    GlobalConsoleManagerStorage& instance = globalConsoleManagerInstance();

    if (!g_globalInitialized.load(gp::MemoryOrder::Acquire) && !g_globalShuttingDown.load(gp::MemoryOrder::Acquire))
    {
        acquireFlagLock(s_initLock);
        if (!g_globalInitialized.load(gp::MemoryOrder::Acquire) && !g_globalShuttingDown.load(gp::MemoryOrder::Acquire))
        {
            instance.markAsGlobalSingleton();
            (void)instance.initialize(ConsoleManagerDesc{});
            g_globalInitialized.store(true, gp::MemoryOrder::Release);
            instance.drainGlobalDeferredQueue();
        }
        releaseFlagLock(s_initLock);
    }

    return instance;
}

bool IConsoleManager::isGlobalInitialized() noexcept
{
    return g_globalInitialized.load(gp::MemoryOrder::Acquire) && !g_globalShuttingDown.load(gp::MemoryOrder::Acquire);
}

}   // namespace gp::console
