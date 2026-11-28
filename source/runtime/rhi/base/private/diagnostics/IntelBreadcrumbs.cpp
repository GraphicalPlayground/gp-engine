// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "diagnostics/IntelBreadcrumbs.hpp"
#include "CoreMinimal.hpp"
#include "RHIDefinitions.hpp"
#include <cstring>

namespace gp::rhi::diagnostics::intel
{

namespace
{

struct IntelBreadcrumbState
{
    BackendCallbacks callbacks{};
    CrashDumpFlags flags{ CrashDumpFlags::MiniDump | CrashDumpFlags::StateDump };
    bool enabled{ false };
    bool initialized{ false };
};

IntelBreadcrumbState& getState() noexcept
{
    static IntelBreadcrumbState s_state{};
    return s_state;
}

}   // namespace

Marker::Marker(const BreadcrumbNode* const node) noexcept
{
    if (node != nullptr && node != BreadcrumbNode::getSentinel())
    {
        const char* const str = node->getDebugStr(m_scratch);
        if (str != nullptr && str[0] != '\0')
        {
            m_resolvedText = str;
            m_sizeInBytes = static_cast<gp::UInt32>(std::strlen(str) + 1u);
            m_nodeId = node->id;
            m_isNodeMarker = true;
            return;
        }
    }

    m_resolvedText = "<root>";
    m_sizeInBytes = 7u;
    m_nodeId = 0u;
    m_isNodeMarker = false;
}

void setEnabled(const bool enabled) noexcept
{
    getState().enabled = enabled;
}

bool isEnabled() noexcept
{
    return getState().enabled;
}

void setCrashDumpFlags(const CrashDumpFlags flags) noexcept
{
    getState().flags = flags;
}

CrashDumpFlags getCrashDumpFlags() noexcept
{
    return getState().flags;
}

void registerBackendCallbacks(const BackendCallbacks& callbacks) noexcept
{
    getState().callbacks = callbacks;
}

bool initializeBeforeDeviceCreation() noexcept
{
    IntelBreadcrumbState& state = getState();
    if (!state.enabled || state.flags == CrashDumpFlags::None)
    {
        state.initialized = false;
        return false;
    }

    if (state.callbacks.initDriver != nullptr)
    {
        state.initialized = state.callbacks.initDriver(state.flags, state.callbacks.userData);
        if (!state.initialized)
        {
            state.enabled = false;
            GP_LOG_WARNING(::gp::LogRHI, "Intel GPU crash dump driver initialization failed; disabling hooks.");
            return false;
        }
    }
    else
    {
        state.initialized = true;
    }

    return true;
}

bool onGPUCrash(const gp::StringView outputDirectory) noexcept
{
    IntelBreadcrumbState& state = getState();
    if (!state.enabled || !state.initialized)
    {
        return false;
    }

    if (state.callbacks.writeDump != nullptr)
    {
        return state.callbacks.writeDump(outputDirectory, state.flags, state.callbacks.userData);
    }

    return true;
}

void reset() noexcept
{
    getState() = IntelBreadcrumbState{};
}

}   // namespace gp::rhi::diagnostics::intel
