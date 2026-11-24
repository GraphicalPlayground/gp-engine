// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/RHIModuleDiscovery.hpp"
#include "commands/CommandListExecutor.hpp"
#include "containers/strings/FixedString.hpp"
#include "device/DynamicRHI.hpp"
#include "device/IDynamicRHIModule.hpp"
#include "device/RHIGlobals.hpp"
#include "logging/LogMacros.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "modules/ModuleEnums.hpp"
#include "RHIDefinitions.hpp"
#include "templates/Optional.hpp"

namespace gp::rhi
{

namespace
{

gp::FixedString<127> g_loadedRHIModuleName{};

[[nodiscard]] bool tryResolveFeatureLevel(
    const IDynamicRHIModule& module,
    const FeatureLevel requestedLevel,
    const bool forcedFeatureLevel,
    FeatureLevel& outResolvedLevel
) noexcept
{
    if (requestedLevel != FeatureLevel::Count)
    {
        if (module.isSupported(requestedLevel))
        {
            outResolvedLevel = requestedLevel;
            return true;
        }
        if (forcedFeatureLevel)
        {
            return false;
        }
    }

    static constexpr FeatureLevel kFallbackOrder[] = { FeatureLevel::SM6, FeatureLevel::SM5, FeatureLevel::ES3_1 };
    for (const FeatureLevel candidate: kFallbackOrder)
    {
        if (requestedLevel != FeatureLevel::Count &&
            static_cast<gp::Int32>(candidate) >= static_cast<gp::Int32>(requestedLevel))
        {
            continue;
        }
        if (module.isSupported(candidate))
        {
            outResolvedLevel = candidate;
            return true;
        }
    }

    return false;
}

[[nodiscard]] bool tryLoadAndProbeCandidate(
    const gp::StringView candidateName,
    const FeatureLevel requestedLevel,
    const bool forcedFeatureLevel,
    gp::modules::ModuleManager& moduleManager,
    IDynamicRHIModule*& outModule,
    FeatureLevel& outResolvedLevel,
    bool& outLoadedByProbe
) noexcept
{
    outModule = nullptr;
    outLoadedByProbe = false;
    if (candidateName.isEmpty())
    {
        return false;
    }

    const bool wasAlreadyLoaded = moduleManager.isModuleLoaded(candidateName);
    gp::modules::ModuleLoadResult loadResult = gp::modules::ModuleLoadResult::NotFound;
    gp::modules::IModule* const rawModule = moduleManager.loadModule(candidateName, &loadResult);
    if (rawModule == nullptr)
    {
        GP_LOG_INFO(
            LogRHI,
            "RHI candidate module '{}' unavailable (result={}).",
            candidateName,
            gp::modules::toStringView(loadResult)
        );
        return false;
    }

    if (rawModule->getInterfaceTag() != IDynamicRHIModule::kInterfaceTag)
    {
        GP_LOG_INFO(LogRHI, "RHI candidate module '{}' does not implement IDynamicRHIModule; skipping.", candidateName);
        if (!wasAlreadyLoaded)
        {
            (void)moduleManager.unloadModule(candidateName, gp::modules::ModuleShutdownPhase::Exit);
        }
        return false;
    }

    IDynamicRHIModule* const module = static_cast<IDynamicRHIModule*>(rawModule);

    FeatureLevel resolvedLevel = requestedLevel;
    if (!tryResolveFeatureLevel(*module, requestedLevel, forcedFeatureLevel, resolvedLevel))
    {
        GP_LOG_INFO(
            LogRHI,
            "RHI candidate module '{}' loaded but does not support requested feature level '{}'.",
            candidateName,
            getFeatureLevelName(requestedLevel)
        );
        if (!wasAlreadyLoaded)
        {
            (void)moduleManager.unloadModule(candidateName, gp::modules::ModuleShutdownPhase::Exit);
        }
        return false;
    }

    outModule = module;
    outResolvedLevel = resolvedLevel;
    outLoadedByProbe = !wasAlreadyLoaded;
    return true;
}

}   // namespace

namespace detail
{

void setLoadedRHIModuleName(const gp::StringView moduleName) noexcept
{
    g_loadedRHIModuleName.clear();
    if (!moduleName.isEmpty())
    {
        g_loadedRHIModuleName.assign(moduleName);
    }
}

void clearLoadedRHIModuleName() noexcept
{
    g_loadedRHIModuleName.clear();
}

}   // namespace detail

gp::StringView getModuleNameForInterfaceType(const InterfaceType interfaceType) noexcept
{
    switch (interfaceType)
    {
    case InterfaceType::Null:
        return kNullRHIModuleName;
    case InterfaceType::D3D12:
        return kD3D12RHIModuleName;
    case InterfaceType::D3D11:
        return kD3D11RHIModuleName;
    case InterfaceType::Vulkan:
        return kVulkanRHIModuleName;
    case InterfaceType::Metal:
        return kMetalRHIModuleName;
    case InterfaceType::OpenGL:
        return kOpenGLRHIModuleName;
    case InterfaceType::NotSet:
    case InterfaceType::Hidden:
    case InterfaceType::Agx:
    default:
        return gp::StringView{};
    }
}

InterfaceType getInterfaceTypeFromName(const gp::StringView name) noexcept
{
    const gp::StringView trimmed = name.trim();
    if (trimmed.isEmpty())
    {
        return InterfaceType::NotSet;
    }

    if (trimmed.equalsIgnoreCase("null") || trimmed.equalsIgnoreCase("nullrhi") ||
        trimmed.equalsIgnoreCase("rhi/null") || trimmed.equalsIgnoreCase("rhi-null"))
    {
        return InterfaceType::Null;
    }
    if (trimmed.equalsIgnoreCase("d3d12") || trimmed.equalsIgnoreCase("dx12") ||
        trimmed.equalsIgnoreCase("rhi/d3d12") || trimmed.equalsIgnoreCase("rhi-d3d12"))
    {
        return InterfaceType::D3D12;
    }
    if (trimmed.equalsIgnoreCase("d3d11") || trimmed.equalsIgnoreCase("dx11") ||
        trimmed.equalsIgnoreCase("rhi/d3d11") || trimmed.equalsIgnoreCase("rhi-d3d11"))
    {
        return InterfaceType::D3D11;
    }
    if (trimmed.equalsIgnoreCase("vulkan") || trimmed.equalsIgnoreCase("vk") ||
        trimmed.equalsIgnoreCase("rhi/vulkan") || trimmed.equalsIgnoreCase("rhi-vulkan"))
    {
        return InterfaceType::Vulkan;
    }
    if (trimmed.equalsIgnoreCase("metal") || trimmed.equalsIgnoreCase("mtl") || trimmed.equalsIgnoreCase("rhi/metal") ||
        trimmed.equalsIgnoreCase("rhi-metal"))
    {
        return InterfaceType::Metal;
    }
    if (trimmed.equalsIgnoreCase("opengl") || trimmed.equalsIgnoreCase("gl") ||
        trimmed.equalsIgnoreCase("rhi/opengl") || trimmed.equalsIgnoreCase("rhi-opengl"))
    {
        return InterfaceType::OpenGL;
    }

    return InterfaceType::NotSet;
}

gp::StringView getFeatureLevelName(const FeatureLevel featureLevel) noexcept
{
    switch (featureLevel)
    {
    case FeatureLevel::ES3_1:
        return "ES3_1";
    case FeatureLevel::SM5:
        return "SM5";
    case FeatureLevel::SM6:
        return "SM6";
    case FeatureLevel::Count:
        return "Highest";
    case FeatureLevel::ES2Removed:
    case FeatureLevel::SM4Removed:
    default:
        return "Unknown";
    }
}

bool tryParseFeatureLevel(const gp::StringView text, FeatureLevel& outFeatureLevel) noexcept
{
    const gp::StringView trimmed = text.trim();
    if (trimmed.equalsIgnoreCase("es31") || trimmed.equalsIgnoreCase("es3_1") ||
        trimmed.equalsIgnoreCase("featureleveles31") || trimmed.equalsIgnoreCase("featureleveles3_1"))
    {
        outFeatureLevel = FeatureLevel::ES3_1;
        return true;
    }
    if (trimmed.equalsIgnoreCase("sm5"))
    {
        outFeatureLevel = FeatureLevel::SM5;
        return true;
    }
    if (trimmed.equalsIgnoreCase("sm6"))
    {
        outFeatureLevel = FeatureLevel::SM6;
        return true;
    }
    return false;
}

void applyRHICommandLineOverrides(RHIBootstrapDesc& inOutDesc) noexcept
{
    if (!gp::CommandLine::isInitialized())
    {
        return;
    }

    if (gp::CommandLine::hasArgument("nullrhi"))
    {
        inOutDesc.forceNullRHI = true;
        inOutDesc.preferredModuleName = kNullRHIModuleName;
        inOutDesc.forcedModuleByCommandLine = true;
    }

    const gp::Optional<gp::StringView> explicitRHI = gp::CommandLine::tryGetString("rhi");
    if (explicitRHI.hasValue() && !explicitRHI->isEmpty())
    {
        const InterfaceType parsedType = getInterfaceTypeFromName(*explicitRHI);
        if (parsedType == InterfaceType::Null)
        {
            inOutDesc.forceNullRHI = true;
            inOutDesc.preferredModuleName = kNullRHIModuleName;
            inOutDesc.forcedModuleByCommandLine = true;
        }
        else if (parsedType != InterfaceType::NotSet)
        {
            inOutDesc.forceNullRHI = false;
            inOutDesc.preferredModuleName = getModuleNameForInterfaceType(parsedType);
            inOutDesc.forcedModuleByCommandLine = true;
        }
        else
        {
            // Allow custom/imported RHI module names passed directly via -rhi=<module>.
            inOutDesc.forceNullRHI = false;
            inOutDesc.preferredModuleName = explicitRHI->trim();
            inOutDesc.forcedModuleByCommandLine = true;
        }
    }

    if (!inOutDesc.forceNullRHI)
    {
        const bool hasD3D12 = gp::CommandLine::hasArgument("d3d12") || gp::CommandLine::hasArgument("dx12");
        const bool hasD3D11 = gp::CommandLine::hasArgument("d3d11") || gp::CommandLine::hasArgument("dx11");
        const bool hasVulkan = gp::CommandLine::hasArgument("vulkan");
        const bool hasMetal = gp::CommandLine::hasArgument("metal") || gp::CommandLine::hasArgument("mtl");
        const bool hasOpenGL = gp::CommandLine::hasArgument("opengl");

        const gp::UInt32 shorthandCount = static_cast<gp::UInt32>(hasD3D12) + static_cast<gp::UInt32>(hasD3D11) +
                                          static_cast<gp::UInt32>(hasVulkan) + static_cast<gp::UInt32>(hasMetal) +
                                          static_cast<gp::UInt32>(hasOpenGL);
        if (shorthandCount > 1u || (shorthandCount > 0u && inOutDesc.forcedModuleByCommandLine))
        {
            GP_LOG_WARNING(
                LogRHI, "Multiple RHI backend command-line switches were specified; using highest-priority selection."
            );
        }

        if (!inOutDesc.forcedModuleByCommandLine)
        {
            if (hasD3D12)
            {
                inOutDesc.preferredModuleName = kD3D12RHIModuleName;
                inOutDesc.forcedModuleByCommandLine = true;
            }
            else if (hasD3D11)
            {
                inOutDesc.preferredModuleName = kD3D11RHIModuleName;
                inOutDesc.forcedModuleByCommandLine = true;
            }
            else if (hasVulkan)
            {
                inOutDesc.preferredModuleName = kVulkanRHIModuleName;
                inOutDesc.forcedModuleByCommandLine = true;
            }
            else if (hasMetal)
            {
                inOutDesc.preferredModuleName = kMetalRHIModuleName;
                inOutDesc.forcedModuleByCommandLine = true;
            }
            else if (hasOpenGL)
            {
                inOutDesc.preferredModuleName = kOpenGLRHIModuleName;
                inOutDesc.forcedModuleByCommandLine = true;
            }
        }
    }

    const bool hasES31 = gp::CommandLine::hasArgument("es31") || gp::CommandLine::hasArgument("FeatureLevelES31") ||
                         gp::CommandLine::hasArgument("FeatureLevelES3_1");
    const bool hasSM5 = gp::CommandLine::hasArgument("sm5");
    const bool hasSM6 = gp::CommandLine::hasArgument("sm6");
    const gp::UInt32 featureLevelFlags =
        static_cast<gp::UInt32>(hasES31) + static_cast<gp::UInt32>(hasSM5) + static_cast<gp::UInt32>(hasSM6);
    if (featureLevelFlags > 1u)
    {
        GP_LOG_WARNING(
            LogRHI, "Multiple feature-level command-line switches (-es31/-sm5/-sm6) were specified; preferring highest."
        );
    }

    if (hasES31)
    {
        inOutDesc.preferredFeatureLevel = FeatureLevel::ES3_1;
        inOutDesc.forcedFeatureLevelByCommandLine = true;
    }
    if (hasSM5)
    {
        inOutDesc.preferredFeatureLevel = FeatureLevel::SM5;
        inOutDesc.forcedFeatureLevelByCommandLine = true;
    }
    if (hasSM6)
    {
        if (gp::CommandLine::hasArgument("mtl"))
        {
            GP_LOG_WARNING(LogRHI, "-mtl and -sm6 are incompatible options; falling back to SM5.");
            inOutDesc.preferredFeatureLevel = FeatureLevel::SM5;
        }
        else
        {
            inOutDesc.preferredFeatureLevel = FeatureLevel::SM6;
        }
        inOutDesc.forcedFeatureLevelByCommandLine = true;
    }
}

gp::UInt32 getPlatformRHISearchOrder(gp::StringView* const outModuleNames, const gp::UInt32 maxCount) noexcept
{
    if (outModuleNames == nullptr || maxCount == 0u)
    {
        return 0u;
    }

#if GP_PLATFORM_WINDOWS
    static constexpr gp::StringView kOrder[] = {
        kD3D12RHIModuleName, kD3D11RHIModuleName, kVulkanRHIModuleName, kOpenGLRHIModuleName
    };
#elif GP_PLATFORM_MACOS
    static constexpr gp::StringView kOrder[] = { kMetalRHIModuleName };
#elif GP_PLATFORM_LINUX || GP_PLATFORM_UNIX
    static constexpr gp::StringView kOrder[] = { kVulkanRHIModuleName, kOpenGLRHIModuleName };
#else
    static constexpr gp::StringView kOrder[] = { kNullRHIModuleName };
#endif

    const gp::UInt32 orderCount = static_cast<gp::UInt32>(sizeof(kOrder) / sizeof(kOrder[0]));
    const gp::UInt32 writeCount = (maxCount < orderCount) ? maxCount : orderCount;
    for (gp::UInt32 i = 0u; i < writeCount; ++i)
    {
        outModuleNames[i] = kOrder[i];
    }
    return writeCount;
}

RHISelectionResult
    selectDynamicRHIModule(const RHIBootstrapDesc& desc, gp::modules::ModuleManager& moduleManager) noexcept
{
    RHIBootstrapDesc effectiveDesc = desc;
    if (effectiveDesc.applyCommandLineOverrides)
    {
        applyRHICommandLineOverrides(effectiveDesc);
    }

    RHISelectionResult result{};
    result.forcedByCommandLine = effectiveDesc.forcedModuleByCommandLine;

    // Headless or explicit -nullrhi runs skip hardware probing entirely.
    if (!effectiveDesc.canRender || effectiveDesc.forceNullRHI)
    {
        IDynamicRHIModule* nullModule = nullptr;
        FeatureLevel resolvedLevel = effectiveDesc.preferredFeatureLevel;
        bool loadedByProbe = false;
        if (tryLoadAndProbeCandidate(
                kNullRHIModuleName,
                effectiveDesc.preferredFeatureLevel,
                false,
                moduleManager,
                nullModule,
                resolvedLevel,
                loadedByProbe
            ))
        {
            result.module = nullModule;
            result.moduleName = kNullRHIModuleName;
            result.featureLevel = resolvedLevel;
            result.usedNullFallback = false;
            result.loadedByProbe = loadedByProbe;
        }
        return result;
    }

    // Build deduplicated candidate list: explicit preferred module first, then platform search order.
    gp::StringView candidates[kMaxPlatformRHICandidates + 1u]{};
    gp::UInt32 candidateCount = 0u;

    if (!effectiveDesc.preferredModuleName.isEmpty())
    {
        candidates[candidateCount++] = effectiveDesc.preferredModuleName;
    }

    if (!effectiveDesc.forcedModuleByCommandLine)
    {
        gp::StringView platformOrder[kMaxPlatformRHICandidates]{};
        const gp::UInt32 platformCount = getPlatformRHISearchOrder(platformOrder, kMaxPlatformRHICandidates);
        for (gp::UInt32 i = 0u; i < platformCount; ++i)
        {
            bool duplicate = false;
            for (gp::UInt32 j = 0u; j < candidateCount; ++j)
            {
                if (candidates[j].equalsIgnoreCase(platformOrder[i]))
                {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate && candidateCount < (kMaxPlatformRHICandidates + 1u))
            {
                candidates[candidateCount++] = platformOrder[i];
            }
        }
    }

    for (gp::UInt32 i = 0u; i < candidateCount; ++i)
    {
        IDynamicRHIModule* candidateModule = nullptr;
        FeatureLevel resolvedLevel = effectiveDesc.preferredFeatureLevel;
        bool loadedByProbe = false;
        if (tryLoadAndProbeCandidate(
                candidates[i],
                effectiveDesc.preferredFeatureLevel,
                effectiveDesc.forcedFeatureLevelByCommandLine,
                moduleManager,
                candidateModule,
                resolvedLevel,
                loadedByProbe
            ))
        {
            result.module = candidateModule;
            result.moduleName = candidates[i];
            result.featureLevel = resolvedLevel;
            result.usedNullFallback = false;
            result.loadedByProbe = loadedByProbe;
            return result;
        }
    }

    // Fall back to the headless null RHI when permitted and no hardware backend was available.
    if (effectiveDesc.allowNullFallback && !effectiveDesc.forcedModuleByCommandLine)
    {
        IDynamicRHIModule* nullModule = nullptr;
        FeatureLevel resolvedLevel = FeatureLevel::SM5;
        bool loadedByProbe = false;
        if (tryLoadAndProbeCandidate(
                kNullRHIModuleName, FeatureLevel::Count, false, moduleManager, nullModule, resolvedLevel, loadedByProbe
            ))
        {
            GP_LOG_WARNING(LogRHI, "No hardware RHI backend was available; falling back to '{}'.", kNullRHIModuleName);
            result.module = nullModule;
            result.moduleName = kNullRHIModuleName;
            result.featureLevel = resolvedLevel;
            result.usedNullFallback = true;
            result.loadedByProbe = loadedByProbe;
            return result;
        }
    }

    GP_LOG_ERROR(LogRHI, "Failed to locate any supported IDynamicRHIModule backend.");
    return result;
}

DynamicRHI* platformCreateDynamicRHI(const RHIBootstrapDesc& desc, gp::modules::ModuleManager& moduleManager) noexcept
{
    const RHISelectionResult selection = selectDynamicRHIModule(desc, moduleManager);
    if (!selection.isValid())
    {
        return nullptr;
    }

    GP_LOG_INFO(
        LogRHI,
        "Creating DynamicRHI from module '{}' (featureLevel={}).",
        selection.moduleName,
        getFeatureLevelName(selection.featureLevel)
    );

    DynamicRHI* const rhi = selection.module->createRHI(selection.featureLevel);
    if (rhi != nullptr)
    {
        detail::setLoadedRHIModuleName(selection.moduleName);
    }
    else if (selection.loadedByProbe && !selection.moduleName.isEmpty())
    {
        (void)moduleManager.unloadModule(selection.moduleName, gp::modules::ModuleShutdownPhase::Exit);
    }
    return rhi;
}

gp::StringView getLoadedRHIModuleName() noexcept
{
    if (!g_loadedRHIModuleName.isEmpty())
    {
        return g_loadedRHIModuleName.view();
    }
    if (hasValidDynamicRHI())
    {
        return getModuleNameForInterfaceType(getInterfaceType());
    }
    return gp::StringView{};
}

gp::StringView
    getSelectedDynamicRHIModuleName(const bool cleanupIfProbed, gp::modules::ModuleManager& moduleManager) noexcept
{
    if (hasValidDynamicRHI())
    {
        return getLoadedRHIModuleName();
    }

    RHIBootstrapDesc probeDesc{};
    const RHISelectionResult selection = selectDynamicRHIModule(probeDesc, moduleManager);
    if (!selection.isValid())
    {
        return gp::StringView{};
    }

    const gp::StringView selectedName = selection.moduleName;
    if (cleanupIfProbed && selection.loadedByProbe && !selectedName.isEmpty())
    {
        (void)moduleManager.unloadModule(selectedName, gp::modules::ModuleShutdownPhase::Exit);
    }
    return selectedName;
}

bool initNullRHI(gp::modules::ModuleManager& moduleManager) noexcept
{
    if (hasValidDynamicRHI())
    {
        return true;
    }

    const bool wasAlreadyLoaded = moduleManager.isModuleLoaded(kNullRHIModuleName);
    gp::modules::ModuleLoadResult loadResult = gp::modules::ModuleLoadResult::NotFound;
    gp::modules::IModule* const rawModule = moduleManager.loadModule(kNullRHIModuleName, &loadResult);
    if (rawModule == nullptr || rawModule->getInterfaceTag() != IDynamicRHIModule::kInterfaceTag)
    {
        GP_LOG_ERROR(
            LogRHI,
            "Failed to load '{}' as IDynamicRHIModule (result={}).",
            kNullRHIModuleName,
            gp::modules::toStringView(loadResult)
        );
        if (rawModule != nullptr && !wasAlreadyLoaded)
        {
            (void)moduleManager.unloadModule(kNullRHIModuleName, gp::modules::ModuleShutdownPhase::Exit);
        }
        return false;
    }

    IDynamicRHIModule* const nullModule = static_cast<IDynamicRHIModule*>(rawModule);
    if (!nullModule->isSupported())
    {
        GP_LOG_ERROR(LogRHI, "Loaded '{}' module reported isSupported() == false.", kNullRHIModuleName);
        if (!wasAlreadyLoaded)
        {
            (void)moduleManager.unloadModule(kNullRHIModuleName, gp::modules::ModuleShutdownPhase::Exit);
        }
        return false;
    }

    DynamicRHI* const nullRHI = nullModule->createRHI();
    if (nullRHI == nullptr)
    {
        GP_LOG_ERROR(LogRHI, "NullDynamicRHIModule::createRHI() returned nullptr.");
        if (!wasAlreadyLoaded)
        {
            (void)moduleManager.unloadModule(kNullRHIModuleName, gp::modules::ModuleShutdownPhase::Exit);
        }
        return false;
    }

    detail::setLoadedRHIModuleName(kNullRHIModuleName);
    rhiInit(nullRHI);

    RHIGlobals& globals = getRHIGlobals();
    globals.usingNullRHI = true;
    globals.supportsTextureStreaming = false;
    return true;
}

bool rhiInit(const RHIBootstrapDesc& desc, gp::modules::ModuleManager& moduleManager) noexcept
{
    if (hasValidDynamicRHI())
    {
        return true;
    }

    RHIBootstrapDesc effectiveDesc = desc;
    if (effectiveDesc.applyCommandLineOverrides)
    {
        applyRHICommandLineOverrides(effectiveDesc);
    }

    // Latch command-list bypass mode from the command line before creating or initializing any backend.
    if (effectiveDesc.applyCommandLineOverrides && gp::CommandLine::isInitialized())
    {
        const gp::Optional<bool> optBypass = gp::CommandLine::tryGetBool("rhibypass");
        if (optBypass.hasValue())
        {
            command::getCommandListExecutor().setBypassRequested(*optBypass);
        }
    }
    command::getCommandListExecutor().latchBypass();

    if (!effectiveDesc.canRender || effectiveDesc.forceNullRHI)
    {
        return initNullRHI(moduleManager);
    }

    // Avoid re-applying command-line overrides a second time inside platformCreateDynamicRHI().
    RHIBootstrapDesc creationDesc = effectiveDesc;
    creationDesc.applyCommandLineOverrides = false;

    DynamicRHI* const createdRHI = platformCreateDynamicRHI(creationDesc, moduleManager);
    if (createdRHI == nullptr)
    {
        if (effectiveDesc.allowNullFallback && !effectiveDesc.forcedModuleByCommandLine)
        {
            return initNullRHI(moduleManager);
        }
        return false;
    }

    rhiInit(createdRHI);

    RHIGlobals& globals = getRHIGlobals();
    if (createdRHI->getInterfaceType() == InterfaceType::Null)
    {
        globals.usingNullRHI = true;
        globals.supportsTextureStreaming = false;
    }

    // Validate editor desktop feature level requirement on hardware backends.
    if (effectiveDesc.hasEditorToken && !globals.usingNullRHI &&
        static_cast<gp::Int32>(globals.maxSupportedFeatureLevel) < static_cast<gp::Int32>(FeatureLevel::SM5))
    {
        GP_LOG_ERROR(
            LogRHI,
            "Editor requires at least FeatureLevel::SM5, but backend '{}' only supports '{}'.",
            createdRHI->getName(),
            getFeatureLevelName(globals.maxSupportedFeatureLevel)
        );
        rhiExit();
        return false;
    }

    GP_LOG_INFO(
        LogRHI,
        "RHI initialized: backend='{}' module='{}' featureLevel='{}' adapter='{}'.",
        createdRHI->getName(),
        getLoadedRHIModuleName(),
        getFeatureLevelName(globals.maxSupportedFeatureLevel),
        globals.gpuInfo.adapterName.view()
    );
    return true;
}

bool rhiInit(const bool hasEditorToken) noexcept
{
    RHIBootstrapDesc desc{};
    desc.hasEditorToken = hasEditorToken;
    return rhiInit(desc, gp::modules::ModuleManager::get());
}

}   // namespace gp::rhi
