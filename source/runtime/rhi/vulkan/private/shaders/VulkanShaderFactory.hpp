// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"
#include "RHIVulkan.hpp"
#include "shaders/Shader.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/VulkanShader.hpp"
#include "shaders/VulkanShaderCode.hpp"
#include "shaders/VulkanShaderForward.hpp"   // IWYU pragma: keep
#include "shaders/VulkanShaderSerialization.hpp"
#include "threading/sync/SharedLock.hpp"
#include "threading/sync/SharedMutex.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "VulkanCommon.hpp"
#include "VulkanForward.hpp"   // IWYU pragma: keep
#include <utility>

namespace gp::rhi::vulkan
{

/// @brief Deduplicating registry of every live Vulkan shader, keyed by the bytes of its serialized blob.
/// @details Identical blobs (same byte size and CRC-32) share one shader object, so pipelines built from separately
///          loaded copies of the same material hit the same module cache. The factory holds **non-owning** pointers:
///          a shader unregisters itself from its destructor through `onDeleteShader()`.
///
///          `createShader()` follows the double-checked pattern of the reference backend:
///          1. Look up the key under a shared lock.
///          2. On a miss, decode the blob with no lock held (decoding is the expensive part).
///          3. Take the exclusive lock, look again (another thread may have won the race), then insert.
///
///          A shader found in the registry may be waiting in the RHI deferred deletion queue with a zero reference
///          count. It is resurrected with `tryAddRefIfNotDeleting()`; once final deletion has started the entry is
///          treated as missing and a new shader is created, and the dying one only removes its own pointer.
/// @note Owned by whoever owns the device until the dynamic RHI exists; it must outlive every shader it created.
class GP_RHI_VULKAN_API VulkanShaderFactory
{
public:
    /// @brief Number of shader frequencies tracked separately.
    static constexpr gp::USize kNumFrequencies =
        static_cast<gp::USize>(shader::ShaderFrequency::WorkGraphComputeNode) + 1u;

    /// @brief Number of graphics pipeline stages (`shader_stage::Stage::NumGraphicsStages`).
    static constexpr gp::USize kNumGraphicsStages = static_cast<gp::USize>(shader_stage::Stage::NumGraphicsStages);

    /// @brief Creates an empty factory.
    VulkanShaderFactory() noexcept = default;

    /// @brief Destroys the factory; logs shaders that are still registered.
    ~VulkanShaderFactory() noexcept;

    VulkanShaderFactory(const VulkanShaderFactory&) = delete;
    VulkanShaderFactory& operator=(const VulkanShaderFactory&) = delete;
    VulkanShaderFactory(VulkanShaderFactory&&) = delete;
    VulkanShaderFactory& operator=(VulkanShaderFactory&&) = delete;

    /// @brief Computes the deduplication key of a serialized blob.
    /// @param[in] code Serialized blob.
    /// @return `byte size | (crc32 << 32)`.
    [[nodiscard]] static gp::UInt64 computeShaderKey(gp::ConstVectorView<gp::UInt8> code) noexcept;

    /// @brief Returns the shader for `code`, decoding and registering it on first use.
    /// @tparam InShaderType Concrete Vulkan shader type, for example `VulkanVertexShader`.
    /// @param[in] device Logical device that creates the modules.
    /// @param[in] deferredDeletionQueue Queue used to release module handles; may be null.
    /// @param[in] code Serialized blob.
    /// @return Shared shader, or a null pointer when the blob is malformed.
    template <typename InShaderType>
    [[nodiscard]] gp::RefCountedPtr<InShaderType> createShader(
        VulkanDevice& device,
        VulkanDeferredDeletionQueue* const deferredDeletionQueue,
        const gp::ConstVectorView<gp::UInt8> code
    ) noexcept
    {
        const gp::UInt64 shaderKey = computeShaderKey(code);
        {
            gp::thread::SharedLock<gp::thread::SharedMutex> readLock(m_mutex);
            gp::RefCountedPtr<InShaderType> found = findAndRetainLocked<InShaderType>(shaderKey);
            if (found.isValid())
            {
                return found;
            }
        }

        VulkanShaderCode decoded{};
        if (!parseVulkanShaderCode(code, decoded))
        {
            return gp::RefCountedPtr<InShaderType>();
        }

        gp::thread::UniqueLock<gp::thread::SharedMutex> writeLock(m_mutex);
        gp::RefCountedPtr<InShaderType> found = findAndRetainLocked<InShaderType>(shaderKey);
        if (found.isValid())
        {
            return found;
        }

        gp::RefCountedPtr<InShaderType> created(
            new InShaderType(device, deferredDeletionQueue, std::move(decoded), shaderKey, this)
        );
        insertLocked(InShaderType::kStaticFrequency, shaderKey, *created.get(), *created.get());
        return created;
    }

    /// @brief Finds a registered shader without creating one.
    /// @tparam InShaderType Concrete Vulkan shader type.
    /// @param[in] shaderKey Deduplication key; zero never matches.
    /// @return Shared shader, or a null pointer.
    template <typename InShaderType>
    [[nodiscard]] gp::RefCountedPtr<InShaderType> lookupShader(const gp::UInt64 shaderKey) const noexcept
    {
        if (shaderKey == 0u)
        {
            return gp::RefCountedPtr<InShaderType>();
        }
        gp::thread::SharedLock<gp::thread::SharedMutex> readLock(m_mutex);
        return findAndRetainLocked<InShaderType>(shaderKey);
    }

    /// @brief Resolves the shader keys of a graphics pipeline into backend shaders.
    /// @details Used when a pipeline is rebuilt from a cached key list. The returned pointers are not retained;
    ///          callers must already hold the shaders alive (for example through the pipeline they belong to).
    /// @param[in] shaderKeys One key per `shader_stage::Stage` graphics stage; zero means the stage is unused.
    /// @param[out] outShaders Receives one pointer per graphics stage, null when missing or unused.
    void lookupGfxShaders(
        const gp::Array<gp::UInt64, kNumGraphicsStages>& shaderKeys,
        gp::Array<VulkanShader*, kNumGraphicsStages>& outShaders
    ) const noexcept;

    /// @brief Unregisters a shader that is being destroyed.
    /// @note Called from `VulkanShader::~VulkanShader()`; removes the entry only when it still points at `shader`.
    /// @param[in] shader Shader being destroyed.
    void onDeleteShader(const VulkanShader& shader) noexcept;

    /// @brief Returns the number of registered shaders of one frequency.
    /// @param[in] frequency Shader stage.
    /// @return Registered shader count.
    [[nodiscard]] gp::USize getNumShaders(shader::ShaderFrequency frequency) const noexcept;

private:
    /// @brief One registered shader.
    struct Entry
    {
        /// @brief Deduplication key; entries are sorted by it.
        gp::UInt64 key{ 0u };

        /// @brief Backend half of the shader.
        VulkanShader* vulkanShader{ nullptr };

        /// @brief RHI half of the shader, used for reference counting.
        shader::Shader* rhiShader{ nullptr };
    };

    /// @brief Finds the entry of `shaderKey`.
    /// @param[in] frequency Shader stage.
    /// @param[in] shaderKey Deduplication key.
    /// @return Entry, or null. Caller holds `m_mutex`.
    [[nodiscard]] const Entry* findLocked(shader::ShaderFrequency frequency, gp::UInt64 shaderKey) const noexcept;

    /// @brief Inserts a shader, replacing a dying entry with the same key.
    /// @param[in] frequency Shader stage.
    /// @param[in] shaderKey Deduplication key.
    /// @param[in] vulkanShader Backend half.
    /// @param[in] rhiShader RHI half.
    /// @note Caller holds `m_mutex` exclusively.
    void insertLocked(
        shader::ShaderFrequency frequency, gp::UInt64 shaderKey, VulkanShader& vulkanShader, shader::Shader& rhiShader
    ) noexcept;

    /// @brief Finds a shader and takes a reference when it is not being destroyed.
    /// @tparam InShaderType Concrete Vulkan shader type.
    /// @param[in] shaderKey Deduplication key.
    /// @return Shared shader, or a null pointer. Caller holds `m_mutex`.
    template <typename InShaderType>
    [[nodiscard]] gp::RefCountedPtr<InShaderType> findAndRetainLocked(const gp::UInt64 shaderKey) const noexcept
    {
        const Entry* const entry = findLocked(InShaderType::kStaticFrequency, shaderKey);
        if ((entry == nullptr) || !entry->rhiShader->tryAddRefIfNotDeleting())
        {
            return gp::RefCountedPtr<InShaderType>();
        }

        // The extra reference taken above keeps the shader alive while the smart pointer takes its own.
        gp::RefCountedPtr<InShaderType> retained(static_cast<InShaderType*>(entry->vulkanShader));
        entry->rhiShader->release();
        return retained;
    }

private:
    /// @brief Guards `m_shaders`.
    mutable gp::thread::SharedMutex m_mutex{};

    /// @brief Registered shaders per frequency, sorted by key.
    gp::Array<gp::Vector<Entry>, kNumFrequencies> m_shaders{};
};

}   // namespace gp::rhi::vulkan
