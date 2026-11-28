// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/Resource.hpp"
#include "core/ResourceForward.hpp"
#include "CoreMinimal.hpp"
#include "memory/allocators/PagedLinearAllocator.hpp"
#include "memory/Memory.hpp"
#include "pipeline/PipelineForward.hpp"
#include "pipeline/SamplerState.hpp"
#include "resources/Texture.hpp"
#include "resources/UniformBuffer.hpp"
#include "resources/UniformBufferEnums.hpp"
#include "RHIBase.hpp"
#include "RHIDefinitions.hpp"          // IWYU pragma: keep
#include "shaders/ShaderForward.hpp"   // IWYU pragma: keep
#include "views/ShaderResourceView.hpp"
#include "views/UnorderedAccessView.hpp"

namespace gp::rhi::shader
{

/// @brief Packed 8-byte constant-data parameter span referencing a slice of the batched parameter byte stream.
struct ShaderParameter
{
    /// @brief Shader constant buffer slot index.
    gp::UInt16 bufferIndex{ 0u };

    /// @brief Destination byte offset inside the shader constant buffer.
    gp::UInt16 baseIndex{ 0u };

    /// @brief Byte offset into the companion `parametersData` blob.
    gp::UInt16 byteOffset{ 0u };

    /// @brief Number of bytes copied from `parametersData` into the constant buffer.
    gp::UInt16 byteSize{ 0u };

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Parameter span to compare against.
    /// @return True when all four fields match.
    [[nodiscard]] constexpr bool operator==(const ShaderParameter& rhs) const noexcept = default;
};

static_assert(sizeof(ShaderParameter) == 8u, "ShaderParameter must stay packed to 8 bytes.");

/// @brief Resource binding entry inside a batched shader parameter stream.
struct ShaderParameterResource
{
    /// @brief Concrete RHI resource kind bound at `index`.
    enum class Type : gp::UInt8
    {
        Texture = 0,
        ResourceView,
        UnorderedAccessView,
        Sampler,
        UniformBuffer,
        ResourceCollection,
        DescriptorRange
    };

    /// @brief Bound RHI resource pointer (may be `nullptr` to clear a slot).
    resource::Resource* resource{ nullptr };

    /// @brief Hardware binding slot index or bindless byte offset.
    gp::UInt16 index{ 0u };

    /// @brief Resource kind identifying how `resource` is cast by the backend.
    Type type{ Type::Texture };

    /// @brief Constructs an empty texture binding entry.
    constexpr ShaderParameterResource() noexcept = default;

    /// @brief Constructs a resource binding entry with explicit kind, pointer, and slot index.
    /// @param[in] inType Resource kind tag.
    /// @param[in] inResource Bound RHI resource pointer.
    /// @param[in] inIndex Hardware slot index.
    constexpr ShaderParameterResource(
        const Type inType, resource::Resource* const inResource, const gp::UInt16 inIndex
    ) noexcept
        : resource(inResource)
        , index(inIndex)
        , type(inType)
    {}

    /// @brief Constructs a texture resource binding entry.
    /// @param[in] inTexture Texture pointer.
    /// @param[in] inIndex Hardware texture slot index.
    ShaderParameterResource(resource::Texture* const inTexture, const gp::UInt16 inIndex) noexcept
        : resource(inTexture)
        , index(inIndex)
        , type(Type::Texture)
    {}

    /// @brief Constructs an SRV resource binding entry.
    /// @param[in] inSRV Shader resource view pointer.
    /// @param[in] inIndex Hardware SRV slot index.
    ShaderParameterResource(view::ShaderResourceView* const inSRV, const gp::UInt16 inIndex) noexcept
        : resource(inSRV)
        , index(inIndex)
        , type(Type::ResourceView)
    {}

    /// @brief Constructs a UAV resource binding entry.
    /// @param[in] inUAV Unordered access view pointer.
    /// @param[in] inIndex Hardware UAV slot index.
    ShaderParameterResource(view::UnorderedAccessView* const inUAV, const gp::UInt16 inIndex) noexcept
        : resource(inUAV)
        , index(inIndex)
        , type(Type::UnorderedAccessView)
    {}

    /// @brief Constructs a sampler state resource binding entry.
    /// @param[in] inSampler Sampler state pointer.
    /// @param[in] inIndex Hardware sampler slot index.
    ShaderParameterResource(pipeline::SamplerState* const inSampler, const gp::UInt16 inIndex) noexcept
        : resource(inSampler)
        , index(inIndex)
        , type(Type::Sampler)
    {}

    /// @brief Constructs a uniform buffer resource binding entry.
    /// @param[in] inUniformBuffer Uniform buffer pointer.
    /// @param[in] inIndex Hardware constant/uniform buffer slot index.
    ShaderParameterResource(resource::UniformBuffer* const inUniformBuffer, const gp::UInt16 inIndex) noexcept
        : resource(inUniformBuffer)
        , index(inIndex)
        , type(Type::UniformBuffer)
    {}

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Entry to compare against.
    /// @return True when resource pointer, index, and type match.
    [[nodiscard]] constexpr bool operator==(const ShaderParameterResource& rhs) const noexcept = default;
};

/// @brief Slot unbind entry clearing an active shader SRV or UAV binding.
struct ShaderParameterUnbind
{
    /// @brief Resource view kind being unbound.
    enum class Type : gp::UInt8
    {
        ResourceView = 0,
        UnorderedAccessView
    };

    /// @brief Hardware binding slot index to clear.
    gp::UInt16 index{ 0u };

    /// @brief Whether the slot holds an SRV or UAV.
    Type type{ Type::ResourceView };

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Unbind entry to compare against.
    /// @return True when index and type match.
    [[nodiscard]] constexpr bool operator==(const ShaderParameterUnbind& rhs) const noexcept = default;
};

/// @brief Page-backed linear allocator storing batched shader parameter arrays without per-draw heap churn.
/// @details Owns four `gp::memory::PagedLinearAllocator` arenas so `parametersData`, `parameters`,
///          `resourceParameters`, and `bindlessParameters` can grow contiguously in place while space remains on the
///          active page. All allocations remain valid until the owning command list resets or destroys the allocator.
class GP_RHI_API BatchedShaderParametersAllocator
{
public:
    /// @brief Intrusive link pointer chaining allocators owned by a `CommandListBase`.
    BatchedShaderParametersAllocator* next{ nullptr };

public:
    /// @brief Constructs an allocator backed by the shared command-list `PagePool`.
    /// @param[in] pagePool Shared lock-free page pool supplying arena pages (may be `nullptr` for malloc fallback).
    explicit BatchedShaderParametersAllocator(gp::memory::PagePool* const pagePool = nullptr) noexcept
        : m_parametersDataArena(pagePool, nullptr, true, "RHI.BatchedShaderParams.Data")
        , m_parametersArena(pagePool, nullptr, true, "RHI.BatchedShaderParams.Params")
        , m_resourceParametersArena(pagePool, nullptr, true, "RHI.BatchedShaderParams.Resources")
        , m_bindlessParametersArena(pagePool, nullptr, true, "RHI.BatchedShaderParams.Bindless")
    {}

    BatchedShaderParametersAllocator(const BatchedShaderParametersAllocator&) = delete;
    BatchedShaderParametersAllocator& operator=(const BatchedShaderParametersAllocator&) = delete;
    BatchedShaderParametersAllocator(BatchedShaderParametersAllocator&&) = delete;
    BatchedShaderParametersAllocator& operator=(BatchedShaderParametersAllocator&&) = delete;
    ~BatchedShaderParametersAllocator() noexcept = default;

    /// @brief Appends raw constant bytes into the contiguous `view`, reallocating in the data arena on page overflow.
    /// @param[in,out] view Active byte view updated to cover the newly appended bytes.
    /// @param[in] srcData Source bytes to copy.
    /// @param[in] numBytes Number of bytes to append.
    /// @return Byte offset of the newly appended slice relative to `view.data()`.
    [[nodiscard]] gp::UInt32
        appendBytes(gp::VectorView<gp::UInt8>& view, const void* const srcData, const gp::UInt32 numBytes) noexcept
    {
        const gp::UInt32 byteOffset = static_cast<gp::UInt32>(view.size());
        if (numBytes == 0u)
        {
            return byteOffset;
        }

        const gp::USize oldSize = view.size();
        const gp::USize newSize = oldSize + static_cast<gp::USize>(numBytes);
        gp::UInt8* const newPtr =
            static_cast<gp::UInt8*>(m_parametersDataArena.reallocate(view.data(), oldSize, newSize, 1u));
        GP_CHECKF(newPtr != nullptr, "Failed to allocate batched shader parameter data.");
        gp::memory::copyMemory(newPtr + oldSize, srcData, static_cast<gp::USize>(numBytes));
        view = gp::VectorView<gp::UInt8>(newPtr, newSize);
        return byteOffset;
    }

    /// @brief Appends a `ShaderParameter` entry into `view` inside the parameter arena.
    /// @param[in,out] view Active parameter span view.
    /// @param[in] entry Parameter entry to append.
    void appendParameter(gp::VectorView<ShaderParameter>& view, const ShaderParameter& entry) noexcept
    {
        appendElement(m_parametersArena, view, entry);
    }

    /// @brief Appends a non-bindless `ShaderParameterResource` into `view` inside the resource arena.
    /// @param[in,out] view Active resource binding view.
    /// @param[in] entry Resource binding entry to append.
    void appendResourceParameter(
        gp::VectorView<ShaderParameterResource>& view, const ShaderParameterResource& entry
    ) noexcept
    {
        appendElement(m_resourceParametersArena, view, entry);
    }

    /// @brief Appends a bindless `ShaderParameterResource` into `view` inside the bindless arena.
    /// @param[in,out] view Active bindless resource binding view.
    /// @param[in] entry Bindless resource binding entry to append.
    void appendBindlessParameter(
        gp::VectorView<ShaderParameterResource>& view, const ShaderParameterResource& entry
    ) noexcept
    {
        appendElement(m_bindlessParametersArena, view, entry);
    }

    /// @brief Releases all leased pages across all four arenas back to the backing page pool.
    void reset() noexcept
    {
        m_parametersDataArena.reset();
        m_parametersArena.reset();
        m_resourceParametersArena.reset();
        m_bindlessParametersArena.reset();
    }

private:
    /// @brief Grows a typed view by one element inside `arena` using O(1) in-place reallocation when space permits.
    /// @tparam InElementType Trivially copyable element type stored in `arena`.
    /// @param[in,out] arena Target paged linear allocator.
    /// @param[in,out] view Active element span updated in place.
    /// @param[in] entry Element value to append.
    template <typename InElementType>
    static void appendElement(
        gp::memory::PagedLinearAllocator& arena, gp::VectorView<InElementType>& view, const InElementType& entry
    ) noexcept
    {
        const gp::USize oldCount = view.size();
        const gp::USize oldBytes = oldCount * sizeof(InElementType);
        const gp::USize newBytes = (oldCount + 1u) * sizeof(InElementType);
        InElementType* const newPtr = static_cast<InElementType*>(
            arena.reallocate(view.data(), oldBytes, newBytes, static_cast<gp::UInt32>(alignof(InElementType)))
        );
        GP_CHECKF(newPtr != nullptr, "Failed to allocate batched shader parameter entry.");
        newPtr[oldCount] = entry;
        view = gp::VectorView<InElementType>(newPtr, oldCount + 1u);
    }

private:
    gp::memory::PagedLinearAllocator m_parametersDataArena;
    gp::memory::PagedLinearAllocator m_parametersArena;
    gp::memory::PagedLinearAllocator m_resourceParametersArena;
    gp::memory::PagedLinearAllocator m_bindlessParametersArena;
};

/// @brief Batched shader parameter payload accumulated before dispatching one RHI `setShaderParameters()` call.
struct BatchedShaderParameters
{
    /// @brief Backing allocator owning the memory referenced by the four spans below.
    BatchedShaderParametersAllocator* allocator{ nullptr };

    /// @brief Contiguous raw constant bytes indexed by `parameters`.
    gp::VectorView<gp::UInt8> parametersData{};

    /// @brief Constant buffer byte-range descriptors.
    gp::VectorView<ShaderParameter> parameters{};

    /// @brief Non-bindless shader resource bindings (textures, SRVs, UAVs, samplers, UBs).
    gp::VectorView<ShaderParameterResource> resourceParameters{};

    /// @brief Bindless shader resource bindings.
    gp::VectorView<ShaderParameterResource> bindlessParameters{};

    /// @brief Constructs a batched parameter builder bound to `inAllocator`.
    /// @param[in] inAllocator Backing parameter allocator.
    explicit BatchedShaderParameters(BatchedShaderParametersAllocator& inAllocator) noexcept
        : allocator(&inAllocator)
    {}

    /// @brief Reports whether any constant data, resource bindings, or bindless bindings have been recorded.
    /// @return True when at least one parameter or resource entry is present.
    [[nodiscard]] GP_FORCEINLINE_HINT bool hasParameters() const noexcept
    {
        return !parametersData.isEmpty() || !parameters.isEmpty() || !resourceParameters.isEmpty() ||
               !bindlessParameters.isEmpty();
    }

    /// @brief Detaches the current views without freeing arena pages so another batch can be recorded cleanly.
    GP_FORCEINLINE_HINT void reset() noexcept
    {
        parametersData = gp::VectorView<gp::UInt8>{};
        parameters = gp::VectorView<ShaderParameter>{};
        resourceParameters = gp::VectorView<ShaderParameterResource>{};
        bindlessParameters = gp::VectorView<ShaderParameterResource>{};
    }

    /// @brief Records a loose constant-buffer byte range into this batch.
    /// @param[in] bufferIndex Constant buffer register index.
    /// @param[in] baseIndex Byte offset inside the constant buffer.
    /// @param[in] numBytes Byte length of `newValue`.
    /// @param[in] newValue Pointer to source constant bytes.
    void setShaderParameter(
        const gp::UInt32 bufferIndex, const gp::UInt32 baseIndex, const gp::UInt32 numBytes, const void* const newValue
    ) noexcept
    {
        GP_CHECKF(allocator != nullptr, "BatchedShaderParameters requires a valid allocator.");
        if (numBytes == 0u)
        {
            return;
        }
        const gp::UInt32 byteOffset = allocator->appendBytes(parametersData, newValue, numBytes);
        GP_CHECKF(bufferIndex <= 0xFFFFu, "Shader parameter bufferIndex exceeds UInt16 range.");
        GP_CHECKF(baseIndex <= 0xFFFFu, "Shader parameter baseIndex exceeds UInt16 range.");
        GP_CHECKF(byteOffset <= 0xFFFFu, "Shader parameter byteOffset exceeds UInt16 range.");
        GP_CHECKF(numBytes <= 0xFFFFu, "Shader parameter numBytes exceeds UInt16 range.");

        allocator->appendParameter(
            parameters,
            ShaderParameter{ static_cast<gp::UInt16>(bufferIndex),
                             static_cast<gp::UInt16>(baseIndex),
                             static_cast<gp::UInt16>(byteOffset),
                             static_cast<gp::UInt16>(numBytes) }
        );
    }

    /// @brief Finalizes the recorded parameter stream before dispatch (no-op hook matching allocator lifetime).
    GP_FORCEINLINE_HINT void finish() noexcept
    {}

    /// @brief Records a uniform buffer binding at `baseIndex`.
    /// @param[in] baseIndex Hardware uniform buffer slot index.
    /// @param[in] uniformBuffer Uniform buffer resource pointer.
    void setShaderUniformBuffer(const gp::UInt32 baseIndex, resource::UniformBuffer* const uniformBuffer) noexcept
    {
        GP_CHECKF(allocator != nullptr, "BatchedShaderParameters requires a valid allocator.");
        GP_CHECKF(baseIndex <= 0xFFFFu, "Shader parameter baseIndex exceeds UInt16 range.");
        allocator->appendResourceParameter(
            resourceParameters, ShaderParameterResource(uniformBuffer, static_cast<gp::UInt16>(baseIndex))
        );
    }

    /// @brief Records a texture binding at `textureIndex`.
    /// @param[in] textureIndex Hardware texture slot index.
    /// @param[in] texture Texture resource pointer.
    void setShaderTexture(const gp::UInt32 textureIndex, resource::Texture* const texture) noexcept
    {
        GP_CHECKF(allocator != nullptr, "BatchedShaderParameters requires a valid allocator.");
        GP_CHECKF(textureIndex <= 0xFFFFu, "Shader parameter textureIndex exceeds UInt16 range.");
        allocator->appendResourceParameter(
            resourceParameters, ShaderParameterResource(texture, static_cast<gp::UInt16>(textureIndex))
        );
    }

    /// @brief Records an SRV binding at `baseIndex`.
    /// @param[in] baseIndex Hardware SRV slot index.
    /// @param[in] srv Shader resource view pointer.
    void setShaderResourceViewParameter(const gp::UInt32 baseIndex, view::ShaderResourceView* const srv) noexcept
    {
        GP_CHECKF(allocator != nullptr, "BatchedShaderParameters requires a valid allocator.");
        GP_CHECKF(baseIndex <= 0xFFFFu, "Shader parameter baseIndex exceeds UInt16 range.");
        allocator->appendResourceParameter(
            resourceParameters, ShaderParameterResource(srv, static_cast<gp::UInt16>(baseIndex))
        );
    }

    /// @brief Records a sampler state binding at `samplerIndex`.
    /// @param[in] samplerIndex Hardware sampler slot index.
    /// @param[in] sampler Sampler state resource pointer.
    void setShaderSampler(const gp::UInt32 samplerIndex, pipeline::SamplerState* const sampler) noexcept
    {
        GP_CHECKF(allocator != nullptr, "BatchedShaderParameters requires a valid allocator.");
        GP_CHECKF(samplerIndex <= 0xFFFFu, "Shader parameter samplerIndex exceeds UInt16 range.");
        allocator->appendResourceParameter(
            resourceParameters, ShaderParameterResource(sampler, static_cast<gp::UInt16>(samplerIndex))
        );
    }

    /// @brief Records a UAV binding at `uavIndex`.
    /// @param[in] uavIndex Hardware UAV slot index.
    /// @param[in] uav Unordered access view pointer.
    void setUAVParameter(const gp::UInt32 uavIndex, view::UnorderedAccessView* const uav) noexcept
    {
        GP_CHECKF(allocator != nullptr, "BatchedShaderParameters requires a valid allocator.");
        GP_CHECKF(uavIndex <= 0xFFFFu, "Shader parameter uavIndex exceeds UInt16 range.");
        allocator->appendResourceParameter(
            resourceParameters, ShaderParameterResource(uav, static_cast<gp::UInt16>(uavIndex))
        );
    }

    /// @brief Records a resource collection binding at `baseIndex`.
    /// @param[in] baseIndex Hardware resource collection slot index.
    /// @param[in] collection Resource collection resource pointer.
    void setResourceCollection(const gp::UInt32 baseIndex, resource::Resource* const collection) noexcept
    {
        GP_CHECKF(allocator != nullptr, "BatchedShaderParameters requires a valid allocator.");
        GP_CHECKF(baseIndex <= 0xFFFFu, "Shader parameter baseIndex exceeds UInt16 range.");
        allocator->appendResourceParameter(
            resourceParameters,
            ShaderParameterResource(
                ShaderParameterResource::Type::ResourceCollection, collection, static_cast<gp::UInt16>(baseIndex)
            )
        );
    }

    /// @brief Records a descriptor range binding at `baseIndex`.
    /// @param[in] baseIndex Hardware descriptor range slot index.
    /// @param[in] descriptorRange Descriptor range resource pointer.
    void setDescriptorRange(const gp::UInt32 baseIndex, resource::Resource* const descriptorRange) noexcept
    {
        GP_CHECKF(allocator != nullptr, "BatchedShaderParameters requires a valid allocator.");
        GP_CHECKF(baseIndex <= 0xFFFFu, "Shader parameter baseIndex exceeds UInt16 range.");
        allocator->appendResourceParameter(
            resourceParameters,
            ShaderParameterResource(
                ShaderParameterResource::Type::DescriptorRange, descriptorRange, static_cast<gp::UInt16>(baseIndex)
            )
        );
    }

    /// @brief Records a bindless uniform buffer binding at `bindlessIndex`.
    /// @param[in] bindlessIndex Bindless parameter index.
    /// @param[in] uniformBuffer Uniform buffer resource pointer.
    void setBindlessShaderUniformBuffer(
        const gp::UInt32 bindlessIndex, resource::UniformBuffer* const uniformBuffer
    ) noexcept
    {
        GP_CHECKF(allocator != nullptr, "BatchedShaderParameters requires a valid allocator.");
        GP_CHECKF(bindlessIndex <= 0xFFFFu, "Shader parameter bindlessIndex exceeds UInt16 range.");
        allocator->appendBindlessParameter(
            bindlessParameters, ShaderParameterResource(uniformBuffer, static_cast<gp::UInt16>(bindlessIndex))
        );
    }

    /// @brief Records a bindless texture binding at `bindlessIndex`.
    /// @param[in] bindlessIndex Bindless parameter index.
    /// @param[in] texture Texture resource pointer.
    void setBindlessTexture(const gp::UInt32 bindlessIndex, resource::Texture* const texture) noexcept
    {
        GP_CHECKF(allocator != nullptr, "BatchedShaderParameters requires a valid allocator.");
        GP_CHECKF(bindlessIndex <= 0xFFFFu, "Shader parameter bindlessIndex exceeds UInt16 range.");
        allocator->appendBindlessParameter(
            bindlessParameters, ShaderParameterResource(texture, static_cast<gp::UInt16>(bindlessIndex))
        );
    }

    /// @brief Records a bindless SRV binding at `bindlessIndex`.
    /// @param[in] bindlessIndex Bindless parameter index.
    /// @param[in] srv Shader resource view pointer.
    void setBindlessResourceView(const gp::UInt32 bindlessIndex, view::ShaderResourceView* const srv) noexcept
    {
        GP_CHECKF(allocator != nullptr, "BatchedShaderParameters requires a valid allocator.");
        GP_CHECKF(bindlessIndex <= 0xFFFFu, "Shader parameter bindlessIndex exceeds UInt16 range.");
        allocator->appendBindlessParameter(
            bindlessParameters, ShaderParameterResource(srv, static_cast<gp::UInt16>(bindlessIndex))
        );
    }

    /// @brief Records a bindless sampler binding at `bindlessIndex`.
    /// @param[in] bindlessIndex Bindless parameter index.
    /// @param[in] sampler Sampler state resource pointer.
    void setBindlessSampler(const gp::UInt32 bindlessIndex, pipeline::SamplerState* const sampler) noexcept
    {
        GP_CHECKF(allocator != nullptr, "BatchedShaderParameters requires a valid allocator.");
        GP_CHECKF(bindlessIndex <= 0xFFFFu, "Shader parameter bindlessIndex exceeds UInt16 range.");
        allocator->appendBindlessParameter(
            bindlessParameters, ShaderParameterResource(sampler, static_cast<gp::UInt16>(bindlessIndex))
        );
    }

    /// @brief Records a bindless UAV binding at `bindlessIndex`.
    /// @param[in] bindlessIndex Bindless parameter index.
    /// @param[in] uav Unordered access view pointer.
    void setBindlessUAV(const gp::UInt32 bindlessIndex, view::UnorderedAccessView* const uav) noexcept
    {
        GP_CHECKF(allocator != nullptr, "BatchedShaderParameters requires a valid allocator.");
        GP_CHECKF(bindlessIndex <= 0xFFFFu, "Shader parameter bindlessIndex exceeds UInt16 range.");
        allocator->appendBindlessParameter(
            bindlessParameters, ShaderParameterResource(uav, static_cast<gp::UInt16>(bindlessIndex))
        );
    }

    /// @brief Records a bindless resource collection binding at `bindlessIndex`.
    /// @param[in] bindlessIndex Bindless parameter index.
    /// @param[in] collection Resource collection resource pointer.
    void setBindlessResourceCollection(const gp::UInt32 bindlessIndex, resource::Resource* const collection) noexcept
    {
        GP_CHECKF(allocator != nullptr, "BatchedShaderParameters requires a valid allocator.");
        GP_CHECKF(bindlessIndex <= 0xFFFFu, "Shader parameter bindlessIndex exceeds UInt16 range.");
        allocator->appendBindlessParameter(
            bindlessParameters,
            ShaderParameterResource(
                ShaderParameterResource::Type::ResourceCollection, collection, static_cast<gp::UInt16>(bindlessIndex)
            )
        );
    }
};

/// @brief Batched list of SRV/UAV slot unbind requests recorded before dispatching `setBatchedShaderUnbinds()`.
struct BatchedShaderUnbinds
{
    /// @brief Queued unbind entries.
    gp::Vector<ShaderParameterUnbind> unbinds{};

    /// @brief Reports whether any unbind entries are queued.
    /// @return True when `unbinds` is non-empty.
    [[nodiscard]] GP_FORCEINLINE_HINT bool hasParameters() const noexcept
    {
        return !unbinds.isEmpty();
    }

    /// @brief Clears all queued unbind entries.
    GP_FORCEINLINE_HINT void reset() noexcept
    {
        unbinds.clear();
    }

    /// @brief Queues an SRV slot unbind at `baseIndex`.
    /// @param[in] baseIndex Hardware SRV slot index to clear.
    void unsetSRV(const gp::UInt32 baseIndex) noexcept
    {
        unbinds.emplaceBack(
            ShaderParameterUnbind{ static_cast<gp::UInt16>(baseIndex), ShaderParameterUnbind::Type::ResourceView }
        );
    }

    /// @brief Queues a UAV slot unbind at `baseIndex`.
    /// @param[in] baseIndex Hardware UAV slot index to clear.
    void unsetUAV(const gp::UInt32 baseIndex) noexcept
    {
        unbinds.emplaceBack(
            ShaderParameterUnbind{ static_cast<gp::UInt16>(baseIndex),
                                   ShaderParameterUnbind::Type::UnorderedAccessView }
        );
    }
};

/// @brief Inline fixed-capacity set of globally bound static uniform buffers passed to `setStaticUniformBuffers()`.
class UniformBufferStaticBindings
{
public:
    /// @brief Maximum number of static uniform buffer bindings carried in a single batch.
    static constexpr gp::UInt32 kMaxBindings = 8u;

public:
    /// @brief Constructs an empty static uniform buffer binding set.
    constexpr UniformBufferStaticBindings() noexcept = default;

    /// @brief Binds `uniformBuffer` to its layout's static slot (asserts the layout carries a valid static slot).
    /// @param[in] uniformBuffer Non-null uniform buffer with a valid static slot.
    void addUniformBuffer(resource::UniformBuffer* const uniformBuffer) noexcept
    {
        GP_CHECKF(uniformBuffer != nullptr, "UniformBufferStaticBindings::addUniformBuffer requires a non-null UB.");
        const resource::UniformBufferStaticSlot slot = uniformBuffer->getLayout().getStaticSlot();
        GP_CHECKF(
            resource::isUniformBufferStaticSlotValid(slot),
            "UniformBufferStaticBindings::addUniformBuffer requires a valid static slot."
        );
        addUniformBufferAtSlot(slot, uniformBuffer);
    }

    /// @brief Binds `uniformBuffer` if non-null and its layout has a valid static slot.
    /// @param[in] uniformBuffer Optional uniform buffer pointer.
    void tryAddUniformBuffer(resource::UniformBuffer* const uniformBuffer) noexcept
    {
        if (uniformBuffer == nullptr)
        {
            return;
        }
        const resource::UniformBufferStaticSlot slot = uniformBuffer->getLayout().getStaticSlot();
        if (resource::isUniformBufferStaticSlotValid(slot))
        {
            addUniformBufferAtSlot(slot, uniformBuffer);
        }
    }

    /// @brief Explicitly binds `uniformBuffer` at `slot`, keeping entries sorted by ascending slot index.
    /// @param[in] slot Valid static uniform buffer slot index.
    /// @param[in] uniformBuffer Uniform buffer pointer (may be `nullptr`).
    void addUniformBufferAtSlot(
        const resource::UniformBufferStaticSlot slot, resource::UniformBuffer* const uniformBuffer
    ) noexcept
    {
        for (gp::UInt32 i = 0u; i < m_entryCount; ++i)
        {
            if (m_slots[i] == slot)
            {
                m_uniformBuffers[i] = uniformBuffer;
                return;
            }
            if (m_slots[i] > slot)
            {
                GP_CHECKF(m_entryCount < kMaxBindings, "UniformBufferStaticBindings exceeded kMaxBindings.");
                for (gp::UInt32 j = m_entryCount; j > i; --j)
                {
                    m_slots[j] = m_slots[j - 1u];
                    m_uniformBuffers[j] = m_uniformBuffers[j - 1u];
                }
                m_slots[i] = slot;
                m_uniformBuffers[i] = uniformBuffer;
                ++m_entryCount;
                return;
            }
        }
        GP_CHECKF(m_entryCount < kMaxBindings, "UniformBufferStaticBindings exceeded kMaxBindings.");
        m_slots[m_entryCount] = slot;
        m_uniformBuffers[m_entryCount] = uniformBuffer;
        ++m_entryCount;
    }

    /// @brief Returns the number of bound entries stored in this set.
    /// @return Entry count in `[0, kMaxBindings]`.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getUniformBufferCount() const noexcept
    {
        return m_entryCount;
    }

    /// @brief Returns the slot-table extent required to index every stored static slot (`maxSlot + 1`, or `0` if
    /// empty).
    /// @return Minimum table size covering all bound static slot indices.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getSlotCount() const noexcept
    {
        return (m_entryCount > 0u) ? (static_cast<gp::UInt32>(m_slots[m_entryCount - 1u]) + 1u) : 0u;
    }

    /// @brief Returns the static slot index at position `index`.
    /// @param[in] index Zero-based entry index in `[0, getUniformBufferCount())`.
    /// @return Static slot index.
    [[nodiscard]] GP_FORCEINLINE_HINT resource::UniformBufferStaticSlot getSlot(const gp::UInt32 index) const noexcept
    {
        GP_CHECK_SLOW(index < m_entryCount);
        return m_slots[index];
    }

    /// @brief Returns the uniform buffer pointer at position `index`.
    /// @param[in] index Zero-based entry index in `[0, getUniformBufferCount())`.
    /// @return Bound uniform buffer pointer.
    [[nodiscard]] GP_FORCEINLINE_HINT resource::UniformBuffer* getUniformBuffer(const gp::UInt32 index) const noexcept
    {
        GP_CHECK_SLOW(index < m_entryCount);
        return m_uniformBuffers[index];
    }

    /// @brief Looks up the uniform buffer bound to `slot`, or `nullptr` if absent.
    /// @param[in] slot Static slot index to query.
    /// @return Bound uniform buffer pointer, or `nullptr`.
    [[nodiscard]] resource::UniformBuffer*
        findUniformBuffer(const resource::UniformBufferStaticSlot slot) const noexcept
    {
        for (gp::UInt32 i = 0u; i < m_entryCount; ++i)
        {
            if (m_slots[i] == slot)
            {
                return m_uniformBuffers[i];
            }
        }
        return nullptr;
    }

    /// @brief Populates a dense slot-indexed `outBindings` table from this sparse set.
    /// @param[out] outBindings Vector resized to `getSlotCount()` and populated with bound pointers (`nullptr` for
    /// gaps).
    void bind(gp::Vector<resource::UniformBuffer*>& outBindings) const noexcept
    {
        const gp::UInt32 requiredSlots = getSlotCount();
        outBindings.clear();
        outBindings.resize(requiredSlots, nullptr);
        for (gp::UInt32 i = 0u; i < m_entryCount; ++i)
        {
            outBindings[static_cast<gp::USize>(m_slots[i])] = m_uniformBuffers[i];
        }
    }

    /// @brief Clears all recorded static bindings.
    GP_FORCEINLINE_HINT void reset() noexcept
    {
        m_entryCount = 0u;
    }

    /// @brief Compares two binding sets regardless of insertion order (entries are stored sorted by slot).
    /// @param[in] rhs Binding set to compare against.
    /// @return True when every `(slot, uniformBuffer)` entry matches.
    [[nodiscard]] bool operator==(const UniformBufferStaticBindings& rhs) const noexcept
    {
        if (m_entryCount != rhs.m_entryCount)
        {
            return false;
        }
        for (gp::UInt32 i = 0u; i < m_entryCount; ++i)
        {
            if (m_slots[i] != rhs.m_slots[i] || m_uniformBuffers[i] != rhs.m_uniformBuffers[i])
            {
                return false;
            }
        }
        return true;
    }

private:
    gp::Array<resource::UniformBufferStaticSlot, kMaxBindings> m_slots{};
    gp::Array<resource::UniformBuffer*, kMaxBindings> m_uniformBuffers{};
    gp::UInt32 m_entryCount{ 0u };
};

}   // namespace gp::rhi::shader
