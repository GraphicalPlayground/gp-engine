// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "backend/SDL3ApplicationBackend.hpp"
#include "event/data/ClipboardEvent.hpp"
#include "event/data/DropEvent.hpp"
#include "event/data/TextInputEvent.hpp"
#include "event/device/DeviceChangeEvent.hpp"
#include "event/EventCategory.hpp"
#include "event/EventHeader.hpp"
#include "event/input/GamepadEvent.hpp"
#include "event/input/KeyboardEvent.hpp"
#include "event/input/MouseEvent.hpp"
#include "event/input/PenEvent.hpp"
#include "event/input/SensorEvent.hpp"
#include "event/input/TouchEvent.hpp"
#include "event/system/ApplicationEvent.hpp"
#include "event/system/DisplayEvent.hpp"
#include "event/system/WindowEvent.hpp"
#include "input/SDL3InputSystem.hpp"
#include "memory/Memory.hpp"
#include "window/SDL3Window.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>

namespace gp::hal::sdl3
{

namespace detail
{

/// @brief Computes the byte length of a null-terminated UTF-8 string up to `maxLength` without `<cstring>`.
/// @param[in] str Pointer to a null-terminated UTF-8 string (may be `nullptr`).
/// @param[in] maxLength Maximum allowed byte count to scan.
/// @return Number of bytes before the null terminator, capped at `maxLength`.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt16 boundedUtf8Length(const char* str, gp::UInt16 maxLength) noexcept
{
    if (str == nullptr) [[unlikely]]
    {
        return 0u;
    }

    gp::UInt16 len = 0u;
    while (len < maxLength && str[len] != '\0')
    {
        ++len;
    }
    return len;
}

/// @brief Maps engine `BackendSubsystemFlags` to `SDL_InitFlags`.
/// @param[in] flags Engine subsystem bitmask.
/// @return Corresponding `SDL_InitFlags` bitmask.
[[nodiscard]] constexpr SDL_InitFlags toSdlInitFlags(BackendSubsystemFlags flags) noexcept
{
    SDL_InitFlags sdlFlags = 0u;

    if ((flags & BackendSubsystemFlags::Events) != BackendSubsystemFlags::None)
    {
        sdlFlags |= SDL_INIT_EVENTS;
    }
    if ((flags & BackendSubsystemFlags::Video) != BackendSubsystemFlags::None)
    {
        sdlFlags |= SDL_INIT_VIDEO;
    }
    if ((flags & BackendSubsystemFlags::Audio) != BackendSubsystemFlags::None)
    {
        sdlFlags |= SDL_INIT_AUDIO;
    }
    if ((flags & BackendSubsystemFlags::Gamepad) != BackendSubsystemFlags::None)
    {
        sdlFlags |= SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK;
    }
    if ((flags & BackendSubsystemFlags::Haptic) != BackendSubsystemFlags::None)
    {
        sdlFlags |= SDL_INIT_HAPTIC;
    }
    if ((flags & BackendSubsystemFlags::Sensor) != BackendSubsystemFlags::None)
    {
        sdlFlags |= SDL_INIT_SENSOR;
    }
    if ((flags & BackendSubsystemFlags::Camera) != BackendSubsystemFlags::None)
    {
        sdlFlags |= SDL_INIT_CAMERA;
    }

    return sdlFlags;
}

/// @brief Translates an `SDL_Keymod` bitmask into `gp::hal::input::KeyModifier`.
/// @param[in] sdlMod Raw SDL3 modifier bitmask.
/// @return Engine `KeyModifier` bitmask.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::hal::input::KeyModifier
    translateKeyModifiers(SDL_Keymod sdlMod) noexcept
{
    gp::UInt16 result = 0u;

    if ((sdlMod & SDL_KMOD_LSHIFT) != 0u)
    {
        result |= static_cast<gp::UInt16>(gp::hal::input::KeyModifier::LeftShift);
    }
    if ((sdlMod & SDL_KMOD_RSHIFT) != 0u)
    {
        result |= static_cast<gp::UInt16>(gp::hal::input::KeyModifier::RightShift);
    }
    if ((sdlMod & SDL_KMOD_LCTRL) != 0u)
    {
        result |= static_cast<gp::UInt16>(gp::hal::input::KeyModifier::LeftCtrl);
    }
    if ((sdlMod & SDL_KMOD_RCTRL) != 0u)
    {
        result |= static_cast<gp::UInt16>(gp::hal::input::KeyModifier::RightCtrl);
    }
    if ((sdlMod & SDL_KMOD_LALT) != 0u)
    {
        result |= static_cast<gp::UInt16>(gp::hal::input::KeyModifier::LeftAlt);
    }
    if ((sdlMod & SDL_KMOD_RALT) != 0u)
    {
        result |= static_cast<gp::UInt16>(gp::hal::input::KeyModifier::RightAlt);
    }
    if ((sdlMod & SDL_KMOD_LGUI) != 0u)
    {
        result |= static_cast<gp::UInt16>(gp::hal::input::KeyModifier::LeftGui);
    }
    if ((sdlMod & SDL_KMOD_RGUI) != 0u)
    {
        result |= static_cast<gp::UInt16>(gp::hal::input::KeyModifier::RightGui);
    }
    if ((sdlMod & SDL_KMOD_NUM) != 0u)
    {
        result |= static_cast<gp::UInt16>(gp::hal::input::KeyModifier::NumLock);
    }
    if ((sdlMod & SDL_KMOD_CAPS) != 0u)
    {
        result |= static_cast<gp::UInt16>(gp::hal::input::KeyModifier::CapsLock);
    }
    if ((sdlMod & SDL_KMOD_SCROLL) != 0u)
    {
        result |= static_cast<gp::UInt16>(gp::hal::input::KeyModifier::ScrollLock);
    }

    return static_cast<gp::hal::input::KeyModifier>(result);
}

/// @brief Translates an SDL3 mouse button index to `gp::hal::input::MouseButton`.
/// @param[in] sdlButton SDL mouse button constant (`SDL_BUTTON_LEFT`, etc.).
/// @return Engine `MouseButton` enumerator.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::hal::input::MouseButton
    translateMouseButton(gp::UInt8 sdlButton) noexcept
{
    switch (sdlButton)
    {
    case SDL_BUTTON_LEFT:
        return gp::hal::input::MouseButton::Left;
    case SDL_BUTTON_MIDDLE:
        return gp::hal::input::MouseButton::Middle;
    case SDL_BUTTON_RIGHT:
        return gp::hal::input::MouseButton::Right;
    case SDL_BUTTON_X1:
        return gp::hal::input::MouseButton::X1;
    case SDL_BUTTON_X2:
        return gp::hal::input::MouseButton::X2;
    default:
        return gp::hal::input::MouseButton::Unknown;
    }
}

/// @brief Translates `SDL_MouseButtonFlags` into `gp::hal::input::MouseButtonMask`.
/// @param[in] sdlMask SDL mouse button state bitmask.
/// @return Engine `MouseButtonMask`.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::hal::input::MouseButtonMask
    translateMouseButtonMask(SDL_MouseButtonFlags sdlMask) noexcept
{
    gp::UInt8 mask = 0u;
    if ((sdlMask & SDL_BUTTON_LMASK) != 0u)
    {
        mask |= static_cast<gp::UInt8>(gp::hal::input::MouseButtonMask::Left);
    }
    if ((sdlMask & SDL_BUTTON_MMASK) != 0u)
    {
        mask |= static_cast<gp::UInt8>(gp::hal::input::MouseButtonMask::Middle);
    }
    if ((sdlMask & SDL_BUTTON_RMASK) != 0u)
    {
        mask |= static_cast<gp::UInt8>(gp::hal::input::MouseButtonMask::Right);
    }
    if ((sdlMask & SDL_BUTTON_X1MASK) != 0u)
    {
        mask |= static_cast<gp::UInt8>(gp::hal::input::MouseButtonMask::X1);
    }
    if ((sdlMask & SDL_BUTTON_X2MASK) != 0u)
    {
        mask |= static_cast<gp::UInt8>(gp::hal::input::MouseButtonMask::X2);
    }
    return static_cast<gp::hal::input::MouseButtonMask>(mask);
}

/// @brief Normalizes a raw 16-bit signed SDL gamepad axis value to `[-1.0f..1.0f]` or `[0.0f..1.0f]`.
/// @param[in] rawValue Raw 16-bit signed integer from SDL (`-32768` to `32767`).
/// @return Normalized floating-point axis reading.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float32 normalizeGamepadAxis(gp::Int16 rawValue) noexcept
{
    return (rawValue < 0) ? (static_cast<gp::Float32>(rawValue) / 32768.0f)
                          : (static_cast<gp::Float32>(rawValue) / 32767.0f);
}

/// @brief Translates an SDL joystick hat bitmask to `gp::hal::input::HatPosition`.
/// @param[in] sdlHat SDL hat state bitmask (`SDL_HAT_*`).
/// @return Engine `HatPosition` bitmask.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::hal::input::HatPosition translateHatPosition(gp::UInt8 sdlHat) noexcept
{
    gp::UInt8 hat = 0u;
    if ((sdlHat & SDL_HAT_UP) != 0u)
    {
        hat |= static_cast<gp::UInt8>(gp::hal::input::HatPosition::Up);
    }
    if ((sdlHat & SDL_HAT_RIGHT) != 0u)
    {
        hat |= static_cast<gp::UInt8>(gp::hal::input::HatPosition::Right);
    }
    if ((sdlHat & SDL_HAT_DOWN) != 0u)
    {
        hat |= static_cast<gp::UInt8>(gp::hal::input::HatPosition::Down);
    }
    if ((sdlHat & SDL_HAT_LEFT) != 0u)
    {
        hat |= static_cast<gp::UInt8>(gp::hal::input::HatPosition::Left);
    }
    return static_cast<gp::hal::input::HatPosition>(hat);
}

/// @brief Translates `SDL_DisplayOrientation` to `gp::hal::event::DisplayOrientation`.
/// @param[in] sdlOrientation SDL display orientation constant.
/// @return Engine `DisplayOrientation` enumerator.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::hal::event::DisplayOrientation
    translateDisplayOrientation(SDL_DisplayOrientation sdlOrientation) noexcept
{
    switch (sdlOrientation)
    {
    case SDL_ORIENTATION_LANDSCAPE:
        return gp::hal::event::DisplayOrientation::Landscape;
    case SDL_ORIENTATION_LANDSCAPE_FLIPPED:
        return gp::hal::event::DisplayOrientation::LandscapeFlipped;
    case SDL_ORIENTATION_PORTRAIT:
        return gp::hal::event::DisplayOrientation::Portrait;
    case SDL_ORIENTATION_PORTRAIT_FLIPPED:
        return gp::hal::event::DisplayOrientation::PortraitFlipped;
    case SDL_ORIENTATION_UNKNOWN:
    default:
        return gp::hal::event::DisplayOrientation::Unknown;
    }
}

/// @brief Translates `SDL_PenInputFlags` into `gp::hal::input::PenButton` bitmask.
/// @param[in] penFlags Raw SDL pen input state flags.
/// @return Engine `PenButton` bitmask.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::hal::input::PenButton
    translatePenButtons(SDL_PenInputFlags penFlags) noexcept
{
    gp::UInt8 mask = 0u;
    if ((penFlags & SDL_PEN_INPUT_DOWN) != 0u)
    {
        mask |= static_cast<gp::UInt8>(gp::hal::input::PenButton::Tip);
    }
    if ((penFlags & SDL_PEN_INPUT_BUTTON_1) != 0u)
    {
        mask |= static_cast<gp::UInt8>(gp::hal::input::PenButton::Barrel);
    }
    if ((penFlags & SDL_PEN_INPUT_BUTTON_2) != 0u)
    {
        mask |= static_cast<gp::UInt8>(gp::hal::input::PenButton::Barrel2);
    }
    if ((penFlags & SDL_PEN_INPUT_ERASER_TIP) != 0u)
    {
        mask |= static_cast<gp::UInt8>(gp::hal::input::PenButton::Eraser);
    }
    return static_cast<gp::hal::input::PenButton>(mask);
}

}   // namespace detail

ApplicationBackend::~ApplicationBackend() noexcept
{
    if (m_isInitialized)
    {
        shutdown();
    }
}

BackendResult ApplicationBackend::initialize(const ApplicationBackendDesc& desc) noexcept
{
    if (m_isInitialized) [[unlikely]]
    {
        return BackendResult::AlreadyInitialized;
    }

    m_config = desc;

    // Register application metadata with the OS prior to initializing subsystems
    (void)SDL_SetAppMetadata(
        desc.appName != nullptr ? desc.appName.data() : "GP-Engine",
        desc.appVersion != nullptr ? desc.appVersion.data() : "1.0.0",
        desc.appIdentifier != nullptr ? desc.appIdentifier.data() : "com.graphical-playground.engine"
    );

    const SDL_InitFlags sdlFlags = detail::toSdlInitFlags(desc.subsystems);
    if (!SDL_Init(sdlFlags)) [[unlikely]]
    {
        return BackendResult::InitializationFailed;
    }

    // Reserve a custom SDL event ID for cross-thread wakeEventLoop() notifications
    m_wakeEventType = SDL_RegisterEvents(1);
    m_isInitialized = true;

    // Enumerate physical monitors if the Video subsystem was initialized; otherwise falls back to headless display
    if ((desc.subsystems & BackendSubsystemFlags::Video) != BackendSubsystemFlags::None)
    {
        refreshDisplays();
    }
    activateHeadlessFallbackIfEmpty();

    return BackendResult::Success;
}

void ApplicationBackend::shutdown() noexcept
{
    if (!m_isInitialized)
    {
        return;
    }

    clearDisplayTable();
    SDL_Quit();
    m_wakeEventType = 0u;
    m_isInitialized = false;
}

bool ApplicationBackend::waitEventsTimeout(gp::UInt32 timeoutMs) noexcept
{
    if (!m_isInitialized) [[unlikely]]
    {
        return false;
    }

    // Pass nullptr to wait for an event to arrive in the internal SDL queue without removing it,
    // allowing the subsequent pumpEvents() call to drain everything uniformly.
    return SDL_WaitEventTimeout(nullptr, static_cast<gp::Int32>(timeoutMs));
}

void ApplicationBackend::wakeEventLoop() noexcept
{
    if (!m_isInitialized || m_wakeEventType == 0u) [[unlikely]]
    {
        return;
    }

    SDL_Event wakeEvent{};
    wakeEvent.type = m_wakeEventType;
    (void)SDL_PushEvent(&wakeEvent);
}

template <typename PayloadT>
GP_FORCEINLINE_HINT void ApplicationBackend::commitFixedEvent(
    gp::SPSCByteRingBuffer& stream,
    gp::hal::event::EventCategory category,
    const PayloadT& payload,
    bool isContinuous,
    EventPumpStats& stats
) const noexcept
{
    constexpr gp::USize kTotalPacketBytes = sizeof(gp::hal::event::EventHeader) + sizeof(PayloadT);

    // Tier 1 Backpressure: Shed high-frequency continuous motion events if free space falls into the critical reserve
    if (isContinuous && !stream.canWrite(kTotalPacketBytes + m_config.criticalReserveBytes)) [[unlikely]]
    {
        ++stats.shedContinuousCount;
        return;
    }

    const gp::hal::event::EventHeader header{ .category = category,
                                              .payloadSize = static_cast<gp::UInt16>(sizeof(PayloadT)) };

    if (stream.tryWritePacket(header, &payload, sizeof(PayloadT))) [[likely]]
    {
        ++stats.pushedCount;
        stats.bytesWritten += kTotalPacketBytes;
    }
    else
    {
        ++stats.droppedCriticalCount;
    }
}

template <typename DescriptorT>
GP_FORCEINLINE_HINT void ApplicationBackend::commitVariableEvent(
    gp::SPSCByteRingBuffer& stream,
    gp::hal::event::EventCategory category,
    const DescriptorT& descriptor,
    const void* trailingData,
    gp::UInt16 trailingBytes,
    EventPumpStats& stats
) const noexcept
{
    const gp::UInt16 totalPayloadSize = static_cast<gp::UInt16>(sizeof(DescriptorT) + trailingBytes);
    const gp::USize totalPacketBytes = sizeof(gp::hal::event::EventHeader) + totalPayloadSize;

    if (!stream.canWrite(totalPacketBytes)) [[unlikely]]
    {
        ++stats.droppedCriticalCount;
        return;
    }

    // Stack-pack the descriptor and trailing UTF-8 string so tryWritePacket commits in a single atomic Release store
    alignas(8) gp::UInt8 combinedPayload[sizeof(DescriptorT) + kMaxVariablePayloadBytes];
    gp::memory::copyMemory(combinedPayload, &descriptor, sizeof(DescriptorT));

    if (trailingBytes > 0u && trailingData != nullptr) [[likely]]
    {
        gp::memory::copyMemory(combinedPayload + sizeof(DescriptorT), trailingData, trailingBytes);
    }

    const gp::hal::event::EventHeader header{ .category = category, .payloadSize = totalPayloadSize };

    if (stream.tryWritePacket(header, combinedPayload, totalPayloadSize)) [[likely]]
    {
        ++stats.pushedCount;
        stats.bytesWritten += totalPacketBytes;
    }
    else
    {
        ++stats.droppedCriticalCount;
    }
}

BackendResult ApplicationBackend::pumpEvents(gp::SPSCByteRingBuffer& eventStream, EventPumpStats* outStats) noexcept
{
    if (!m_isInitialized) [[unlikely]]
    {
        return BackendResult::NotInitialized;
    }

    if (!eventStream.isValid()) [[unlikely]]
    {
        return BackendResult::InvalidArguments;
    }

    EventPumpStats localStats{};
    SDL_Event sdlEvent;
    const gp::UInt32 maxEvents = (m_config.maxEventsPerPump > 0u) ? m_config.maxEventsPerPump : ~0u;

    while (localStats.polledCount < maxEvents && SDL_PollEvent(&sdlEvent))
    {
        ++localStats.polledCount;

        switch (sdlEvent.type)
        {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_TERMINATING:
        {
            const gp::hal::event::ApplicationEvent ev{ .action = gp::hal::event::ApplicationAction::QuitRequested };
            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Application, ev, false, localStats);
            break;
        }
        case SDL_EVENT_WILL_ENTER_BACKGROUND:
        {
            const gp::hal::event::ApplicationEvent ev{ .action =
                                                           gp::hal::event::ApplicationAction::WillEnterBackground };
            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Application, ev, false, localStats);
            break;
        }
        case SDL_EVENT_DID_ENTER_BACKGROUND:
        {
            const gp::hal::event::ApplicationEvent ev{ .action =
                                                           gp::hal::event::ApplicationAction::DidEnterBackground };
            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Application, ev, false, localStats);
            break;
        }
        case SDL_EVENT_WILL_ENTER_FOREGROUND:
        {
            const gp::hal::event::ApplicationEvent ev{ .action =
                                                           gp::hal::event::ApplicationAction::WillEnterForeground };
            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Application, ev, false, localStats);
            break;
        }
        case SDL_EVENT_DID_ENTER_FOREGROUND:
        {
            const gp::hal::event::ApplicationEvent ev{ .action =
                                                           gp::hal::event::ApplicationAction::DidEnterForeground };
            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Application, ev, false, localStats);
            break;
        }
        case SDL_EVENT_LOW_MEMORY:
        {
            const gp::hal::event::ApplicationEvent ev{ .action = gp::hal::event::ApplicationAction::LowMemoryWarning };
            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Application, ev, false, localStats);
            break;
        }
        case SDL_EVENT_LOCALE_CHANGED:
        {
            const gp::hal::event::ApplicationEvent ev{ .action = gp::hal::event::ApplicationAction::LocaleChanged };
            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Application, ev, false, localStats);
            break;
        }
        case SDL_EVENT_SYSTEM_THEME_CHANGED:
        {
            const gp::hal::event::ApplicationEvent ev{ .action =
                                                           gp::hal::event::ApplicationAction::SystemThemeChanged };
            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Application, ev, false, localStats);
            break;
        }

        case SDL_EVENT_WINDOW_SHOWN:
        case SDL_EVENT_WINDOW_HIDDEN:
        case SDL_EVENT_WINDOW_EXPOSED:
        case SDL_EVENT_WINDOW_MOUSE_ENTER:
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        {
            gp::hal::event::WindowEvent ev{};
            ev.windowId = sdlEvent.window.windowID;

            switch (sdlEvent.type)
            {
            case SDL_EVENT_WINDOW_SHOWN:
                ev.action = gp::hal::event::WindowAction::Shown;
                break;
            case SDL_EVENT_WINDOW_HIDDEN:
                ev.action = gp::hal::event::WindowAction::Hidden;
                ev.state = gp::hal::WindowState::Hidden;
                break;
            case SDL_EVENT_WINDOW_EXPOSED:
                ev.action = gp::hal::event::WindowAction::Exposed;
                break;
            case SDL_EVENT_WINDOW_MOUSE_ENTER:
                ev.action = gp::hal::event::WindowAction::MouseEntered;
                break;
            case SDL_EVENT_WINDOW_MOUSE_LEAVE:
                ev.action = gp::hal::event::WindowAction::MouseLeft;
                break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                ev.action = gp::hal::event::WindowAction::FocusGained;
                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                ev.action = gp::hal::event::WindowAction::FocusLost;
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                ev.action = gp::hal::event::WindowAction::CloseRequested;
                break;
            default:
                break;
            }

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Window, ev, false, localStats);
            break;
        }
        case SDL_EVENT_WINDOW_MOVED:
        {
            gp::hal::event::WindowEvent ev{};
            ev.windowId = sdlEvent.window.windowID;
            ev.action = gp::hal::event::WindowAction::Moved;
            ev.position = gp::hal::Point2D(sdlEvent.window.data1, sdlEvent.window.data2);
            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Window, ev, false, localStats);
            break;
        }
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        {
            gp::hal::event::WindowEvent ev{};
            ev.windowId = sdlEvent.window.windowID;
            ev.action = (sdlEvent.type == SDL_EVENT_WINDOW_RESIZED) ? gp::hal::event::WindowAction::Resized
                                                                    : gp::hal::event::WindowAction::PixelSizeChanged;
            ev.extent = gp::hal::Extent2D(
                static_cast<gp::UInt32>(sdlEvent.window.data1 > 0 ? sdlEvent.window.data1 : 0),
                static_cast<gp::UInt32>(sdlEvent.window.data2 > 0 ? sdlEvent.window.data2 : 0)
            );
            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Window, ev, false, localStats);
            break;
        }
        case SDL_EVENT_WINDOW_MINIMIZED:
        case SDL_EVENT_WINDOW_MAXIMIZED:
        case SDL_EVENT_WINDOW_RESTORED:
        case SDL_EVENT_WINDOW_OCCLUDED:
        {
            gp::hal::event::WindowEvent ev{};
            ev.windowId = sdlEvent.window.windowID;
            ev.action = gp::hal::event::WindowAction::StateChanged;

            if (sdlEvent.type == SDL_EVENT_WINDOW_MINIMIZED)
            {
                ev.state = gp::hal::WindowState::Minimized;
            }
            else if (sdlEvent.type == SDL_EVENT_WINDOW_MAXIMIZED)
            {
                ev.state = gp::hal::WindowState::Maximized;
            }
            else if (sdlEvent.type == SDL_EVENT_WINDOW_OCCLUDED)
            {
                ev.state = gp::hal::WindowState::Occluded;
            }
            else
            {
                ev.state = gp::hal::WindowState::Normal;
            }

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Window, ev, false, localStats);
            break;
        }
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
        case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
        {
            gp::hal::event::WindowEvent ev{};
            ev.windowId = sdlEvent.window.windowID;
            ev.action = gp::hal::event::WindowAction::ModeChanged;
            ev.mode = (sdlEvent.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN) ? gp::hal::WindowMode::Fullscreen
                                                                           : gp::hal::WindowMode::Windowed;
            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Window, ev, false, localStats);
            break;
        }
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        {
            gp::hal::event::WindowEvent ev{};
            ev.windowId = sdlEvent.window.windowID;
            ev.action = gp::hal::event::WindowAction::DisplayChanged;
            ev.display.displayId = static_cast<gp::UInt32>(sdlEvent.window.data1);
            ev.display.reservedData = 0u;
            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Window, ev, false, localStats);
            break;
        }
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        {
            gp::hal::event::WindowEvent ev{};
            ev.windowId = sdlEvent.window.windowID;
            ev.action = gp::hal::event::WindowAction::DPIChanged;
            SDL_Window* sdlWin = SDL_GetWindowFromID(sdlEvent.window.windowID);
            const gp::Float32 scale = (sdlWin != nullptr) ? SDL_GetWindowDisplayScale(sdlWin) : 1.0f;
            ev.dpi.scaleX = scale;
            ev.dpi.scaleY = scale;
            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Window, ev, false, localStats);
            break;
        }
        case SDL_EVENT_WINDOW_HDR_STATE_CHANGED:
        {
            gp::hal::event::WindowEvent ev{};
            ev.windowId = sdlEvent.window.windowID;
            ev.action = gp::hal::event::WindowAction::HDRStateChanged;
            ev.isHDREnabled = (sdlEvent.window.data1 != 0) ? 1u : 0u;
            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Window, ev, false, localStats);
            break;
        }

        case SDL_EVENT_DISPLAY_ADDED:
        case SDL_EVENT_DISPLAY_REMOVED:
        case SDL_EVENT_DISPLAY_MOVED:
        case SDL_EVENT_DISPLAY_ORIENTATION:
        case SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED:
        case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
        case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED:
        {
            gp::hal::event::DisplayEvent ev{};
            ev.displayId = sdlEvent.display.displayID;
            ev.contentScale = SDL_GetDisplayContentScale(sdlEvent.display.displayID);
            ev.orientation =
                detail::translateDisplayOrientation(SDL_GetCurrentDisplayOrientation(sdlEvent.display.displayID));

            switch (sdlEvent.type)
            {
            case SDL_EVENT_DISPLAY_ADDED:
                ev.action = gp::hal::event::DisplayAction::Connected;
                break;
            case SDL_EVENT_DISPLAY_REMOVED:
                ev.action = gp::hal::event::DisplayAction::Disconnected;
                break;
            case SDL_EVENT_DISPLAY_MOVED:
            {
                ev.action = gp::hal::event::DisplayAction::Moved;
                SDL_Rect bounds{};
                if (SDL_GetDisplayBounds(sdlEvent.display.displayID, &bounds))
                {
                    ev.position = gp::hal::Point2D(bounds.x, bounds.y);
                }
                break;
            }
            case SDL_EVENT_DISPLAY_ORIENTATION:
                ev.action = gp::hal::event::DisplayAction::OrientationChanged;
                ev.orientation =
                    detail::translateDisplayOrientation(static_cast<SDL_DisplayOrientation>(sdlEvent.display.data1));
                break;
            case SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED:
            case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
            {
                ev.action = gp::hal::event::DisplayAction::Resized;
                if (const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(sdlEvent.display.displayID))
                {
                    ev.extent = gp::hal::Extent2D(
                        static_cast<gp::UInt32>(mode->w > 0 ? mode->w : 0),
                        static_cast<gp::UInt32>(mode->h > 0 ? mode->h : 0)
                    );
                }
                break;
            }
            case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED:
                ev.action = gp::hal::event::DisplayAction::ContentScaleChanged;
                break;
            default:
                break;
            }

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Display, ev, false, localStats);
            break;
        }

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        {
            gp::hal::event::KeyboardEvent ev{};
            ev.windowId = sdlEvent.key.windowID;
            ev.keycode = static_cast<gp::hal::input::Keycode>(sdlEvent.key.key);
            ev.scancode = static_cast<gp::hal::input::Scancode>(sdlEvent.key.scancode);
            ev.modifiers = detail::translateKeyModifiers(sdlEvent.key.mod);
            ev.state = sdlEvent.key.down ? gp::hal::input::KeyState::Pressed : gp::hal::input::KeyState::Released;
            ev.isRepeat = sdlEvent.key.repeat ? 1u : 0u;

            // Key repeat events are continuous; initial Press and Release transitions are critical
            const bool isContinuousRepeat = sdlEvent.key.repeat;
            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Keyboard, ev, isContinuousRepeat, localStats);
            break;
        }

        case SDL_EVENT_MOUSE_MOTION:
        {
            gp::hal::event::MouseEvent ev{};
            ev.windowId = sdlEvent.motion.windowID;
            ev.deviceId = sdlEvent.motion.which;
            ev.x = sdlEvent.motion.x;
            ev.y = sdlEvent.motion.y;
            ev.deltaX = sdlEvent.motion.xrel;
            ev.deltaY = sdlEvent.motion.yrel;
            ev.action = gp::hal::event::MouseAction::Moved;
            ev.buttonMask = detail::translateMouseButtonMask(sdlEvent.motion.state);

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Mouse, ev, true, localStats);
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            gp::hal::event::MouseEvent ev{};
            ev.windowId = sdlEvent.button.windowID;
            ev.deviceId = sdlEvent.button.which;
            ev.x = sdlEvent.button.x;
            ev.y = sdlEvent.button.y;
            ev.action = gp::hal::event::MouseAction::ButtonChanged;
            ev.button = detail::translateMouseButton(sdlEvent.button.button);
            ev.state = sdlEvent.button.down ? gp::hal::input::KeyState::Pressed : gp::hal::input::KeyState::Released;
            ev.clicks = sdlEvent.button.clicks;

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Mouse, ev, false, localStats);
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL:
        {
            gp::hal::event::MouseEvent ev{};
            ev.windowId = sdlEvent.wheel.windowID;
            ev.deviceId = sdlEvent.wheel.which;
            ev.x = sdlEvent.wheel.mouse_x;
            ev.y = sdlEvent.wheel.mouse_y;
            ev.deltaX = sdlEvent.wheel.x;
            ev.deltaY = sdlEvent.wheel.y;
            ev.action = gp::hal::event::MouseAction::WheelScrolled;
            ev.isWheelFlipped = (sdlEvent.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) ? 1u : 0u;

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Mouse, ev, true, localStats);
            break;
        }

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        {
            gp::hal::event::GamepadEvent ev{};
            ev.deviceId = sdlEvent.gbutton.which;
            ev.value = sdlEvent.gbutton.down ? 1.0f : 0.0f;
            ev.action = gp::hal::event::GamepadAction::ButtonChanged;
            ev.control.button = static_cast<gp::hal::input::GamepadButton>(sdlEvent.gbutton.button);
            ev.state = sdlEvent.gbutton.down ? gp::hal::input::KeyState::Pressed : gp::hal::input::KeyState::Released;

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Gamepad, ev, false, localStats);
            break;
        }
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        {
            gp::hal::event::GamepadEvent ev{};
            ev.deviceId = sdlEvent.gaxis.which;
            ev.value = detail::normalizeGamepadAxis(sdlEvent.gaxis.value);
            ev.action = gp::hal::event::GamepadAction::AxisMoved;
            ev.control.axis = static_cast<gp::hal::input::GamepadAxis>(sdlEvent.gaxis.axis);

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Gamepad, ev, true, localStats);
            break;
        }
        case SDL_EVENT_JOYSTICK_HAT_MOTION:
        {
            gp::hal::event::GamepadEvent ev{};
            ev.deviceId = sdlEvent.jhat.which;
            ev.action = gp::hal::event::GamepadAction::HatChanged;
            ev.control.hatPosition = detail::translateHatPosition(sdlEvent.jhat.value);

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Gamepad, ev, false, localStats);
            break;
        }
        case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
        {
            gp::hal::event::GamepadEvent ev{};
            ev.deviceId = sdlEvent.gtouchpad.which;
            ev.value = sdlEvent.gtouchpad.x;
            ev.secondaryValue = sdlEvent.gtouchpad.y;
            ev.action = gp::hal::event::GamepadAction::TouchpadChanged;

            if (sdlEvent.type == SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN)
            {
                ev.control.touchState = gp::hal::input::TouchFingerState::Down;
            }
            else if (sdlEvent.type == SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION)
            {
                ev.control.touchState = gp::hal::input::TouchFingerState::Motion;
            }
            else
            {
                ev.control.touchState = gp::hal::input::TouchFingerState::Up;
            }

            ev.state = static_cast<gp::hal::input::KeyState>(sdlEvent.gtouchpad.finger);
            ev.batteryPercent = static_cast<gp::UInt8>(sdlEvent.gtouchpad.touchpad);

            const bool isMotion = (sdlEvent.type == SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION);
            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Gamepad, ev, isMotion, localStats);
            break;
        }
        case SDL_EVENT_JOYSTICK_BATTERY_UPDATED:
        {
            gp::hal::event::GamepadEvent ev{};
            ev.deviceId = sdlEvent.jbattery.which;
            ev.action = gp::hal::event::GamepadAction::BatteryUpdated;
            ev.batteryPercent =
                (sdlEvent.jbattery.percent >= 0) ? static_cast<gp::UInt8>(sdlEvent.jbattery.percent) : 0u;

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Gamepad, ev, false, localStats);
            break;
        }

        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_CANCELED:
        {
            gp::hal::event::TouchEvent ev{};
            ev.touchDeviceId = sdlEvent.tfinger.touchID;
            ev.fingerId = sdlEvent.tfinger.fingerID;
            ev.x = sdlEvent.tfinger.x;
            ev.y = sdlEvent.tfinger.y;
            ev.deltaX = sdlEvent.tfinger.dx;
            ev.deltaY = sdlEvent.tfinger.dy;
            ev.pressure = sdlEvent.tfinger.pressure;
            ev.windowId = sdlEvent.tfinger.windowID;

            if (sdlEvent.type == SDL_EVENT_FINGER_DOWN)
            {
                ev.state = gp::hal::input::TouchFingerState::Down;
            }
            else if (sdlEvent.type == SDL_EVENT_FINGER_MOTION)
            {
                ev.state = gp::hal::input::TouchFingerState::Motion;
            }
            else
            {
                ev.state = gp::hal::input::TouchFingerState::Up;
            }

            ev.isCancelled = (sdlEvent.type == SDL_EVENT_FINGER_CANCELED) ? 1u : 0u;

            const bool isMotion = (sdlEvent.type == SDL_EVENT_FINGER_MOTION);
            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Touch, ev, isMotion, localStats);
            break;
        }

        case SDL_EVENT_PEN_PROXIMITY_IN:
        case SDL_EVENT_PEN_PROXIMITY_OUT:
        {
            gp::hal::event::PenEvent ev{};
            ev.windowId = sdlEvent.pproximity.windowID;
            ev.penId = sdlEvent.pproximity.which;
            ev.action = (sdlEvent.type == SDL_EVENT_PEN_PROXIMITY_IN) ? gp::hal::event::PenAction::ProximityIn
                                                                      : gp::hal::event::PenAction::ProximityOut;

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Pen, ev, false, localStats);
            break;
        }
        case SDL_EVENT_PEN_DOWN:
        case SDL_EVENT_PEN_UP:
        {
            gp::hal::event::PenEvent ev{};
            ev.windowId = sdlEvent.ptouch.windowID;
            ev.penId = sdlEvent.ptouch.which;
            ev.x = sdlEvent.ptouch.x;
            ev.y = sdlEvent.ptouch.y;
            ev.action = gp::hal::event::PenAction::TipChanged;
            ev.buttons = detail::translatePenButtons(sdlEvent.ptouch.pen_state);
            ev.state = sdlEvent.ptouch.down ? gp::hal::input::KeyState::Pressed : gp::hal::input::KeyState::Released;
            ev.isEraser = sdlEvent.ptouch.eraser ? 1u : 0u;

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Pen, ev, false, localStats);
            break;
        }
        case SDL_EVENT_PEN_MOTION:
        {
            gp::hal::event::PenEvent ev{};
            ev.windowId = sdlEvent.pmotion.windowID;
            ev.penId = sdlEvent.pmotion.which;
            ev.x = sdlEvent.pmotion.x;
            ev.y = sdlEvent.pmotion.y;
            ev.action = gp::hal::event::PenAction::Moved;
            ev.buttons = detail::translatePenButtons(sdlEvent.pmotion.pen_state);
            ev.isEraser = (sdlEvent.pmotion.pen_state & SDL_PEN_INPUT_ERASER_TIP) != 0u ? 1u : 0u;

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Pen, ev, true, localStats);
            break;
        }
        case SDL_EVENT_PEN_AXIS:
        {
            gp::hal::event::PenEvent ev{};
            ev.windowId = sdlEvent.paxis.windowID;
            ev.penId = sdlEvent.paxis.which;
            ev.x = sdlEvent.paxis.x;
            ev.y = sdlEvent.paxis.y;
            ev.action = gp::hal::event::PenAction::Moved;
            ev.buttons = detail::translatePenButtons(sdlEvent.paxis.pen_state);
            if (static_cast<gp::UInt8>(sdlEvent.paxis.axis) < static_cast<gp::UInt8>(gp::hal::input::PenAxis::Count))
            {
                ev.axes[static_cast<gp::UInt8>(sdlEvent.paxis.axis)] = sdlEvent.paxis.value;
            }

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Pen, ev, true, localStats);
            break;
        }
        case SDL_EVENT_PEN_BUTTON_DOWN:
        case SDL_EVENT_PEN_BUTTON_UP:
        {
            gp::hal::event::PenEvent ev{};
            ev.windowId = sdlEvent.pbutton.windowID;
            ev.penId = sdlEvent.pbutton.which;
            ev.x = sdlEvent.pbutton.x;
            ev.y = sdlEvent.pbutton.y;
            ev.action = gp::hal::event::PenAction::ButtonChanged;
            ev.buttons = detail::translatePenButtons(sdlEvent.pbutton.pen_state);
            ev.state = sdlEvent.pbutton.down ? gp::hal::input::KeyState::Pressed : gp::hal::input::KeyState::Released;

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Pen, ev, false, localStats);
            break;
        }

        case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
        {
            gp::hal::event::SensorEvent ev{};
            ev.timestampNs = sdlEvent.gsensor.sensor_timestamp;
            ev.deviceId = sdlEvent.gsensor.which;
            ev.values[0] = sdlEvent.gsensor.data[0];
            ev.values[1] = sdlEvent.gsensor.data[1];
            ev.values[2] = sdlEvent.gsensor.data[2];

            switch (sdlEvent.gsensor.sensor)
            {
            case SDL_SENSOR_ACCEL:
                ev.type = gp::hal::input::GamepadSensorType::Accelerometer;
                ev.side = gp::hal::input::ActuatorSide::Both;
                break;
            case SDL_SENSOR_GYRO:
                ev.type = gp::hal::input::GamepadSensorType::Gyroscope;
                ev.side = gp::hal::input::ActuatorSide::Both;
                break;
            case SDL_SENSOR_ACCEL_L:
                ev.type = gp::hal::input::GamepadSensorType::Accelerometer;
                ev.side = gp::hal::input::ActuatorSide::Left;
                break;
            case SDL_SENSOR_GYRO_L:
                ev.type = gp::hal::input::GamepadSensorType::Gyroscope;
                ev.side = gp::hal::input::ActuatorSide::Left;
                break;
            case SDL_SENSOR_ACCEL_R:
                ev.type = gp::hal::input::GamepadSensorType::Accelerometer;
                ev.side = gp::hal::input::ActuatorSide::Right;
                break;
            case SDL_SENSOR_GYRO_R:
                ev.type = gp::hal::input::GamepadSensorType::Gyroscope;
                ev.side = gp::hal::input::ActuatorSide::Right;
                break;
            default:
                ev.type = gp::hal::input::GamepadSensorType::Unknown;
                break;
            }

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Sensor, ev, true, localStats);
            break;
        }
        case SDL_EVENT_SENSOR_UPDATE:
        {
            gp::hal::event::SensorEvent ev{};
            ev.timestampNs = sdlEvent.sensor.sensor_timestamp;
            ev.deviceId = sdlEvent.sensor.which;
            ev.values[0] = sdlEvent.sensor.data[0];
            ev.values[1] = sdlEvent.sensor.data[1];
            ev.values[2] = sdlEvent.sensor.data[2];
            ev.side = gp::hal::input::ActuatorSide::Both;

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Sensor, ev, true, localStats);
            break;
        }

        case SDL_EVENT_TEXT_INPUT:
        {
            const gp::UInt16 textLen = detail::boundedUtf8Length(sdlEvent.text.text, kMaxVariablePayloadBytes);
            if (textLen > 0u)
            {
                gp::hal::event::TextInputEvent desc{};
                desc.windowId = sdlEvent.text.windowID;
                desc.byteLength = textLen;
                desc.action = gp::hal::event::TextInputAction::Committed;

                commitVariableEvent(
                    eventStream, gp::hal::event::EventCategory::TextInput, desc, sdlEvent.text.text, textLen, localStats
                );
            }
            break;
        }
        case SDL_EVENT_TEXT_EDITING:
        {
            const gp::UInt16 textLen = detail::boundedUtf8Length(sdlEvent.edit.text, kMaxVariablePayloadBytes);
            gp::hal::event::TextInputEvent desc{};
            desc.windowId = sdlEvent.edit.windowID;
            desc.cursorStart = sdlEvent.edit.start;
            desc.selectionLength = sdlEvent.edit.length;
            desc.byteLength = textLen;
            desc.action = gp::hal::event::TextInputAction::Editing;

            commitVariableEvent(
                eventStream, gp::hal::event::EventCategory::TextInput, desc, sdlEvent.edit.text, textLen, localStats
            );
            break;
        }
        case SDL_EVENT_DROP_BEGIN:
        case SDL_EVENT_DROP_POSITION:
        case SDL_EVENT_DROP_COMPLETE:
        case SDL_EVENT_DROP_FILE:
        case SDL_EVENT_DROP_TEXT:
        {
            gp::hal::event::DropEvent desc{};
            desc.windowId = sdlEvent.drop.windowID;
            desc.x = sdlEvent.drop.x;
            desc.y = sdlEvent.drop.y;

            const char* payloadStr = nullptr;
            gp::UInt16 payloadBytes = 0u;

            if (sdlEvent.type == SDL_EVENT_DROP_BEGIN)
            {
                desc.action = gp::hal::event::DropAction::Begin;
            }
            else if (sdlEvent.type == SDL_EVENT_DROP_POSITION)
            {
                desc.action = gp::hal::event::DropAction::Position;
            }
            else if (sdlEvent.type == SDL_EVENT_DROP_COMPLETE)
            {
                desc.action = gp::hal::event::DropAction::Complete;
            }
            else
            {
                desc.action = (sdlEvent.type == SDL_EVENT_DROP_FILE) ? gp::hal::event::DropAction::FileDropped
                                                                     : gp::hal::event::DropAction::TextDropped;
                payloadStr = sdlEvent.drop.data;
                payloadBytes = detail::boundedUtf8Length(payloadStr, kMaxVariablePayloadBytes);
            }

            desc.byteLength = payloadBytes;
            commitVariableEvent(
                eventStream, gp::hal::event::EventCategory::Drop, desc, payloadStr, payloadBytes, localStats
            );
            break;
        }
        case SDL_EVENT_CLIPBOARD_UPDATE:
        {
            gp::hal::event::ClipboardEvent desc{};
            desc.byteLength = 0u;
            desc.action = gp::hal::event::ClipboardAction::Updated;
            desc.isOwner = sdlEvent.clipboard.owner ? 1u : 0u;
            desc.mimeTypeCount =
                (sdlEvent.clipboard.num_mime_types > 0)
                    ? static_cast<gp::UInt8>(
                          sdlEvent.clipboard.num_mime_types > 255 ? 255 : sdlEvent.clipboard.num_mime_types
                      )
                    : 0u;

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::Clipboard, desc, false, localStats);
            break;
        }

        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
        case SDL_EVENT_GAMEPAD_REMAPPED:
        {
            if (sdlEvent.type == SDL_EVENT_GAMEPAD_ADDED)
            {
                // Automatically open the gamepad so SDL begins generating button/axis events for it
                (void)SDL_OpenGamepad(sdlEvent.gdevice.which);
            }
            else if (sdlEvent.type == SDL_EVENT_GAMEPAD_REMOVED)
            {
                if (SDL_Gamepad* pad = SDL_GetGamepadFromID(sdlEvent.gdevice.which))
                {
                    SDL_CloseGamepad(pad);
                }
            }

            gp::hal::event::DeviceChangeEvent ev{};
            ev.deviceId = sdlEvent.gdevice.which;
            ev.domain = gp::hal::event::DeviceDomain::Input;
            ev.inputType = gp::hal::input::InputDeviceType::Gamepad;

            if (sdlEvent.type == SDL_EVENT_GAMEPAD_ADDED)
            {
                ev.action = gp::hal::event::DeviceAction::Connected;
            }
            else if (sdlEvent.type == SDL_EVENT_GAMEPAD_REMOVED)
            {
                ev.action = gp::hal::event::DeviceAction::Disconnected;
            }
            else
            {
                ev.action = gp::hal::event::DeviceAction::Remapped;
            }

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::DeviceChange, ev, false, localStats);
            break;
        }
        case SDL_EVENT_KEYBOARD_ADDED:
        case SDL_EVENT_KEYBOARD_REMOVED:
        {
            gp::hal::event::DeviceChangeEvent ev{};
            ev.deviceId = sdlEvent.kdevice.which;
            ev.domain = gp::hal::event::DeviceDomain::Input;
            ev.inputType = gp::hal::input::InputDeviceType::Keyboard;
            ev.action = (sdlEvent.type == SDL_EVENT_KEYBOARD_ADDED) ? gp::hal::event::DeviceAction::Connected
                                                                    : gp::hal::event::DeviceAction::Disconnected;

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::DeviceChange, ev, false, localStats);
            break;
        }
        case SDL_EVENT_MOUSE_ADDED:
        case SDL_EVENT_MOUSE_REMOVED:
        {
            gp::hal::event::DeviceChangeEvent ev{};
            ev.deviceId = sdlEvent.mdevice.which;
            ev.domain = gp::hal::event::DeviceDomain::Input;
            ev.inputType = gp::hal::input::InputDeviceType::Mouse;
            ev.action = (sdlEvent.type == SDL_EVENT_MOUSE_ADDED) ? gp::hal::event::DeviceAction::Connected
                                                                 : gp::hal::event::DeviceAction::Disconnected;

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::DeviceChange, ev, false, localStats);
            break;
        }
        case SDL_EVENT_AUDIO_DEVICE_ADDED:
        case SDL_EVENT_AUDIO_DEVICE_REMOVED:
        case SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED:
        {
            gp::hal::event::DeviceChangeEvent ev{};
            ev.deviceId = sdlEvent.adevice.which;
            ev.domain = sdlEvent.adevice.recording ? gp::hal::event::DeviceDomain::AudioCapture
                                                   : gp::hal::event::DeviceDomain::AudioOutput;

            if (sdlEvent.type == SDL_EVENT_AUDIO_DEVICE_ADDED)
            {
                ev.action = gp::hal::event::DeviceAction::Connected;
            }
            else if (sdlEvent.type == SDL_EVENT_AUDIO_DEVICE_REMOVED)
            {
                ev.action = gp::hal::event::DeviceAction::Disconnected;
            }
            else
            {
                ev.action = gp::hal::event::DeviceAction::DefaultChanged;
            }

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::DeviceChange, ev, false, localStats);
            break;
        }
        case SDL_EVENT_CAMERA_DEVICE_ADDED:
        case SDL_EVENT_CAMERA_DEVICE_REMOVED:
        case SDL_EVENT_CAMERA_DEVICE_APPROVED:
        case SDL_EVENT_CAMERA_DEVICE_DENIED:
        {
            gp::hal::event::DeviceChangeEvent ev{};
            ev.deviceId = sdlEvent.cdevice.which;
            ev.domain = gp::hal::event::DeviceDomain::Camera;

            if (sdlEvent.type == SDL_EVENT_CAMERA_DEVICE_ADDED)
            {
                ev.action = gp::hal::event::DeviceAction::Connected;
            }
            else if (sdlEvent.type == SDL_EVENT_CAMERA_DEVICE_REMOVED)
            {
                ev.action = gp::hal::event::DeviceAction::Disconnected;
            }
            else if (sdlEvent.type == SDL_EVENT_CAMERA_DEVICE_APPROVED)
            {
                ev.action = gp::hal::event::DeviceAction::PermissionGranted;
            }
            else
            {
                ev.action = gp::hal::event::DeviceAction::PermissionDenied;
            }

            commitFixedEvent(eventStream, gp::hal::event::EventCategory::DeviceChange, ev, false, localStats);
            break;
        }

        default:
            break;
        }
    }

    if (outStats != nullptr)
    {
        *outStats = localStats;
    }

    return (localStats.droppedCriticalCount == 0u) ? BackendResult::Success : BackendResult::BufferOverflow;
}

UniquePtr<input::IInputSystem> ApplicationBackend::createInputSystem() noexcept
{
    if (!m_isInitialized) [[unlikely]]
    {
        return nullptr;
    }

    return gp::makeUnique<sdl3::InputSystem>();
}

UniquePtr<IWindow> ApplicationBackend::createWindow(const WindowDesc& desc) noexcept
{
    if (!m_isInitialized) [[unlikely]]
    {
        return nullptr;
    }

    return gp::makeUnique<gp::hal::sdl3::Window>(desc);
}

void ApplicationBackend::refreshDisplays() noexcept
{
    clearDisplayTable();

    const DisplayId primaryId = static_cast<DisplayId>(SDL_GetPrimaryDisplay());
    int count = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&count);

    if (ids != nullptr && count > 0)
    {
        const gp::UInt32 limit =
            (static_cast<gp::UInt32>(count) < kMaxDisplays) ? static_cast<gp::UInt32>(count) : kMaxDisplays;

        for (gp::UInt32 i = 0u; i < limit; ++i)
        {
            const DisplayId currentId = static_cast<DisplayId>(ids[i]);
            m_sdlDisplays[i].initialize(currentId, currentId == primaryId);
            (void)registerDisplay(&m_sdlDisplays[i]);
        }
    }

    if (ids != nullptr)
    {
        SDL_free(ids);
    }

    activateHeadlessFallbackIfEmpty();
}

void ApplicationBackend::onBackendDisplayConnected(DisplayId displayId) noexcept
{
    if (displayId == kInvalidDisplayId) [[unlikely]]
    {
        return;
    }

    const DisplayId primaryId = static_cast<DisplayId>(SDL_GetPrimaryDisplay());

    // Check if this display ID already occupies a slot in m_sdlDisplays
    for (gp::UInt32 i = 0u; i < kMaxDisplays; ++i)
    {
        if (m_sdlDisplays[i].getId() == displayId)
        {
            m_sdlDisplays[i].initialize(displayId, displayId == primaryId);
            (void)registerDisplay(&m_sdlDisplays[i]);
            return;
        }
    }

    // Otherwise claim the first disconnected/unused slot in m_sdlDisplays
    for (gp::UInt32 i = 0u; i < kMaxDisplays; ++i)
    {
        if (!m_sdlDisplays[i].isValid())
        {
            m_sdlDisplays[i].initialize(displayId, displayId == primaryId);
            (void)registerDisplay(&m_sdlDisplays[i]);
            return;
        }
    }
}

void ApplicationBackend::onBackendDisplayDisconnected(DisplayId displayId) noexcept
{
    // Refresh the OS primary display designation in case the primary monitor was unplugged
    const DisplayId newPrimaryId = static_cast<DisplayId>(SDL_GetPrimaryDisplay());
    if (newPrimaryId != kInvalidDisplayId && newPrimaryId != displayId)
    {
        m_primaryDisplayId = newPrimaryId;
    }
}

}   // namespace gp::hal::sdl3
