// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "display/Display.hpp"

namespace gp::hal::generic
{

/// @brief Platform-agnostic base implementation of `gp::hal::IDisplay` maintaining shadow state on the Game Thread.
/// @details Stores monitor geometry, DPI scale, HDR metadata, and supported fullscreen modes in fixed-capacity
///          inline buffers to guarantee zero heap allocations and 0ns L1-cache reads (ADR-0028).
class Display : public gp::hal::IDisplay
{
public:
    /// @brief Maximum UTF-8 byte length for a monitor's human-readable name (including null terminator).
    static constexpr gp::USize kMaxDisplayNameBytes = 128u;

    /// @brief Maximum number of deduplicated fullscreen display modes cached inline per monitor.
    static constexpr gp::UInt32 kMaxCachedModes = 64u;

protected:
    DisplayDesc m_desc{};
    DisplayMode m_supportedModes[kMaxCachedModes]{};
    gp::UInt32 m_supportedModeCount{ 0u };
    char m_nameBuffer[kMaxDisplayNameBytes]{ "Generic Display" };

public:
    /// @brief Default constructor initializes an unattached generic display slot.
    [[nodiscard]] constexpr Display() noexcept
    {
        cacheNameString(StringView("Generic Display"));
    }

    /// @brief Constructs a generic display initialized from the provided snapshot descriptor.
    /// @param[in] desc Initial display configuration and metrics.
    [[nodiscard]] explicit constexpr Display(const DisplayDesc& desc) noexcept
    {
        initialize(desc);
    }

    /// @brief Non-copyable to enforce unique ownership of hardware display slots.
    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

    /// @brief Move constructor that safely transfers mode tables and rebinds the internal name StringView.
    /// @param[in,out] other Source display to move from (reset to an invalid state).
    [[nodiscard]] constexpr Display(Display&& other) noexcept
        : gp::hal::IDisplay()
        , m_desc(other.m_desc)
        , m_supportedModeCount(other.m_supportedModeCount)
    {
        for (gp::UInt32 i = 0u; i < m_supportedModeCount; ++i)
        {
            m_supportedModes[i] = other.m_supportedModes[i];
        }
        cacheNameString(other.m_desc.name);
        other.reset();
    }

    /// @brief Move assignment operator that safely transfers mode tables and rebinds the internal name StringView.
    /// @param[in,out] other Source display to move from (reset to an invalid state).
    /// @return Reference to this display instance.
    constexpr Display& operator=(Display&& other) noexcept
    {
        if (this != &other)
        {
            m_desc = other.m_desc;
            m_supportedModeCount = other.m_supportedModeCount;
            for (gp::UInt32 i = 0u; i < m_supportedModeCount; ++i)
            {
                m_supportedModes[i] = other.m_supportedModes[i];
            }
            cacheNameString(other.m_desc.name);
            other.reset();
        }
        return *this;
    }

    /// @brief Virtual destructor.
    ~Display() noexcept override = default;

public:
    /// @brief Initializes or reconfigures this display instance in-place from a descriptor.
    /// @param[in] desc Display configuration descriptor.
    constexpr void initialize(const DisplayDesc& desc) noexcept
    {
        m_desc = desc;
        m_supportedModeCount = 0u;
        cacheNameString(desc.name);

        if (m_desc.desktopMode.resolution.width > 0u)
        {
            addSupportedModeUnique(m_desc.desktopMode);
        }
        if (m_desc.currentMode.resolution.width > 0u)
        {
            addSupportedModeUnique(m_desc.currentMode);
        }
    }

    /// @brief Resets this display slot back to an uninitialized, disconnected state.
    constexpr void reset() noexcept
    {
        m_desc = DisplayDesc{};
        m_supportedModeCount = 0u;
        cacheNameString(StringView("Disconnected Display"));
    }

    /// @brief Updates whether this display is designated as the OS primary monitor.
    /// @param[in] isPrimary True to set `DisplayFlags::Primary`, false to clear it.
    constexpr void setPrimary(bool isPrimary) noexcept
    {
        if (isPrimary)
        {
            m_desc.flags |= DisplayFlags::Primary;
        }
        else
        {
            m_desc.flags &= ~DisplayFlags::Primary;
        }
    }

public:
    /// @copydoc gp::hal::Display::getId()
    [[nodiscard]] constexpr DisplayId getId() const noexcept override
    {
        return m_desc.id;
    }

    /// @copydoc gp::hal::Display::getNativeId()
    [[nodiscard]] constexpr DisplayId getNativeId() const noexcept override
    {
        return m_desc.id;
    }

    /// @copydoc gp::hal::Display::getName()
    [[nodiscard]] constexpr StringView getName() const noexcept override
    {
        return m_desc.name;
    }

    /// @copydoc gp::hal::Display::isValid()
    [[nodiscard]] constexpr bool isValid() const noexcept override
    {
        return (m_desc.id != kInvalidDisplayId) && isConnected();
    }

    /// @copydoc gp::hal::Display::isConnected()
    [[nodiscard]] constexpr bool isConnected() const noexcept override
    {
        return (m_desc.flags & DisplayFlags::Connected) != DisplayFlags::None;
    }

    /// @copydoc gp::hal::Display::isPrimary()
    [[nodiscard]] constexpr bool isPrimary() const noexcept override
    {
        return (m_desc.flags & DisplayFlags::Primary) != DisplayFlags::None;
    }

    /// @copydoc gp::hal::Display::isHDREnabled()
    [[nodiscard]] constexpr bool isHDREnabled() const noexcept override
    {
        return (m_desc.flags & DisplayFlags::HDREnabled) != DisplayFlags::None;
    }

    /// @copydoc gp::hal::Display::getFlags()
    [[nodiscard]] constexpr DisplayFlags getFlags() const noexcept override
    {
        return m_desc.flags;
    }

    /// @copydoc gp::hal::Display::getOrientation()
    [[nodiscard]] constexpr DisplayOrientation getOrientation() const noexcept override
    {
        return m_desc.orientation;
    }

    /// @copydoc gp::hal::Display::getBounds()
    [[nodiscard]] constexpr DisplayRect getBounds() const noexcept override
    {
        return m_desc.bounds;
    }

    /// @copydoc gp::hal::Display::getWorkArea()
    [[nodiscard]] constexpr DisplayRect getWorkArea() const noexcept override
    {
        return m_desc.workArea;
    }

    /// @copydoc gp::hal::Display::getCurrentMode()
    [[nodiscard]] constexpr const DisplayMode& getCurrentMode() const noexcept override
    {
        return m_desc.currentMode;
    }

    /// @copydoc gp::hal::Display::getDesktopMode()
    [[nodiscard]] constexpr const DisplayMode& getDesktopMode() const noexcept override
    {
        return m_desc.desktopMode;
    }

    /// @copydoc gp::hal::Display::getContentScale()
    [[nodiscard]] constexpr Float32 getContentScale() const noexcept override
    {
        return m_desc.contentScale;
    }

    /// @copydoc gp::hal::Display::getHDRMetadata()
    [[nodiscard]] constexpr const DisplayHDRMetadata& getHDRMetadata() const noexcept override
    {
        return m_desc.hdrMetadata;
    }

    /// @copydoc gp::hal::Display::getSupportedModeCount()
    [[nodiscard]] constexpr gp::UInt32 getSupportedModeCount() const noexcept override
    {
        return m_supportedModeCount;
    }

    /// @copydoc gp::hal::Display::getSupportedMode()
    [[nodiscard]] constexpr const DisplayMode* getSupportedMode(gp::UInt32 index) const noexcept override
    {
        return (index < m_supportedModeCount) ? &m_supportedModes[index] : nullptr;
    }

    /// @copydoc gp::hal::Display::findClosestMode()
    [[nodiscard]] constexpr bool findClosestMode(
        const Extent2D& targetResolution, Float32 targetRefreshRate, DisplayMode& outMode
    ) const noexcept override
    {
        if (m_supportedModeCount == 0u) [[unlikely]]
        {
            if (m_desc.currentMode.resolution.width > 0u)
            {
                outMode = m_desc.currentMode;
                return true;
            }
            return false;
        }

        gp::UInt32 bestIndex = 0u;
        gp::UInt64 bestAreaDiff = ~0ULL;
        Float32 bestRefreshDiff = 100000.0f;

        for (gp::UInt32 i = 0u; i < m_supportedModeCount; ++i)
        {
            const DisplayMode& candidate = m_supportedModes[i];
            const gp::Int64 dw =
                static_cast<gp::Int64>(candidate.resolution.width) - static_cast<gp::Int64>(targetResolution.width);
            const gp::Int64 dh =
                static_cast<gp::Int64>(candidate.resolution.height) - static_cast<gp::Int64>(targetResolution.height);
            const gp::UInt64 distSq = static_cast<gp::UInt64>((dw * dw) + (dh * dh));

            const Float32 refreshDiff = (targetRefreshRate > 0.0f) ? ((candidate.refreshRate >= targetRefreshRate)
                                                                          ? (candidate.refreshRate - targetRefreshRate)
                                                                          : (targetRefreshRate - candidate.refreshRate))
                                                                   : (1000.0f - candidate.refreshRate);

            if (distSq < bestAreaDiff || (distSq == bestAreaDiff && refreshDiff < bestRefreshDiff))
            {
                bestAreaDiff = distSq;
                bestRefreshDiff = refreshDiff;
                bestIndex = i;
            }
        }

        outMode = m_supportedModes[bestIndex];
        return true;
    }

    /// @copydoc gp::hal::Display::refresh()
    constexpr void refresh() noexcept override
    {}

    /// @copydoc gp::hal::Display::onDisplayEvent()
    constexpr void onDisplayEvent(const event::DisplayEvent& ev) noexcept override
    {
        if (ev.displayId != m_desc.id && m_desc.id != kInvalidDisplayId) [[unlikely]]
        {
            return;
        }

        switch (ev.action)
        {
        case event::DisplayAction::Connected:
            m_desc.flags |= DisplayFlags::Connected;
            m_desc.contentScale = ev.contentScale;
            m_desc.orientation = ev.orientation;
            break;

        case event::DisplayAction::Disconnected:
            m_desc.flags &= ~DisplayFlags::Connected;
            break;

        case event::DisplayAction::Moved:
            m_desc.bounds.origin = ev.position;
            break;

        case event::DisplayAction::Resized:
            m_desc.bounds.extent = ev.extent;
            m_desc.currentMode.resolution = ev.extent;
            break;

        case event::DisplayAction::OrientationChanged:
            m_desc.orientation = ev.orientation;
            break;

        case event::DisplayAction::ContentScaleChanged:
            m_desc.contentScale = ev.contentScale;
            break;

        case event::DisplayAction::HDRStateChanged:
            if (ev.isHDREnabled != 0u)
            {
                m_desc.flags |= DisplayFlags::HDREnabled | DisplayFlags::HDRCapable;
            }
            else
            {
                m_desc.flags &= ~DisplayFlags::HDREnabled;
            }
            break;

        case event::DisplayAction::None:
        default:
            break;
        }
    }

protected:
    /// @brief Copies a `StringView` into the internal null-terminated buffer and rebinds `m_desc.name`.
    /// @param[in] name Source UTF-8 display name.
    constexpr void cacheNameString(StringView name) noexcept
    {
        const gp::USize maxCopy =
            (name.size() < (kMaxDisplayNameBytes - 1u)) ? name.size() : (kMaxDisplayNameBytes - 1u);

        for (gp::USize i = 0u; i < maxCopy; ++i)
        {
            m_nameBuffer[i] = name[i];
        }
        m_nameBuffer[maxCopy] = '\0';
        m_desc.name = StringView(m_nameBuffer, maxCopy);
    }

    /// @brief Appends a display mode to the fixed-capacity table if not already present.
    /// @param[in] mode DisplayMode to register.
    constexpr void addSupportedModeUnique(const DisplayMode& mode) noexcept
    {
        if (mode.resolution.width == 0u || mode.resolution.height == 0u)
        {
            return;
        }

        for (gp::UInt32 i = 0u; i < m_supportedModeCount; ++i)
        {
            if (m_supportedModes[i] == mode)
            {
                return;
            }
        }

        if (m_supportedModeCount < kMaxCachedModes)
        {
            m_supportedModes[m_supportedModeCount++] = mode;
        }
    }
};

}   // namespace gp::hal::generic
