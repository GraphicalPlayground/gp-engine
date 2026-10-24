// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "backend/SDL3ApplicationBackend.hpp"
#include "containers/queues/SPSCRingBuffer.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "event/EventCategory.hpp"
#include "event/EventHeader.hpp"
#include "event/input/KeyboardEvent.hpp"
#include "event/input/MouseEvent.hpp"
#include "event/system/ApplicationEvent.hpp"
#include "event/system/WindowEvent.hpp"
#include "Launch.hpp"
#include "platforms/base/system/Thread.hpp"
#include <iostream>

namespace gp
{

template <typename PayloadT>
bool decodeAndPrintEvent(gp::SPSCByteRingBuffer& stream, [[maybe_unused]] const char* eventName, auto&& printLambda)
{
    PayloadT payload{};
    if (stream.tryPeekAt(sizeof(gp::hal::event::EventHeader), &payload, sizeof(PayloadT)))
    {
        printLambda(payload);
        stream.advanceConsumer(sizeof(gp::hal::event::EventHeader) + sizeof(PayloadT));
        return true;
    }
    return false;
}

int launch([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    std::cout << "--- GP Engine: Lock-Free Event Pump Test ---" << std::endl;

    // 1. Initialize the SPSC Byte Ring Buffer (4KB)
    gp::SPSCByteRingBuffer eventStream;
    if (!eventStream.initialize(4096u))
    {
        std::cerr << "Failed to allocate SPSC ring buffer!" << std::endl;
        return -1;
    }

    gp::hal::sdl3::ApplicationBackend backend;
    gp::hal::ApplicationBackendDesc desc{};
    desc.appName = "GP Engine Test";
    desc.subsystems = gp::hal::BackendSubsystemFlags::Default;
    desc.maxEventsPerPump = 256u;
    desc.criticalReserveBytes = 256u;

    if (backend.initialize(desc) != gp::hal::BackendResult::Success)
    {
        std::cerr << "Failed to initialize SDL3 Backend!" << std::endl;
        return -1;
    }

    auto window = backend.createWindow({ .startOpened = true });
    if (!window)
    {
        std::cerr << "Failed to create SDL window!" << std::endl;
        backend.shutdown();
        return -1;
    }

    std::cout << "\nWindow ID: " << window->getId() << std::endl;
    std::cout << "Initial Client Size: " << window->getSize().width << "x" << window->getSize().height << std::endl;
    std::cout << "Initial Framebuffer Size: " << window->getFramebufferSize().width << "x"
              << window->getFramebufferSize().height << std::endl;
    std::cout << "\nWindow opened. Press 'Q' or 'Escape' or close the window to exit." << std::endl;
    std::cout << "Move the mouse and press keys to see lock-free TLV routing in action...\n" << std::endl;

    bool isRunning = true;

    while (isRunning)
    {
        // OS Thread Pump: Translate raw SDL events -> Pack into TLV bytes -> Push to SPSC queue
        gp::hal::EventPumpStats stats{};
        (void)backend.pumpEvents(eventStream, &stats);

        // Game Thread Drain: Read TLV Headers -> Decode Payloads -> Advance Consumer Index
        while (!eventStream.isEmptyApprox())
        {
            gp::hal::event::EventHeader header{};
            if (!eventStream.tryPeekAt(0, &header, sizeof(gp::hal::event::EventHeader)))
            {
                break;   // Not enough bytes to form a header
            }

            switch (header.category)
            {
            case gp::hal::event::EventCategory::Application:
            {
                decodeAndPrintEvent<gp::hal::event::ApplicationEvent>(
                    eventStream,
                    "AppEvent",
                    [&](const gp::hal::event::ApplicationEvent& ev)
                {
                    if (ev.action == gp::hal::event::ApplicationAction::QuitRequested)
                    {
                        std::cout << "[SYSTEM] Quit Requested. Shutting down." << std::endl;
                        isRunning = false;
                    }
                }
                );
                break;
            }

            case gp::hal::event::EventCategory::Window:
            {
                decodeAndPrintEvent<gp::hal::event::WindowEvent>(
                    eventStream,
                    "WindowEvent",
                    [&](const gp::hal::event::WindowEvent& ev)
                {
                    window->onWindowEvent(ev);

                    if (ev.action == gp::hal::event::WindowAction::CloseRequested)
                    {
                        std::cout << "[WINDOW] Close requested." << std::endl;
                        window->close();
                        isRunning = false;
                    }
                    else if (ev.action == gp::hal::event::WindowAction::Resized)
                    {
                        // Query shadow state in 0ns without syscalls!
                        const gp::hal::Extent2D sz = window->getSize();
                        const gp::hal::Extent2D fb = window->getFramebufferSize();
                        std::cout << "[WINDOW] Resized. Logical: " << sz.width << "x" << sz.height
                                  << " | Framebuffer: " << fb.width << "x" << fb.height << std::endl;
                    }
                    else if (ev.action == gp::hal::event::WindowAction::FocusGained)
                    {
                        std::cout << "[WINDOW] Focus Gained. Active: " << (window->isActive() ? "True" : "False")
                                  << std::endl;
                    }
                    else if (ev.action == gp::hal::event::WindowAction::FocusLost)
                    {
                        std::cout << "[WINDOW] Focus Lost." << std::endl;
                    }
                }
                );
                break;
            }

            case gp::hal::event::EventCategory::Keyboard:
            {
                decodeAndPrintEvent<gp::hal::event::KeyboardEvent>(
                    eventStream,
                    "KeyboardEvent",
                    [&](const gp::hal::event::KeyboardEvent& ev)
                {
                    const char* stateStr = (ev.state == gp::hal::input::KeyState::Pressed) ? "Pressed" : "Released";

                    // Treat 'Q' or 'Escape' as a quit command
                    if (ev.state == gp::hal::input::KeyState::Released &&
                        (ev.keycode == gp::hal::input::Keycode::Q || ev.keycode == gp::hal::input::Keycode::Escape))
                    {
                        std::cout << "[KEYBOARD] " << stateStr << " Q/Esc. Shutting down." << std::endl;
                        isRunning = false;
                    }
                    else if (ev.isRepeat == 0u)   // Only print initial presses/releases to reduce spam
                    {
                        std::cout << "[KEYBOARD] " << stateStr << " KeyCode: " << static_cast<UInt32>(ev.keycode)
                                  << std::endl;
                    }
                }
                );
                break;
            }
            case gp::hal::event::EventCategory::Mouse:
            {
                decodeAndPrintEvent<gp::hal::event::MouseEvent>(
                    eventStream,
                    "MouseEvent",
                    [&](const gp::hal::event::MouseEvent& ev)
                {
                    if (ev.action == gp::hal::event::MouseAction::ButtonChanged)
                    {
                        const char* stateStr = (ev.state == gp::hal::input::KeyState::Pressed) ? "Pressed" : "Released";
                        std::cout << "[MOUSE] Button " << static_cast<UInt32>(ev.button) << " " << stateStr
                                  << " at X: " << ev.x << " Y: " << ev.y << std::endl;
                    }
                    else if (ev.action == gp::hal::event::MouseAction::WheelScrolled)
                    {
                        std::cout << "[MOUSE] Scroll DeltaY: " << ev.deltaY << std::endl;
                    }
                }
                );
                break;
            }

            default:
            {
                // For any event category we don't explicitly print in this test,
                // just advance the consumer pointer by the header size + payload size.
                eventStream.advanceConsumer(sizeof(gp::hal::event::EventHeader) + header.payloadSize);
                break;
            }
            }
        }

        // Prevent the test loop from burning 100% of a CPU core
        platform::Thread::sleep(16);
    }

    window->close();
    backend.shutdown();
    eventStream.shutdown();

    std::cout << "--- Shutdown Complete ---" << std::endl;

    return 0;
}

}   // namespace gp
