// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/Name.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceEnums.hpp"
#include "core/ResourceForward.hpp"          // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "templates/NumericLimits.hpp"

namespace gp::rhi::resource
{

/// @brief Integer 2D coordinate or texel extent `(x, y)`.
struct IntPoint
{
    /// @brief Horizontal coordinate or width.
    gp::Int32 x{ 0 };

    /// @brief Vertical coordinate or height.
    gp::Int32 y{ 0 };

    /// @brief Constructs `(0, 0)`.
    constexpr IntPoint() noexcept = default;

    /// @brief Constructs `(inX, inY)`.
    /// @param[in] inX Horizontal value.
    /// @param[in] inY Vertical value.
    constexpr IntPoint(const gp::Int32 inX, const gp::Int32 inY) noexcept
        : x(inX)
        , y(inY)
    {}

    /// @brief Constructs a square extent `(inValue, inValue)`.
    /// @param[in] inValue Value assigned to both `x` and `y`.
    explicit constexpr IntPoint(const gp::Int32 inValue) noexcept
        : x(inValue)
        , y(inValue)
    {}

    /// @brief Default equality comparison.
    /// @param[in] rhs Point to compare against.
    /// @return True when both `x` and `y` match.
    [[nodiscard]] constexpr bool operator==(const IntPoint& rhs) const noexcept = default;

    /// @brief Appends `x` and `y` to a streaming hasher.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target hasher accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.appendAll(x, y);
    }
};

/// @brief Integer 3D coordinate or texel extent `(x, y, z)`.
struct IntVector
{
    /// @brief Width or X coordinate.
    gp::Int32 x{ 0 };

    /// @brief Height or Y coordinate.
    gp::Int32 y{ 0 };

    /// @brief Depth, slice count, or Z coordinate.
    gp::Int32 z{ 0 };

    /// @brief Constructs `(0, 0, 0)`.
    constexpr IntVector() noexcept = default;

    /// @brief Constructs `(inX, inY, inZ)`.
    /// @param[in] inX X component.
    /// @param[in] inY Y component.
    /// @param[in] inZ Z component.
    constexpr IntVector(const gp::Int32 inX, const gp::Int32 inY, const gp::Int32 inZ) noexcept
        : x(inX)
        , y(inY)
        , z(inZ)
    {}

    /// @brief Default equality comparison.
    /// @param[in] rhs Vector to compare against.
    /// @return True when `x`, `y`, and `z` match.
    [[nodiscard]] constexpr bool operator==(const IntVector& rhs) const noexcept = default;

    /// @brief Appends `x`, `y`, and `z` to a streaming hasher.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target hasher accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.appendAll(x, y, z);
    }
};

/// @brief Integer 2D axis-aligned rectangle defined by inclusive `min` and exclusive `max` corners.
struct IntRect
{
    /// @brief Upper-left inclusive corner.
    IntPoint min{};

    /// @brief Lower-right exclusive corner.
    IntPoint max{};

    /// @brief Constructs an empty rectangle at `(0, 0) -> (0, 0)`.
    constexpr IntRect() noexcept = default;

    /// @brief Constructs a rectangle from `inMin` and `inMax` corners.
    /// @param[in] inMin Inclusive upper-left corner.
    /// @param[in] inMax Exclusive lower-right corner.
    constexpr IntRect(const IntPoint inMin, const IntPoint inMax) noexcept
        : min(inMin)
        , max(inMax)
    {}

    /// @brief Constructs a rectangle from explicit scalar bounds.
    /// @param[in] x0 Left coordinate.
    /// @param[in] y0 Top coordinate.
    /// @param[in] x1 Right coordinate.
    /// @param[in] y1 Bottom coordinate.
    constexpr IntRect(const gp::Int32 x0, const gp::Int32 y0, const gp::Int32 x1, const gp::Int32 y1) noexcept
        : min(x0, y0)
        , max(x1, y1)
    {}

    /// @brief Returns the horizontal span `max.x - min.x`.
    /// @return Width in pixels or texels.
    [[nodiscard]] constexpr gp::Int32 width() const noexcept
    {
        return max.x - min.x;
    }

    /// @brief Returns the vertical span `max.y - min.y`.
    /// @return Height in pixels or texels.
    [[nodiscard]] constexpr gp::Int32 height() const noexcept
    {
        return max.y - min.y;
    }

    /// @brief Default equality comparison.
    /// @param[in] rhs Rectangle to compare against.
    /// @return True when both corners match.
    [[nodiscard]] constexpr bool operator==(const IntRect& rhs) const noexcept = default;
};

/// @brief Compact unsigned `[first, first + num)` subresource range for mip levels or array slices.
/// @tparam InType Unsigned integer storage type (`gp::UInt8` or `gp::UInt16`).
template <typename InType>
struct Range
{
    /// @brief Inclusive start index of the range.
    InType first{ 0u };

    /// @brief Number of elements in the range (`0` acts as an "all remaining" sentinel in view descriptors).
    InType num{ 0u };

    /// @brief Constructs a zero-initialized range `{0, 0}`.
    constexpr Range() noexcept = default;

    /// @brief Constructs a validated range starting at `inFirst` with `inNum` elements.
    /// @param[in] inFirst Inclusive starting index.
    /// @param[in] inNum Number of elements in the range.
    constexpr Range(const gp::UInt32 inFirst, const gp::UInt32 inNum) noexcept
        : first(static_cast<InType>(inFirst))
        , num(static_cast<InType>(inNum))
    {
        constexpr gp::UInt32 kMaxVal = static_cast<gp::UInt32>(gp::NumericLimits<InType>::max());
        GP_CHECK(inFirst < kMaxVal && inNum < kMaxVal && (inFirst + inNum) < kMaxVal);
    }

    /// @brief Returns the exclusive upper bound `first + num`.
    /// @return One past the final index covered by this range.
    [[nodiscard]] constexpr InType exclusiveLast() const noexcept
    {
        return static_cast<InType>(first + num);
    }

    /// @brief Returns the inclusive upper bound `first + num - 1`.
    /// @return Last index covered by this range (requires `num > 0`).
    [[nodiscard]] constexpr InType inclusiveLast() const noexcept
    {
        GP_CHECK(num > 0u);
        return static_cast<InType>(first + num - 1u);
    }

    /// @brief Checks whether `value` falls within `[first, first + num)`.
    /// @param[in] value Index to test.
    /// @return True when `value >= first && value < exclusiveLast()`.
    [[nodiscard]] constexpr bool isInRange(const gp::UInt32 value) const noexcept
    {
        GP_CHECK(value < static_cast<gp::UInt32>(gp::NumericLimits<InType>::max()));
        return (value >= static_cast<gp::UInt32>(first)) && (value < static_cast<gp::UInt32>(exclusiveLast()));
    }

    /// @brief Default equality comparison.
    /// @param[in] rhs Range to compare against.
    /// @return True when both `first` and `num` match.
    [[nodiscard]] constexpr bool operator==(const Range& rhs) const noexcept = default;
};

/// @brief 8-bit subresource index range used for texture mip levels.
using Range8 = Range<gp::UInt8>;

/// @brief 16-bit subresource index range used for texture array slices.
using Range16 = Range<gp::UInt16>;

/// @brief Interface supplying CPU-side upload bytes for buffer creation.
class IResourceArrayUpload
{
public:
    /// @brief Virtual destructor.
    virtual ~IResourceArrayUpload() noexcept = default;

    /// @brief Returns a pointer to the contiguous upload data block.
    /// @return Read-only pointer to source bytes, or `nullptr` if empty.
    [[nodiscard]] virtual const void* getResourceData() const noexcept = 0;

    /// @brief Returns the total size in bytes of the upload data block.
    /// @return Upload payload size in bytes.
    [[nodiscard]] virtual gp::UInt32 getResourceDataSize() const noexcept = 0;

    /// @brief Releases CPU-side backing storage once the RHI has consumed the upload data.
    virtual void discard() noexcept = 0;
};

/// @brief Non-owning stack adapter exposing a raw pointer or `gp::ConstVectorView` as an `IResourceArrayUpload`.
class ResourceArrayUploadView final : public IResourceArrayUpload
{
public:
    /// @brief Constructs an upload view over raw bytes `[inData, inData + inSizeBytes)`.
    /// @param[in] inData Pointer to source data.
    /// @param[in] inSizeBytes Payload size in bytes.
    constexpr ResourceArrayUploadView(const void* const inData, const gp::UInt32 inSizeBytes) noexcept
        : m_data(inData)
        , m_sizeBytes(inSizeBytes)
    {}

    /// @brief Constructs an upload view over a typed `gp::ConstVectorView`.
    /// @tparam InElementType Element type stored in the view.
    /// @param[in] elements Contiguous source elements.
    template <typename InElementType>
    explicit constexpr ResourceArrayUploadView(const gp::ConstVectorView<InElementType> elements) noexcept
        : m_data(elements.data())
        , m_sizeBytes(static_cast<gp::UInt32>(elements.size() * sizeof(InElementType)))
    {}

    /// @copydoc IResourceArrayUpload::getResourceData()
    [[nodiscard]] virtual const void* getResourceData() const noexcept override
    {
        return m_data;
    }

    /// @copydoc IResourceArrayUpload::getResourceDataSize()
    [[nodiscard]] virtual gp::UInt32 getResourceDataSize() const noexcept override
    {
        return m_sizeBytes;
    }

    /// @copydoc IResourceArrayUpload::discard()
    virtual void discard() noexcept override
    {}

private:
    const void* m_data{ nullptr };
    gp::UInt32 m_sizeBytes{ 0u };
};

/// @brief Interface supplying bulk CPU-side subresource bytes for texture creation.
class IResourceBulkData
{
public:
    /// @brief Virtual destructor.
    virtual ~IResourceBulkData() noexcept = default;

    /// @brief Returns a pointer to the contiguous bulk data payload.
    /// @return Read-only pointer to bulk data bytes, or `nullptr` if empty.
    [[nodiscard]] virtual const void* getResourceBulkData() const noexcept = 0;

    /// @brief Returns the total size in bytes of the bulk data payload.
    /// @return Bulk data payload size in bytes.
    [[nodiscard]] virtual gp::UInt32 getResourceBulkDataSize() const noexcept = 0;

    /// @brief Frees the CPU-side bulk payload once the RHI has uploaded it to the GPU.
    virtual void discard() noexcept = 0;
};

/// @brief Pairs a buffer or allocation byte size with its element stride.
struct SizeAndStride
{
    /// @brief Total size in bytes.
    gp::UInt64 size{ 0u };

    /// @brief Size of a single element in bytes, or zero for untyped byte buffers.
    gp::UInt32 stride{ 0u };

    /// @brief Compares two `SizeAndStride` values for equality.
    /// @param[in] other Value to compare against.
    /// @return True when both size and stride match.
    [[nodiscard]] constexpr bool operator==(const SizeAndStride& other) const noexcept = default;
};

/// @brief Byte offset and span of a GPU video memory allocation.
struct VRamAllocation
{
    /// @brief Start address or heap offset of the allocation in bytes.
    gp::UInt64 allocationStart{ 0u };

    /// @brief Total length of the allocation in bytes.
    gp::UInt64 allocationSize{ 0u };

    /// @brief Constructs an empty, invalid VRAM allocation.
    constexpr VRamAllocation() noexcept = default;

    /// @brief Constructs a VRAM allocation with an explicit offset and byte size.
    /// @param[in] inAllocationStart Start address or heap offset in bytes.
    /// @param[in] inAllocationSize Allocation length in bytes.
    constexpr VRamAllocation(const gp::UInt64 inAllocationStart, const gp::UInt64 inAllocationSize) noexcept
        : allocationStart(inAllocationStart)
        , allocationSize(inAllocationSize)
    {}

    /// @brief Reports whether this allocation covers a non-zero byte span.
    /// @return True when `allocationSize > 0`.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return allocationSize > 0u;
    }

    /// @brief Compares two VRAM allocations for equality.
    /// @param[in] other Allocation to compare against.
    /// @return True when both start offset and size match.
    [[nodiscard]] constexpr bool operator==(const VRamAllocation& other) const noexcept = default;
};

/// @brief Diagnostic snapshot describing an RHI resource's identity, type, and VRAM footprint.
struct ResourceInfo
{
    /// @brief Debug name assigned to the resource.
    gp::Name name{};

    /// @brief Concrete RHI resource type tag.
    ResourceType type{ ResourceType::None };

    /// @brief Backing GPU memory range reported by the active backend.
    VRamAllocation vramAllocation{};

    /// @brief True when the resource comes from a transient frame allocator.
    bool isTransient{ false };

    /// @brief True when the populated diagnostic data is valid.
    bool isValid{ true };

    /// @brief True when the resource's GPU backing memory is currently resident.
    bool isResident{ true };

    /// @brief Compares two `ResourceInfo` records for equality.
    /// @param[in] other Record to compare against.
    /// @return True when all fields match.
    [[nodiscard]] constexpr bool operator==(const ResourceInfo& other) const noexcept = default;
};

}   // namespace gp::rhi::resource
