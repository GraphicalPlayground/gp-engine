// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/FixedString.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"

namespace gp::platform
{

/// @brief Maximum UTF-8 byte length (excluding null terminator) for an engine path buffer (ADR-0043).
/// @details Sized to `1024` bytes so `PathBuffer` occupies `1027` bytes (3-byte `m_data[1025]` + `UInt16 m_size`) on
///          the stack, supporting Windows long paths (`> 260` chars) and deep POSIX paths with zero heap allocations.
inline constexpr gp::USize kMaxPathLength = 1024u;

/// @brief Canonical cross-platform directory separator used internally across all `gp-engine` paths (`'/'`).
inline constexpr char kPreferredSeparator = '/';

/// @brief Alternative directory separator recognized during lexical path parsing and normalization (`'\\'`).
inline constexpr char kAltSeparator = '\\';

/// @brief Host operating system native directory separator (`'\\'` on Windows, `'/'` on POSIX/macOS/Linux).
#if GP_PLATFORM_WINDOWS
inline constexpr char kNativeSeparator = '\\';
#else
inline constexpr char kNativeSeparator = '/';
#endif

/// @brief Zero-allocation, stack-allocated UTF-8 path string buffer with capacity `kMaxPathLength` (`1024` bytes).
using PathBuffer = gp::FixedString<kMaxPathLength>;

/// @brief Result of decomposing a path into its directory prefix, base filename stem, and extension views.
/// @note All three `gp::StringView` members slice directly into the input path passed to `Paths::split()` and
///       perform zero memory allocations.
struct PathSplitResult
{
public:
    /// @brief Directory prefix preceding the final path separator (empty if the path has no directory component).
    gp::StringView directory{};

    /// @brief Base filename stem without directory prefix or extension (e.g., `"Hero"` from `"Meshes/Hero.uasset"`).
    gp::StringView filename{};

    /// @brief File extension without leading dot (e.g., `"uasset"` from `"Meshes/Hero.uasset"`).
    gp::StringView extension{};

public:
    /// @brief Compares two decomposed path split results for exact equality.
    /// @param[in] other The split result to compare against.
    /// @return True if `directory`, `filename`, and `extension` are all equal, false otherwise.
    [[nodiscard]] constexpr bool operator==(const PathSplitResult& other) const noexcept = default;
};

}   // namespace gp::platform

namespace gp
{

/// @brief Convenience alias for `gp::platform::PathBuffer` in the root `gp` namespace.
using PathBuffer = gp::platform::PathBuffer;

}   // namespace gp
