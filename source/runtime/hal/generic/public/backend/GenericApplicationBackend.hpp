// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "backend/ApplicationBackendEnums.hpp"
#include "backend/IApplicationBackend.hpp"
#include "display/GenericDisplay.hpp"
#include "window/GenericWindow.hpp"

namespace gp::hal::generic
{

/// @brief Generic application backend that serves as a base for platform-specific backends (e.g., SDL, GLFW, Win32).
class ApplicationBackend : public IApplicationBackend
{
public:
    /// @brief Reserved identifier for the synthetic virtual display used in headless environments.
    static constexpr DisplayId kHeadlessDisplayId = 0xFFFFFFFFu;

protected:
    ApplicationBackendDesc m_config{};

    /// @brief Active table of pointers pointing to concrete backend Display storage (or `m_headlessDisplay`).
    gp::hal::generic::Display* m_displayTable[kMaxDisplays]{};
    gp::UInt32 m_displayCount{ 0u };
    DisplayId m_primaryDisplayId{ kInvalidDisplayId };

    /// @brief Synthetic 1080p@60Hz fallback display activated when no physical monitors are available.
    gp::hal::generic::Display m_headlessDisplay{};

    bool m_isInitialized{ false };

public:
    [[nodiscard]] constexpr ApplicationBackend() noexcept = default;
    virtual ~ApplicationBackend() noexcept override = default;

    ApplicationBackend(const ApplicationBackend&) = delete;
    ApplicationBackend& operator=(const ApplicationBackend&) = delete;
    ApplicationBackend(ApplicationBackend&&) = delete;
    ApplicationBackend& operator=(ApplicationBackend&&) = delete;

public:
    /// @copydoc IApplicationBackend::initialize()
    [[nodiscard]] constexpr BackendResult initialize(const ApplicationBackendDesc& desc) noexcept override
    {
        if (m_isInitialized) [[unlikely]]
        {
            return BackendResult::AlreadyInitialized;
        }

        m_config = desc;
        activateHeadlessFallbackIfEmpty();
        m_isInitialized = true;
        return BackendResult::Success;
    }

    /// @copydoc IApplicationBackend::shutdown()
    constexpr void shutdown() noexcept override
    {
        clearDisplayTable();
        m_isInitialized = false;
    }

    /// @copydoc IApplicationBackend::isInitialized()
    [[nodiscard]] constexpr bool isInitialized() const noexcept override
    {
        return m_isInitialized;
    }

    /// @copydoc IApplicationBackend::pumpEvents()
    constexpr BackendResult pumpEvents(
        [[maybe_unused]] gp::SPSCByteRingBuffer& eventStream, EventPumpStats* outStats = nullptr
    ) noexcept override
    {
        if (outStats != nullptr)
        {
            *outStats = EventPumpStats{};
        }
        return m_isInitialized ? BackendResult::Success : BackendResult::NotInitialized;
    }

    /// @copydoc IApplicationBackend::waitEventsTimeout()
    [[nodiscard]] constexpr bool waitEventsTimeout([[maybe_unused]] gp::UInt32 timeoutMs) noexcept override
    {
        return false;
    }

    /// @copydoc IApplicationBackend::wakeEventLoop()
    constexpr void wakeEventLoop() noexcept override
    {}

    /// @copydoc IApplicationBackend::getBackendName()
    [[nodiscard]] constexpr gp::StringView getBackendName() const noexcept override
    {
        return "Generic";
    }

    /// @copydoc IApplicationBackend::createWindow()
    [[nodiscard]] gp::UniquePtr<IWindow> createWindow(const WindowDesc& desc) noexcept override
    {
        if (!m_isInitialized) [[unlikely]]
        {
            return nullptr;
        }
        return gp::makeUnique<gp::hal::generic::Window>(desc);
    }

    /// @copydoc IApplicationBackend::getDisplayCount()
    [[nodiscard]] constexpr gp::UInt32 getDisplayCount() const noexcept override
    {
        return m_displayCount;
    }

    /// @copydoc IApplicationBackend::getDisplay()
    [[nodiscard]] constexpr const IDisplay* getDisplay(gp::UInt32 index) const noexcept override
    {
        return (index < m_displayCount) ? m_displayTable[index] : nullptr;
    }

    /// @copydoc IApplicationBackend::findDisplayById()
    [[nodiscard]] constexpr const IDisplay* findDisplayById(DisplayId displayId) const noexcept override
    {
        if (displayId == kInvalidDisplayId) [[unlikely]]
        {
            return nullptr;
        }

        for (gp::UInt32 i = 0u; i < m_displayCount; ++i)
        {
            if (m_displayTable[i] != nullptr && m_displayTable[i]->getId() == displayId)
            {
                return m_displayTable[i];
            }
        }
        return nullptr;
    }

    /// @copydoc IApplicationBackend::getPrimaryDisplay()
    [[nodiscard]] constexpr const IDisplay* getPrimaryDisplay() const noexcept override
    {
        // 1. Check cached primary ID first
        if (m_primaryDisplayId != kInvalidDisplayId)
        {
            if (const IDisplay* primary = findDisplayById(m_primaryDisplayId))
            {
                return primary;
            }
        }

        // 2. Scan for any display flagged with DisplayFlags::Primary
        for (gp::UInt32 i = 0u; i < m_displayCount; ++i)
        {
            if (m_displayTable[i] != nullptr && m_displayTable[i]->isPrimary())
            {
                return m_displayTable[i];
            }
        }

        // 3. Fall back to the first available display or headless display
        return (m_displayCount > 0u && m_displayTable[0] != nullptr) ? m_displayTable[0] : &m_headlessDisplay;
    }

    /// @copydoc IApplicationBackend::getDisplayForPoint()
    [[nodiscard]] constexpr const IDisplay* getDisplayForPoint(const Point2D& point) const noexcept override
    {
        if (m_displayCount == 0u) [[unlikely]]
        {
            return &m_headlessDisplay;
        }

        // 1. Fast path: Exact containment test
        for (gp::UInt32 i = 0u; i < m_displayCount; ++i)
        {
            const gp::hal::generic::Display* disp = m_displayTable[i];
            if (disp != nullptr && disp->isConnected() && disp->getBounds().contains(point))
            {
                return disp;
            }
        }

        // 2. Fallback: Nearest monitor by squared Euclidean distance to clamped display rectangle bounds
        const IDisplay* closestDisplay = getPrimaryDisplay();
        gp::UInt64 minDistanceSq = ~0ULL;

        for (gp::UInt32 i = 0u; i < m_displayCount; ++i)
        {
            const gp::hal::generic::Display* disp = m_displayTable[i];
            if (disp == nullptr || !disp->isConnected())
            {
                continue;
            }

            const DisplayRect bounds = disp->getBounds();
            const Int32 minX = bounds.origin.x;
            const Int32 minY = bounds.origin.y;
            const Int32 maxX = bounds.origin.x + static_cast<Int32>(bounds.extent.width);
            const Int32 maxY = bounds.origin.y + static_cast<Int32>(bounds.extent.height);

            const Int32 clampedX = (point.x < minX) ? minX : ((point.x > maxX) ? maxX : point.x);
            const Int32 clampedY = (point.y < minY) ? minY : ((point.y > maxY) ? maxY : point.y);

            const gp::Int64 dx = static_cast<gp::Int64>(point.x) - static_cast<gp::Int64>(clampedX);
            const gp::Int64 dy = static_cast<gp::Int64>(point.y) - static_cast<gp::Int64>(clampedY);
            const gp::UInt64 distSq = static_cast<gp::UInt64>((dx * dx) + (dy * dy));

            if (distSq < minDistanceSq)
            {
                minDistanceSq = distSq;
                closestDisplay = disp;
            }
        }

        return closestDisplay;
    }

    /// @copydoc IApplicationBackend::getDisplayForRect()
    [[nodiscard]] constexpr const IDisplay* getDisplayForRect(const DisplayRect& rect) const noexcept override
    {
        if (m_displayCount == 0u) [[unlikely]]
        {
            return &m_headlessDisplay;
        }

        const IDisplay* bestDisplay = nullptr;
        gp::UInt64 maxOverlapArea = 0ULL;

        const Int32 rectLeft = rect.origin.x;
        const Int32 rectTop = rect.origin.y;
        const Int32 rectRight = rect.origin.x + static_cast<Int32>(rect.extent.width);
        const Int32 rectBottom = rect.origin.y + static_cast<Int32>(rect.extent.height);

        for (gp::UInt32 i = 0u; i < m_displayCount; ++i)
        {
            const gp::hal::generic::Display* disp = m_displayTable[i];
            if (disp == nullptr || !disp->isConnected())
            {
                continue;
            }

            const DisplayRect bounds = disp->getBounds();
            const Int32 dispLeft = bounds.origin.x;
            const Int32 dispTop = bounds.origin.y;
            const Int32 dispRight = bounds.origin.x + static_cast<Int32>(bounds.extent.width);
            const Int32 dispBottom = bounds.origin.y + static_cast<Int32>(bounds.extent.height);

            const Int32 overlapLeft = (rectLeft > dispLeft) ? rectLeft : dispLeft;
            const Int32 overlapTop = (rectTop > dispTop) ? rectTop : dispTop;
            const Int32 overlapRight = (rectRight < dispRight) ? rectRight : dispRight;
            const Int32 overlapBottom = (rectBottom < dispBottom) ? rectBottom : dispBottom;

            if (overlapRight > overlapLeft && overlapBottom > overlapTop)
            {
                const gp::UInt64 area = static_cast<gp::UInt64>(overlapRight - overlapLeft) *
                                        static_cast<gp::UInt64>(overlapBottom - overlapTop);

                if (area > maxOverlapArea)
                {
                    maxOverlapArea = area;
                    bestDisplay = disp;
                }
            }
        }

        if (bestDisplay != nullptr)
        {
            return bestDisplay;
        }

        // If the rectangle is completely outside all monitors, resolve using the rectangle's center point
        const Point2D center(
            rect.origin.x + static_cast<Int32>(rect.extent.width / 2u),
            rect.origin.y + static_cast<Int32>(rect.extent.height / 2u)
        );
        return getDisplayForPoint(center);
    }

    /// @copydoc IApplicationBackend::notifyDisplayEvent()
    constexpr void notifyDisplayEvent(const event::DisplayEvent& ev) noexcept override
    {
        if (ev.action == event::DisplayAction::Connected)
        {
            // Allow derived backend to register and refresh the newly connected monitor
            onBackendDisplayConnected(ev.displayId);
            if (gp::hal::generic::Display* added = findMutableDisplayById(ev.displayId))
            {
                added->onDisplayEvent(ev);
            }
            return;
        }

        if (ev.action == event::DisplayAction::Disconnected)
        {
            if (gp::hal::generic::Display* removed = findMutableDisplayById(ev.displayId))
            {
                removed->onDisplayEvent(ev);
            }
            onBackendDisplayDisconnected(ev.displayId);
            unregisterDisplayById(ev.displayId);
            return;
        }

        // Standard state/geometry transition (Moved, Resized, OrientationChanged, ContentScaleChanged, HDRStateChanged)
        if (gp::hal::generic::Display* target = findMutableDisplayById(ev.displayId))
        {
            target->onDisplayEvent(ev);
        }
    }

protected:
    /// @brief Hook invoked when a `DisplayAction::Connected` event arrives for a new monitor.
    /// @param[in] displayId Native display identifier of the connected monitor.
    virtual void onBackendDisplayConnected([[maybe_unused]] DisplayId displayId) noexcept
    {}

    /// @brief Hook invoked when a `DisplayAction::Disconnected` event arrives for an existing monitor.
    /// @param[in] displayId Native display identifier of the disconnected monitor.
    virtual void onBackendDisplayDisconnected([[maybe_unused]] DisplayId displayId) noexcept
    {}

    /// @brief Finds a mutable pointer to a registered display by its `DisplayId`.
    /// @param[in] displayId Target display identifier.
    /// @return Mutable pointer to `generic::Display`, or `nullptr` if not found.
    [[nodiscard]] constexpr gp::hal::generic::Display* findMutableDisplayById(DisplayId displayId) noexcept
    {
        for (gp::UInt32 i = 0u; i < m_displayCount; ++i)
        {
            if (m_displayTable[i] != nullptr && m_displayTable[i]->getId() == displayId)
            {
                return m_displayTable[i];
            }
        }
        return nullptr;
    }

    /// @brief Registers a backend-owned `generic::Display` instance into the active pointer table.
    /// @param[in] display Pointer to the backend display slot to register.
    /// @return True if registered or updated, false if the table is at `kMaxDisplays` capacity.
    constexpr bool registerDisplay(gp::hal::generic::Display* display) noexcept
    {
        if (display == nullptr || display->getId() == kInvalidDisplayId) [[unlikely]]
        {
            return false;
        }

        // Remove the synthetic headless fallback if a real physical monitor is being registered
        if (m_displayCount == 1u && m_displayTable[0] == &m_headlessDisplay && display != &m_headlessDisplay)
        {
            m_displayTable[0] = nullptr;
            m_displayCount = 0u;
        }

        // Check if already present
        for (gp::UInt32 i = 0u; i < m_displayCount; ++i)
        {
            if (m_displayTable[i] != nullptr && m_displayTable[i]->getId() == display->getId())
            {
                m_displayTable[i] = display;
                if (display->isPrimary())
                {
                    m_primaryDisplayId = display->getId();
                }
                return true;
            }
        }

        if (m_displayCount >= kMaxDisplays) [[unlikely]]
        {
            return false;
        }

        m_displayTable[m_displayCount++] = display;
        if (display->isPrimary() || m_primaryDisplayId == kInvalidDisplayId || m_primaryDisplayId == kHeadlessDisplayId)
        {
            m_primaryDisplayId = display->getId();
        }
        return true;
    }

    /// @brief Removes a disconnected display from the active pointer table and reassigns the primary ID if needed.
    /// @param[in] displayId Identifier of the display to remove.
    constexpr void unregisterDisplayById(DisplayId displayId) noexcept
    {
        for (gp::UInt32 i = 0u; i < m_displayCount; ++i)
        {
            if (m_displayTable[i] != nullptr && m_displayTable[i]->getId() == displayId)
            {
                // Shift remaining pointers left to keep the active table contiguous
                for (gp::UInt32 j = i; j + 1u < m_displayCount; ++j)
                {
                    m_displayTable[j] = m_displayTable[j + 1u];
                }
                m_displayTable[m_displayCount - 1u] = nullptr;
                --m_displayCount;
                break;
            }
        }

        // If all physical monitors were unplugged, restore the synthetic headless fallback
        if (m_displayCount == 0u)
        {
            activateHeadlessFallbackIfEmpty();
        }
        else if (m_primaryDisplayId == displayId)
        {
            m_primaryDisplayId = m_displayTable[0]->getId();
        }
    }

    /// @brief Clears all entries from the display pointer table.
    constexpr void clearDisplayTable() noexcept
    {
        for (gp::UInt32 i = 0u; i < kMaxDisplays; ++i)
        {
            m_displayTable[i] = nullptr;
        }
        m_displayCount = 0u;
        m_primaryDisplayId = kInvalidDisplayId;
    }

    /// @brief Ensures that `m_headlessDisplay` is active in slot 0 when no physical displays are registered.
    constexpr void activateHeadlessFallbackIfEmpty() noexcept
    {
        if (m_displayCount == 0u)
        {
            m_displayTable[0] = &m_headlessDisplay;
            m_displayCount = 1u;
            m_primaryDisplayId = kHeadlessDisplayId;
        }
    }

private:
    /// @brief Initializes the default 1920x1080 @ 60Hz virtual display descriptor for headless execution.
    constexpr void configureHeadlessFallbackDisplay() noexcept
    {
        DisplayDesc desc{};
        desc.id = kHeadlessDisplayId;
        desc.name = "GP Virtual Headless Display";
        desc.bounds = DisplayRect(Point2D(0, 0), Extent2D(1920u, 1080u));
        desc.workArea = DisplayRect(Point2D(0, 0), Extent2D(1920u, 1080u));
        desc.currentMode = DisplayMode{ Extent2D(1920u, 1080u), 60.0f, 1.0f, DisplayPixelFormat::RGBA8888 };
        desc.desktopMode = desc.currentMode;
        desc.contentScale = 1.0f;
        desc.orientation = DisplayOrientation::Landscape;
        desc.flags = DisplayFlags::Primary | DisplayFlags::Connected;

        m_headlessDisplay = gp::hal::generic::Display(desc);
    }
};

}   // namespace gp::hal::generic
