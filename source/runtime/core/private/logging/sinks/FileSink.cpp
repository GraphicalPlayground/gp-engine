// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "logging/sinks/FileSink.hpp"
#include "containers/strings/FixedString.hpp"
#include "logging/LogFormatter.hpp"
#include "memory/Memory.hpp"
#include "platforms/base/system/FileEnums.hpp"
#include "platforms/base/system/FileSystem.hpp"
#include "platforms/base/system/Paths.hpp"
#include "templates/Expected.hpp"
#include <utility>

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

    const gp::StringView parentDir = gp::platform::Paths::getPath(m_filePath.view());
    if (!parentDir.isEmpty() && !gp::platform::FileSystem::isDirectory(parentDir))
    {
        gp::platform::FileSystem::createDirectoryTree(parentDir);
    }

    const gp::platform::FileOpenMode openMode =
        desc.truncateOnOpen ? gp::platform::FileOpenMode::CreateAlways : gp::platform::FileOpenMode::Append;
    gp::Expected<gp::platform::FileHandle, gp::platform::FileError> openResult = gp::platform::FileSystem::open(
        m_filePath.view(), gp::platform::FileAccessMode::Write, openMode, gp::platform::FileShareMode::Read
    );
    if (!openResult.hasValue())
    {
        return LogResult::IoError;
    }

    m_fileHandle = std::move(openResult.value());
    return LogResult::Success;
}

void FileSink::close() noexcept
{
    if (m_fileHandle.isValid())
    {
        flush();
        m_fileHandle.close();
    }
}

void FileSink::write(const LogRecordView& record) noexcept
{
    if (!m_fileHandle.isValid() || !shouldWrite(record.level))
    {
        return;
    }

    LogLineBuffer lineBuffer{};
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
        gp::memory::copyMemory(m_writeBuffer + m_bufferedBytes, formattedLine.data(), lineLen);
        m_bufferedBytes += lineLen;
    }
    else
    {
        const gp::Expected<gp::USize, gp::platform::FileError> writeRes =
            m_fileHandle.write(formattedLine.data(), lineLen);
        if (writeRes.hasValue())
        {
            m_totalBytesWritten += static_cast<gp::UInt64>(writeRes.value());
        }
    }

    const bool flushEvery = (m_flags & LogSinkFlags::FlushEveryRecord) != LogSinkFlags::None;
    if (flushEvery || static_cast<gp::UInt8>(record.level) >= static_cast<gp::UInt8>(LogLevel::Error))
    {
        flush();
    }
}

void FileSink::flush() noexcept
{
    if (!m_fileHandle.isValid())
    {
        return;
    }

    flushInternalBuffer();
    m_fileHandle.flush();
}

void FileSink::flushInternalBuffer() noexcept
{
    if (!m_fileHandle.isValid() || m_bufferedBytes == 0u)
    {
        return;
    }

    const gp::Expected<gp::USize, gp::platform::FileError> writeRes =
        m_fileHandle.write(m_writeBuffer, m_bufferedBytes);
    if (writeRes.hasValue())
    {
        m_totalBytesWritten += static_cast<gp::UInt64>(writeRes.value());
    }
    m_bufferedBytes = 0u;
}

}   // namespace gp::log
