// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "logging/sinks/MemoryRingSink.hpp"
#include "memory/Memory.hpp"

namespace gp::log
{

MemoryRingSink::MemoryRingSink(const LogLevel minLevel) noexcept
    : ILogSink(minLevel)
{}

void MemoryRingSink::write(const LogRecordView& record) noexcept
{
    if (!shouldWrite(record.level))
    {
        return;
    }

    const gp::USize currentWrite = m_writeCount.load(gp::MemoryOrder::Relaxed);
    const gp::USize slotIndex = currentWrite & (kCapacity - 1u);
    LogEntry& dst = m_entries[slotIndex];

    dst.timestampNs = record.timestamp.nanoseconds;
    dst.threadId = record.threadId;
    dst.category = !record.category.isNone()
                       ? record.category
                       : (!record.categoryName.isEmpty() ? gp::Name(record.categoryName) : gp::Name{});
    dst.categoryLiteral = !dst.category.isNone() ? dst.category.baseCString() : "";
    const gp::StringView resolvedCatView = !dst.category.isNone() ? dst.category.baseView() : gp::StringView{};
    dst.categoryLength =
        (resolvedCatView.size() > 255u) ? static_cast<gp::UInt8>(255u) : static_cast<gp::UInt8>(resolvedCatView.size());
    dst.file = (record.location.file != nullptr) ? record.location.file : "";
    dst.function = (record.location.function != nullptr) ? record.location.function : "";
    dst.line = record.location.line;
    dst.fiberId = record.fiberId;
    dst.dateTime = record.dateTime;
    dst.level = record.level;

    const gp::USize msgLen =
        (record.message.size() <= kMaxLogMessageLength) ? record.message.size() : kMaxLogMessageLength;
    if (msgLen > 0u)
    {
        (void)gp::memory::copyMemory(dst.message, record.message.data(), msgLen);
    }
    dst.message[msgLen] = '\0';
    dst.messageLength = static_cast<gp::UInt16>(msgLen);

    m_writeCount.store(currentWrite + 1u, gp::MemoryOrder::Release);
}

void MemoryRingSink::flush() noexcept
{
    m_flushCount.fetchAdd(1u, gp::MemoryOrder::Release);
}

void MemoryRingSink::clear() noexcept
{
    m_writeCount.store(0u, gp::MemoryOrder::Release);
    m_flushCount.store(0u, gp::MemoryOrder::Release);
}

const LogEntry& MemoryRingSink::getEntry(const gp::USize index) const noexcept
{
    const gp::USize total = m_writeCount.load(gp::MemoryOrder::Acquire);
    [[maybe_unused]] const gp::USize currentSize = (total < kCapacity) ? total : kCapacity;
    GP_ASSERT(index < currentSize && "MemoryRingSink::getEntry index out of bounds.");

    const gp::USize oldestIndex = (total <= kCapacity) ? 0u : (total - kCapacity);
    const gp::USize physicalSlot = (oldestIndex + index) & (kCapacity - 1u);
    return m_entries[physicalSlot];
}

LogRecordView MemoryRingSink::getLastRecord() const noexcept
{
    const gp::USize total = m_writeCount.load(gp::MemoryOrder::Acquire);
    if (total == 0u)
    {
        return LogRecordView{};
    }

    const gp::USize lastPhysicalSlot = (total - 1u) & (kCapacity - 1u);
    return m_entries[lastPhysicalSlot].toView();
}

}   // namespace gp::log
