// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "Application.hpp"
#include "containers/strings/FixedString.hpp"
#include "device/DynamicRHI.hpp"
#include "device/RHIGlobals.hpp"
#include "device/RHIModuleDiscovery.hpp"
#include "logging/LogMacros.hpp"
#include "modules/ModuleEnums.hpp"
#include "modules/ModuleManager.hpp"

namespace gp
{

rhi::DynamicRHI* Application::getDynamicRHI() noexcept
{
    return rhi::getDynamicRHI();
}

bool Application::isRHIInitialized() noexcept
{
    return rhi::hasValidDynamicRHI() && rhi::getRHIGlobals().isRHIInitialized;
}

gp::StringView Application::getLoadedRHIModuleName() noexcept
{
    return rhi::getLoadedRHIModuleName();
}

bool Application::initializeRHIInternal() noexcept
{
    if (!m_desc.enableRHI)
    {
        m_ownsRHIInitialization = false;
        return true;
    }

    // Preserve a pre-initialized DynamicRHI if the host or test harness initialized RHI before Application::run().
    if (rhi::hasValidDynamicRHI())
    {
        m_ownsRHIInitialization = false;
        return true;
    }

    if (!rhi::rhiInit(m_desc.rhi, modules::ModuleManager::get())) [[unlikely]]
    {
        GP_LOG_ERROR(LogApplication, "Failed to initialize RHI backend during Application bootstrap.");
        m_ownsRHIInitialization = false;
        return false;
    }

    rhi::rhiPostInit();
    m_ownsRHIInitialization = true;

    rhi::DynamicRHI* const activeRHI = rhi::getDynamicRHI();
    GP_LOG_INFO(
        LogApplication,
        "RHI Ready: backend='{}' module='{}' featureLevel='{}' nullRHI={}.",
        (activeRHI != nullptr) ? activeRHI->getName() : gp::StringView("None"),
        rhi::getLoadedRHIModuleName(),
        rhi::getFeatureLevelName(rhi::getRHIGlobals().maxSupportedFeatureLevel),
        rhi::getRHIGlobals().usingNullRHI
    );
    return true;
}

void Application::shutdownRHIInternal() noexcept
{
    if (!m_ownsRHIInitialization)
    {
        return;
    }

    gp::FixedString<127> activeModuleName{};
    activeModuleName.assign(rhi::getLoadedRHIModuleName());
    GP_LOG_INFO(LogApplication, "Shutting down RHI backend '{}'...", activeModuleName.view());
    rhi::rhiExit();
    m_ownsRHIInitialization = false;

    if (!m_desc.enableModuleSystem && m_desc.unloadModulesOnShutdown && !activeModuleName.isEmpty())
    {
        (void)modules::ModuleManager::get().unloadModule(activeModuleName.view(), modules::ModuleShutdownPhase::Exit);
    }
}

}   // namespace gp
