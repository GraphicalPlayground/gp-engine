// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/DeviceEnums.hpp"
#include "device/DeviceForward.hpp"
#include "device/RHIBootstrapDesc.hpp"
#include "modules/ModuleManager.hpp"
#include "RHIBase.hpp"

namespace gp::rhi
{

/// @brief Canonical module identifier for the headless null RHI backend.
inline constexpr gp::StringView kNullRHIModuleName = "rhi/null";

/// @brief Canonical module identifier for the Direct3D 12 RHI backend.
inline constexpr gp::StringView kD3D12RHIModuleName = "rhi/d3d12";

/// @brief Canonical module identifier for the Direct3D 11 RHI backend.
inline constexpr gp::StringView kD3D11RHIModuleName = "rhi/d3d11";

/// @brief Canonical module identifier for the Vulkan RHI backend.
inline constexpr gp::StringView kVulkanRHIModuleName = "rhi/vulkan";

/// @brief Canonical module identifier for the Apple Metal RHI backend.
inline constexpr gp::StringView kMetalRHIModuleName = "rhi/metal";

/// @brief Canonical module identifier for the OpenGL RHI backend.
inline constexpr gp::StringView kOpenGLRHIModuleName = "rhi/opengl";

/// @brief Maximum number of platform RHI candidates returned by `getPlatformRHISearchOrder()`.
inline constexpr gp::UInt32 kMaxPlatformRHICandidates = 4u;

/// @brief Maps an `InterfaceType` enumerator to its canonical `ModuleManager` module identifier.
/// @param[in] interfaceType Graphics API interface type.
/// @return Canonical module identifier (for example `"rhi/vulkan"`), or empty view for `NotSet`/`Hidden`/`Agx`.
[[nodiscard]] GP_RHI_API gp::StringView getModuleNameForInterfaceType(InterfaceType interfaceType) noexcept;

/// @brief Parses a backend alias or module identifier into its corresponding `InterfaceType`.
/// @details Recognizes short flags and full module names case-insensitively (`"null"`, `"rhi/null"`, `"d3d12"`,
///          `"dx12"`, `"rhi/d3d12"`, `"d3d11"`, `"dx11"`, `"rhi/d3d11"`, `"vulkan"`, `"vk"`, `"rhi/vulkan"`,
///          `"metal"`, `"mtl"`, `"rhi/metal"`, `"opengl"`, `"gl"`, `"rhi/opengl"`).
/// @param[in] name Backend alias or module identifier.
/// @return Matching `InterfaceType`, or `InterfaceType::NotSet` if unrecognized.
[[nodiscard]] GP_RHI_API InterfaceType getInterfaceTypeFromName(gp::StringView name) noexcept;

/// @brief Returns a human-readable ASCII label for a `FeatureLevel` enumerator.
/// @param[in] featureLevel Shader model feature level.
/// @return Short label such as `"ES3_1"`, `"SM5"`, `"SM6"`, or `"Unknown"`.
[[nodiscard]] GP_RHI_API gp::StringView getFeatureLevelName(FeatureLevel featureLevel) noexcept;

/// @brief Parses a feature level name (`"es31"`, `"es3_1"`, `"sm5"`, `"sm6"`) case-insensitively.
/// @param[in] text Input feature level label.
/// @param[out] outFeatureLevel Receives the parsed `FeatureLevel` on success.
/// @return True if `text` matched a supported feature level; false otherwise.
[[nodiscard]] GP_RHI_API bool tryParseFeatureLevel(gp::StringView text, FeatureLevel& outFeatureLevel) noexcept;

/// @brief Applies RHI command-line switches from `gp::CommandLine` onto `inOutDesc`.
/// @details Handles `-nullrhi`, `-rhi=<name>`, `-d3d12`/`-dx12`, `-d3d11`/`-dx11`, `-vulkan`, `-opengl`,
///          `-metal`/`-mtl`, `-sm6`, `-sm5`, and `-es31`/`-FeatureLevelES31`/`-FeatureLevelES3_1`.
/// @param[in,out] inOutDesc Bootstrap descriptor mutated in place when matching command-line flags are present.
GP_RHI_API void applyRHICommandLineOverrides(RHIBootstrapDesc& inOutDesc) noexcept;

/// @brief Writes the default hardware RHI backend search order for the active host operating system.
/// @param[out] outModuleNames Caller-allocated buffer receiving canonical module identifiers.
/// @param[in] maxCount Maximum number of entries that fit in `outModuleNames`.
/// @return Number of module identifiers written into `outModuleNames`.
[[nodiscard]] GP_RHI_API gp::UInt32
    getPlatformRHISearchOrder(gp::StringView* outModuleNames, gp::UInt32 maxCount) noexcept;

/// @brief Probes candidate `IDynamicRHIModule` plugins via `moduleManager` and selects the first supported backend.
/// @details Evaluates command-line overrides (when enabled), honors headless/null-RHI mode, negotiates the highest
///          supported feature level (stepping down from `SM6` to `SM5` to `ES3_1` when not forced), unloads probed
///          modules that report `isSupported() == false`, and optionally falls back to `"rhi/null"`.
/// @param[in] desc Bootstrap configuration controlling preferred backend, feature level, and fallback policy.
/// @param[in,out] moduleManager Module manager used to load and probe `IDynamicRHIModule` instances.
/// @return `RHISelectionResult` containing the selected module pointer and feature level, or an invalid result if
///         no candidate backend could be loaded and supported.
[[nodiscard]] GP_RHI_API RHISelectionResult selectDynamicRHIModule(
    const RHIBootstrapDesc& desc, gp::modules::ModuleManager& moduleManager = gp::modules::ModuleManager::get()
) noexcept;

/// @brief Selects an `IDynamicRHIModule` and creates a concrete `DynamicRHI` device instance for the current platform.
/// @details Mirrors platform dynamic RHI creation: loads the best supported module, invokes `createRHI()`, and
///          records the loaded module name for diagnostic queries.
/// @param[in] desc Bootstrap configuration controlling backend selection and fallback behavior.
/// @param[in,out] moduleManager Module manager used to load the backend module.
/// @return Newly allocated `DynamicRHI*` (owned by caller / `rhiExit()`), or `nullptr` if creation failed.
[[nodiscard]] GP_RHI_API DynamicRHI* platformCreateDynamicRHI(
    const RHIBootstrapDesc& desc = RHIBootstrapDesc{},
    gp::modules::ModuleManager& moduleManager = gp::modules::ModuleManager::get()
) noexcept;

/// @brief Returns the canonical module name of the active RHI backend without triggering module loads.
/// @return Loaded module name string view (for example `"rhi/null"`), or an empty view if RHI is not initialized.
[[nodiscard]] GP_RHI_API gp::StringView getLoadedRHIModuleName() noexcept;

/// @brief Returns the name of the RHI module currently active or selected by `selectDynamicRHIModule()`.
/// @details When a `DynamicRHI` is already initialized, returns the active loaded module name immediately.
///          Otherwise probes candidate modules and optionally unloads the probed module when `cleanupIfProbed` is
///          true.
/// @note Probing before `rhiInit()` may load backend shared libraries and query GPU adapters.
/// @param[in] cleanupIfProbed If true and RHI is not yet initialized, unloads the probed module before returning.
/// @param[in,out] moduleManager Module manager used when probing before initialization.
/// @return Canonical module identifier, or an empty view if no supported RHI module was found.
[[nodiscard]] GP_RHI_API gp::StringView getSelectedDynamicRHIModuleName(
    bool cleanupIfProbed = true, gp::modules::ModuleManager& moduleManager = gp::modules::ModuleManager::get()
) noexcept;

/// @brief Loads `"rhi/null"`, verifies support, creates `NullDynamicRHI`, and initializes the global RHI subsystem.
/// @details Marks `getRHIGlobals().usingNullRHI = true` and disables texture streaming so headless servers,
///          commandlets, and automated tests run without GPU drivers.
/// @param[in,out] moduleManager Module manager used to load `"rhi/null"`.
/// @return True if `NullDynamicRHI` was created and initialized; false if `"rhi/null"` could not be loaded.
[[nodiscard]] GP_RHI_API bool
    initNullRHI(gp::modules::ModuleManager& moduleManager = gp::modules::ModuleManager::get()) noexcept;

/// @brief Discovers, creates, and initializes the active `DynamicRHI` using `desc` and `moduleManager`.
/// @details Idempotent when a `DynamicRHI` is already active. Latches command-list bypass mode (`-rhibypass`),
///          selects either `initNullRHI()` or `platformCreateDynamicRHI()`, wires `TransitionController` hooks,
///          calls `DynamicRHI::init()`, logs adapter telemetry, and validates desktop feature level (`>= SM5`) when
///          `desc.hasEditorToken` is true on a hardware backend.
/// @param[in] desc Bootstrap configuration controlling backend selection, editor validation, and fallback policy.
/// @param[in,out] moduleManager Module manager used to discover and load `IDynamicRHIModule` plugins.
/// @return True if the global `DynamicRHI` is initialized and ready; false on discovery or validation failure.
[[nodiscard]] GP_RHI_API bool rhiInit(
    const RHIBootstrapDesc& desc, gp::modules::ModuleManager& moduleManager = gp::modules::ModuleManager::get()
) noexcept;

/// @brief Convenience overload initializing the RHI subsystem with default platform discovery and `hasEditorToken`.
/// @param[in] hasEditorToken True when booting an editor host that requires desktop `FeatureLevel::SM5` or higher.
/// @return True if the global `DynamicRHI` was initialized; false on failure.
[[nodiscard]] GP_RHI_API bool rhiInit(bool hasEditorToken = false) noexcept;

/// @brief Internal helpers managing the recorded active RHI module name across startup and teardown.
namespace detail
{

/// @brief Records the active RHI module name after backend creation.
/// @param[in] moduleName Canonical module identifier to store.
void setLoadedRHIModuleName(gp::StringView moduleName) noexcept;

/// @brief Clears the stored active RHI module name on teardown.
void clearLoadedRHIModuleName() noexcept;

}   // namespace detail

}   // namespace gp::rhi
