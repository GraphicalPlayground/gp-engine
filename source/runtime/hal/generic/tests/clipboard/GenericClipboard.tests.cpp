// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "clipboard/GenericClipboard.hpp"
#include <gtest/gtest.h>

namespace gp::hal::generic::tests
{

TEST(GenericClipboardTest, InitialEmptyState)
{
    Clipboard clipboard;

    EXPECT_FALSE(clipboard.hasText(ClipboardTarget::Standard));
    EXPECT_FALSE(clipboard.hasText(ClipboardTarget::PrimarySelection));
    EXPECT_TRUE(clipboard.getText(ClipboardTarget::Standard).isEmpty());
    EXPECT_TRUE(clipboard.getText(ClipboardTarget::PrimarySelection).isEmpty());
    EXPECT_EQ(clipboard.getSequenceNumber(), 0ULL);
}

TEST(GenericClipboardTest, SetAndGetTextStandardTarget)
{
    Clipboard clipboard;
    const StringView sampleText = "Hello, Graphical Playground Clipboard!";

    const ClipboardResult res = clipboard.setText(sampleText, ClipboardTarget::Standard);
    EXPECT_EQ(res, ClipboardResult::Success);

    EXPECT_TRUE(clipboard.hasText(ClipboardTarget::Standard));
    EXPECT_EQ(clipboard.getText(ClipboardTarget::Standard), sampleText);
    EXPECT_GT(clipboard.getSequenceNumber(), 0ULL);

    // PrimarySelection target must remain untouched
    EXPECT_FALSE(clipboard.hasText(ClipboardTarget::PrimarySelection));
    EXPECT_TRUE(clipboard.getText(ClipboardTarget::PrimarySelection).isEmpty());
}

TEST(GenericClipboardTest, SetAndGetTextPrimaryTarget)
{
    Clipboard clipboard;
    const StringView primaryText = "Selected Linux Primary Text";

    const ClipboardResult res = clipboard.setText(primaryText, ClipboardTarget::PrimarySelection);
    EXPECT_EQ(res, ClipboardResult::Success);

    EXPECT_TRUE(clipboard.hasText(ClipboardTarget::PrimarySelection));
    EXPECT_EQ(clipboard.getText(ClipboardTarget::PrimarySelection), primaryText);

    // Standard clipboard remains unaffected
    EXPECT_FALSE(clipboard.hasText(ClipboardTarget::Standard));
}

TEST(GenericClipboardTest, GetTextBufferCopiesIntoCallerMemory)
{
    Clipboard clipboard;
    (void)clipboard.setText("Test Payload");

    char buffer[64] = {};
    const gp::USize copied = clipboard.getTextBuffer(buffer, sizeof(buffer), ClipboardTarget::Standard);

    EXPECT_EQ(copied, 12u);
    EXPECT_STREQ(buffer, "Test Payload");

    // Null pointer or 0 capacity safety
    EXPECT_EQ(clipboard.getTextBuffer(nullptr, 10u), 0u);
    EXPECT_EQ(clipboard.getTextBuffer(buffer, 0u), 0u);
}

TEST(GenericClipboardTest, SetAndReadBinaryMimeData)
{
    Clipboard clipboard;
    const gp::UInt8 rawBytes[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02 };
    const StringView mime = "application/x-custom-binary";

    EXPECT_FALSE(clipboard.hasData(mime));

    const ClipboardResult setRes = clipboard.setData(mime, rawBytes, sizeof(rawBytes));
    EXPECT_EQ(setRes, ClipboardResult::Success);
    EXPECT_TRUE(clipboard.hasData(mime));
    EXPECT_FALSE(clipboard.hasData("text/plain"));

    struct ReadContext
    {
        bool callbackExecuted{ false };
        gp::USize bytesObserved{ 0u };
    };

    ReadContext ctx;
    const auto callback = [](void* userData, const ClipboardDataView& view) noexcept
    {
        ReadContext* const readCtx = static_cast<ReadContext*>(userData);
        if (readCtx != nullptr)
        {
            readCtx->callbackExecuted = true;
            readCtx->bytesObserved = view.sizeBytes;
            const auto* data = static_cast<const gp::UInt8*>(view.data);
            EXPECT_EQ(data[0], 0xDE);
            EXPECT_EQ(data[1], 0xAD);
        }
    };

    const ClipboardResult readRes = clipboard.readData(mime, callback, &ctx);
    EXPECT_EQ(readRes, ClipboardResult::Success);
    EXPECT_TRUE(ctx.callbackExecuted);
    EXPECT_EQ(ctx.bytesObserved, sizeof(rawBytes));
}

TEST(GenericClipboardTest, SetDataErrorHandling)
{
    Clipboard clipboard;
    const gp::UInt8 dummyByte = 0xFF;

    // Empty MIME string rejects with BackendError
    EXPECT_EQ(clipboard.setData("", &dummyByte, 1u), ClipboardResult::BackendError);

    // Null pointer with positive size rejects with BackendError
    EXPECT_EQ(clipboard.setData("image/png", nullptr, 100u), ClipboardResult::BackendError);

    // Reading unsupported MIME returns UnsupportedMimeType
    EXPECT_EQ(clipboard.readData("image/jpeg", nullptr), ClipboardResult::UnsupportedMimeType);
}

TEST(GenericClipboardTest, ClearEmptiesAllCachesAndIncrementsSequenceNumber)
{
    Clipboard clipboard;
    (void)clipboard.setText("Text to clear");
    const gp::UInt8 byte = 42;
    (void)clipboard.setData("data/test", &byte, 1u);

    const gp::UInt64 seqBefore = clipboard.getSequenceNumber();
    EXPECT_TRUE(clipboard.clear());
    EXPECT_GT(clipboard.getSequenceNumber(), seqBefore);

    EXPECT_FALSE(clipboard.hasText(ClipboardTarget::Standard));
    EXPECT_FALSE(clipboard.hasText(ClipboardTarget::PrimarySelection));
    EXPECT_FALSE(clipboard.hasData("data/test"));
}

}   // namespace gp::hal::generic::tests
