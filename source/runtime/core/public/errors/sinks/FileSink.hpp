// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "errors/ErrorSink.hpp"
#include <fstream>   // TODO: gp::fs::FileStream?
#include <mutex>     // TODO: gp::Mutex?

namespace gp::error
{

/// @brief A simple sink that appends UTF-8 text to a file.
class FileSink final : public Sink
{
private:
    std::mutex m_mutex;
    std::ofstream m_file;

public:
    /// @brief Constructs a FileSink that appends UTF-8 text to the specified path.
    /// @param[in] path The file path to write logs to.
    explicit FileSink(const gp::String& path);

public:
    /// @brief Called by the ErrorSystem for every record that passes the global filter.
    /// @param[in] record The error record to process.
    void onRecord(const ErrorRecord& record) override;

    /// @brief Flush any buffered output (file handles, network sockets, etc.).
    void flush() override;
};

}   // namespace gp::error
