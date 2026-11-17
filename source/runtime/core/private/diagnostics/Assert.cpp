// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "diagnostics/Assert.hpp"
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#if GP_PLATFORM_WINDOWS
    #include "platforms/windows/AllowWindowsPlatformTypes.hpp"
    #include "platforms/windows/HideWindowsPlatformTypes.hpp"
    #include <intrin.h>
    #include <Windows.h>
#elif GP_PLATFORM_LINUX
    #include <fcntl.h>
    #include <unistd.h>
#elif GP_PLATFORM_MACOS
    #include <sys/sysctl.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif

namespace gp::diagnostics
{

namespace
{

std::atomic<AssertHandlerFn> g_assertHandler{ nullptr };
thread_local UInt32 t_reentrancyDepth = 0u;

[[nodiscard]] const char* assertKindToString(const AssertKind kind) noexcept
{
    switch (kind)
    {
    case AssertKind::Check:
        return "CHECK";
    case AssertKind::CheckSlow:
        return "CHECK_SLOW";
    case AssertKind::Verify:
        return "VERIFY";
    case AssertKind::Ensure:
        return "ENSURE";
    case AssertKind::NoEntry:
        return "CHECK_NO_ENTRY";
    case AssertKind::Unimplemented:
        return "UNIMPLEMENTED";
    }
    return "ASSERT";
}

void writeDiagnosticReport(const AssertContext& context) noexcept
{
    char outputBuffer[1536]{};
    const char* const kindLabel = assertKindToString(context.kind);
    const char* const safeExpr = (context.expression != nullptr) ? context.expression : "<none>";
    const char* const safeFile = (context.file != nullptr) ? context.file : "<unknown>";
    const char* const safeFunc = (context.function != nullptr) ? context.function : "<unknown>";

    if (context.message != nullptr && context.message[0] != '\0')
    {
        std::snprintf(
            outputBuffer,
            sizeof(outputBuffer),
            "[GP %s FAILED] %s:%d (%s)\n  Expression: %s\n  Message:    %s\n",
            kindLabel,
            safeFile,
            static_cast<int>(context.line),
            safeFunc,
            safeExpr,
            context.message
        );
    }
    else
    {
        std::snprintf(
            outputBuffer,
            sizeof(outputBuffer),
            "[GP %s FAILED] %s:%d (%s)\n  Expression: %s\n",
            kindLabel,
            safeFile,
            static_cast<int>(context.line),
            safeFunc,
            safeExpr
        );
    }

    std::fputs(outputBuffer, stderr);
    std::fflush(stderr);

#if GP_PLATFORM_WINDOWS
    OutputDebugStringA(outputBuffer);
#endif
}

struct ReentrancyScope final
{
    ReentrancyScope() noexcept
    {
        ++t_reentrancyDepth;
    }

    ~ReentrancyScope() noexcept
    {
        --t_reentrancyDepth;
    }
};

void dispatchFailure(const AssertContext& context, const bool isFatal) noexcept
{
    ReentrancyScope guard;

    // Prevent infinite recursion if diagnostic output or a user handler triggers another failure.
    if (t_reentrancyDepth > 1u)
    {
        if (isFatal)
        {
            if (isDebuggerPresent())
            {
                GP_PLATFORM_BREAK();
            }
            std::abort();
        }
        return;
    }

    const AssertHandlerFn customHandler = g_assertHandler.load(std::memory_order_acquire);
    if (customHandler != nullptr)
    {
        const AssertAction action = customHandler(context);
        if (action == AssertAction::Continue)
        {
            return;
        }
        if (action == AssertAction::Break)
        {
            GP_PLATFORM_BREAK();
            if (isFatal)
            {
                std::abort();
            }
            return;
        }
        if (isDebuggerPresent())
        {
            GP_PLATFORM_BREAK();
        }
        std::abort();
    }

    writeDiagnosticReport(context);

    if (isDebuggerPresent())
    {
        GP_PLATFORM_BREAK();
    }

    if (isFatal)
    {
        std::abort();
    }
}

void formatVarArgsMessage(
    char* const buffer, const USize bufferSize, const char* const format, std::va_list args
) noexcept
{
    if (format == nullptr)
    {
        buffer[0] = '\0';
        return;
    }

    GP_MSVC_WARNING_PUSH()
    GP_MSVC_WARNING_DISABLE(4774)
    GP_GCC_WARNING_PUSH()
    GP_GCC_WARNING_DISABLE("-Wformat-nonliteral")
    std::vsnprintf(buffer, bufferSize, format, args);
    GP_GCC_WARNING_POP()
    GP_MSVC_WARNING_POP()
    buffer[bufferSize - 1u] = '\0';
}

[[nodiscard]] bool tryClaimEnsureSite(UInt8& triggeredFlag) noexcept
{
    std::atomic_ref<UInt8> atomicFlag(triggeredFlag);
    if (atomicFlag.load(std::memory_order_acquire) != 0u)
    {
        return false;
    }
    return atomicFlag.exchange(1u, std::memory_order_acq_rel) == 0u;
}

}   // namespace

bool isDebuggerPresent() noexcept
{
#if GP_PLATFORM_WINDOWS
    return ::IsDebuggerPresent() != 0;
#elif GP_PLATFORM_LINUX
    const int fd = ::open("/proc/self/status", O_RDONLY);
    if (fd < 0)
    {
        return false;
    }

    char statusBuf[2048]{};
    const ssize_t bytesRead = ::read(fd, statusBuf, sizeof(statusBuf) - 1u);
    ::close(fd);
    if (bytesRead <= 0)
    {
        return false;
    }

    statusBuf[bytesRead] = '\0';
    const char* cursor = statusBuf;
    constexpr const char kTracerPrefix[] = "TracerPid:";
    constexpr USize kPrefixLen = sizeof(kTracerPrefix) - 1u;

    while (*cursor != '\0')
    {
        bool matched = true;
        for (USize i = 0u; i < kPrefixLen; ++i)
        {
            if (cursor[i] != kTracerPrefix[i])
            {
                matched = false;
                break;
            }
        }
        if (matched)
        {
            cursor += kPrefixLen;
            while (*cursor == ' ' || *cursor == '\t')
            {
                ++cursor;
            }
            return *cursor != '0' && *cursor >= '1' && *cursor <= '9';
        }
        while (*cursor != '\0' && *cursor != '\n')
        {
            ++cursor;
        }
        if (*cursor == '\n')
        {
            ++cursor;
        }
    }
    return false;
#elif GP_PLATFORM_MACOS
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, ::getpid() };
    kinfo_proc info{};
    USize size = sizeof(info);
    if (::sysctl(mib, 4u, &info, &size, nullptr, 0u) != 0)
    {
        return false;
    }
    return (info.kp_proc.p_flag & P_TRACED) != 0;
#else
    return false;
#endif
}

AssertHandlerFn setAssertHandler(const AssertHandlerFn handler) noexcept
{
    return g_assertHandler.exchange(handler, std::memory_order_acq_rel);
}

AssertHandlerFn getAssertHandler() noexcept
{
    return g_assertHandler.load(std::memory_order_acquire);
}

void reportCheckFailure(
    const AssertKind kind,
    const char* const expression,
    const char* const message,
    const char* const file,
    const Int32 line,
    const char* const function
) noexcept
{
    const AssertContext context{
        .kind = kind,
        .expression = expression,
        .message = message,
        .file = file,
        .function = function,
        .line = line,
    };
    dispatchFailure(context, true);
}

void reportCheckFailureFormatted(
    const AssertKind kind,
    const char* const expression,
    const char* const file,
    const Int32 line,
    const char* const function,
    const char* const format,
    ...
) noexcept
{
    char messageBuffer[1024]{};
    if (format != nullptr)
    {
        std::va_list args;
        va_start(args, format);
        formatVarArgsMessage(messageBuffer, sizeof(messageBuffer), format, args);
        va_end(args);
    }

    const AssertContext context{
        .kind = kind,
        .expression = expression,
        .message = (format != nullptr) ? messageBuffer : nullptr,
        .file = file,
        .function = function,
        .line = line,
    };
    dispatchFailure(context, true);
}

void reportEnsureFailure(
    UInt8& triggeredFlag,
    const char* const expression,
    const char* const message,
    const char* const file,
    const Int32 line,
    const char* const function
) noexcept
{
    if (!tryClaimEnsureSite(triggeredFlag))
    {
        return;
    }

    const AssertContext context{
        .kind = AssertKind::Ensure,
        .expression = expression,
        .message = message,
        .file = file,
        .function = function,
        .line = line,
    };
    dispatchFailure(context, false);
}

void reportEnsureFailureFormatted(
    UInt8& triggeredFlag,
    const char* const expression,
    const char* const file,
    const Int32 line,
    const char* const function,
    const char* const format,
    ...
) noexcept
{
    if (!tryClaimEnsureSite(triggeredFlag))
    {
        return;
    }

    char messageBuffer[1024]{};
    if (format != nullptr)
    {
        std::va_list args;
        va_start(args, format);
        formatVarArgsMessage(messageBuffer, sizeof(messageBuffer), format, args);
        va_end(args);
    }

    const AssertContext context{
        .kind = AssertKind::Ensure,
        .expression = expression,
        .message = (format != nullptr) ? messageBuffer : nullptr,
        .file = file,
        .function = function,
        .line = line,
    };
    dispatchFailure(context, false);
}

}   // namespace gp::diagnostics
