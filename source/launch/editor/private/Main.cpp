// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "Application.hpp"
#include "backend/SDL3ApplicationBackend.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/ConsoleTypes.hpp"
#include "console/IConsoleManager.hpp"
#include "containers/strings/CharTraits.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/DynamicRHI.hpp"
#include "device/RHIGlobals.hpp"
#include "Launch.hpp"
#include "logging/LogMacros.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "modules/ModuleManager.hpp"
#include "profiling/Profiler.hpp"
#include "profiling/ProfilingControl.hpp"
#include "vfs/VirtualFileSystem.hpp"
#include <cstdio>

namespace gp
{

namespace
{

GP_DEFINE_LOG_CATEGORY_INLINE(LogEditor, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);
GP_DEFINE_LOG_CATEGORY_INLINE(LogDisplay, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);
GP_DEFINE_LOG_CATEGORY_INLINE(LogInput, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);
GP_DEFINE_LOG_CATEGORY_INLINE(LogDialog, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);
GP_DEFINE_LOG_CATEGORY_INLINE(LogClipboard, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);

gp::console::AutoConsoleVariable<bool> g_cvarEditorLogInput(
    "editor.LogInput",
    true,
    "Logs keyboard, mouse, and gamepad input polling diagnostics from EditorApplication.",
    gp::console::ConsoleVarFlags::None
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarEditorFpsLogInterval(
    "editor.FpsLogInterval",
    120,
    "Frame interval between periodic EditorApplication FPS logs (<= 0 disables).",
    gp::console::ConsoleVarFlags::None
);

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
        GP_PROFILE_SCOPE_NC_ACTIVE(
            "EditorApplication::OnInitialize", GP_U32_COLOR_PURPLE, profiling::ProfilingControl::isZonesEnabled()
        );

        // TODO(vfs): Mount editor-only content schemes here (e.g., `editor://` pointing to the editor resources root,
        //            plus any active project or developer override layers) once the editor asset directory layout is
        //            defined. The six base ADR-0044 schemes (`engine://`, `game://`, `user://`, `appdata://`,
        //            `cache://`, `temp://`) are already mounted by `Application::initializeInternal()`.
        GP_LOG_INFO(
            LogEditor, "VirtualFileSystem Ready ({} active mounts).", gp::vfs::VirtualFileSystem::getMountCount()
        );

        const gp::console::IConsoleManager& consoleMgr = getConsoleManager();
        GP_LOG_INFO(
            LogEditor,
            "Console CVar Registry Ready ({} CVars, {} Commands).",
            consoleMgr.getVariableCount(),
            consoleMgr.getCommandCount()
        );

        gp::modules::ModuleManager& moduleMgr = getModuleManager();
        GP_LOG_INFO(
            LogEditor,
            "ModuleManager Ready ({} loaded, {} registered).",
            moduleMgr.getLoadedModuleCount(),
            moduleMgr.getRegisteredModuleCount()
        );

        if (isRHIInitialized())
        {
            const gp::rhi::DynamicRHI* const activeRHI = getDynamicRHI();
            GP_LOG_INFO(
                LogEditor,
                "RHI Backend Ready: {} (module='{}', nullRHI={}).",
                (activeRHI != nullptr) ? activeRHI->getName() : gp::StringView("None"),
                getLoadedRHIModuleName(),
                gp::rhi::getRHIGlobals().usingNullRHI
            );
        }

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

        GP_LOG_INFO(
            LogEditor,
            "Controls: 'ESC'/'Q' exit | 'F1'/'`' temporary stdin console shell | WASD, mouse, gamepad test input."
        );
        return true;
    }

private:
    // TEMP: Modal stdin shell invoked on F1 / Grave (`~`) for ad-hoc CVar & command testing in gp-editor.
    void runTemporaryStdinShell() noexcept
    {
        getLogger().flush();

        std::fputs(
            "\n========================================================\n"
            "  [TEMP] Console Stdin Shell Active\n"
            "  Examples: help | cvar.list | cvar.dump | prof.Status | log.Stats | app.Stats | jobs.Stats | rhi.Status | vfs.Status | vfs.Mounts | module.Status | module.List\n"
            "  Set CVar: t.TimeScale 0.5  |  app.TargetFps=60  |  log.GlobalLevel Warning\n"
            "  Commands: 'resume' (or Enter) returns to gp-editor | 'quit' / 'app.Quit' exits\n"
            "========================================================\n",
            stdout
        );
        std::fflush(stdout);

        char lineBuffer[512]{};
        while (true)
        {
            std::fputs("gp-console> ", stdout);
            std::fflush(stdout);

            if (std::fgets(lineBuffer, static_cast<int>(sizeof(lineBuffer)), stdin) == nullptr)
            {
                std::fputs("\n[TEMP] stdin EOF reached; resuming gp-editor.\n", stdout);
                std::fflush(stdout);
                break;
            }

            const gp::USize rawLen = gp::container::CharTraits<char>::length(lineBuffer);
            const gp::StringView trimmed = gp::console::trimConsoleWhitespace(gp::StringView(lineBuffer, rawLen));
            if (trimmed.isEmpty() || trimmed.equalsIgnoreCase("resume") || trimmed.equalsIgnoreCase("exit"))
            {
                std::fputs("[TEMP] Resuming gp-editor window loop...\n", stdout);
                std::fflush(stdout);
                break;
            }

            gp::USize emittedLines = 0u;
            gp::console::ConsoleOutputDelegate shellOut = gp::console::ConsoleOutputDelegate::createLambda(
                [&emittedLines](const gp::StringView text) noexcept -> void
            {
                if (!text.isEmpty())
                {
                    (void)std::fwrite(text.data(), 1u, text.size(), stdout);
                }
                std::fputc('\n', stdout);
                ++emittedLines;
            }
            );

            if (trimmed.equalsIgnoreCase("quit"))
            {
                (void)executeConsoleCommand("app.Quit", &shellOut);
                std::fputs("[TEMP] Shutdown requested.\n", stdout);
                std::fflush(stdout);
                break;
            }

            const gp::console::ConsoleResult res = executeConsoleCommand(trimmed, &shellOut);
            if (emittedLines == 0u && res == gp::console::ConsoleResult::Success)
            {
                std::fputs("[ok]\n", stdout);
            }
            std::fflush(stdout);

            if (!isRunning())
            {
                break;
            }
        }

        // Flush any log messages triggered synchronously during shell execution before returning to frames.
        getLogger().flush();
    }

protected:
    /// @brief Invoked at the deterministic fixed frequency (60 Hz by default) for physics calculations.
    /// @param[in] fixedDelta Exact fixed timestep duration (e.g., 16.66ms).
    void onFixedUpdate([[maybe_unused]] gp::time::Duration fixedDelta) noexcept override
    {}

    /// @brief Invoked once per frame to handle gameplay logic and state polling.
    /// @param[in] timeStats Comprehensive per-frame timing snapshot.
    void onUpdate(const gp::time::FrameTimeStats& timeStats) noexcept override
    {
        GP_PROFILE_SCOPE_NC_ACTIVE(
            "EditorApplication::OnUpdate", GP_U32_COLOR_GREEN, profiling::ProfilingControl::isZonesEnabled()
        );

        const gp::hal::input::IInputSystem& input = getInput();

        // Check for exit commands using keyboard Scancodes or Keycodes
        if (input.isScancodeDown(gp::hal::input::Scancode::Escape) || input.isScancodeDown(gp::hal::input::Scancode::Q))
        {
            GP_LOG_INFO(LogEditor, "Exit key detected. Requesting shutdown...");
            requestQuit();
            return;
        }

        // TEMP: Switch terminal stdin into an interactive console command shell on F1 or Grave (` / ~).
        if (input.wasScancodeJustPressed(gp::hal::input::Scancode::F1) ||
            input.wasScancodeJustPressed(gp::hal::input::Scancode::Grave))
        {
            runTemporaryStdinShell();
            return;
        }

        const bool logInputDiagnostics = g_cvarEditorLogInput.getValueOnGameThread();

        // Query Keyboard State (Scancode-based WASD movement)
        const bool moveForward = input.isScancodeDown(gp::hal::input::Scancode::W);
        const bool moveBack = input.isScancodeDown(gp::hal::input::Scancode::S);
        const bool moveLeft = input.isScancodeDown(gp::hal::input::Scancode::A);
        const bool moveRight = input.isScancodeDown(gp::hal::input::Scancode::D);

        if (logInputDiagnostics && (moveForward || moveBack || moveLeft || moveRight))
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
        if (logInputDiagnostics && mouseDelta.lengthSquared() > 0.0f)
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
                if (logInputDiagnostics && leftStick.lengthSquared() > 0.01f)
                {
                    GP_LOG_INFO(LogInput, "Gamepad P1 Left Stick: ({:.2f}, {:.2f})", leftStick.x, leftStick.y);
                }

                // Example: Trigger dual-motor rumble if 'South' (A/Cross) is pressed
                if (pad->wasButtonJustPressed(gp::hal::input::GamepadButton::South))
                {
                    if (logInputDiagnostics)
                    {
                        GP_LOG_INFO(LogInput, "Gamepad P1 'South' button pressed! Triggering haptic rumble...");
                    }
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
        const gp::Int32 fpsInterval = g_cvarEditorFpsLogInterval.getValueOnGameThread();
        if (fpsInterval > 0 && ++s_frameThrottle >= fpsInterval)
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

            getBackend()->getDialog().showOpenFileDialog(fileDesc);
        }
    }

    /// @brief Invoked once per frame to record rendering commands.
    /// @param[in] timeStats Frame metrics including render interpolation alpha ($\alpha$).
    void onRender(const gp::time::FrameTimeStats& timeStats) noexcept override
    {
        GP_PROFILE_SCOPE_NC_ACTIVE(
            "EditorApplication::OnRender", GP_U32_COLOR_BLUE, profiling::ProfilingControl::isZonesEnabled()
        );

        // Use timeStats.fixedStep.interpolationAlpha ($\alpha \in [0.0, 1.0)$)
        // to lerp visual transform matrices between previous and current physics ticks!
        [[maybe_unused]] const gp::Float32 alpha = timeStats.fixedStep.interpolationAlpha;
    }

    /// @brief Invoked upon application shutdown.
    void onShutdown() noexcept override
    {
        GP_PROFILE_SCOPE_NC_ACTIVE(
            "EditorApplication::OnShutdown", GP_U32_COLOR_RED, profiling::ProfilingControl::isZonesEnabled()
        );
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
    desc.rhi.hasEditorToken = true;

    EditorApplication app(desc, gp::makeUnique<gp::hal::sdl3::ApplicationBackend>());
    return app.run();
}

}   // namespace gp
