// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/CharTraits.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "logging/LogEnums.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::log
{

/// @brief Named diagnostic logging category with two-tier (compile-time + lock-free atomic runtime) severity filtering
///        ([ADR-0054]).
/// @details Designed for constant initialization (`constexpr`) at namespace or class scope so categories are ready in
///          the binary's `.data` segment prior to `main()`, completely immune to the C++ Static Initialization Order
///          Fiasco and performing zero heap allocations.
class LogCategory final
{
private:
    const char* m_nameLiteral{ "Default" };
    gp::UInt8 m_nameLength{ 7u };
    LogLevel m_defaultLevel{ LogLevel::Info };
    LogLevel m_compileTimeMinLevel{ LogLevel::Trace };
    gp::thread::Atomic<LogLevel> m_activeLevel{ LogLevel::Info };

public:
    /// @brief Constructs a `LogCategory` with an explicit name literal, default runtime level, and compile-time minimum
    ///        level.
    /// @param[in] nameLiteral Null-terminated static UTF-8 category identifier (e.g., `"LogRHI"`).
    /// @param[in] defaultLevel Initial runtime minimum severity threshold (`LogLevel::Info` by default).
    /// @param[in] compileTimeMinLevel Compile-time minimum severity floor (`LogLevel::Trace` by default). Any level
    ///                                below `compileTimeMinLevel` is permanently suppressed even if `setLevel()` is
    ///                                called at runtime.
    [[nodiscard]] constexpr explicit LogCategory(
        const char* const nameLiteral,
        const LogLevel defaultLevel = LogLevel::Info,
        const LogLevel compileTimeMinLevel = LogLevel::Trace
    ) noexcept
        : m_nameLiteral((nameLiteral != nullptr && nameLiteral[0] != '\0') ? nameLiteral : "Default")
        , m_nameLength(0u)
        , m_defaultLevel(
              (static_cast<gp::UInt8>(defaultLevel) < static_cast<gp::UInt8>(compileTimeMinLevel)) ? compileTimeMinLevel
                                                                                                   : defaultLevel
          )
        , m_compileTimeMinLevel(compileTimeMinLevel)
        , m_activeLevel(m_defaultLevel)
    {
        const gp::USize rawLen = gp::container::CharTraits<char>::length(m_nameLiteral);
        m_nameLength = (rawLen > 255u) ? static_cast<gp::UInt8>(255u) : static_cast<gp::UInt8>(rawLen);
    }

    LogCategory(const LogCategory&) = delete;
    LogCategory& operator=(const LogCategory&) = delete;
    LogCategory(LogCategory&&) = delete;
    LogCategory& operator=(LogCategory&&) = delete;

    /// @brief Trivial destructor.
    constexpr ~LogCategory() noexcept = default;

public:
    /// @brief Queries the non-owning UTF-8 string view of the category name.
    /// @return `gp::StringView` of the category name.
    [[nodiscard]] constexpr gp::StringView getName() const noexcept
    {
        return gp::StringView(m_nameLiteral, static_cast<gp::USize>(m_nameLength));
    }

    /// @brief Queries the null-terminated static C-string pointer of the category name.
    /// @return Non-null `const char*` category name literal.
    [[nodiscard]] constexpr const char* getNameCString() const noexcept
    {
        return m_nameLiteral;
    }

    /// @brief Queries the cached UTF-8 byte length of the category name (`<= 255`).
    /// @return Byte length of `getNameCString()`.
    [[nodiscard]] constexpr gp::UInt8 getNameLength() const noexcept
    {
        return m_nameLength;
    }

    /// @brief Queries the initial default runtime `LogLevel` configured at construction.
    /// @return Default `LogLevel`.
    [[nodiscard]] constexpr LogLevel getDefaultLevel() const noexcept
    {
        return m_defaultLevel;
    }

    /// @brief Queries the compile-time minimum severity floor (`compileTimeMinLevel`).
    /// @return Compile-time minimum `LogLevel`.
    [[nodiscard]] constexpr LogLevel getCompileTimeMinLevel() const noexcept
    {
        return m_compileTimeMinLevel;
    }

    /// @brief Atomically queries the currently active runtime minimum severity level (`MemoryOrder::Relaxed`).
    /// @return Active `LogLevel` threshold.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr LogLevel getActiveLevel() const noexcept
    {
        return m_activeLevel.load(gp::MemoryOrder::Relaxed);
    }

    /// @brief Atomically updates the runtime minimum severity level, clamped against `getCompileTimeMinLevel()`.
    /// @param[in] newLevel Desired minimum severity threshold.
    constexpr void setActiveLevel(const LogLevel newLevel) noexcept
    {
        const LogLevel clamped = (static_cast<gp::UInt8>(newLevel) < static_cast<gp::UInt8>(m_compileTimeMinLevel))
                                     ? m_compileTimeMinLevel
                                     : newLevel;
        m_activeLevel.store(clamped, gp::MemoryOrder::Relaxed);
    }

    /// @brief Resets the active runtime severity level back to `getDefaultLevel()`.
    constexpr void resetToDefaultLevel() noexcept
    {
        m_activeLevel.store(m_defaultLevel, gp::MemoryOrder::Relaxed);
    }

    /// @brief Checks whether a log message at `level` passes both the compile-time floor and the runtime atomic filter.
    /// @param[in] level Severity level of the candidate log message.
    /// @return True if `level` should be emitted, false if suppressed.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isEnabled(const LogLevel level) const noexcept
    {
        if (level == LogLevel::Off) [[unlikely]]
        {
            return false;
        }
        if (static_cast<gp::UInt8>(level) < static_cast<gp::UInt8>(m_compileTimeMinLevel))
        {
            return false;
        }
        const LogLevel active = m_activeLevel.load(gp::MemoryOrder::Relaxed);
        return static_cast<gp::UInt8>(level) >= static_cast<gp::UInt8>(active);
    }
};

/// @brief Built-in fallback engine log category (`"LogTemp"`) for quick diagnostics and general logging.
inline LogCategory LogTemp("LogTemp", LogLevel::Trace, LogLevel::Trace);

/// @brief Built-in core module log category (`"LogCore"`).
inline LogCategory LogCore("LogCore", LogLevel::Info, LogLevel::Trace);

}   // namespace gp::log
