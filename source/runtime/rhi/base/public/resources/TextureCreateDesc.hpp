// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/Name.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "core/ClearValueBinding.hpp"
#include "core/GPUMask.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "maths/base/Arithmetic.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "RHIPixelFormat.hpp"
#include "sync/SyncEnums.hpp"
#include "templates/Function.hpp"
#include <utility>

namespace gp::rhi::resource
{

/// @brief Identifies a single texture subresource by cube face, array slice, and mip level.
struct TextureSubresourceIndex
{
    /// @brief Cube face index in `[0, 5]`, or `0` for non-cube textures.
    gp::Int32 faceIndex{ 0 };

    /// @brief Array slice index in `[0, arraySize)`.
    gp::Int32 arrayIndex{ 0 };

    /// @brief Mip level index in `[0, numMips)`.
    gp::Int32 mipIndex{ 0 };

    /// @brief Default equality comparison.
    /// @param[in] rhs Subresource index to compare against.
    /// @return True when face, array, and mip indices all match.
    [[nodiscard]] constexpr bool operator==(const TextureSubresourceIndex& rhs) const noexcept = default;
};

/// @brief Callback pair supplying and releasing per-subresource upload payloads during texture creation.
struct TextureDataProvider
{
    /// @brief Pointer, total byte size, and row pitch describing one subresource's upload bytes.
    struct SubresourceData
    {
        /// @brief Pointer to the start of the subresource's pixel data.
        const void* data{ nullptr };

        /// @brief Total size of the subresource payload in bytes.
        gp::UInt64 size{ 0u };

        /// @brief Row pitch in bytes between consecutive scanlines or block rows.
        gp::UInt32 rowPitch{ 0u };
    };

    /// @brief Function signature invoked to request upload bytes for a subresource.
    using RequesterSignature = SubresourceData(TextureSubresourceIndex);

    /// @brief Function signature invoked to release upload bytes for a subresource.
    using DiscarderSignature = void(TextureSubresourceIndex);

    /// @brief Callback invoked to fetch upload bytes for a requested subresource.
    gp::Function<RequesterSignature> requester{};

    /// @brief Optional callback invoked once the RHI has finished uploading a subresource.
    gp::Function<DiscarderSignature> discarder{};

    /// @brief Constructs an empty data provider with unset callbacks.
    TextureDataProvider() noexcept = default;

    /// @brief Constructs a data provider with a subresource request callback.
    /// @tparam InRequesterType Callable matching `SubresourceData(TextureSubresourceIndex)`.
    /// @param[in] inRequester Callback producing subresource upload payloads.
    template <typename InRequesterType>
    explicit TextureDataProvider(InRequesterType&& inRequester) noexcept
        : requester(std::forward<InRequesterType>(inRequester))
    {}

    /// @brief Constructs a data provider with both request and discard callbacks.
    /// @tparam InRequesterType Callable matching `SubresourceData(TextureSubresourceIndex)`.
    /// @tparam InDiscarderType Callable matching `void(TextureSubresourceIndex)`.
    /// @param[in] inRequester Callback producing subresource upload payloads.
    /// @param[in] inDiscarder Callback releasing subresource memory after upload.
    template <typename InRequesterType, typename InDiscarderType>
    TextureDataProvider(InRequesterType&& inRequester, InDiscarderType&& inDiscarder) noexcept
        : requester(std::forward<InRequesterType>(inRequester))
        , discarder(std::forward<InDiscarderType>(inDiscarder))
    {}
};

/// @brief Fluent creation parameters for allocating and initializing a GPU `Texture`.
/// @details Extends `TextureDesc` with debug/owner names, initial barrier state, virtual-mip metadata, and initial
///          subresource upload sources. Use the static `create2D` / `create2DArray` / `create3D` / `createCube` /
///          `createCubeArray` factories and chain `set*` / `add*` methods.
struct TextureCreateDesc : public TextureDesc
{
    /// @brief Human-readable debug label attached to the GPU texture.
    gp::StringView debugName{};

    /// @brief Initial GPU resource access state recorded for barrier tracking.
    sync::ResourceAccess initialState{ sync::ResourceAccess::Unknown };

    /// @brief Policy governing how the texture's initial subresources are populated.
    TextureInitAction initAction{ TextureInitAction::Default };

    /// @brief Optional bulk data provider consumed when `initAction == TextureInitAction::BulkData`.
    IResourceBulkData* bulkData{ nullptr };

    /// @brief Optional streaming subresource callback provider consumed when `initAction == DataProvider`.
    const TextureDataProvider* dataProvider{ nullptr };

    /// @brief First mip index backed by virtual memory (`-1` when not a partially virtual texture).
    gp::Int32 firstVirtualMipLevel{ -1 };

    /// @brief Diagnostic class name reported to memory profilers and trace tools.
    gp::Name className{};

    /// @brief Diagnostic owner identifier forwarded to `Resource::setOwnerName()`.
    gp::Name ownerName{};

    /// @brief Constructs an empty texture creation descriptor.
    TextureCreateDesc() noexcept = default;

    /// @brief Constructs a texture creation descriptor with a debug name and dimensionality.
    /// @param[in] inDebugName Debug label for diagnostics.
    /// @param[in] inDimension Texture dimensionality.
    TextureCreateDesc(const gp::StringView inDebugName, const TextureDimension inDimension) noexcept
        : TextureDesc(inDimension)
        , debugName(inDebugName)
    {}

    /// @brief Constructs a texture creation descriptor from an existing `TextureDesc` and initial access state.
    /// @param[in] inDesc Base texture layout descriptor.
    /// @param[in] inInitialState Initial GPU resource access state.
    /// @param[in] inDebugName Debug label for diagnostics.
    /// @param[in] inBulkData Optional bulk data payload (automatically sets `TextureInitAction::BulkData` if non-null).
    TextureCreateDesc(
        const TextureDesc& inDesc,
        const sync::ResourceAccess inInitialState,
        const gp::StringView inDebugName,
        IResourceBulkData* const inBulkData = nullptr
    ) noexcept
        : TextureDesc(inDesc)
        , debugName(inDebugName)
        , initialState(inInitialState)
    {
        if (inBulkData != nullptr)
        {
            setInitActionBulkData(inBulkData);
        }
    }

    /// @brief Creates a texture creation descriptor for `inDimension`.
    /// @param[in] inDebugName Debug label.
    /// @param[in] inDimension Texture dimensionality.
    /// @return Initialized `TextureCreateDesc`.
    [[nodiscard]] static TextureCreateDesc
        create(const gp::StringView inDebugName, const TextureDimension inDimension) noexcept
    {
        return TextureCreateDesc(inDebugName, inDimension);
    }

    /// @brief Creates a 2D texture creation descriptor with default extent and format.
    /// @param[in] inDebugName Debug label.
    /// @return 2D `TextureCreateDesc`.
    [[nodiscard]] static TextureCreateDesc create2D(const gp::StringView inDebugName) noexcept
    {
        return TextureCreateDesc(inDebugName, TextureDimension::Texture2D);
    }

    /// @brief Creates a 2D array texture creation descriptor with default extent and format.
    /// @param[in] inDebugName Debug label.
    /// @return 2D-array `TextureCreateDesc`.
    [[nodiscard]] static TextureCreateDesc create2DArray(const gp::StringView inDebugName) noexcept
    {
        return TextureCreateDesc(inDebugName, TextureDimension::Texture2DArray);
    }

    /// @brief Creates a 3D volume texture creation descriptor with default extent and format.
    /// @param[in] inDebugName Debug label.
    /// @return 3D `TextureCreateDesc`.
    [[nodiscard]] static TextureCreateDesc create3D(const gp::StringView inDebugName) noexcept
    {
        return TextureCreateDesc(inDebugName, TextureDimension::Texture3D);
    }

    /// @brief Creates a cubemap texture creation descriptor with default extent and format.
    /// @param[in] inDebugName Debug label.
    /// @return Cube `TextureCreateDesc`.
    [[nodiscard]] static TextureCreateDesc createCube(const gp::StringView inDebugName) noexcept
    {
        return TextureCreateDesc(inDebugName, TextureDimension::TextureCube);
    }

    /// @brief Creates a cubemap array texture creation descriptor with default extent and format.
    /// @param[in] inDebugName Debug label.
    /// @return Cube-array `TextureCreateDesc`.
    [[nodiscard]] static TextureCreateDesc createCubeArray(const gp::StringView inDebugName) noexcept
    {
        return TextureCreateDesc(inDebugName, TextureDimension::TextureCubeArray);
    }

    /// @brief Creates a 2D texture creation descriptor with explicit size and pixel format.
    /// @param[in] inDebugName Debug label.
    /// @param[in] inSize Width and height in texels.
    /// @param[in] inFormat Primary pixel format.
    /// @return Populated 2D `TextureCreateDesc`.
    [[nodiscard]] static TextureCreateDesc
        create2D(const gp::StringView inDebugName, const IntPoint inSize, const PixelFormat inFormat) noexcept
    {
        return create2D(inDebugName).setExtent(inSize).setFormat(inFormat);
    }

    /// @brief Creates a 2D texture creation descriptor with explicit scalar dimensions and format.
    /// @param[in] inDebugName Debug label.
    /// @param[in] inSizeX Width in texels.
    /// @param[in] inSizeY Height in texels.
    /// @param[in] inFormat Primary pixel format.
    /// @return Populated 2D `TextureCreateDesc`.
    [[nodiscard]] static TextureCreateDesc create2D(
        const gp::StringView inDebugName, const gp::Int32 inSizeX, const gp::Int32 inSizeY, const PixelFormat inFormat
    ) noexcept
    {
        return create2D(inDebugName, IntPoint(inSizeX, inSizeY), inFormat);
    }

    /// @brief Creates a 2D texture array descriptor with explicit size, slice count, and format.
    /// @param[in] inDebugName Debug label.
    /// @param[in] inSize Width and height in texels.
    /// @param[in] inArraySize Number of array slices.
    /// @param[in] inFormat Primary pixel format.
    /// @return Populated 2D-array `TextureCreateDesc`.
    [[nodiscard]] static TextureCreateDesc create2DArray(
        const gp::StringView inDebugName,
        const IntPoint inSize,
        const gp::UInt16 inArraySize,
        const PixelFormat inFormat
    ) noexcept
    {
        return create2DArray(inDebugName).setExtent(inSize).setArraySize(inArraySize).setFormat(inFormat);
    }

    /// @brief Creates a 2D texture array descriptor with scalar dimensions, slice count, and format.
    /// @param[in] inDebugName Debug label.
    /// @param[in] inSizeX Width in texels.
    /// @param[in] inSizeY Height in texels.
    /// @param[in] inArraySize Number of array slices.
    /// @param[in] inFormat Primary pixel format.
    /// @return Populated 2D-array `TextureCreateDesc`.
    [[nodiscard]] static TextureCreateDesc create2DArray(
        const gp::StringView inDebugName,
        const gp::Int32 inSizeX,
        const gp::Int32 inSizeY,
        const gp::UInt16 inArraySize,
        const PixelFormat inFormat
    ) noexcept
    {
        return create2DArray(inDebugName, IntPoint(inSizeX, inSizeY), inArraySize, inFormat);
    }

    /// @brief Creates a 3D volume texture descriptor with explicit 3D size and format.
    /// @param[in] inDebugName Debug label.
    /// @param[in] inSize Width, height, and depth in texels.
    /// @param[in] inFormat Primary pixel format.
    /// @return Populated 3D `TextureCreateDesc`.
    [[nodiscard]] static TextureCreateDesc
        create3D(const gp::StringView inDebugName, const IntVector inSize, const PixelFormat inFormat) noexcept
    {
        return create3D(inDebugName)
            .setExtent(inSize.x, inSize.y)
            .setDepth(static_cast<gp::UInt16>(inSize.z))
            .setFormat(inFormat);
    }

    /// @brief Creates a 3D volume texture descriptor with scalar dimensions and format.
    /// @param[in] inDebugName Debug label.
    /// @param[in] inSizeX Width in texels.
    /// @param[in] inSizeY Height in texels.
    /// @param[in] inSizeZ Depth in texels.
    /// @param[in] inFormat Primary pixel format.
    /// @return Populated 3D `TextureCreateDesc`.
    [[nodiscard]] static TextureCreateDesc create3D(
        const gp::StringView inDebugName,
        const gp::Int32 inSizeX,
        const gp::Int32 inSizeY,
        const gp::Int32 inSizeZ,
        const PixelFormat inFormat
    ) noexcept
    {
        return create3D(inDebugName, IntVector(inSizeX, inSizeY, inSizeZ), inFormat);
    }

    /// @brief Creates a cubemap texture descriptor with square face dimension and format.
    /// @param[in] inDebugName Debug label.
    /// @param[in] inSize Face width and height in texels.
    /// @param[in] inFormat Primary pixel format.
    /// @return Populated cubemap `TextureCreateDesc`.
    [[nodiscard]] static TextureCreateDesc
        createCube(const gp::StringView inDebugName, const gp::UInt32 inSize, const PixelFormat inFormat) noexcept
    {
        return createCube(inDebugName).setExtent(inSize).setFormat(inFormat);
    }

    /// @brief Creates a cubemap array texture descriptor with square face dimension, array size, and format.
    /// @param[in] inDebugName Debug label.
    /// @param[in] inSize Face width and height in texels.
    /// @param[in] inArraySize Number of cubemaps in the array.
    /// @param[in] inFormat Primary pixel format.
    /// @return Populated cubemap-array `TextureCreateDesc`.
    [[nodiscard]] static TextureCreateDesc createCubeArray(
        const gp::StringView inDebugName,
        const gp::UInt32 inSize,
        const gp::UInt16 inArraySize,
        const PixelFormat inFormat
    ) noexcept
    {
        return createCubeArray(inDebugName).setExtent(inSize).setArraySize(inArraySize).setFormat(inFormat);
    }

    /// @brief Validates both the base `TextureDesc` and that `initialState` is not `ResourceAccess::Unknown`.
    /// @param[in] limits Hardware limit set to validate against.
    void checkValidity(const TextureValidationLimits& limits = TextureValidationLimits{}) const noexcept
    {
        TextureDesc::checkValidity(*this, debugName, limits);
        GP_ENSURE_MSG(
            initialState != sync::ResourceAccess::Unknown,
            "TextureCreateDesc requires an explicit initialState (call setInitialState() or determineInitialState())."
        );
    }

    /// @brief Replaces all texture creation flags.
    /// @param[in] inFlags New flag bitmask.
    /// @return Reference to `*this`.
    TextureCreateDesc& setFlags(const TextureCreateFlags inFlags) noexcept
    {
        flags = inFlags;
        return *this;
    }

    /// @brief Adds texture creation flags into `flags`.
    /// @param[in] inFlags Additional flags to OR into `flags`.
    /// @return Reference to `*this`.
    TextureCreateDesc& addFlags(const TextureCreateFlags inFlags) noexcept
    {
        flags |= inFlags;
        return *this;
    }

    /// @brief Sets the hardware fast-clear value bound to this texture.
    /// @param[in] inClearValue Color or depth-stencil clear binding.
    /// @return Reference to `*this`.
    TextureCreateDesc& setClearValue(const ClearValueBinding& inClearValue) noexcept
    {
        clearValue = inClearValue;
        return *this;
    }

    /// @brief Sets the platform-specific offline metadata word.
    /// @param[in] inExtData Platform metadata value.
    /// @return Reference to `*this`.
    TextureCreateDesc& setExtData(const gp::UInt32 inExtData) noexcept
    {
        extData = inExtData;
        return *this;
    }

    /// @brief Sets the 2D base mip extent from an `IntPoint`.
    /// @param[in] inExtent Width and height in texels.
    /// @return Reference to `*this`.
    TextureCreateDesc& setExtent(const IntPoint inExtent) noexcept
    {
        extent = inExtent;
        return *this;
    }

    /// @brief Sets the 2D base mip extent from scalar width and height.
    /// @param[in] inSizeX Width in texels.
    /// @param[in] inSizeY Height in texels.
    /// @return Reference to `*this`.
    TextureCreateDesc& setExtent(const gp::Int32 inSizeX, const gp::Int32 inSizeY) noexcept
    {
        extent = IntPoint(inSizeX, inSizeY);
        return *this;
    }

    /// @brief Sets a square base mip extent `(inSize, inSize)`.
    /// @param[in] inSize Square width and height in texels.
    /// @return Reference to `*this`.
    TextureCreateDesc& setExtent(const gp::UInt32 inSize) noexcept
    {
        extent = IntPoint(static_cast<gp::Int32>(inSize));
        return *this;
    }

    /// @brief Sets the 3D base mip depth.
    /// @param[in] inDepth Depth in texels.
    /// @return Reference to `*this`.
    TextureCreateDesc& setDepth(const gp::UInt16 inDepth) noexcept
    {
        depth = inDepth;
        return *this;
    }

    /// @brief Sets the texture array slice count.
    /// @param[in] inArraySize Number of slices (or cubemaps for cube arrays).
    /// @return Reference to `*this`.
    TextureCreateDesc& setArraySize(const gp::UInt16 inArraySize) noexcept
    {
        arraySize = inArraySize;
        return *this;
    }

    /// @brief Sets the number of mip levels.
    /// @param[in] inNumMips Mip count.
    /// @return Reference to `*this`.
    TextureCreateDesc& setNumMips(const gp::UInt8 inNumMips) noexcept
    {
        numMips = inNumMips;
        return *this;
    }

    /// @brief Sets the MSAA sample count per pixel.
    /// @param[in] inNumSamples Sample count.
    /// @return Reference to `*this`.
    TextureCreateDesc& setNumSamples(const gp::UInt8 inNumSamples) noexcept
    {
        numSamples = inNumSamples;
        return *this;
    }

    /// @brief Sets the texture dimensionality.
    /// @param[in] inDimension Texture dimensionality.
    /// @return Reference to `*this`.
    TextureCreateDesc& setDimension(const TextureDimension inDimension) noexcept
    {
        dimension = inDimension;
        return *this;
    }

    /// @brief Sets the primary storage pixel format.
    /// @param[in] inFormat Primary `PixelFormat`.
    /// @return Reference to `*this`.
    TextureCreateDesc& setFormat(const PixelFormat inFormat) noexcept
    {
        format = inFormat;
        return *this;
    }

    /// @brief Sets the alternate pixel format used for UAV creation.
    /// @param[in] inUAVFormat Alternate UAV `PixelFormat`.
    /// @return Reference to `*this`.
    TextureCreateDesc& setUAVFormat(const PixelFormat inUAVFormat) noexcept
    {
        uavFormat = inUAVFormat;
        return *this;
    }

    /// @brief Sets the debug label associated with the texture.
    /// @param[in] inDebugName Debug label string view.
    /// @return Reference to `*this`.
    TextureCreateDesc& setDebugName(const gp::StringView inDebugName) noexcept
    {
        debugName = inDebugName;
        return *this;
    }

    /// @brief Sets the explicit initial GPU resource access state.
    /// @param[in] inInitialState Initial `sync::ResourceAccess` mask.
    /// @return Reference to `*this`.
    TextureCreateDesc& setInitialState(const sync::ResourceAccess inInitialState) noexcept
    {
        initialState = inInitialState;
        return *this;
    }

    /// @brief Sets the texture initialization action policy.
    /// @param[in] inInitAction Desired `TextureInitAction`.
    /// @return Reference to `*this`.
    TextureCreateDesc& setInitAction(const TextureInitAction inInitAction) noexcept
    {
        initAction = inInitAction;
        return *this;
    }

    /// @brief Sets the multi-GPU node affinity mask.
    /// @param[in] inGPUMask GPU node mask.
    /// @return Reference to `*this`.
    TextureCreateDesc& setGPUMask(const GPUMask inGPUMask) noexcept
    {
        gpuMask = inGPUMask;
        return *this;
    }

    /// @brief Sets the first virtual-memory-backed mip level.
    /// @param[in] inFirstVirtualMipLevel Zero-based mip index, or `-1` when non-virtual.
    /// @return Reference to `*this`.
    TextureCreateDesc& setFirstVirtualMipLevel(const gp::Int32 inFirstVirtualMipLevel) noexcept
    {
        firstVirtualMipLevel = inFirstVirtualMipLevel;
        return *this;
    }

    /// @brief Sets the diagnostic class name for memory tracing.
    /// @param[in] inClassName Class identifier.
    /// @return Reference to `*this`.
    TextureCreateDesc& setClassName(const gp::Name inClassName) noexcept
    {
        className = inClassName;
        return *this;
    }

    /// @brief Sets the diagnostic owner name for resource tracking.
    /// @param[in] inOwnerName Owner identifier.
    /// @return Reference to `*this`.
    TextureCreateDesc& setOwnerName(const gp::Name inOwnerName) noexcept
    {
        ownerName = inOwnerName;
        return *this;
    }

    /// @brief Appends an additional pixel format permitted to alias this texture through views.
    /// @param[in] inFormat Aliasable `PixelFormat`.
    /// @return Reference to `*this`.
    TextureCreateDesc& addAliasableFormat(const PixelFormat inFormat) noexcept
    {
        aliasableFormats.emplaceBack(inFormat);
        return *this;
    }

    /// @brief Sets the fast VRAM percentage hint from a normalized fraction in `[0.0f, 1.0f]`.
    /// @param[in] inFraction Normalized fraction clamped to `[0.0f, 1.0f]` and quantized to `[0, 255]`.
    /// @return Reference to `*this`.
    TextureCreateDesc& setFastVRAMPercentage(const gp::Float32 inFraction) noexcept
    {
        const gp::Float32 clamped = gp::math::clamp(inFraction, 0.0f, 1.0f);
        fastVRAMPercentage = static_cast<gp::UInt8>(clamped * 255.0f);
        return *this;
    }

    /// @brief Reports whether this descriptor supplies initial subresource upload data.
    /// @return True when `initAction` is `BulkData` or `DataProvider`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool hasInitialData() const noexcept
    {
        return (initAction == TextureInitAction::BulkData) || (initAction == TextureInitAction::DataProvider);
    }

    /// @brief Resolves `initialState` from `flags` and `hasInitialData()` when `initialState` is `Unknown`.
    /// @return Reference to `*this`.
    TextureCreateDesc& determineInitialState() noexcept
    {
        if (initialState == sync::ResourceAccess::Unknown)
        {
            initialState = getDefaultResourceState(flags, hasInitialData());
        }
        return *this;
    }

    /// @brief Selects default initialization (contents left undefined until written).
    /// @return Reference to `*this`.
    TextureCreateDesc& setInitActionNone() noexcept
    {
        bulkData = nullptr;
        dataProvider = nullptr;
        return setInitAction(TextureInitAction::Default);
    }

    /// @brief Selects bulk-data upload initialization using `inBulkData`.
    /// @param[in] inBulkData Bulk data provider (if null, selects `TextureInitAction::Default`).
    /// @return Reference to `*this`.
    TextureCreateDesc& setInitActionBulkData(IResourceBulkData* const inBulkData) noexcept
    {
        bulkData = inBulkData;
        dataProvider = nullptr;
        return setInitAction(inBulkData != nullptr ? TextureInitAction::BulkData : TextureInitAction::Default);
    }

    /// @brief Selects streaming data-provider upload initialization using `inDataProvider`.
    /// @param[in] inDataProvider Non-null provider with an active `requester` callback.
    /// @return Reference to `*this`.
    TextureCreateDesc& setInitActionDataProvider(const TextureDataProvider* const inDataProvider) noexcept
    {
        GP_CHECKF(
            inDataProvider != nullptr && static_cast<bool>(inDataProvider->requester),
            "TextureCreateDesc::setInitActionDataProvider requires a non-null provider with a valid requester callback."
        );
        bulkData = nullptr;
        dataProvider = inDataProvider;
        return setInitAction(TextureInitAction::DataProvider);
    }

    /// @brief Selects caller-driven initialization through a texture initializer handle.
    /// @return Reference to `*this`.
    TextureCreateDesc& setInitActionInitializer() noexcept
    {
        bulkData = nullptr;
        dataProvider = nullptr;
        return setInitAction(TextureInitAction::Initializer);
    }

    /// @brief Returns `className` when set, or `"Texture"` as a default trace class identifier.
    /// @return Non-empty diagnostic `gp::Name`.
    [[nodiscard]] gp::Name getTraceClassName() const noexcept
    {
        return !className.isNone() ? className : gp::Name("Texture");
    }
};

}   // namespace gp::rhi::resource
