// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "container/BasicString.hpp"   // IWYU pragma: keep
#include "container/Forward.hpp"
#include "container/Optional.hpp"
#include "CoreMinimal.hpp"
#include "errors/ErrorCode.hpp"
#include <mutex>           // TODO: gp::Mutex?
#include <unordered_map>   // TODO: gp::HashMap?

namespace gp::error
{

/// @brief Metadata registry for error codes.
struct ErrorEntry
{
    gp::String description;   //<! One-line description of what this code means.
    gp::String remediation;   //<! Actionable fix / investigation hint.
    gp::String wikiUrl;       //<! Link to documentation (optional).
    bool isExpected;          //<! True → not a bug (e.g. EOF, not-found on probe).
    bool isAlwaysFatal;       //<! True → registry forces severity to Fatal.
};

/// @brief Singleton registry mapping ErrorCodes to human-readable metadata.
/// @note
/// The ErrorRegistry is a database that maps every known ErrorCode to:
///   • A short human-readable description  ("File not found on disk")
///   • A remediation hint                  ("Verify the asset path is correct and the pak is mounted")
///   • A documentation URL / wiki anchor
///   • A severity override flag            (some codes are always Fatal)
///   • Whether the error is expected       (i.e. not a bug, e.g. EOF)
///
/// The registry is consulted by:
///   • The ConsoleSink (appends hint to output in Debug builds)
///   • The TelemetrySink (enriches the JSON payload)
///   • The CrashReport generator
///   • Editor tooling / diagnostic overlays
///
/// Registration
///
/// Built-in codes are pre-registered in this file.
/// Project-level codes can be registered at startup via:
/// @code
///     GP_REGISTER_ERROR(
///         gp::error::codes::kMyCode,
///         "Short description",
///         "What to do about it",
///         "https://wiki.mygame.dev/errors/MY_CODE"
///     );
/// @endcode
class ErrorRegistry
{
private:
    mutable std::mutex m_mutex;
    std::unordered_map<gp::UInt32, ErrorEntry> m_table;

public:
    /// @brief Delete copy/move constructors and assignment operators to enforce singleton pattern.
    ErrorRegistry(const ErrorRegistry&) = delete;
    ErrorRegistry& operator=(const ErrorRegistry&) = delete;

private:
    /// @brief Private constructor to prevent external instantiation, registers all built-in codes.
    ErrorRegistry();

public:
    /// @brief Registers a new ErrorCode with its associated metadata in the registry.
    /// @param[in] code The ErrorCode to register.
    /// @param[in] entry The ErrorEntry containing the metadata for the code.
    void registerCode(ErrorCode code, ErrorEntry entry);

    /// @brief Looks up the given ErrorCode in the registry and returns its associated ErrorEntry if found.
    /// @param[in] code The ErrorCode to look up in the registry.
    /// @return An optional ErrorEntry containing the metadata if the code is registered; otherwise, an empty optional.
    GP_NODISCARD gp::Optional<ErrorEntry> lookup(ErrorCode code) const;

    /// @brief Gets the human-readable description associated with the given ErrorCode.
    /// @note
    /// Returns a copy of the description string, safe to hold across concurrent registry mutations.
    /// @param[in] code The ErrorCode for which to retrieve the description.
    /// @return A String containing the description if the code is registered; otherwise, "<unregistered code>".
    GP_NODISCARD gp::String describe(ErrorCode code) const;

    /// @brief Gets the remediation hint associated with the given ErrorCode, if available.
    /// @param[in] code The ErrorCode for which to retrieve the remediation hint.
    /// @return A String containing the remediation hint if it exists; otherwise, an empty string.
    GP_NODISCARD gp::String remediationHint(ErrorCode code) const;

    /// @brief Gets the documentation URL associated with the given ErrorCode, if available.
    /// @param[in] code The ErrorCode for which to retrieve the documentation URL.
    /// @return A String containing the documentation URL if it exists; otherwise, an empty string.
    GP_NODISCARD gp::String wikiUrl(ErrorCode code) const;

    /// @brief Checks if the given ErrorCode is registered and marked as expected (i.e. not a bug).
    /// @param[in] code The ErrorCode to check for expected status.
    /// @return True if the code is registered and marked as expected; otherwise, false.
    GP_NODISCARD bool isExpected(ErrorCode code) const;

    /// @brief Checks if the given ErrorCode is registered and marked as always fatal.
    /// @param[in] code The ErrorCode to check for fatality.
    /// @return True if the code is registered and marked as always fatal; otherwise, false.
    GP_NODISCARD bool isAlwaysFatal(ErrorCode code) const;

    /// @brief Returns the total number of registered error codes in the registry.
    /// @return The count of registered error codes.
    GP_NODISCARD gp::USize size() const;

    /// @brief Dumps the entire registry as a human-readable string (for tooling/debug).
    /// @return A formatted string containing all registered error codes and their metadata.
    GP_NODISCARD gp::String dumpAll() const;

private:
    /// @brief Registers all built-in error codes with their descriptions and remediation hints.
    void registerBuiltins();

public:
    /// @brief Singleton accessor for the ErrorRegistry instance.
    /// @return Reference to the singleton ErrorRegistry instance.
    static ErrorRegistry& instance() noexcept
    {
        static ErrorRegistry s_instance;
        return s_instance;
    }
};

// Initialize the registry with built-in codes and their metadata.
inline void ErrorRegistry::registerBuiltins()
{
    auto reg = [&](ErrorCode code,
                   gp::String description,
                   gp::String remediation,
                   gp::String wikiUrl = {},
                   bool expected = false,
                   bool alwaysFatal = false)
    {
        registerCode(
            code,
            ErrorEntry{ .description = std::move(description),
                        .remediation = std::move(remediation),
                        .wikiUrl = std::move(wikiUrl),
                        .isExpected = expected,
                        .isAlwaysFatal = alwaysFatal }
        );
    };

    using namespace codes;

    // clang-format off

    // Generic / Uncategorized
    reg(kUnknown,         "Unknown error",               "Consult the full stack trace.");
    reg(kNotImplemented,  "Feature not implemented",     "This code path is a stub. Implement or route around it.");
    reg(kInvalidArgument, "Invalid argument",            "Check the calling convention and argument constraints.");
    reg(kOutOfRange,      "Value out of range",          "Clamp or validate inputs before passing to this API.");
    reg(kNullPointer,     "Null pointer dereference",    "Ensure the object was successfully constructed before use.", {}, false, true);
    reg(kTimeout,         "Operation timed out",         "Increase the timeout budget or check for deadlocks.");
    reg(kNotFound,        "Resource not found",          "Verify the identifier and that the resource is registered.", {}, true);
    reg(kAlreadyExists,   "Resource already exists",     "Use a unique identifier or destroy the existing resource first.");
    reg(kPermission,      "Permission denied",           "Check OS permissions and file/socket ownership.");

    // Memory
    reg(kOutOfMemory,   "Out of memory",
        "Reduce allocation pressure: pool allocators, streaming, or LOD budgets. "
        "Profile with GP MemTracker.",
        {}, false, true);
    reg(kAlignmentFault, "Alignment fault",
        "Ensure allocations meet the required alignment for SIMD types. "
        "Use gp::AlignedAlloc<N>().");
    reg(kHeapCorruption, "Heap corruption detected",
        "Enable GP_HEAP_GUARD in debug builds and inspect surrounding allocations.",
        {}, false, true);

    // IO
    reg(kFileNotFound,   "File not found",              "Verify the path and check the virtual filesystem mount table.", {}, true);
    reg(kFileOpenFailed, "File open failed",            "Check read/write permissions and ensure the path is not locked.");
    reg(kFileReadFailed, "File read failed",            "The file may be truncated or the handle was closed prematurely.");
    reg(kFileWriteFailed,"File write failed",           "Check disk space and write permissions.");
    reg(kEOF,            "End of file reached",         "Normal termination condition, not a bug.", {}, true);

    // RHI / Renderer
    reg(kDeviceLost,    "GPU device lost",
        "Possible causes: driver crash, TDR timeout, GPU hot-unplug. "
        "Attempt device recovery or present a safe-mode fallback.",
        {}, false, true);
    reg(kSwapchainFail, "Swapchain creation or present failed",
        "Verify the window is valid and has a non-zero client area. "
        "Handle DXGI_ERROR_INVALID_CALL / VK_ERROR_OUT_OF_DATE_KHR.");
    reg(kShaderCompile, "Shader compilation failed",
        "Check the shader source for syntax errors. Run with GP_SHADER_VERBOSE=1 "
        "to capture the full compiler log.");
    reg(kPipelineCreate,"Pipeline state object creation failed",
        "Validate all PSO descriptors. Ensure shaders are compiled and "
        "root signatures match.");

    // Platform / Window
    reg(kWindowCreate,  "Window creation failed",
        "Ensure the display server is running, the resolution is supported, "
        "and the platform backend is initialized.");
    reg(kWindowResize,  "Window resize failed",
        "The OS may have rejected the requested dimensions. Clamp to monitor bounds.");
    reg(kInputInit,     "Input system initialization failed",
        "Check device permissions (e.g. /dev/input on Linux) and platform backend.");
    reg(kNetConnect,    "Network connection failed",
        "Verify the endpoint address, firewall rules, and that the remote "
        "service is reachable.");

    // Audio
    reg(kAudioDeviceInit, "Audio device initialization failed",
        "Ensure an audio output device is present and not exclusively locked by "
        "another process. Check WASAPI / ALSA / CoreAudio permissions.");

    // clang-format on
}

}   // namespace gp::error

/// @brief Macro to register a custom ErrorCode in the global registry with its description, remediation hint, and
/// optional documentation URL.
/// @param[in] code_ The ErrorCode to register.
/// @param[in] desc_ A short description of what the error code means.
/// @param[in] hint_ An actionable fix or investigation hint for the error code.
/// @param[in] ... Optional variadic argument for the documentation URL (e.g. "https://wiki.mygame.dev/errors/MY_CODE").
#define GP_REGISTER_ERROR(code_, desc_, hint_, ...)                 \
    ::gp::error::ErrorRegistry::instance().registerCode(            \
        (code_),                                                    \
        ::gp::error::ErrorEntry{                                    \
            .description    = (desc_),                              \
            .remediation    = (hint_),                              \
            .wikiUrl        = [] {                                  \
                constexpr const char* _args[] = { __VA_ARGS__ "" }; \
                return gp::String(_args[0]); }(),                   \
            .isExpected     = false,                                \
            .isAlwaysFatal  = false                                 \
        })

/// @brief Registers an expected error code with its description and remediation hint.
/// @param[in] code_ The ErrorCode to register as expected.
/// @param[in] desc_ A short description of what the error code means.
/// @param[in] hint_ An actionable fix or investigation hint for the error code.
#define GP_REGISTER_EXPECTED_ERROR(code_, desc_, hint_)     \
    ::gp::error::ErrorRegistry::instance().registerCode(    \
        (code_),                                            \
        ::gp::error::ErrorEntry{                            \
            .description    = (desc_),                      \
            .remediation    = (hint_),                      \
            .wikiUrl        = {},                           \
            .isExpected     = true,                         \
            .isAlwaysFatal  = false                         \
        })
