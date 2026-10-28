// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "dialog/GenericDialog.hpp"
#include <gtest/gtest.h>

namespace gp::hal::generic::tests
{

TEST(GenericDialogTest, ShowAlertResolvesDefaultButtonPerPreset)
{
    Dialog dialog;

    EXPECT_EQ(dialog.showAlert(DialogIcon::Information, "Title", "Msg", DialogButtonPreset::Ok), DialogButtonId::Ok);
    EXPECT_EQ(dialog.showAlert(DialogIcon::Warning, "Title", "Msg", DialogButtonPreset::OkCancel), DialogButtonId::Ok);
    EXPECT_EQ(
        dialog.showAlert(DialogIcon::Information, "Title", "Msg", DialogButtonPreset::YesNo), DialogButtonId::Yes
    );
    EXPECT_EQ(
        dialog.showAlert(DialogIcon::Information, "Title", "Msg", DialogButtonPreset::YesNoCancel), DialogButtonId::Yes
    );
    EXPECT_EQ(
        dialog.showAlert(DialogIcon::Error, "Title", "Msg", DialogButtonPreset::RetryCancel), DialogButtonId::Cancel
    );
    EXPECT_EQ(
        dialog.showAlert(DialogIcon::Error, "Title", "Msg", DialogButtonPreset::AbortRetryIgnore), DialogButtonId::Abort
    );
}

TEST(GenericDialogTest, ShowMessageBoxPicksReturnKeyDefault)
{
    Dialog dialog;

    DialogButtonDesc buttons[3]{};
    buttons[0].id = 101;
    buttons[0].flags = DialogButtonFlags::None;

    buttons[1].id = 202;
    buttons[1].flags = DialogButtonFlags::ReturnKeyDefault;

    buttons[2].id = 303;
    buttons[2].flags = DialogButtonFlags::EscapeKeyDefault;

    MessageBoxDesc desc{};
    desc.buttons = buttons;
    desc.buttonCount = 3u;

    EXPECT_EQ(dialog.showMessageBox(desc), 202);

    // Fall back to button 0 if no ReturnKeyDefault
    buttons[1].flags = DialogButtonFlags::None;
    EXPECT_EQ(dialog.showMessageBox(desc), 101);

    // Empty buttons rejects with -1
    desc.buttonCount = 0u;
    EXPECT_EQ(dialog.showMessageBox(desc), -1);
}

TEST(GenericDialogTest, FileDialogsInvokeCallbackWithCancelled)
{
    Dialog dialog;

    struct Context
    {
        bool callbackExecuted{ false };
        DialogFileStatus status{ DialogFileStatus::Accepted };
    };

    Context ctx;
    const auto callback = [](void* userData, const FileDialogResult& result) noexcept
    {
        Context* const c = static_cast<Context*>(userData);
        if (c != nullptr)
        {
            c->callbackExecuted = true;
            c->status = result.status;
        }
    };

    FileDialogDesc desc{};
    desc.callback = callback;
    desc.userData = &ctx;

    EXPECT_FALSE(dialog.showOpenFileDialog(desc));
    EXPECT_TRUE(ctx.callbackExecuted);
    EXPECT_EQ(ctx.status, DialogFileStatus::Cancelled);

    ctx.callbackExecuted = false;
    EXPECT_FALSE(dialog.showSaveFileDialog(desc));
    EXPECT_TRUE(ctx.callbackExecuted);
    EXPECT_EQ(ctx.status, DialogFileStatus::Cancelled);
}

}   // namespace gp::hal::generic::tests
