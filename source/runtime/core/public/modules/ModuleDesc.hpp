// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/Name.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "memory/MemoryForward.hpp"
#include "modules/ModuleEnums.hpp"
#include "modules/ModuleForward.hpp"   // IWYU pragma: export
#include "platforms/base/system/PathTypes.hpp"

namespace gp::modules
{

/// @brief Plain-Old-Data (POD) descriptor configuring an explicit module load request ([ADR-0061]).
struct ModuleDesc final
{
    /// @brief Logical module identifier (e.g., `"rhi/d3d12"`, `"audio/openal"`, or `"rhi-d3d12"`).
    gp::StringView name{};

    /// @brief Optional explicit shared library path or filename override.
    /// @details When empty, `ModuleManager` automatically formats the canonical GPBT shared library name
    ///          (`"gp-rhi-d3d12.dll"` on Windows, `"libgp-rhi-d3d12.so"` on Linux, `"libgp-rhi-d3d12.dylib"` on macOS).
    gp::StringView explicitLibraryPath{};

    /// @brief Optional capability hint flags merged into the module entry on load.
    ModuleFlags flags{ ModuleFlags::None };
};

/// @brief Read-only diagnostic snapshot of a module slot inside `ModuleManager` ([ADR-0061]).
struct ModuleStatusView final
{
    /// @brief Canonical normalized module name (e.g., `gp::Name("rhi-d3d12")`).
    gp::Name name{};

    /// @brief Resolved shared library filename or path used to load the module (empty for static modules).
    gp::PathBuffer libraryPath{};

    /// @brief Active `IModule*` instance when `state == ModuleState::Loaded`, or `nullptr`.
    IModule* instance{ nullptr };

    /// @brief Opaque OS library handle (`HMODULE` / `dlopen` handle) for dynamic modules, or `nullptr`.
    void* libraryHandle{ nullptr };

    /// @brief Synchronized shadow `gp::memory::Malloc*` reported by the module during its entry handshake.
    gp::memory::Malloc* boundShadowMalloc{ nullptr };

    /// @brief Monotonic 1-based load order ticket (`0` if not currently loaded), used for reverse-LIFO teardown.
    gp::UInt32 loadOrder{ 0u };

    /// @brief Current lifecycle state of the module slot.
    ModuleState state{ ModuleState::Unloaded };

    /// @brief Active capability and provenance flags (`IsStaticLinked`, `ShadowMallocBound`, etc.).
    ModuleFlags flags{ ModuleFlags::None };

    /// @brief Outcome of the most recent `loadModule()` operation on this slot.
    ModuleLoadResult lastLoadResult{ ModuleLoadResult::Success };
};

}   // namespace gp::modules
