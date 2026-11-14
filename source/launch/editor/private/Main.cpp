// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "Application.hpp"
#include "backend/SDL3ApplicationBackend.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "Launch.hpp"
#include "logging/LogMacros.hpp"
#include "miscellaneous/CommandLine.hpp"

namespace gp
{

namespace
{

GP_DEFINE_LOG_CATEGORY_INLINE(LogEditor, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);
GP_DEFINE_LOG_CATEGORY_INLINE(LogDisplay, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);
GP_DEFINE_LOG_CATEGORY_INLINE(LogInput, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);
GP_DEFINE_LOG_CATEGORY_INLINE(LogDialog, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);
GP_DEFINE_LOG_CATEGORY_INLINE(LogClipboard, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);

}   // namespace

class EditorApplication : public Application
{
public:
    /// @brief Inherit parent constructors.
    using gp::Application::Application;

    ~EditorApplication() noexcept override = default;

protected:
    /// @brief Invoked upon successful initialization of all HAL subsystems and the main window.
    /// @return True to proceed into the main loop.
    [[nodiscard]] bool onInitialize() noexcept override
    {
        const gp::UInt32 argumentCount = CommandLine::getArgumentCount();
        for (gp::UInt32 i = 0u; i < argumentCount; ++i)
        {
            const gp::StringView arg = CommandLine::getArgument(i);
            GP_LOG_INFO(LogEditor, "Command-line argument [{}]: {}", i, arg);
        }

        GP_LOG_INFO(LogEditor, "========================================================");
        GP_LOG_INFO(LogEditor, "  GP Engine: EditorApplication Initialized Successfully ");
        GP_LOG_INFO(LogEditor, "========================================================");

        if (const gp::hal::IDisplay* const primary = getBackend()->getPrimaryDisplay())
        {
            const gp::hal::DisplayMode& mode = primary->getCurrentMode();
            GP_LOG_INFO(
                LogDisplay,
                "Primary Monitor: {} ({}x{} @ {:.2f} Hz)",
                primary->getName(),
                mode.resolution.width,
                mode.resolution.height,
                mode.refreshRate
            );
        }

        if (getJobSystem().isInitialized())
        {
            GP_LOG_INFO(
                LogEditor,
                "JobSystem Workers: {} (Total Slots: {} | Standard Fibers: {} | Large Fibers: {})",
                getJobSystem().getWorkerThreadCount(),
                getJobSystem().getTotalWorkerSlotCount(),
                getJobSystem().getStandardFiberCapacity(),
                getJobSystem().getLargeFiberCapacity()
            );
        }

        if (getFileSink().isOpen())
        {
            GP_LOG_INFO(LogEditor, "File log sink active: {}", getFileSink().getFilePath());
        }

        GP_LOG_INFO(LogEditor, "Controls: Press 'ESC' or 'Q' to exit; WASD, mouse, or gamepad to test input routing.");
        return true;
    }

    /// @brief Invoked at the deterministic fixed frequency (60 Hz by default) for physics calculations.
    /// @param[in] fixedDelta Exact fixed timestep duration (e.g., 16.66ms).
    void onFixedUpdate([[maybe_unused]] gp::time::Duration fixedDelta) noexcept override
    {}

    /// @brief Invoked once per frame to handle gameplay logic and state polling.
    /// @param[in] timeStats Comprehensive per-frame timing snapshot.
    void onUpdate(const gp::time::FrameTimeStats& timeStats) noexcept override
    {
        const gp::hal::input::IInputSystem& input = getInput();

        // Check for exit commands using keyboard Scancodes or Keycodes
        if (input.isScancodeDown(gp::hal::input::Scancode::Escape) || input.isScancodeDown(gp::hal::input::Scancode::Q))
        {
            GP_LOG_INFO(LogEditor, "Exit key detected. Requesting shutdown...");
            requestQuit();
            return;
        }

        // Query Keyboard State (Scancode-based WASD movement)
        const bool moveForward = input.isScancodeDown(gp::hal::input::Scancode::W);
        const bool moveBack = input.isScancodeDown(gp::hal::input::Scancode::S);
        const bool moveLeft = input.isScancodeDown(gp::hal::input::Scancode::A);
        const bool moveRight = input.isScancodeDown(gp::hal::input::Scancode::D);

        if (moveForward || moveBack || moveLeft || moveRight)
        {
            GP_LOG_INFO(
                LogInput,
                "Movement Held -> Forward: {} | Back: {} | Left: {} | Right: {}",
                moveForward,
                moveBack,
                moveLeft,
                moveRight
            );
        }

        // Query Mouse State (Position and relative motion deltas)
        const auto mousePos = input.getMouse().getPosition();
        const auto mouseDelta = input.getMouse().getDelta();
        if (mouseDelta.lengthSquared() > 0.0f)
        {
            GP_LOG_INFO(
                LogInput,
                "Mouse Position: ({:.1f}, {:.1f}) | Delta: ({:.1f}, {:.1f})",
                mousePos.x,
                mousePos.y,
                mouseDelta.x,
                mouseDelta.y
            );
        }

        // Query Gamepad State (Player 1 left thumbstick with 2D radial deadzone)
        if (const gp::hal::input::IGamepad* const pad = input.getGamepad(gp::hal::input::GamepadSlot::Player1))
        {
            if (pad->isConnected())
            {
                const auto leftStick = pad->getLeftStick();
                if (leftStick.lengthSquared() > 0.01f)
                {
                    GP_LOG_INFO(LogInput, "Gamepad P1 Left Stick: ({:.2f}, {:.2f})", leftStick.x, leftStick.y);
                }

                // Example: Trigger dual-motor rumble if 'South' (A/Cross) is pressed
                if (pad->wasButtonJustPressed(gp::hal::input::GamepadButton::South))
                {
                    GP_LOG_INFO(LogInput, "Gamepad P1 'South' button pressed! Triggering haptic rumble...");
                    gp::hal::input::HapticRumbleDesc rumble{ 0.75f, 0.4f, 250u };
                    const_cast<gp::hal::input::IGamepad*>(pad)->setRumble(rumble);
                }

                // Move the windows around with the gamepad's left stick (for testing purposes)
                if (getMainWindow()->isActive())
                {
                    const gp::hal::Point2D winPos = getMainWindow()->getPosition();
                    const gp::hal::Point2D newPos{ winPos.x + static_cast<gp::Int32>(leftStick.x * 5.0f),
                                                   winPos.y + static_cast<gp::Int32>(leftStick.y * 5.0f) };
                    getMainWindow()->setPosition(newPos);
                }
            }
        }

        // Periodic FPS printout from the master EngineClock telemetry
        static gp::Int32 s_frameThrottle = 0;
        if (++s_frameThrottle >= 120)
        {
            s_frameThrottle = 0;
            GP_LOG_INFO(
                LogEditor,
                "Smoothed FPS: {:.2f} | Frame Index: {} | Active Device: {}",
                timeStats.smoothedFps,
                timeStats.frameIndex,
                static_cast<gp::Int32>(input.getActiveDeviceType())
            );
        }

        // Trigger a themed 3-button custom modal message box when pressing 'M'
        if (getInput().wasKeyJustPressed(gp::hal::input::Keycode::M))
        {
            using namespace gp::hal;
            const DialogButtonDesc buttons[] = {
                DialogButtonDesc(100, "Save & Continue", DialogButtonFlags::ReturnKeyDefault),
                DialogButtonDesc(200, "Discard Changes"),
                DialogButtonDesc(300, "Cancel", DialogButtonFlags::EscapeKeyDefault)
            };
            const DialogColorScheme darkTheme{};

            MessageBoxDesc box{};
            box.title = "Unsaved Scene Changes";
            box.message = "Your level has unsaved modifications. How would you like to proceed?";
            box.parentWindow = getMainWindow();
            box.buttons = buttons;
            box.buttonCount = 3u;
            box.colorScheme = &darkTheme;
            box.icon = DialogIcon::Warning;

            const gp::Int32 choice = getBackend()->getDialog().showMessageBox(box);
            GP_LOG_INFO(LogDialog, "User clicked button ID: {}", choice);
        }

        // Copy text to clipboard on Ctrl+C, and paste on Ctrl+V
        const gp::hal::input::IKeyboard& kb = getInput().getKeyboard();
        if (kb.hasModifiers(gp::hal::input::KeyModifier::Ctrl))
        {
            if (kb.wasKeyJustPressed(gp::hal::input::Keycode::C))
            {
                (void)getBackend()->getClipboard().setText("Copied from Graphical Playground Engine!");
            }
            else if (kb.wasKeyJustPressed(gp::hal::input::Keycode::V))
            {
                const gp::StringView pasted = getBackend()->getClipboard().getText();
                GP_LOG_INFO(LogClipboard, "Pasted UTF-8 Text: {}", pasted);
            }
        }

        // Open an asynchronous native File Picker when pressing 'O'
        if (getInput().wasKeyJustPressed(gp::hal::input::Keycode::O))
        {
            using namespace gp::hal;
            static constexpr DialogFileFilter kFilters[] = { DialogFileFilter("3D Models (glTF/FBX)", "gltf;glb;fbx"),
                                                             DialogFileFilter("All Files", "*") };

            FileDialogDesc fileDesc{};
            fileDesc.title = "Import 3D Asset";
            fileDesc.parentWindow = getMainWindow();
            fileDesc.filters = kFilters;
            fileDesc.filterCount = 2u;
            fileDesc.allowMultiple = true;
            fileDesc.callback = [](void*, const FileDialogResult& res) noexcept
            {
                if (res.status == DialogFileStatus::Accepted)
                {
                    for (gp::UInt32 i = 0u; i < res.pathCount; ++i)
                    {
                        GP_LOG_INFO(LogDialog, "Selected [{}]: {}", i, res.paths[i]);
                    }
                }
            };

            (void)getBackend()->getDialog().showOpenFileDialog(fileDesc);
        }
    }

    /// @brief Invoked once per frame to record rendering commands.
    /// @param[in] timeStats Frame metrics including render interpolation alpha ($\alpha$).
    void onRender(const gp::time::FrameTimeStats& timeStats) noexcept override
    {
        // Use timeStats.fixedStep.interpolationAlpha ($\alpha \in [0.0, 1.0)$)
        // to lerp visual transform matrices between previous and current physics ticks!
        [[maybe_unused]] const gp::Float32 alpha = timeStats.fixedStep.interpolationAlpha;
    }

    /// @brief Invoked upon application shutdown.
    void onShutdown() noexcept override
    {
        GP_LOG_INFO(LogEditor, "Application shut down cleanly.");
    }
};

int launch([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    gp::ApplicationDesc desc{};
    desc.backend.appName = "GP Engine Sandbox";
    desc.backend.appVersion = "1.0.0";
    desc.mainWindow.title = "Graphical Playground - Sandbox Test Host";
    desc.mainWindow.size = gp::hal::Extent2D(1280u, 720u);
    desc.targetForegroundFps = 120.0;
    desc.targetBackgroundFps = 30.0;
    desc.enableFileLogSink = true;

    EditorApplication app(desc, gp::makeUnique<gp::hal::sdl3::ApplicationBackend>());
    return app.run();
}

}   // namespace gp
