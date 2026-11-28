// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"
#include "RHIVulkan.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/VulkanShaderCode.hpp"      // IWYU pragma: keep
#include "shaders/VulkanShaderForward.hpp"   // IWYU pragma: keep
#include "shaders/VulkanShaderModule.hpp"
#include "threading/sync/Mutex.hpp"
#include "VulkanForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Backend half of every Vulkan shader: decoded header, SPIR-V and the per-layout module cache.
/// @details The RHI shader object (`shader::VertexShader`, ...) and this class are combined by `VulkanBaseShader`.
///          A shader does not own a single `VkShaderModule`: the same SPIR-V is turned into one module per pipeline
///          layout hash, plus a patched module when the pipeline rasterizes with several samples and the shader reads
///          input attachments. Modules are created on first use under `m_modulesMutex` and are shared by every
///          pipeline built against the same layout.
///
///          The entry point name is `main_<spirv byte size>_<spirv crc>` (both as 8 hex digits): the shader compiler
///          renames `main` this way so that pipeline caches can never confuse two modules with equal layouts.
/// @note Non-copyable and non-movable: the factory and the RHI shader both refer to it by address.
class GP_RHI_VULKAN_API VulkanShader
{
public:
    /// @brief Size of the buffer that holds an entry point name, including the terminator.
    static constexpr gp::USize kEntryPointNameCapacity = 24u;

    VulkanShader(const VulkanShader&) = delete;
    VulkanShader& operator=(const VulkanShader&) = delete;
    VulkanShader(VulkanShader&&) = delete;
    VulkanShader& operator=(VulkanShader&&) = delete;

    /// @brief Returns the module for the unpatched SPIR-V and `layoutHash`, creating it on first use.
    /// @param[in] layoutHash Hash of the pipeline layout the module will be bound with.
    /// @return Shared module; its handle is null when creation was skipped or failed.
    [[nodiscard]] gp::RefCountedPtr<VulkanShaderModule> getOrCreateHandle(gp::UInt32 layoutHash) noexcept;

    /// @brief Returns the module for a graphics pipeline, applying the input attachment patch when needed.
    /// @param[in] rasterizationSamples Sample count of the pipeline (`VkSampleCountFlagBits` value, 1 for none).
    /// @param[in] layoutHash Hash of the pipeline layout the module will be bound with.
    /// @return Shared module; null pointer when the SPIR-V could not be patched.
    [[nodiscard]] gp::RefCountedPtr<VulkanShaderModule>
        getOrCreateHandle(gp::UInt32 rasterizationSamples, gp::UInt32 layoutHash) noexcept;

    /// @brief Returns the layout-independent module used by bindless pipelines, creating it on first use.
    /// @return Shared module stored under key 0.
    [[nodiscard]] gp::RefCountedPtr<VulkanShaderModule> getOrCreateBindlessHandle() noexcept;

    /// @brief Drops every cached module; pipelines that still hold a reference keep theirs alive.
    void purgeShaderModules() noexcept;

    /// @brief Returns the number of cached modules.
    /// @return Module count.
    [[nodiscard]] gp::USize getNumShaderModules() const noexcept;

    /// @brief Reports whether a pipeline with `rasterizationSamples` samples needs the input attachment patch.
    /// @param[in] rasterizationSamples Sample count of the pipeline.
    /// @return True when the shader reads input attachments and the pipeline is multisampled.
    [[nodiscard]] bool needsSpirvInputAttachmentPatching(gp::UInt32 rasterizationSamples) const noexcept;

    /// @brief Returns the entry point name the shader compiler emitted.
    /// @return Null-terminated name; valid as long as the shader lives.
    [[nodiscard]] const char* getEntryPoint() const noexcept
    {
        return m_entryPoint;
    }

    /// @brief Returns the decoded descriptor metadata.
    /// @return Header.
    [[nodiscard]] const VulkanShaderHeader& getCodeHeader() const noexcept
    {
        return m_codeHeader;
    }

    /// @brief Returns the SPIR-V words of the main module.
    /// @return View of the words.
    [[nodiscard]] gp::ConstVectorView<gp::UInt32> getSpirv() const noexcept
    {
        return gp::ConstVectorView<gp::UInt32>(m_spirv);
    }

    /// @brief Returns the deduplication key (`blob size | blob crc << 32`).
    /// @return Key computed by the factory, or zero for shaders created outside a factory.
    [[nodiscard]] gp::UInt64 getShaderKey() const noexcept
    {
        return m_shaderKey;
    }

    /// @brief Returns the stage of the shader.
    /// @return Shader frequency.
    [[nodiscard]] shader::ShaderFrequency getVulkanFrequency() const noexcept
    {
        return m_frequency;
    }

    /// @brief Returns the shader debug name recorded by the compiler.
    /// @return Debug name, possibly empty.
    [[nodiscard]] const gp::String& getDebugName() const noexcept
    {
        return m_codeHeader.debugName;
    }

protected:
    /// @brief Takes ownership of decoded shader code.
    /// @param[in] device Logical device that creates the modules.
    /// @param[in] deferredDeletionQueue Queue used to release module handles; may be null.
    /// @param[in] frequency Shader stage.
    /// @param[in] header Decoded descriptor metadata.
    /// @param[in] spirv Main SPIR-V module.
    /// @param[in] shaderKey Deduplication key, or zero.
    /// @param[in] factory Factory that registered the shader; notified on destruction. May be null.
    VulkanShader(
        VulkanDevice& device,
        VulkanDeferredDeletionQueue* deferredDeletionQueue,
        shader::ShaderFrequency frequency,
        VulkanShaderHeader&& header,
        gp::Vector<gp::UInt32>&& spirv,
        gp::UInt64 shaderKey,
        VulkanShaderFactory* factory
    ) noexcept;

    /// @brief Unregisters the shader from its factory and drops the module cache.
    ~VulkanShader() noexcept;

private:
    /// @brief One cached module.
    struct ModuleEntry
    {
        /// @brief Layout hash, possibly combined with the patch marker.
        gp::UInt32 key{ 0u };

        /// @brief Cached module.
        gp::RefCountedPtr<VulkanShaderModule> module{};
    };

    /// @brief Finds a cached module.
    /// @param[in] key Cache key.
    /// @return Module, or a null pointer. Caller holds `m_modulesMutex`.
    [[nodiscard]] gp::RefCountedPtr<VulkanShaderModule> findModuleLocked(gp::UInt32 key) const noexcept;

    /// @brief Creates and caches a module.
    /// @param[in] spirv Words to build the module from.
    /// @param[in] key Cache key.
    /// @return New module. Caller holds `m_modulesMutex`.
    [[nodiscard]] gp::RefCountedPtr<VulkanShaderModule>
        createModuleLocked(gp::ConstVectorView<gp::UInt32> spirv, gp::UInt32 key) noexcept;

private:
    /// @brief Logical device that creates the modules.
    VulkanDevice& m_device;

    /// @brief Queue used to release module handles; may be null.
    VulkanDeferredDeletionQueue* m_deferredDeletionQueue{ nullptr };

    /// @brief Factory that registered the shader; may be null.
    VulkanShaderFactory* m_factory{ nullptr };

    /// @brief Decoded descriptor metadata.
    VulkanShaderHeader m_codeHeader{};

    /// @brief Main SPIR-V module.
    gp::Vector<gp::UInt32> m_spirv{};

    /// @brief Module cache keyed by layout hash.
    gp::Vector<ModuleEntry> m_modules{};

    /// @brief Guards `m_modules`.
    mutable gp::thread::Mutex m_modulesMutex{};

    /// @brief Deduplication key.
    gp::UInt64 m_shaderKey{ 0u };

    /// @brief Entry point name.
    char m_entryPoint[kEntryPointNameCapacity]{};

    /// @brief Shader stage.
    shader::ShaderFrequency m_frequency{ shader::ShaderFrequency::Vertex };
};

}   // namespace gp::rhi::vulkan
