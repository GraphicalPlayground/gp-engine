// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "container/BasicStringView.hpp"   // IWYU pragma: keep
#include "container/Forward.hpp"
#include "CoreMinimal.hpp"
#include <compare>   // IWYU pragma: keep

namespace gp::error
{

/// @brief Domain enumeration, each engine subsystem owns a unique domain for its error codes.
/// @note
///   - Domains 0x00–0x0F are reserved for core engine systems (e.g. memory, threading, IO).
///   - Domains 0x10–0x1F are reserved for graphics/RHI-related systems (e.g. renderer, shader, texture).
///   - Domains 0x20–0x2F are reserved for audio-related systems (e.g. audio device, mixer).
///   - Domains 0x30–0x3F are reserved for simulation-related systems (e.g. physics, animation, AI).
///   - Domains 0x40–0x4F are reserved for platform/OS-related systems (e.g. windowing, input, network).
///   - Domains 0x50–0x5F are reserved for gameplay-related systems (e.g. scripting, asset/scene management).
///   - Domains 0xE0–0xFF are reserved for user-defined project-level codes.
enum class Domain : gp::UInt8
{
    // Core engine
    Generic = 0x00,
    Memory = 0x01,
    Threading = 0x02,
    IO = 0x03,
    Serialization = 0x04,
    Plugin = 0x05,

    // Graphics / RHI
    RHI = 0x10,
    Renderer = 0x11,
    Shader = 0x12,
    Texture = 0x13,
    Mesh = 0x14,

    // Audio
    Audio = 0x20,
    AudioDevice = 0x21,

    // Simulation
    Physics = 0x30,
    Animation = 0x31,
    AI = 0x32,

    // Platform / OS
    Platform = 0x40,
    Window = 0x41,
    Input = 0x42,
    Network = 0x43,

    // Gameplay
    Script = 0x50,
    Asset = 0x51,
    Scene = 0x52,
    Entity = 0x53,

    // User-defined range, reserve 0xE0–0xFF for project-level codes
    UserDefined = 0xE0,

    COUNT = 0xFF
};

/// @brief Value-semantic, 32-bit error code class that encapsulates a domain and a specific error code within that
/// domain.
/// @note
/// The high 16 bits represent the domain, and the low 16 bits represent the specific error code. A value of 0
/// represents success, while any non-zero value represents an error. The class provides utility functions for checking
/// success/failure and retrieving the domain and code.
class ErrorCode
{
private:
    gp::UInt32 m_value{ 0u };

public:
    /// @brief Default constructor, creates a success code (0).
    constexpr ErrorCode() noexcept = default;

    /// @brief Constructs an ErrorCode from a domain and a code.
    /// @param[in] domain The domain of the error code.
    /// @param[in] code The specific error code within the domain.
    constexpr ErrorCode(Domain domain, gp::UInt16 code) noexcept
        : m_value((static_cast<gp::UInt32>(domain) << 16u) | static_cast<gp::UInt32>(code))
    {}

    /// @brief Creates an ErrorCode from a raw 32-bit value.
    /// @param[in] rawValue The raw error code value.
    explicit constexpr ErrorCode(gp::UInt32 rawValue) noexcept
        : m_value(rawValue)
    {}

public:
    /// @brief Enables implicit conversion to bool, allowing ErrorCode to be used in boolean contexts.
    /// @return True if the error code represents an error, false if it represents success.
    explicit constexpr operator bool() const noexcept
    {
        return isError();
    }

    /// @brief Enables three-way comparison between ErrorCode instances.
    /// @note This allows for comparisons using ==, !=, <, >, <=, >= operators.
    /// @param[in] other The other ErrorCode to compare with.
    /// @return A std::strong_ordering value indicating the result of the comparison.
    constexpr auto operator<=>(const ErrorCode&) const noexcept = default;

public:
    /// @brief Returns the domain of the error code.
    /// @return The domain.
    GP_NODISCARD constexpr Domain domain() const noexcept
    {
        return static_cast<Domain>((m_value >> 16u) & 0xFFu);
    }

    /// @brief Returns the error code within its domain.
    /// @return The error code.
    GP_NODISCARD constexpr gp::UInt16 code() const noexcept
    {
        return static_cast<gp::UInt16>(m_value & 0xFFFFu);
    }

    /// @brief Returns the raw value of the error code.
    /// @return The raw error code value.
    GP_NODISCARD constexpr gp::UInt32 raw() const noexcept
    {
        return m_value;
    }

    /// @brief Checks if the error code represents a successful operation.
    /// @return True if the code represents a success, false otherwise.
    GP_NODISCARD constexpr bool isOk() const noexcept
    {
        return m_value == 0u;
    }

    /// @brief Checks if the error code represents an error.
    /// @return True if the code represents an error, false otherwise.
    GP_NODISCARD constexpr bool isError() const noexcept
    {
        return m_value != 0u;
    }

public:
    /// @brief Returns a successful error code.
    /// @return A success code (0u).
    static constexpr ErrorCode ok() noexcept
    {
        return ErrorCode{ 0u };
    }

    /// @brief Returns an unknown error code.
    /// @return An error code with all bits set (0xFFFF'FFFF), representing an unknown error.
    static constexpr ErrorCode unknown() noexcept
    {
        return ErrorCode{ 0xFFFF'FFFFu };
    }
};

// Pre-defined error codes for common scenarios, organized by domain.
// Extend this list by adding new codes in the gp::error::codes namespace for your specific subsystem.
namespace codes
{

// Generic
inline constexpr ErrorCode kOk{ Domain::Generic, 0x0000 };
inline constexpr ErrorCode kUnknown{ Domain::Generic, 0x0001 };
inline constexpr ErrorCode kNotImplemented{ Domain::Generic, 0x0002 };
inline constexpr ErrorCode kInvalidArgument{ Domain::Generic, 0x0003 };
inline constexpr ErrorCode kOutOfRange{ Domain::Generic, 0x0004 };
inline constexpr ErrorCode kNullPointer{ Domain::Generic, 0x0005 };
inline constexpr ErrorCode kTimeout{ Domain::Generic, 0x0006 };
inline constexpr ErrorCode kNotFound{ Domain::Generic, 0x0007 };
inline constexpr ErrorCode kAlreadyExists{ Domain::Generic, 0x0008 };
inline constexpr ErrorCode kPermission{ Domain::Generic, 0x0009 };

// Memory
inline constexpr ErrorCode kOutOfMemory{ Domain::Memory, 0x0001 };
inline constexpr ErrorCode kAlignmentFault{ Domain::Memory, 0x0002 };
inline constexpr ErrorCode kHeapCorruption{ Domain::Memory, 0x0003 };

// IO
inline constexpr ErrorCode kFileNotFound{ Domain::IO, 0x0001 };
inline constexpr ErrorCode kFileOpenFailed{ Domain::IO, 0x0002 };
inline constexpr ErrorCode kFileReadFailed{ Domain::IO, 0x0003 };
inline constexpr ErrorCode kFileWriteFailed{ Domain::IO, 0x0004 };
inline constexpr ErrorCode kEOF{ Domain::IO, 0x0005 };

// RHI
inline constexpr ErrorCode kDeviceLost{ Domain::RHI, 0x0001 };
inline constexpr ErrorCode kSwapchainFail{ Domain::RHI, 0x0002 };
inline constexpr ErrorCode kShaderCompile{ Domain::Shader, 0x0001 };
inline constexpr ErrorCode kPipelineCreate{ Domain::RHI, 0x0003 };

// Platform
inline constexpr ErrorCode kWindowCreate{ Domain::Window, 0x0001 };
inline constexpr ErrorCode kWindowResize{ Domain::Window, 0x0002 };
inline constexpr ErrorCode kInputInit{ Domain::Input, 0x0001 };
inline constexpr ErrorCode kNetConnect{ Domain::Network, 0x0001 };

// Audio
inline constexpr ErrorCode kAudioDeviceInit{ Domain::AudioDevice, 0x0001 };

}   // namespace codes

/// @brief Returns a human-readable name for a given error domain.
/// @param[in] domain The error domain to get the name of.
/// @return A string view containing the name of the domain, or "<unknown>" if the domain is not recognized.
GP_NODISCARD constexpr gp::StringView getDomainName(Domain domain) noexcept
{
    switch (domain)
    {
        // clang-format off
        case Domain::Generic:       return "Generic";
        case Domain::Memory:        return "Memory";
        case Domain::Threading:     return "Threading";
        case Domain::IO:            return "IO";
        case Domain::Serialization: return "Serialization";
        case Domain::Plugin:        return "Plugin";
        case Domain::RHI:           return "RHI";
        case Domain::Renderer:      return "Renderer";
        case Domain::Shader:        return "Shader";
        case Domain::Texture:       return "Texture";
        case Domain::Mesh:          return "Mesh";
        case Domain::Audio:         return "Audio";
        case Domain::AudioDevice:   return "AudioDevice";
        case Domain::Physics:       return "Physics";
        case Domain::Animation:     return "Animation";
        case Domain::AI:            return "AI";
        case Domain::Platform:      return "Platform";
        case Domain::Window:        return "Window";
        case Domain::Input:         return "Input";
        case Domain::Network:       return "Network";
        case Domain::Script:        return "Script";
        case Domain::Asset:         return "Asset";
        case Domain::Scene:         return "Scene";
        case Domain::Entity:        return "Entity";
        case Domain::UserDefined:   return "UserDefined";
        default:                    return "<unknown>";
        // clang-format on
    }
}

}   // namespace gp::error
