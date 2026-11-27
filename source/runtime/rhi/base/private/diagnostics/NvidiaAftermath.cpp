// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "diagnostics/NvidiaAftermath.hpp"
#include "containers/arrays/Vector.hpp"
#include "CoreMinimal.hpp"
#include "hashing/Fnv1a.hpp"
#include "RHIDefinitions.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/UniqueLock.hpp"
#include <cstring>
#include <utility>

namespace gp::rhi::diagnostics::nvidia
{

namespace
{

struct AftermathState
{
    gp::thread::Mutex shaderMutex{};
    gp::Vector<RegisteredShaderEntry> shaders{};
    BackendCallbacks callbacks{};
    LateShaderAssociateFn lateAssociateCallback{ nullptr };
    FeatureFlags flags{ FeatureFlags::Default };
    bool enabled{ false };
    bool preDeviceInitialized{ false };
    bool deviceInitialized{ false };
};

AftermathState& getState() noexcept
{
    static AftermathState s_state{};
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
            return;
        }
    }

    m_resolvedText = "<root>";
    m_sizeInBytes = 7u;
    m_nodeId = 0u;
}

void setEnabled(const bool enabled) noexcept
{
    getState().enabled = enabled;
}

bool isEnabled() noexcept
{
    return getState().enabled;
}

bool areMarkersEnabled() noexcept
{
    const AftermathState& state = getState();
    return state.enabled && ((state.flags & FeatureFlags::EnableMarkers) != FeatureFlags::None);
}

void setFeatureFlags(const FeatureFlags flags) noexcept
{
    getState().flags = flags;
}

FeatureFlags getFeatureFlags() noexcept
{
    return getState().flags;
}

void registerBackendCallbacks(const BackendCallbacks& callbacks) noexcept
{
    getState().callbacks = callbacks;
}

bool initializeBeforeDeviceCreation() noexcept
{
    AftermathState& state = getState();
    if (!state.enabled || state.flags == FeatureFlags::None)
    {
        state.preDeviceInitialized = false;
        return false;
    }

    if (state.callbacks.initBeforeDevice != nullptr)
    {
        state.preDeviceInitialized = state.callbacks.initBeforeDevice(state.flags, state.callbacks.userData);
        if (!state.preDeviceInitialized)
        {
            state.enabled = false;
            GP_LOG_WARNING(::gp::LogRHI, "NVIDIA GPU crash dump pre-device setup failed; disabling hooks.");
            return false;
        }
    }
    else
    {
        state.preDeviceInitialized = true;
    }

    return true;
}

bool initializeDevice(void* const nativeDevice) noexcept
{
    AftermathState& state = getState();
    if (!state.enabled)
    {
        return false;
    }

    if (state.callbacks.initDevice != nullptr)
    {
        state.deviceInitialized = state.callbacks.initDevice(nativeDevice, state.flags, state.callbacks.userData);
        return state.deviceInitialized;
    }

    state.deviceInitialized = (nativeDevice != nullptr);
    return state.deviceInitialized;
}

ShaderHash registerShaderBinary(
    const void* const shaderData, const gp::UInt32 byteSize, const gp::StringView debugName
) noexcept
{
    if (shaderData == nullptr || byteSize == 0u)
    {
        return 0u;
    }

    ShaderHash hash =
        gp::hashing::Fnv1a64::hash(static_cast<const gp::UInt8*>(shaderData), static_cast<gp::USize>(byteSize));
    if (hash == 0u)
    {
        hash = 1u;
    }

    AftermathState& state = getState();
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(state.shaderMutex);
        bool updated = false;
        for (RegisteredShaderEntry& item: state.shaders)
        {
            if (item.hash == hash)
            {
                item.byteSize = byteSize;
                item.debugName = gp::String(debugName);
                updated = true;
                break;
            }
        }
        if (!updated)
        {
            RegisteredShaderEntry entry{};
            entry.hash = hash;
            entry.byteSize = byteSize;
            entry.debugName = gp::String(debugName);
            state.shaders.emplaceBack(std::move(entry));
        }
    }

    if (state.enabled && state.callbacks.registerShader != nullptr)
    {
        state.callbacks.registerShader(hash, shaderData, byteSize, state.callbacks.userData);
    }

    return hash;
}

bool deregisterShaderBinary(const ShaderHash hash) noexcept
{
    if (hash == 0u)
    {
        return false;
    }

    AftermathState& state = getState();
    bool removed = false;
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(state.shaderMutex);
        for (gp::USize idx = 0u; idx < state.shaders.size(); ++idx)
        {
            if (state.shaders[idx].hash == hash)
            {
                const gp::USize last = state.shaders.size() - 1u;
                if (idx != last)
                {
                    state.shaders[idx] = std::move(state.shaders[last]);
                }
                state.shaders.popBack();
                removed = true;
                break;
            }
        }
    }

    if (removed && state.enabled && state.callbacks.deregisterShader != nullptr)
    {
        state.callbacks.deregisterShader(hash, state.callbacks.userData);
    }

    return removed;
}

gp::USize getRegisteredShaderCount() noexcept
{
    AftermathState& state = getState();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(state.shaderMutex);
    return state.shaders.size();
}

bool findRegisteredShader(const ShaderHash hash, RegisteredShaderEntry& outEntry) noexcept
{
    AftermathState& state = getState();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(state.shaderMutex);
    for (const RegisteredShaderEntry& item: state.shaders)
    {
        if (item.hash == hash)
        {
            outEntry = item;
            return true;
        }
    }
    return false;
}

void setLateShaderAssociateCallback(const LateShaderAssociateFn callback) noexcept
{
    getState().lateAssociateCallback = callback;
}

CrashResult onGPUCrash(const gp::Float32 timeoutSeconds, const gp::StringView outputDirectory) noexcept
{
    AftermathState& state = getState();
    if (!state.enabled || (!state.preDeviceInitialized && !state.deviceInitialized))
    {
        return CrashResult::Disabled;
    }

    if (state.lateAssociateCallback != nullptr)
    {
        state.lateAssociateCallback();
    }

    if (state.callbacks.waitCrashDump != nullptr)
    {
        return state.callbacks.waitCrashDump(timeoutSeconds, outputDirectory, state.callbacks.userData);
    }

    return CrashResult::Dumped;
}

void reset() noexcept
{
    AftermathState& state = getState();
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(state.shaderMutex);
        state.shaders.clear();
    }
    state.callbacks = BackendCallbacks{};
    state.lateAssociateCallback = nullptr;
    state.flags = FeatureFlags::Default;
    state.enabled = false;
    state.preDeviceInitialized = false;
    state.deviceInitialized = false;
}

}   // namespace gp::rhi::diagnostics::nvidia
