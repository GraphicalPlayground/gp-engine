// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "logging/sinks/FileSink.hpp"
#include "containers/strings/FixedString.hpp"
#include "logging/LogFormatter.hpp"
#include "memory/Memory.hpp"
#include "platforms/base/system/Paths.hpp"
#include <cstdio>

namespace gp::log
{

FileSink::FileSink(const FileSinkDesc& desc) noexcept
    : ILogSink(desc.minLevel)
    , m_flags(desc.flags)
{
    (void)open(desc);
}

FileSink::~FileSink() noexcept
{
    close();
}

LogResult FileSink::open(const FileSinkDesc& desc) noexcept
{
    close();

    setMinLevel(desc.minLevel);
    m_flags = desc.flags;
    m_totalBytesWritten = 0ULL;
    m_bufferedBytes = 0u;

    if (!desc.filePath.isEmpty())
    {
        m_filePath.assign(desc.filePath);
        gp::platform::Paths::makeStandardFilename(m_filePath);
    }
    else
    {
        const gp::StringView baseDir = gp::platform::Paths::tempDir();
        gp::StringView exeName = gp::platform::Paths::executableName(true);
        if (exeName.isEmpty())
        {
            exeName = "gp_engine";
        }

        gp::FixedString<128> fileName{};
        fileName.append(exeName);
        fileName.append(".log");

        m_filePath = gp::platform::Paths::combine(baseDir, fileName.view());
    }

    if (m_filePath.isEmpty())
    {
        return LogResult::InvalidArguments;
    }

    const char* const openMode = desc.truncateOnOpen ? "wb" : "ab";
#if GP_PLATFORM_WINDOWS
    std::FILE* rawFile = nullptr;
    const errno_t err = ::fopen_s(&rawFile, m_filePath.cString(), openMode);
    if (err != 0 || rawFile == nullptr)
    {
        m_fileHandle = nullptr;
        return LogResult::IoError;
    }
    m_fileHandle = rawFile;
#else
    std::FILE* const rawFile = std::fopen(m_filePath.cString(), openMode);
    if (rawFile == nullptr)
    {
        m_fileHandle = nullptr;
        return LogResult::IoError;
    }
    m_fileHandle = rawFile;
#endif

    return LogResult::Success;
}

void FileSink::close() noexcept
{
    if (m_fileHandle != nullptr)
    {
        flush();
        std::FILE* const rawFile = static_cast<std::FILE*>(m_fileHandle);
        (void)std::fclose(rawFile);
        m_fileHandle = nullptr;
    }
}

void FileSink::write(const LogRecordView& record) noexcept
{
    if (m_fileHandle == nullptr || !shouldWrite(record.level))
    {
        return;
    }

    gp::FixedString<640> lineBuffer{};
    const gp::StringView formattedLine = formatLogRecordLine(lineBuffer, record, m_flags);
    const gp::USize lineLen = formattedLine.size();

    if (lineLen == 0u)
    {
        return;
    }

    // If the incoming line would overflow the 8 KiB user-space buffer, flush staged bytes first.
    if (m_bufferedBytes + lineLen > kWriteBufferCapacity)
    {
        flushInternalBuffer();
    }

    if (lineLen <= kWriteBufferCapacity)
    {
        (void)gp::memory::copyMemory(m_writeBuffer + m_bufferedBytes, formattedLine.data(), lineLen);
        m_bufferedBytes += lineLen;
    }
    else
    {
        std::FILE* const rawFile = static_cast<std::FILE*>(m_fileHandle);
        const gp::USize written = std::fwrite(formattedLine.data(), sizeof(char), lineLen, rawFile);
        m_totalBytesWritten += static_cast<gp::UInt64>(written);
    }

    const bool flushEvery = (m_flags & LogSinkFlags::FlushEveryRecord) != LogSinkFlags::None;
    if (flushEvery || static_cast<gp::UInt8>(record.level) >= static_cast<gp::UInt8>(LogLevel::Error))
    {
        flush();
    }
}

void FileSink::flush() noexcept
{
    if (m_fileHandle == nullptr)
    {
        return;
    }

    flushInternalBuffer();
    std::FILE* const rawFile = static_cast<std::FILE*>(m_fileHandle);
    (void)std::fflush(rawFile);
}

void FileSink::flushInternalBuffer() noexcept
{
    if (m_fileHandle == nullptr || m_bufferedBytes == 0u)
    {
        return;
    }

    std::FILE* const rawFile = static_cast<std::FILE*>(m_fileHandle);
    const gp::USize written = std::fwrite(m_writeBuffer, sizeof(char), m_bufferedBytes, rawFile);
    m_totalBytesWritten += static_cast<gp::UInt64>(written);
    m_bufferedBytes = 0u;
}

}   // namespace gp::log
