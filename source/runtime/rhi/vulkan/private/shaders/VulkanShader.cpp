// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "shaders/VulkanShader.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/VulkanDevice.hpp"
#include "hashing/HashCombine.hpp"
#include "logging/LogMacros.hpp"
#include "shaders/VulkanShaderFactory.hpp"
#include "shaders/VulkanSpirvPatching.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <utility>

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Cache key of the bindless module, which does not depend on a pipeline layout.
constexpr gp::UInt32 kBindlessModuleKey = 0u;

/// @brief Value folded into a layout hash to key the input-attachment-patched module.
constexpr gp::UInt32 kInputAttachmentPatchMarker = 1u;

/// @brief Writes `value` as 8 lowercase hex digits.
/// @param[in] value Value to format.
/// @param[out] outDigits Receives exactly 8 characters (no terminator).
void writeHex8(const gp::UInt32 value, char* const outDigits) noexcept
{
    constexpr char kDigits[] = "0123456789abcdef";
    for (gp::UInt32 i = 0u; i < 8u; ++i)
    {
        const gp::UInt32 nibble = (value >> ((7u - i) * 4u)) & 0xFu;
        outDigits[i] = kDigits[nibble];
    }
}

}   // namespace

VulkanShader::VulkanShader(
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* const deferredDeletionQueue,
    const shader::ShaderFrequency frequency,
    VulkanShaderHeader&& header,
    gp::Vector<gp::UInt32>&& spirv,
    const gp::UInt64 shaderKey,
    VulkanShaderFactory* const factory
) noexcept
    : m_device(device)
    , m_deferredDeletionQueue(deferredDeletionQueue)
    , m_factory(factory)
    , m_codeHeader(std::move(header))
    , m_spirv(std::move(spirv))
    , m_shaderKey(shaderKey)
    , m_frequency(frequency)
{
    // "main_" + 8 hex digits + "_" + 8 hex digits + terminator = 23 characters.
    const gp::UInt32 spirvSizeInBytes = static_cast<gp::UInt32>(m_spirv.size() * sizeof(gp::UInt32));
    m_entryPoint[0] = 'm';
    m_entryPoint[1] = 'a';
    m_entryPoint[2] = 'i';
    m_entryPoint[3] = 'n';
    m_entryPoint[4] = '_';
    writeHex8(spirvSizeInBytes, m_entryPoint + 5);
    m_entryPoint[13] = '_';
    writeHex8(m_codeHeader.spirvCRC, m_entryPoint + 14);
    m_entryPoint[22] = '\0';
}

VulkanShader::~VulkanShader() noexcept
{
    if (m_factory != nullptr)
    {
        m_factory->onDeleteShader(*this);
    }
    purgeShaderModules();
}

gp::RefCountedPtr<VulkanShaderModule> VulkanShader::getOrCreateHandle(const gp::UInt32 layoutHash) noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_modulesMutex);
    gp::RefCountedPtr<VulkanShaderModule> found = findModuleLocked(layoutHash);
    if (found.isValid())
    {
        return found;
    }
    return createModuleLocked(getSpirv(), layoutHash);
}

gp::RefCountedPtr<VulkanShaderModule>
    VulkanShader::getOrCreateHandle(const gp::UInt32 rasterizationSamples, gp::UInt32 layoutHash) noexcept
{
    const bool needsPatching = needsSpirvInputAttachmentPatching(rasterizationSamples);
    if (needsPatching)
    {
        layoutHash = gp::hashing::hashCombineFast(layoutHash, kInputAttachmentPatchMarker);
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_modulesMutex);
    gp::RefCountedPtr<VulkanShaderModule> found = findModuleLocked(layoutHash);
    if (found.isValid())
    {
        return found;
    }

    if (!needsPatching)
    {
        return createModuleLocked(getSpirv(), layoutHash);
    }

    gp::Vector<gp::UInt32> patched{};
    if (!patchSpirvInputAttachments(getSpirv(), patched))
    {
        GP_LOG_ERROR(
            LogVulkanRHI,
            "Failed to patch the input attachments of shader '{}' for {} samples.",
            m_codeHeader.debugName.data(),
            rasterizationSamples
        );
        return gp::RefCountedPtr<VulkanShaderModule>();
    }
    return createModuleLocked(gp::ConstVectorView<gp::UInt32>(patched), layoutHash);
}

gp::RefCountedPtr<VulkanShaderModule> VulkanShader::getOrCreateBindlessHandle() noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_modulesMutex);
    gp::RefCountedPtr<VulkanShaderModule> found = findModuleLocked(kBindlessModuleKey);
    if (found.isValid())
    {
        return found;
    }

    gp::RefCountedPtr<VulkanShaderModule> module = createModuleLocked(getSpirv(), kBindlessModuleKey);
    if (module->isValid() && !m_codeHeader.debugName.isEmpty())
    {
        m_device.getDebugMarkers().setObjectName(
            m_device.getHandle(), VK_OBJECT_TYPE_SHADER_MODULE, module->getHandle(), m_codeHeader.debugName.data()
        );
    }
    return module;
}

void VulkanShader::purgeShaderModules() noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_modulesMutex);
    m_modules.clear();
}

gp::USize VulkanShader::getNumShaderModules() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_modulesMutex);
    return static_cast<gp::USize>(m_modules.size());
}

bool VulkanShader::needsSpirvInputAttachmentPatching(const gp::UInt32 rasterizationSamples) const noexcept
{
    return (rasterizationSamples > 1u) && (m_codeHeader.inputAttachmentsMask != 0u);
}

gp::RefCountedPtr<VulkanShaderModule> VulkanShader::findModuleLocked(const gp::UInt32 key) const noexcept
{
    for (const ModuleEntry& entry: m_modules)
    {
        if (entry.key == key)
        {
            return entry.module;
        }
    }
    return gp::RefCountedPtr<VulkanShaderModule>();
}

gp::RefCountedPtr<VulkanShaderModule>
    VulkanShader::createModuleLocked(const gp::ConstVectorView<gp::UInt32> spirv, const gp::UInt32 key) noexcept
{
    gp::RefCountedPtr<VulkanShaderModule> module(new VulkanShaderModule(m_device, spirv, m_deferredDeletionQueue));
    m_modules.emplaceBack(ModuleEntry{ key, module });
    return module;
}

}   // namespace gp::rhi::vulkan
