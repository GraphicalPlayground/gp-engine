// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "logging/LogCategory.hpp"   // IWYU pragma: keep
#include "logging/LogEnums.hpp"      // IWYU pragma: keep
#include "logging/Logger.hpp"        // IWYU pragma: keep

/// @section Category Declaration & Definition Macros ([ADR-0054])

/// @brief Declares an `extern` `gp::log::LogCategory` in a header file.
/// @param CategoryName Identifier of the category variable (e.g., `LogRHI`).
#define GP_DECLARE_LOG_CATEGORY_EXTERN(CategoryName) extern ::gp::log::LogCategory CategoryName

/// @brief Declares a DLL-exported/imported `extern` `gp::log::LogCategory` in a module public header.
/// @param ApiMacro Module visibility macro (e.g., `GP_CORE_API`, `GP_APPLICATION_API`).
/// @param CategoryName Identifier of the category variable (e.g., `LogApplication`).
#define GP_DECLARE_EXPORTED_LOG_CATEGORY_EXTERN(ApiMacro, CategoryName) \
    extern ApiMacro ::gp::log::LogCategory CategoryName

/// @brief Defines a `gp::log::LogCategory` in a `.cpp` file matching a prior `GP_DECLARE_LOG_CATEGORY_EXTERN`.
/// @param CategoryName Identifier of the category variable.
/// @param DefaultLevel Initial runtime `gp::log::LogLevel`.
/// @param CompileTimeMinLevel Minimum `gp::log::LogLevel` allowed at compile time.
#define GP_DEFINE_LOG_CATEGORY(CategoryName, DefaultLevel, CompileTimeMinLevel) \
    ::gp::log::LogCategory CategoryName(#CategoryName, DefaultLevel, CompileTimeMinLevel)

/// @brief Defines an `inline` `gp::log::LogCategory` usable directly inside a header or `.cpp` translation unit.
/// @param CategoryName Identifier of the category variable.
/// @param DefaultLevel Initial runtime `gp::log::LogLevel`.
/// @param CompileTimeMinLevel Minimum `gp::log::LogLevel` allowed at compile time.
#define GP_DEFINE_LOG_CATEGORY_INLINE(CategoryName, DefaultLevel, CompileTimeMinLevel) \
    inline ::gp::log::LogCategory CategoryName(#CategoryName, DefaultLevel, CompileTimeMinLevel)

/// @section Short-Circuiting & `.rdata`-Stripping Logging Macros ([ADR-0054])

#if !GP_NO_LOGGING

    /// @brief Emits a formatted log message to a specific `Logger` instance (`LoggerInstance`), short-circuiting
    ///        variadic argument evaluation when `(Category).isEnabled(Level)` is false.
    /// @param LoggerInstance Reference to the target `gp::log::Logger`.
    /// @param Category Target `gp::log::LogCategory`.
    /// @param Level Unqualified `gp::log::LogLevel` enumerator name (`Trace`, `Debug`, `Info`, `Warning`, `Error`,
    ///              `Fatal`).
    /// @param Format Compile-time validated format string literal.
    #define GP_LOGGER_LOG(LoggerInstance, Category, Level, Format, ...)                      \
        do                                                                                   \
        {                                                                                    \
            if ((LoggerInstance).shouldLog((Category), ::gp::log::LogLevel::Level))          \
            {                                                                                \
                (LoggerInstance)                                                             \
                    .log((Category), ::gp::log::LogLevel::Level, Format __VA_OPT__(, ) __VA_ARGS__); \
            }                                                                                \
        } while (false)

    /// @brief Primary engine logging macro routing to `gp::log::Logger::instance()`.
    /// @details Combines C++23 `consteval` format string verification with preprocessor short-circuiting so disabled
    ///          log statements never evaluate their argument expressions and compile out completely when
    ///          `GP_NO_LOGGING` is enabled. Emitting `Fatal` flushes all sinks and aborts the process.
    /// @param Category Target `gp::log::LogCategory` (e.g., `gp::log::LogCore`).
    /// @param Level Unqualified `gp::log::LogLevel` enumerator (`Trace`, `Debug`, `Info`, `Warning`, `Error`, `Fatal`).
    /// @param Format Compile-time format string literal (`"{}"`).
    #define GP_LOG(Category, Level, Format, ...)                                                       \
        do                                                                                             \
        {                                                                                              \
            GP_LOGGER_LOG(::gp::log::Logger::instance(), Category, Level, Format __VA_OPT__(, ) __VA_ARGS__); \
            if constexpr (::gp::log::LogLevel::Level == ::gp::log::LogLevel::Fatal)                    \
            {                                                                                          \
                ::gp::log::Logger::abortFatal(::gp::log::LogSourceLocation::current());                \
            }                                                                                          \
        } while (false)

    /// @brief Conditional logging macro targeting a specific `Logger` instance (`LoggerInstance`).
    /// @details Evaluates `Condition` first and checks `(LoggerInstance).shouldLog(...)`, so neither format arguments
    ///          nor formatting run when the condition is false or the category level is masked out.
    /// @param Condition Boolean condition guarding the log emission.
    /// @param LoggerInstance Reference to the target `gp::log::Logger`.
    /// @param Category Target `gp::log::LogCategory`.
    /// @param Level Unqualified `gp::log::LogLevel` enumerator.
    /// @param Format Compile-time validated format string literal.
    #define GP_LOGGER_CLOG(Condition, LoggerInstance, Category, Level, Format, ...)            \
        do                                                                                     \
        {                                                                                      \
            if ((Condition) && (LoggerInstance).shouldLog((Category), ::gp::log::LogLevel::Level)) \
            {                                                                                  \
                (LoggerInstance)                                                               \
                    .log((Category), ::gp::log::LogLevel::Level, Format __VA_OPT__(, ) __VA_ARGS__); \
            }                                                                                  \
        } while (false)

    /// @brief Conditional logging macro routing to `gp::log::Logger::instance()` when `Condition` holds true.
    /// @details Mirrors Unreal's `UE_CLOG`. When `Level` is `Fatal` and `Condition` is true, flushes sinks and aborts.
    /// @param Condition Boolean condition guarding the log emission.
    /// @param Category Target `gp::log::LogCategory`.
    /// @param Level Unqualified `gp::log::LogLevel` enumerator.
    /// @param Format Compile-time validated format string literal.
    #define GP_CLOG(Condition, Category, Level, Format, ...)                                               \
        do                                                                                                 \
        {                                                                                                  \
            if (Condition)                                                                                 \
            {                                                                                              \
                GP_LOGGER_LOG(::gp::log::Logger::instance(), Category, Level, Format __VA_OPT__(, ) __VA_ARGS__); \
                if constexpr (::gp::log::LogLevel::Level == ::gp::log::LogLevel::Fatal)                    \
                {                                                                                          \
                    ::gp::log::Logger::abortFatal(::gp::log::LogSourceLocation::current());                \
                }                                                                                          \
            }                                                                                              \
        } while (false)

#else

    #define GP_LOGGER_LOG(LoggerInstance, Category, Level, Format, ...) \
        do                                                              \
        {                                                               \
            (void)(LoggerInstance);                                     \
            (void)(Category);                                           \
        } while (false)

    #define GP_LOG(Category, Level, Format, ...)                                        \
        do                                                                              \
        {                                                                               \
            (void)(Category);                                                           \
            if constexpr (::gp::log::LogLevel::Level == ::gp::log::LogLevel::Fatal)     \
            {                                                                           \
                ::gp::log::Logger::abortFatal(::gp::log::LogSourceLocation::current()); \
            }                                                                           \
        } while (false)

    #define GP_LOGGER_CLOG(Condition, LoggerInstance, Category, Level, Format, ...) \
        do                                                                          \
        {                                                                           \
            (void)sizeof(static_cast<bool>(Condition));                             \
            (void)(LoggerInstance);                                                 \
            (void)(Category);                                                       \
        } while (false)

    #define GP_CLOG(Condition, Category, Level, Format, ...)                            \
        do                                                                              \
        {                                                                               \
            if constexpr (::gp::log::LogLevel::Level == ::gp::log::LogLevel::Fatal)     \
            {                                                                           \
                if (Condition)                                                          \
                {                                                                       \
                    ::gp::log::Logger::abortFatal(::gp::log::LogSourceLocation::current()); \
                }                                                                       \
            }                                                                           \
            else                                                                        \
            {                                                                           \
                (void)sizeof(static_cast<bool>(Condition));                             \
            }                                                                           \
            (void)(Category);                                                           \
        } while (false)

#endif

/// @brief Convenience macro for `GP_LOG(Category, Trace, Format, ...)`.
#define GP_LOG_TRACE(Category, Format, ...) GP_LOG(Category, Trace, Format __VA_OPT__(, ) __VA_ARGS__)

/// @brief Convenience macro for `GP_LOG(Category, Debug, Format, ...)`.
#define GP_LOG_DEBUG(Category, Format, ...) GP_LOG(Category, Debug, Format __VA_OPT__(, ) __VA_ARGS__)

/// @brief Convenience macro for `GP_LOG(Category, Info, Format, ...)`.
#define GP_LOG_INFO(Category, Format, ...) GP_LOG(Category, Info, Format __VA_OPT__(, ) __VA_ARGS__)

/// @brief Convenience macro for `GP_LOG(Category, Warning, Format, ...)`.
#define GP_LOG_WARNING(Category, Format, ...) GP_LOG(Category, Warning, Format __VA_OPT__(, ) __VA_ARGS__)

/// @brief Convenience macro for `GP_LOG(Category, Error, Format, ...)`.
#define GP_LOG_ERROR(Category, Format, ...) GP_LOG(Category, Error, Format __VA_OPT__(, ) __VA_ARGS__)

/// @brief Convenience macro for `GP_LOG(Category, Fatal, Format, ...)`, flushing sinks synchronously and aborting.
#define GP_LOG_FATAL(Category, Format, ...) GP_LOG(Category, Fatal, Format __VA_OPT__(, ) __VA_ARGS__)

/// @brief Convenience macro for `GP_CLOG(Condition, Category, Trace, Format, ...)`.
#define GP_CLOG_TRACE(Condition, Category, Format, ...) \
    GP_CLOG(Condition, Category, Trace, Format __VA_OPT__(, ) __VA_ARGS__)

/// @brief Convenience macro for `GP_CLOG(Condition, Category, Debug, Format, ...)`.
#define GP_CLOG_DEBUG(Condition, Category, Format, ...) \
    GP_CLOG(Condition, Category, Debug, Format __VA_OPT__(, ) __VA_ARGS__)

/// @brief Convenience macro for `GP_CLOG(Condition, Category, Info, Format, ...)`.
#define GP_CLOG_INFO(Condition, Category, Format, ...) \
    GP_CLOG(Condition, Category, Info, Format __VA_OPT__(, ) __VA_ARGS__)

/// @brief Convenience macro for `GP_CLOG(Condition, Category, Warning, Format, ...)`.
#define GP_CLOG_WARNING(Condition, Category, Format, ...) \
    GP_CLOG(Condition, Category, Warning, Format __VA_OPT__(, ) __VA_ARGS__)

/// @brief Convenience macro for `GP_CLOG(Condition, Category, Error, Format, ...)`.
#define GP_CLOG_ERROR(Condition, Category, Format, ...) \
    GP_CLOG(Condition, Category, Error, Format __VA_OPT__(, ) __VA_ARGS__)

/// @brief Convenience macro for `GP_CLOG(Condition, Category, Fatal, Format, ...)`, aborting when `Condition` is true.
#define GP_CLOG_FATAL(Condition, Category, Format, ...) \
    GP_CLOG(Condition, Category, Fatal, Format __VA_OPT__(, ) __VA_ARGS__)
