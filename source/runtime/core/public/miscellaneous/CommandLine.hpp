// File: source/runtime/core/public/miscellaneous/CommandLine.hpp
// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "templates/Optional.hpp"
#include <type_traits>

namespace gp
{

/// @brief Global, zero-allocation command-line argument parser and typed value extractor.
/// @details Copies and tokenizes OS boot arguments into a fixed-capacity static .bss arena so
///          returned StringViews never dangle. Supports space-separated ("-key value"),
///          equals-separated ("-key=value"), and colon-separated ("-key:value") syntax,
///          quote stripping, "-no-<flag>" boolean negation, last-wins overrides, and
///          zero-allocation <charconv> numeric parsing.
class GP_CORE_API CommandLine
{
public:
    /// @brief Maximum number of discrete tokens tracked simultaneously.
    static constexpr UInt32 kMaxArguments = 128u;

    /// @brief Total byte capacity of the internal static .bss character arena.
    static constexpr USize kMaxBufferBytes = 8192u;

public:
    CommandLine() = delete;
    ~CommandLine() = delete;
    CommandLine(const CommandLine&) = delete;
    CommandLine& operator=(const CommandLine&) = delete;
    CommandLine(CommandLine&&) = delete;
    CommandLine& operator=(CommandLine&&) = delete;

public:
    /// @brief Initializes the command-line cache from standard C/C++ main(argc, argv) parameters.
    /// @details Copies all argument strings into the internal static .bss buffer so caller-owned
    ///          argv pointers do not need to outlive this call.
    /// @param[in] argc Number of arguments provided by the OS.
    /// @param[in] argv Array of null-terminated UTF-8 argument strings.
    static void initialize(const Int32 argc, const char* const* const argv) noexcept;

    /// @brief Initializes the command-line cache by tokenizing a single raw command-line string.
    /// @details Respects single ('...') and double ("...") quotes and strips outer quotes around values.
    ///          Ideal for Win32 GetCommandLineA() or embedded launch configuration strings.
    /// @param[in] rawCommandLine Raw UTF-8 command-line string view (including executable name at token 0).
    static void initialize(const StringView rawCommandLine) noexcept;

    /// @brief Appends additional arguments from a raw string to an already-initialized command line.
    /// @details Useful for injecting arguments from response files or developer overrides. Because queries
    ///          use last-wins semantics, appended arguments override earlier values.
    /// @param[in] additionalArgs Raw UTF-8 argument string to tokenize and append (does not overwrite token 0).
    /// @return Number of new argument tokens successfully appended.
    static UInt32 append(const StringView additionalArgs) noexcept;

    /// @brief Resets the global command-line cache and zeroes the internal .bss character arena.
    static void shutdown() noexcept;

    /// @brief Checks whether the command-line subsystem has been initialized with at least one token.
    /// @return True if initialized and non-empty, false otherwise.
    [[nodiscard]] static bool isInitialized() noexcept;

    /// @brief Retrieves the reconstructed raw command-line string (excluding the executable path at index 0).
    /// @return StringView spanning all arguments after argv[0].
    [[nodiscard]] static StringView getOriginalCommandLine() noexcept;

    /// @brief Retrieves the executable path or program name stored at token index 0.
    /// @return StringView of argv[0], or an empty view if uninitialized.
    [[nodiscard]] static StringView getExecutablePath() noexcept;

    /// @brief Retrieves the total number of cached tokens (including the executable at index 0).
    /// @return Token count in the range [0, kMaxArguments].
    [[nodiscard]] static UInt32 getArgumentCount() noexcept;

    /// @brief Retrieves the raw token at the specified index.
    /// @param[in] index Zero-based token index in [0, getArgumentCount()).
    /// @return StringView of the token, or an empty view if out of bounds.
    [[nodiscard]] static StringView getArgument(const UInt32 index) noexcept;

    /// @brief Collects all non-flag positional arguments (excluding argv[0] and values consumed by -key value pairs).
    /// @details Any token appearing after a standalone "--" delimiter is unconditionally treated as positional.
    /// @param[out] outPositional Caller-allocated array receiving positional StringViews.
    /// @param[in] maxCapacity Maximum number of elements that can be written to outPositional.
    /// @return Total number of positional arguments written to outPositional.
    static UInt32 getPositionalArguments(StringView* const outPositional, const UInt32 maxCapacity) noexcept;

    /// @brief Checks whether a specific flag or key is present on the command line.
    /// @details Leading dashes ('-' or '--') are normalized automatically, so querying "headless",
    ///          "-headless", or "--headless" all match "-headless", "--headless", or "-headless=true".
    /// @param[in] key The flag or key name to search for.
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive key matching.
    /// @return True if the flag or key is present, false otherwise.
    [[nodiscard]] static bool hasArgument(const StringView key, const bool caseSensitive = false) noexcept;

    /// @brief Attempts to retrieve the string value associated with a command-line key (last-wins).
    /// @details Supports "-key value", "-key=value", and "-key:value". Outer quotes around the value are stripped.
    /// @param[in] key The argument key to search for (e.g., "-map" or "map").
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive key matching.
    /// @return Optional containing the extracted StringView if present, or an empty Optional if missing.
    [[nodiscard]] static Optional<StringView>
        tryGetString(const StringView key, const bool caseSensitive = false) noexcept;

    /// @brief Retrieves the string value associated with a key, falling back to defaultValue if absent.
    /// @param[in] key The argument key to search for.
    /// @param[in] defaultValue Fallback view returned if the key is missing or has no value.
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive key matching.
    /// @return Extracted StringView or defaultValue.
    [[nodiscard]] static StringView
        getValue(const StringView key, const StringView defaultValue = {}, const bool caseSensitive = false) noexcept;

    /// @brief Collects all values assigned to a repeatable key in forward order (e.g., "-plugin=A -plugin=B").
    /// @param[in] key The argument key to search for.
    /// @param[out] outValues Caller-allocated array of StringViews to populate.
    /// @param[in] maxCapacity Maximum number of elements that can be written to outValues.
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive key matching.
    /// @return Number of values written to outValues.
    static UInt32 getValues(
        const StringView key, StringView* const outValues, const UInt32 maxCapacity, const bool caseSensitive = false
    ) noexcept;

    /// @brief Attempts to parse a boolean option from the command line (last-wins).
    /// @details Evaluates to true for bare flags ("-vsync") or truthy values ("-vsync=true/1/on/yes").
    ///          Evaluates to false for negated flags ("-no-vsync", "--no-vsync") or falsy values
    ///          ("-vsync=false/0/off/no").
    /// @param[in] key The boolean flag or key name (e.g., "-vsync" or "vsync").
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive matching.
    /// @return Optional<bool> containing the parsed boolean, or empty if neither the flag nor its negation was found.
    [[nodiscard]] static Optional<bool> tryGetBool(const StringView key, const bool caseSensitive = false) noexcept;

    /// @brief Attempts to parse a 64-bit signed integer associated with a key (supports decimal, 0x hex, and 0b
    /// binary).
    /// @param[in] key The argument key to search for.
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive key matching.
    /// @return Optional<Int64> containing the parsed integer, or empty if missing or malformed.
    [[nodiscard]] static Optional<Int64> tryGetInt64(const StringView key, const bool caseSensitive = false) noexcept;

    /// @brief Attempts to parse a 32-bit signed integer associated with a key (supports decimal, 0x hex, and 0b
    /// binary).
    /// @param[in] key The argument key to search for.
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive key matching.
    /// @return Optional<Int32> containing the parsed integer, or empty if missing, malformed, or out of 32-bit range.
    [[nodiscard]] static Optional<Int32> tryGetInt32(const StringView key, const bool caseSensitive = false) noexcept;

    /// @brief Attempts to parse a 64-bit unsigned integer associated with a key (supports decimal, 0x hex, and 0b
    /// binary).
    /// @param[in] key The argument key to search for.
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive key matching.
    /// @return Optional<UInt64> containing the parsed integer, or empty if missing, negative, or malformed.
    [[nodiscard]] static Optional<UInt64> tryGetUInt64(const StringView key, const bool caseSensitive = false) noexcept;

    /// @brief Attempts to parse a 32-bit unsigned integer associated with a key (supports decimal, 0x hex, and 0b
    /// binary).
    /// @param[in] key The argument key to search for.
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive key matching.
    /// @return Optional<UInt32> containing the parsed integer, or empty if missing, negative, malformed, or out of
    /// range.
    [[nodiscard]] static Optional<UInt32> tryGetUInt32(const StringView key, const bool caseSensitive = false) noexcept;

    /// @brief Attempts to parse a 32-bit floating-point number associated with a key.
    /// @param[in] key The argument key to search for.
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive key matching.
    /// @return Optional<Float32> containing the parsed float, or empty if missing or malformed.
    [[nodiscard]] static Optional<Float32>
        tryGetFloat32(const StringView key, const bool caseSensitive = false) noexcept;

    /// @brief Attempts to parse a 64-bit floating-point number associated with a key.
    /// @param[in] key The argument key to search for.
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive key matching.
    /// @return Optional<Float64> containing the parsed double, or empty if missing or malformed.
    [[nodiscard]] static Optional<Float64>
        tryGetFloat64(const StringView key, const bool caseSensitive = false) noexcept;

    /// @brief Generic template dispatcher attempting to extract and parse a typed value for key.
    /// @tparam InValueType Target type (bool, Int8..Int64, UInt8..UInt64, Float32, Float64, or StringView).
    /// @param[in] key The argument key to search for.
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive key matching.
    /// @return Optional<InValueType> with the parsed value, or empty if missing or out of bounds.
    template <typename InValueType>
    [[nodiscard]] static Optional<InValueType> tryGet(const StringView key, const bool caseSensitive = false) noexcept
    {
        using CleanType = std::remove_cv_t<InValueType>;

        if constexpr (std::is_same_v<CleanType, bool>)
        {
            return tryGetBool(key, caseSensitive);
        }
        else if constexpr (std::is_same_v<CleanType, StringView>)
        {
            return tryGetString(key, caseSensitive);
        }
        else if constexpr (std::is_same_v<CleanType, Float32>)
        {
            return tryGetFloat32(key, caseSensitive);
        }
        else if constexpr (std::is_same_v<CleanType, Float64>)
        {
            return tryGetFloat64(key, caseSensitive);
        }
        else if constexpr (std::is_integral_v<CleanType> && std::is_signed_v<CleanType>)
        {
            const Optional<Int64> parsed = tryGetInt64(key, caseSensitive);
            if (!parsed.hasValue())
            {
                return Optional<InValueType>{};
            }

            const Int64 val = *parsed;
            const CleanType narrowed = static_cast<CleanType>(val);
            if (static_cast<Int64>(narrowed) != val)
            {
                return Optional<InValueType>{};
            }
            return Optional<InValueType>(narrowed);
        }
        else if constexpr (std::is_integral_v<CleanType> && std::is_unsigned_v<CleanType>)
        {
            const Optional<UInt64> parsed = tryGetUInt64(key, caseSensitive);
            if (!parsed.hasValue())
            {
                return Optional<InValueType>{};
            }

            const UInt64 val = *parsed;
            const CleanType narrowed = static_cast<CleanType>(val);
            if (static_cast<UInt64>(narrowed) != val)
            {
                return Optional<InValueType>{};
            }
            return Optional<InValueType>(narrowed);
        }
        else
        {
            static_assert(sizeof(CleanType) == 0u, "Unsupported type passed to CommandLine::tryGet<T>()");
            return Optional<InValueType>{};
        }
    }

    /// @brief Generic template dispatcher returning the parsed value for key, or defaultValue on failure.
    /// @tparam InValueType Target type (bool, Int8..Int64, UInt8..UInt64, Float32, Float64, or StringView).
    /// @param[in] key The argument key to search for.
    /// @param[in] defaultValue Fallback value returned when the key is absent or unparseable.
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive key matching.
    /// @return Parsed value of type InValueType, or defaultValue.
    template <typename InValueType>
    [[nodiscard]] static InValueType
        get(const StringView key, const InValueType defaultValue, const bool caseSensitive = false) noexcept
    {
        const Optional<InValueType> result = tryGet<InValueType>(key, caseSensitive);
        return result.hasValue() ? *result : defaultValue;
    }

    /// @brief Convenience helper returning a boolean flag or defaultValue if unspecified.
    /// @param[in] key The boolean flag or key name.
    /// @param[in] defaultValue Fallback boolean returned if the flag is absent.
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive matching.
    /// @return Parsed boolean or defaultValue.
    [[nodiscard]] static bool
        getBool(const StringView key, const bool defaultValue = false, const bool caseSensitive = false) noexcept
    {
        return get<bool>(key, defaultValue, caseSensitive);
    }

    /// @brief Convenience helper returning a 32-bit signed integer or defaultValue if unspecified/invalid.
    /// @param[in] key The argument key to search for.
    /// @param[in] defaultValue Fallback integer returned if absent or invalid.
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive key matching.
    /// @return Parsed Int32 or defaultValue.
    [[nodiscard]] static Int32
        getInt32(const StringView key, const Int32 defaultValue = 0, const bool caseSensitive = false) noexcept
    {
        return get<Int32>(key, defaultValue, caseSensitive);
    }

    /// @brief Convenience helper returning a 64-bit signed integer or defaultValue if unspecified/invalid.
    /// @param[in] key The argument key to search for.
    /// @param[in] defaultValue Fallback integer returned if absent or invalid.
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive key matching.
    /// @return Parsed Int64 or defaultValue.
    [[nodiscard]] static Int64
        getInt64(const StringView key, const Int64 defaultValue = 0LL, const bool caseSensitive = false) noexcept
    {
        return get<Int64>(key, defaultValue, caseSensitive);
    }

    /// @brief Convenience helper returning a 32-bit unsigned integer or defaultValue if unspecified/invalid.
    /// @param[in] key The argument key to search for.
    /// @param[in] defaultValue Fallback unsigned integer returned if absent or invalid.
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive key matching.
    /// @return Parsed UInt32 or defaultValue.
    [[nodiscard]] static UInt32
        getUInt32(const StringView key, const UInt32 defaultValue = 0u, const bool caseSensitive = false) noexcept
    {
        return get<UInt32>(key, defaultValue, caseSensitive);
    }

    /// @brief Convenience helper returning a 64-bit unsigned integer or defaultValue if unspecified/invalid.
    /// @param[in] key The argument key to search for.
    /// @param[in] defaultValue Fallback unsigned integer returned if absent or invalid.
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive key matching.
    /// @return Parsed UInt64 or defaultValue.
    [[nodiscard]] static UInt64
        getUInt64(const StringView key, const UInt64 defaultValue = 0ULL, const bool caseSensitive = false) noexcept
    {
        return get<UInt64>(key, defaultValue, caseSensitive);
    }

    /// @brief Convenience helper returning a 32-bit float or defaultValue if unspecified/invalid.
    /// @param[in] key The argument key to search for.
    /// @param[in] defaultValue Fallback float returned if absent or invalid.
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive key matching.
    /// @return Parsed Float32 or defaultValue.
    [[nodiscard]] static Float32
        getFloat32(const StringView key, const Float32 defaultValue = 0.0f, const bool caseSensitive = false) noexcept
    {
        return get<Float32>(key, defaultValue, caseSensitive);
    }

    /// @brief Convenience helper returning a 64-bit double or defaultValue if unspecified/invalid.
    /// @param[in] key The argument key to search for.
    /// @param[in] defaultValue Fallback double returned if absent or invalid.
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive key matching.
    /// @return Parsed Float64 or defaultValue.
    [[nodiscard]] static Float64
        getFloat64(const StringView key, const Float64 defaultValue = 0.0, const bool caseSensitive = false) noexcept
    {
        return get<Float64>(key, defaultValue, caseSensitive);
    }
};

}   // namespace gp
