// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "device/DeviceEnums.hpp"
#include "device/DeviceForward.hpp"

namespace gp::rhi
{

/// @brief Configuration parameters governing RHI backend module discovery, feature level selection, and startup.
/// @details Passed to `selectDynamicRHIModule()`, `platformCreateDynamicRHI()`, and `rhiInit()`. Command-line
///          overrides (such as `-nullrhi`, `-vulkan`, `-d3d12`, `-d3d11`, `-opengl`, `-metal`, `-rhi=<name>`, `-sm6`,
///          `-sm5`, `-es31`, and `-rhibypass`) are applied on top when `applyCommandLineOverrides` is true.
struct RHIBootstrapDesc
{
    /// @brief Optional preferred RHI module identifier (for example `"rhi/vulkan"`, `"rhi/d3d12"`, or `"rhi/null"`).
    /// @details When empty, `selectDynamicRHIModule()` probes the default search order for the active host platform.
    gp::StringView preferredModuleName{};

    /// @brief Requested shader model feature tier (`FeatureLevel::Count` selects the highest tier supported).
    FeatureLevel preferredFeatureLevel{ FeatureLevel::Count };

    /// @brief If true, skips hardware backends and loads `"rhi/null"` directly.
    bool forceNullRHI{ false };

    /// @brief If true, falls back to `"rhi/null"` when no hardware backend module can be loaded or initialized.
    bool allowNullFallback{ true };

    /// @brief Indicates whether the host process can render to a GPU (false for headless servers or offline tools).
    /// @details When false, startup bypasses hardware probing and initializes `"rhi/null"` directly.
    bool canRender{ true };

    /// @brief True when running with an editor host token, requiring desktop feature level `FeatureLevel::SM5` or
    ///        higher on hardware backends.
    bool hasEditorToken{ false };

    /// @brief If true, reads RHI backend, feature level, and bypass overrides from `gp::CommandLine`.
    bool applyCommandLineOverrides{ true };

    /// @brief Internal flag set when a command-line switch explicitly forced a specific backend module.
    bool forcedModuleByCommandLine{ false };

    /// @brief Internal flag set when a command-line switch explicitly forced a specific feature level.
    bool forcedFeatureLevelByCommandLine{ false };
};

/// @brief Output of `selectDynamicRHIModule()` describing the chosen backend module and negotiated feature tier.
struct RHISelectionResult
{
    /// @brief Loaded and supported `IDynamicRHIModule` instance, or `nullptr` if no candidate succeeded.
    IDynamicRHIModule* module{ nullptr };

    /// @brief Module identifier string passed to `ModuleManager` (for example `"rhi/null"` or `"rhi/d3d12"`).
    gp::StringView moduleName{};

    /// @brief Negotiated feature level passed to `IDynamicRHIModule::createRHI()`.
    FeatureLevel featureLevel{ FeatureLevel::Count };

    /// @brief True when hardware backend probing failed and selection fell back to `"rhi/null"`.
    bool usedNullFallback{ false };

    /// @brief True when the selected backend module was forced via a command-line switch.
    bool forcedByCommandLine{ false };

    /// @brief True when this selection call freshly loaded `module` (false if it was already loaded).
    bool loadedByProbe{ false };

    /// @brief Checks whether a supported `IDynamicRHIModule` was resolved.
    /// @return True when `module != nullptr`.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return module != nullptr;
    }
};

}   // namespace gp::rhi
