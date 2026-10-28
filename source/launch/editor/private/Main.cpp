// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "Application.hpp"
#include "backend/SDL3ApplicationBackend.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "Launch.hpp"
#include "miscellaneous/CommandLine.hpp"
#include <iostream>

namespace gp
{

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
        std::cout << "========================================================\n";
        std::cout << "  GP Engine: EditorApplication Initialized Successfully \n";
        std::cout << "========================================================\n";

        if (const auto* primary = getBackend()->getPrimaryDisplay())
        {
            const auto& mode = primary->getCurrentMode();
            std::cout << "[DISPLAY] Primary Monitor: " << primary->getName().data() << " (" << mode.resolution.width
                      << "x" << mode.resolution.height << " @ " << mode.refreshRate << " Hz)\n";
        }

        std::cout << "\nControls:\n"
                  << "  • Press 'ESC' or 'Q' to exit.\n"
                  << "  • Move mouse, press keyboard keys, or use a gamepad to test input routing.\n"
                  << "--------------------------------------------------------\n\n";

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
        const auto& input = getInput();

        // Check for exit commands using keyboard Scancodes or Keycodes
        if (input.isScancodeDown(gp::hal::input::Scancode::Escape) || input.isScancodeDown(gp::hal::input::Scancode::Q))
        {
            std::cout << "[SYSTEM] Exit key detected. Requesting shutdown..." << std::endl;
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
            std::cout << "[KEYBOARD] Movement Held -> Forward: " << moveForward << " | Back: " << moveBack
                      << " | Left: " << moveLeft << " | Right: " << moveRight << "\n";
        }

        // Query Mouse State (Position and relative motion deltas)
        const auto mousePos = input.getMouse().getPosition();
        const auto mouseDelta = input.getMouse().getDelta();
        if (mouseDelta.lengthSquared() > 0.0f)
        {
            std::cout << "[MOUSE] Position: (" << mousePos.x << ", " << mousePos.y << ") | Delta: (" << mouseDelta.x
                      << ", " << mouseDelta.y << ")\n";
        }

        // Query Gamepad State (Player 1 left thumbstick with 2D radial deadzone)
        if (const auto* pad = input.getGamepad(gp::hal::input::GamepadSlot::Player1))
        {
            if (pad->isConnected())
            {
                const auto leftStick = pad->getLeftStick();
                if (leftStick.lengthSquared() > 0.01f)
                {
                    std::cout << "[GAMEPAD P1] Left Stick: (" << leftStr(leftStick.x) << ", " << leftStr(leftStick.y)
                              << ")\n";
                }

                // Example: Trigger dual-motor rumble if 'South' (A/Cross) is pressed
                if (pad->wasButtonJustPressed(gp::hal::input::GamepadButton::South))
                {
                    std::cout << "[GAMEPAD P1] 'South' button pressed! Triggering haptic rumble...\n";
                    gp::hal::input::HapticRumbleDesc rumble{ 0.75f, 0.4f, 250u };
                    const_cast<gp::hal::input::IGamepad*>(pad)->setRumble(rumble);
                }

                // Move the windows around with the gamepad's left stick (for testing purposes)
                if (getMainWindow()->isActive())
                {
                    const auto winPos = getMainWindow()->getPosition();
                    const gp::hal::Point2D newPos{ winPos.x + static_cast<gp::Int32>(leftStick.x * 5.0f),
                                                   winPos.y + static_cast<gp::Int32>(leftStick.y * 5.0f) };
                    getMainWindow()->setPosition(newPos);
                }
            }
        }

        // Periodic FPS printout from the master EngineClock telemetry
        static int s_frameThrottle = 0;
        if (++s_frameThrottle >= 120)
        {
            s_frameThrottle = 0;
            std::cout << "[CLOCK] Smoothed FPS: " << timeStats.smoothedFps << " | Frame Index: " << timeStats.frameIndex
                      << " | Active Device: " << static_cast<int>(input.getActiveDeviceType()) << "\n";
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
            std::cout << "[DIALOG] User clicked button ID: " << choice << "\n";
        }

        // Copy text to clipboard on Ctrl+C, and paste on Ctrl+V
        const auto& kb = getInput().getKeyboard();
        if (kb.hasModifiers(gp::hal::input::KeyModifier::Ctrl))
        {
            if (kb.wasKeyJustPressed(gp::hal::input::Keycode::C))
            {
                (void)getBackend()->getClipboard().setText("Copied from Graphical Playground Engine!");
            }
            else if (kb.wasKeyJustPressed(gp::hal::input::Keycode::V))
            {
                const gp::StringView pasted = getBackend()->getClipboard().getText();
                std::cout << "[CLIPBOARD] Pasted UTF-8 Text: " << pasted.data() << "\n";
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
                        std::cout << "[FILE DIALOG] Selected [" << i << "]: " << res.paths[i].data() << "\n";
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
        std::cout << "[SYSTEM] Application shut down cleanly." << std::endl;
    }

private:
    /// @brief Helper to format float values for console logs.
    [[nodiscard]] static constexpr gp::Float32 leftStr(gp::Float32 val) noexcept
    {
        return val;
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

    UInt32 argumentCount = CommandLine::getArgumentCount();
    for (UInt32 i = 0u; i < argumentCount; ++i)
    {
        const gp::StringView arg = CommandLine::getArgument(i);
        std::cout << "[CMDLINE] Argument [" << i << "]: " << arg.data() << "\n";
    }

    EditorApplication app(desc, gp::makeUnique<gp::hal::sdl3::ApplicationBackend>());
    return app.run();
}

}   // namespace gp
