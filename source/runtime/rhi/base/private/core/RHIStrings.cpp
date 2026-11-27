// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "core/RHIStrings.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/strings/Name.hpp"
#include "containers/strings/String.hpp"
#include "containers/views/StringView.hpp"
#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "device/DeviceEnums.hpp"
#include "device/RHIGlobals.hpp"
#include "maths/base/Bitwise.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/TextureEnums.hpp"
#include "RHIPixelFormat.hpp"
#include "shaders/ShaderEnums.hpp"
#include "sync/SyncEnums.hpp"
#include "views/ViewEnums.hpp"
#include <type_traits>

namespace gp::rhi
{

namespace
{

constexpr gp::Array<const char*, kFeatureLevelCount> kFeatureLevelNames{
    { "ES2", "ES3_1", "SM4_REMOVED", "SM5", "SM6" }
};

constexpr gp::Array<const char*, kShadingPathCount> kShadingPathNames{
    { "Deferred", "Forward", "Mobile" }
};

struct ResourceTypeNameEntry
{
    resource::ResourceType type;
    const char* name;
};

// clang-format off
constexpr gp::Array<ResourceTypeNameEntry, resource::kResourceTypeCount> kResourceTypeNames{ {
    { resource::ResourceType::None,                           "None"                           },
    { resource::ResourceType::SamplerState,                   "SamplerState"                   },
    { resource::ResourceType::RasterizerState,                "RasterizerState"                },
    { resource::ResourceType::DepthStencilState,              "DepthStencilState"              },
    { resource::ResourceType::BlendState,                     "BlendState"                     },
    { resource::ResourceType::VertexDeclaration,              "VertexDeclaration"              },
    { resource::ResourceType::VertexShader,                   "VertexShader"                   },
    { resource::ResourceType::MeshShader,                     "MeshShader"                     },
    { resource::ResourceType::AmplificationShader,            "AmplificationShader"            },
    { resource::ResourceType::PixelShader,                    "PixelShader"                    },
    { resource::ResourceType::GeometryShader,                 "GeometryShader"                 },
    { resource::ResourceType::RayTracingShader,               "RayTracingShader"               },
    { resource::ResourceType::ComputeShader,                  "ComputeShader"                  },
    { resource::ResourceType::GraphicsPipelineState,          "GraphicsPipelineState"          },
    { resource::ResourceType::ComputePipelineState,           "ComputePipelineState"           },
    { resource::ResourceType::RayTracingPipelineState,        "RayTracingPipelineState"        },
    { resource::ResourceType::BoundShaderState,               "BoundShaderState"               },
    { resource::ResourceType::UniformBufferLayout,            "UniformBufferLayout"            },
    { resource::ResourceType::UniformBuffer,                  "UniformBuffer"                  },
    { resource::ResourceType::Buffer,                         "Buffer"                         },
    { resource::ResourceType::Texture,                        "Texture"                        },
    { resource::ResourceType::TextureReference,               "TextureReference"               },
    { resource::ResourceType::GPUFence,                       "GPUFence"                       },
    { resource::ResourceType::RenderQuery,                    "RenderQuery"                    },
    { resource::ResourceType::RenderQueryPool,                "RenderQueryPool"                },
    { resource::ResourceType::Viewport,                       "Viewport"                       },
    { resource::ResourceType::UnorderedAccessView,            "UnorderedAccessView"            },
    { resource::ResourceType::ShaderResourceView,             "ShaderResourceView"             },
    { resource::ResourceType::RayTracingGeometry,             "RayTracingGeometry"             },
    { resource::ResourceType::RayTracingScene,                "RayTracingScene"                },
    { resource::ResourceType::RayTracingOpacityMicromapArray, "RayTracingOpacityMicromapArray" },
    { resource::ResourceType::RayTracingShaderBindingTable,   "RayTracingShaderBindingTable"   },
    { resource::ResourceType::StagingBuffer,                  "StagingBuffer"                  },
    { resource::ResourceType::CustomPresent,                  "CustomPresent"                  },
    { resource::ResourceType::ShaderLibrary,                  "ShaderLibrary"                  },
    { resource::ResourceType::ShaderBundle,                   "ShaderBundle"                   },
    { resource::ResourceType::WorkGraphShader,                "WorkGraphShader"                },
    { resource::ResourceType::WorkGraphPipelineState,         "WorkGraphPipelineState"         },
    { resource::ResourceType::StreamSourceSlot,               "StreamSourceSlot"               },
    { resource::ResourceType::ResourceCollection,             "ResourceCollection"             },
    { resource::ResourceType::DescriptorRange,                "DescriptorRange"                }
} };
// clang-format on

template <typename InEnumType>
[[nodiscard]] gp::String
    buildEnumNameBitList(const InEnumType value, const char* (*const getBitName)(InEnumType) noexcept) noexcept
{
    using Underlying = std::underlying_type_t<InEnumType>;
    Underlying rawValue = static_cast<Underlying>(value);
    if (rawValue == 0u)
    {
        return gp::String(getBitName(static_cast<InEnumType>(0u)));
    }

    gp::String result;
    result.reserve(64u);

    gp::UInt32 bitIndex = 0u;
    while (rawValue != 0u)
    {
        if ((rawValue & Underlying{ 1u }) != 0u)
        {
            if (!result.isEmpty())
            {
                result.pushBack('|');
            }
            const InEnumType singleBit = static_cast<InEnumType>(Underlying{ 1u } << bitIndex);
            result.append(getBitName(singleBit));
        }

        ++bitIndex;
        rawValue >>= 1u;
    }

    return result;
}

[[nodiscard]] const char* getPrefixedBufferUsageBitName(const resource::BufferUsageFlags singleBit) noexcept
{
    constexpr gp::UInt32 kRayTracingScratchExclusiveBit = 1u << 19u;
    const gp::UInt32 raw = static_cast<gp::UInt32>(singleBit);
    if (raw == kRayTracingScratchExclusiveBit)
    {
        return "BUF_RayTracingScratch";
    }

    switch (singleBit)
    {
    case resource::BufferUsageFlags::None:
        return "BUF_None";
    case resource::BufferUsageFlags::Static:
        return "BUF_Static";
    case resource::BufferUsageFlags::Dynamic:
        return "BUF_Dynamic";
    case resource::BufferUsageFlags::Volatile:
        return "BUF_Volatile";
    case resource::BufferUsageFlags::UnorderedAccess:
        return "BUF_UnorderedAccess";
    case resource::BufferUsageFlags::ByteAddressBuffer:
        return "BUF_ByteAddressBuffer";
    case resource::BufferUsageFlags::SourceCopy:
        return "BUF_SourceCopy";
    case resource::BufferUsageFlags::DrawIndirect:
        return "BUF_DrawIndirect";
    case resource::BufferUsageFlags::ShaderResource:
        return "BUF_ShaderResource";
    case resource::BufferUsageFlags::KeepCPUAccessible:
        return "BUF_KeepCPUAccessible";
    case resource::BufferUsageFlags::FastVRAM:
        return "BUF_FastVRAM";
    case resource::BufferUsageFlags::NNE:
        return "BUF_NNE";
    case resource::BufferUsageFlags::Shared:
        return "BUF_Shared";
    case resource::BufferUsageFlags::AccelerationStructure:
        return "BUF_AccelerationStructure";
    case resource::BufferUsageFlags::VertexBuffer:
        return "BUF_VertexBuffer";
    case resource::BufferUsageFlags::IndexBuffer:
        return "BUF_IndexBuffer";
    case resource::BufferUsageFlags::StructuredBuffer:
        return "BUF_StructuredBuffer";
    case resource::BufferUsageFlags::MultiGPUAllocate:
        return "BUF_MultiGPUAllocate";
    case resource::BufferUsageFlags::MultiGPUGraphIgnore:
        return "BUF_MultiGPUGraphIgnore";
    case resource::BufferUsageFlags::NullResource:
        return "BUF_NullResource";
    case resource::BufferUsageFlags::UniformBuffer:
        return "BUF_UniformBuffer";
    case resource::BufferUsageFlags::ReservedResource:
        return "BUF_ReservedResource";
    default:
        return "BUF_Unknown";
    }
}

[[nodiscard]] const char* getTextureCreateBitNameOrUnknown(const resource::TextureCreateFlags singleBit) noexcept
{
    const char* const known = getTextureCreateFlagString(singleBit);
    return (known[0] != '\0') ? known : "Unknown";
}

[[nodiscard]] const char* getResourceAccessBitName(const sync::ResourceAccess singleBit) noexcept
{
    switch (singleBit)
    {
    case sync::ResourceAccess::Unknown:
        return "Unknown";
    case sync::ResourceAccess::CPURead:
        return "CPURead";
    case sync::ResourceAccess::Present:
        return "Present";
    case sync::ResourceAccess::IndirectArgs:
        return "IndirectArgs";
    case sync::ResourceAccess::VertexOrIndexBuffer:
        return "VertexOrIndexBuffer";
    case sync::ResourceAccess::SRVCompute:
        return "SRVCompute";
    case sync::ResourceAccess::SRVGraphicsPixel:
        return "SRVGraphicsPixel";
    case sync::ResourceAccess::SRVGraphicsNonPixel:
        return "SRVGraphicsNonPixel";
    case sync::ResourceAccess::CopySrc:
        return "CopySrc";
    case sync::ResourceAccess::ResolveSrc:
        return "ResolveSrc";
    case sync::ResourceAccess::DSVRead:
        return "DSVRead";
    case sync::ResourceAccess::UAVCompute:
        return "UAVCompute";
    case sync::ResourceAccess::UAVGraphics:
        return "UAVGraphics";
    case sync::ResourceAccess::RTV:
        return "RTV";
    case sync::ResourceAccess::CopyDest:
        return "CopyDest";
    case sync::ResourceAccess::ResolveDst:
        return "ResolveDst";
    case sync::ResourceAccess::DSVWrite:
        return "DSVWrite";
    case sync::ResourceAccess::BVHRead:
        return "BVHRead";
    case sync::ResourceAccess::BVHWrite:
        return "BVHWrite";
    case sync::ResourceAccess::Discard:
        return "Discard";
    case sync::ResourceAccess::ShadingRateSource:
        return "ShadingRateSource";
    default:
        return "Unknown";
    }
}

[[nodiscard]] const char* getResourceTransitionBitName(const sync::ResourceTransitionFlags singleBit) noexcept
{
    switch (singleBit)
    {
    case sync::ResourceTransitionFlags::None:
        return "None";
    case sync::ResourceTransitionFlags::MaintainCompression:
        return "MaintainCompression";
    case sync::ResourceTransitionFlags::IgnoreAfterState:
        return "IgnoreAfterState";
    default:
        return "Unknown";
    }
}

[[nodiscard]] const char* getPipelineStageBitName(const sync::PipelineStage singleBit) noexcept
{
    switch (singleBit)
    {
    case sync::PipelineStage::None:
        return "None";
    case sync::PipelineStage::Graphics:
        return "Graphics";
    case sync::PipelineStage::AsyncCompute:
        return "AsyncCompute";
    default:
        return "Unknown";
    }
}

[[nodiscard]] const char* getPixelFormatCapabilityBitName(const PixelFormatCapabilities singleBit) noexcept
{
    switch (singleBit)
    {
    case PixelFormatCapabilities::None:
        return "None";
    case PixelFormatCapabilities::Texture1D:
        return "Texture1D";
    case PixelFormatCapabilities::Texture2D:
        return "Texture2D";
    case PixelFormatCapabilities::Texture3D:
        return "Texture3D";
    case PixelFormatCapabilities::TextureCube:
        return "TextureCube";
    case PixelFormatCapabilities::RenderTarget:
        return "RenderTarget";
    case PixelFormatCapabilities::DepthStencil:
        return "DepthStencil";
    case PixelFormatCapabilities::TextureMipmaps:
        return "TextureMipmaps";
    case PixelFormatCapabilities::TextureLoad:
        return "TextureLoad";
    case PixelFormatCapabilities::TextureSample:
        return "TextureSample";
    case PixelFormatCapabilities::TextureGather:
        return "TextureGather";
    case PixelFormatCapabilities::TextureAtomics:
        return "TextureAtomics";
    case PixelFormatCapabilities::TextureBlendable:
        return "TextureBlendable";
    case PixelFormatCapabilities::TextureStore:
        return "TextureStore";
    case PixelFormatCapabilities::TextureFilterable:
        return "TextureFilterable";
    case PixelFormatCapabilities::Buffer:
        return "Buffer";
    case PixelFormatCapabilities::VertexBuffer:
        return "VertexBuffer";
    case PixelFormatCapabilities::IndexBuffer:
        return "IndexBuffer";
    case PixelFormatCapabilities::BufferLoad:
        return "BufferLoad";
    case PixelFormatCapabilities::BufferStore:
        return "BufferStore";
    case PixelFormatCapabilities::BufferAtomics:
        return "BufferAtomics";
    case PixelFormatCapabilities::UAV:
        return "UAV";
    case PixelFormatCapabilities::TypedUAVLoad:
        return "TypedUAVLoad";
    case PixelFormatCapabilities::TypedUAVStore:
        return "TypedUAVStore";
    case PixelFormatCapabilities::LossyCompressible:
        return "LossyCompressible";
    default:
        return "Unknown";
    }
}

}   // namespace

const char* rhiVendorIdToString() noexcept
{
    return rhiVendorIdToString(getRHIGlobals().gpuInfo.vendorId);
}

const char* rhiVendorIdToString(const GpuVendorId vendorId) noexcept
{
    if (static_cast<gp::UInt32>(vendorId) == kQualcommAlternateVendorId)
    {
        return "Qualcomm";
    }

    switch (vendorId)
    {
    case GpuVendorId::Amd:
        return "AMD";
    case GpuVendorId::ImgTec:
        return "ImgTec";
    case GpuVendorId::Nvidia:
        return "NVIDIA";
    case GpuVendorId::Arm:
        return "ARM";
    case GpuVendorId::Broadcom:
        return "Broadcom";
    case GpuVendorId::Qualcomm:
        return "Qualcomm";
    case GpuVendorId::Apple:
        return "Apple";
    case GpuVendorId::Intel:
        return "Intel";
    case GpuVendorId::Vivante:
        return "Vivante";
    case GpuVendorId::VeriSilicon:
        return "VeriSilicon";
    case GpuVendorId::Kazan:
        return "Kazan";
    case GpuVendorId::Codeplay:
        return "Codeplay";
    case GpuVendorId::Mesa:
        return "Mesa";
    case GpuVendorId::SamsungAMD:
        return "SamsungAMD";
    case GpuVendorId::Microsoft:
        return "Microsoft";
    case GpuVendorId::NotQueried:
        return "Not Queried";
    case GpuVendorId::Unknown:
    default:
        return "Unknown";
    }
}

const char* getInterfaceTypeString(const InterfaceType interfaceType) noexcept
{
    switch (interfaceType)
    {
    case InterfaceType::NotSet:
        return "NotSet";
    case InterfaceType::Hidden:
        return "Hidden";
    case InterfaceType::Null:
        return "Null";
    case InterfaceType::D3D11:
        return "D3D11";
    case InterfaceType::D3D12:
        return "D3D12";
    case InterfaceType::Vulkan:
        return "Vulkan";
    case InterfaceType::Metal:
        return "Metal";
    case InterfaceType::Agx:
        return "Agx";
    case InterfaceType::OpenGL:
        return "OpenGL";
    default:
        return "Unknown";
    }
}

gp::String featureLevelToString(const FeatureLevel featureLevel) noexcept
{
    switch (featureLevel)
    {
    case FeatureLevel::ES2Removed:
        return gp::String("ES2_REMOVED");
    case FeatureLevel::ES3_1:
        return gp::String("ES3_1");
    case FeatureLevel::SM4Removed:
        return gp::String("SM4_REMOVED");
    case FeatureLevel::SM5:
        return gp::String("SM5");
    case FeatureLevel::SM6:
        return gp::String("SM6");
    case FeatureLevel::Count:
    default:
        return gp::String("UnknownFeatureLevel");
    }
}

void getFeatureLevelName(const FeatureLevel featureLevel, gp::String& outName) noexcept
{
    const gp::Int32 index = static_cast<gp::Int32>(featureLevel);
    if (index >= 0 && static_cast<gp::USize>(index) < kFeatureLevelNames.size())
    {
        outName.assign(kFeatureLevelNames[static_cast<gp::USize>(index)]);
    }
    else
    {
        outName.assign("InvalidFeatureLevel");
    }
}

void getFeatureLevelName(const FeatureLevel featureLevel, gp::Name& outName) noexcept
{
    const gp::Int32 index = static_cast<gp::Int32>(featureLevel);
    if (index >= 0 && static_cast<gp::USize>(index) < kFeatureLevelNames.size())
    {
        outName = gp::Name(kFeatureLevelNames[static_cast<gp::USize>(index)]);
    }
    else
    {
        outName = gp::Name("InvalidFeatureLevel");
    }
}

bool getFeatureLevelFromName(const gp::StringView name, FeatureLevel& outFeatureLevel) noexcept
{
    for (gp::USize index = 0u; index < kFeatureLevelNames.size(); ++index)
    {
        if (name.equalsIgnoreCase(kFeatureLevelNames[index]))
        {
            outFeatureLevel = static_cast<FeatureLevel>(index);
            return true;
        }
    }

    if (name.equalsIgnoreCase("ES2_REMOVED"))
    {
        outFeatureLevel = FeatureLevel::ES2Removed;
        return true;
    }

    outFeatureLevel = FeatureLevel::Count;
    return false;
}

bool getFeatureLevelFromName(const gp::Name name, FeatureLevel& outFeatureLevel) noexcept
{
    if (name.hasNumber())
    {
        const gp::String fullName = name.toString();
        return getFeatureLevelFromName(fullName.view(), outFeatureLevel);
    }

    return getFeatureLevelFromName(name.baseView(), outFeatureLevel);
}

const char* getShadingPathString(const ShadingPath shadingPath) noexcept
{
    const gp::USize index = static_cast<gp::USize>(shadingPath);
    if (index < kShadingPathNames.size())
    {
        return kShadingPathNames[index];
    }
    return "InvalidShadingPath";
}

void getShadingPathName(const ShadingPath shadingPath, gp::String& outName) noexcept
{
    outName.assign(getShadingPathString(shadingPath));
}

void getShadingPathName(const ShadingPath shadingPath, gp::Name& outName) noexcept
{
    outName = gp::Name(getShadingPathString(shadingPath));
}

bool getShadingPathFromName(const gp::StringView name, ShadingPath& outShadingPath) noexcept
{
    for (gp::USize index = 0u; index < kShadingPathNames.size(); ++index)
    {
        if (name.equalsIgnoreCase(kShadingPathNames[index]))
        {
            outShadingPath = static_cast<ShadingPath>(index);
            return true;
        }
    }

    outShadingPath = static_cast<ShadingPath>(kShadingPathCount);
    return false;
}

bool getShadingPathFromName(const gp::Name name, ShadingPath& outShadingPath) noexcept
{
    if (name.hasNumber())
    {
        outShadingPath = static_cast<ShadingPath>(kShadingPathCount);
        return false;
    }

    return getShadingPathFromName(name.baseView(), outShadingPath);
}

const char* getBindlessConfigurationString(const view::BindlessConfiguration configuration) noexcept
{
    switch (configuration)
    {
    case view::BindlessConfiguration::Disabled:
        return "Disabled";
    case view::BindlessConfiguration::RayTracing:
        return "RayTracing";
    case view::BindlessConfiguration::Minimal:
        return "Minimal";
    case view::BindlessConfiguration::All:
        return "All";
    default:
        return "";
    }
}

bool getBindlessConfigurationFromString(
    const gp::StringView inString, view::BindlessConfiguration& outConfiguration
) noexcept
{
    if (inString.equalsIgnoreCase("Disabled"))
    {
        outConfiguration = view::BindlessConfiguration::Disabled;
        return true;
    }

    if (inString.equalsIgnoreCase("RayTracing"))
    {
        outConfiguration = view::BindlessConfiguration::RayTracing;
        return true;
    }

    if (inString.equalsIgnoreCase("Minimal"))
    {
        outConfiguration = view::BindlessConfiguration::Minimal;
        return true;
    }

    if (inString.equalsIgnoreCase("All"))
    {
        outConfiguration = view::BindlessConfiguration::All;
        return true;
    }

    return false;
}

const char* getDescriptorHeapTypeString(const view::DescriptorHeapType heapType) noexcept
{
    switch (heapType)
    {
    case view::DescriptorHeapType::Standard:
        return "Standard";
    case view::DescriptorHeapType::Sampler:
        return "Sampler";
    case view::DescriptorHeapType::RenderTarget:
        return "RenderTarget";
    case view::DescriptorHeapType::DepthStencil:
        return "DepthStencil";
    case view::DescriptorHeapType::Count:
    case view::DescriptorHeapType::Invalid:
    default:
        return "UnknownDescriptorHeapType";
    }
}

const char* getDescriptorTypeString(const view::DescriptorType descriptorType) noexcept
{
    switch (descriptorType)
    {
    case view::DescriptorType::BufferSRV:
        return "BufferSRV";
    case view::DescriptorType::BufferUAV:
        return "BufferUAV";
    case view::DescriptorType::TypedBufferSRV:
        return "TypedBufferSRV";
    case view::DescriptorType::TypedBufferUAV:
        return "TypedBufferUAV";
    case view::DescriptorType::TextureSRV:
        return "TextureSRV";
    case view::DescriptorType::TextureUAV:
        return "TextureUAV";
    case view::DescriptorType::CBV:
        return "CBV";
    case view::DescriptorType::Sampler:
        return "Sampler";
    case view::DescriptorType::AccelerationStructure:
        return "AccelerationStructure";
    case view::DescriptorType::Invalid:
        return "Invalid";
    default:
        return "";
    }
}

gp::String getDescriptorTypeMaskString(const view::DescriptorTypeMask typeMask) noexcept
{
    if (typeMask == view::DescriptorTypeMask::None)
    {
        return gp::String("<empty>");
    }

    gp::String result;
    result.reserve(64u);

    gp::UInt32 iterMask = static_cast<gp::UInt32>(typeMask);
    while (iterMask != 0u)
    {
        const gp::UInt32 bitIndex = static_cast<gp::UInt32>(gp::math::countTrailingZeros(iterMask));
        iterMask &= (iterMask - 1u);

        if (!result.isEmpty())
        {
            result.pushBack('|');
        }

        const char* const typeStr = getDescriptorTypeString(static_cast<view::DescriptorType>(bitIndex));
        result.append((typeStr[0] != '\0') ? typeStr : "Unknown");
    }

    return result;
}

const char* stringFromRHIResourceType(const resource::ResourceType resourceType) noexcept
{
    const gp::USize index = static_cast<gp::USize>(resourceType);
    if (index < kResourceTypeNames.size() && kResourceTypeNames[index].type == resourceType)
    {
        return kResourceTypeNames[index].name;
    }

    for (const ResourceTypeNameEntry& entry: kResourceTypeNames)
    {
        if (entry.type == resourceType)
        {
            return entry.name;
        }
    }

    return "<unknown>";
}

resource::ResourceType rhiResourceTypeFromString(const gp::StringView inString) noexcept
{
    for (const ResourceTypeNameEntry& entry: kResourceTypeNames)
    {
        if (inString.equalsIgnoreCase(entry.name))
        {
            return entry.type;
        }
    }

    return resource::ResourceType::None;
}

const char* getBufferUsageFlagString(const resource::BufferUsageFlags singleFlag) noexcept
{
    constexpr gp::UInt32 kRayTracingScratchExclusiveBit = 1u << 19u;
    if (static_cast<gp::UInt32>(singleFlag) == kRayTracingScratchExclusiveBit)
    {
        return "RayTracingScratch";
    }

    switch (singleFlag)
    {
    case resource::BufferUsageFlags::None:
        return "None";
    case resource::BufferUsageFlags::Static:
        return "Static";
    case resource::BufferUsageFlags::Dynamic:
        return "Dynamic";
    case resource::BufferUsageFlags::Volatile:
        return "Volatile";
    case resource::BufferUsageFlags::UnorderedAccess:
        return "UnorderedAccess";
    case resource::BufferUsageFlags::ByteAddressBuffer:
        return "ByteAddressBuffer";
    case resource::BufferUsageFlags::SourceCopy:
        return "SourceCopy";
    case resource::BufferUsageFlags::DrawIndirect:
        return "DrawIndirect";
    case resource::BufferUsageFlags::ShaderResource:
        return "ShaderResource";
    case resource::BufferUsageFlags::KeepCPUAccessible:
        return "KeepCPUAccessible";
    case resource::BufferUsageFlags::FastVRAM:
        return "FastVRAM";
    case resource::BufferUsageFlags::NNE:
        return "NNE";
    case resource::BufferUsageFlags::Shared:
        return "Shared";
    case resource::BufferUsageFlags::AccelerationStructure:
        return "AccelerationStructure";
    case resource::BufferUsageFlags::VertexBuffer:
        return "VertexBuffer";
    case resource::BufferUsageFlags::IndexBuffer:
        return "IndexBuffer";
    case resource::BufferUsageFlags::StructuredBuffer:
        return "StructuredBuffer";
    case resource::BufferUsageFlags::MultiGPUAllocate:
        return "MultiGPUAllocate";
    case resource::BufferUsageFlags::MultiGPUGraphIgnore:
        return "MultiGPUGraphIgnore";
    case resource::BufferUsageFlags::RayTracingScratch:
        return "RayTracingScratch";
    case resource::BufferUsageFlags::NullResource:
        return "NullResource";
    case resource::BufferUsageFlags::UniformBuffer:
        return "UniformBuffer";
    case resource::BufferUsageFlags::ReservedResource:
        return "ReservedResource";
    default:
        return "";
    }
}

gp::String getBufferUsageFlagsName(const resource::BufferUsageFlags bufferUsage) noexcept
{
    return buildEnumNameBitList<resource::BufferUsageFlags>(bufferUsage, &getPrefixedBufferUsageBitName);
}

const char* getTextureDimensionString(const resource::TextureDimension dimension) noexcept
{
    switch (dimension)
    {
    case resource::TextureDimension::Texture2D:
        return "Texture2D";
    case resource::TextureDimension::Texture2DArray:
        return "Texture2DArray";
    case resource::TextureDimension::Texture3D:
        return "Texture3D";
    case resource::TextureDimension::TextureCube:
        return "TextureCube";
    case resource::TextureDimension::TextureCubeArray:
        return "TextureCubeArray";
    default:
        return "";
    }
}

const char* getTextureCreateFlagString(const resource::TextureCreateFlags singleFlag) noexcept
{
    switch (singleFlag)
    {
    case resource::TextureCreateFlags::None:
        return "None";
    case resource::TextureCreateFlags::RenderTargetable:
        return "RenderTargetable";
    case resource::TextureCreateFlags::ResolveTargetable:
        return "ResolveTargetable";
    case resource::TextureCreateFlags::DepthStencilTargetable:
        return "DepthStencilTargetable";
    case resource::TextureCreateFlags::ShaderResource:
        return "ShaderResource";
    case resource::TextureCreateFlags::SRGB:
        return "SRGB";
    case resource::TextureCreateFlags::CPUWritable:
        return "CPUWritable";
    case resource::TextureCreateFlags::NoTiling:
        return "NoTiling";
    case resource::TextureCreateFlags::VideoDecode:
        return "VideoDecode";
    case resource::TextureCreateFlags::Dynamic:
        return "Dynamic";
    case resource::TextureCreateFlags::InputAttachmentRead:
        return "InputAttachmentRead";
    case resource::TextureCreateFlags::Foveation:
        return "Foveation";
    case resource::TextureCreateFlags::Tiling3D:
        return "Tiling3D";
    case resource::TextureCreateFlags::Memoryless:
        return "Memoryless";
    case resource::TextureCreateFlags::FastVRAMPartialAlloc:
        return "FastVRAMPartialAlloc";
    case resource::TextureCreateFlags::DisableSRVCreation:
        return "DisableSRVCreation";
    case resource::TextureCreateFlags::DisableDCC:
        return "DisableDCC";
    case resource::TextureCreateFlags::UAV:
        return "UAV";
    case resource::TextureCreateFlags::Presentable:
        return "Presentable";
    case resource::TextureCreateFlags::CPUReadback:
        return "CPUReadback";
    case resource::TextureCreateFlags::OfflineProcessed:
        return "OfflineProcessed";
    case resource::TextureCreateFlags::FastVRAM:
        return "FastVRAM";
    case resource::TextureCreateFlags::HideInVisualizeTexture:
        return "HideInVisualizeTexture";
    case resource::TextureCreateFlags::Virtual:
        return "Virtual";
    case resource::TextureCreateFlags::TargetArraySlicesIndependently:
        return "TargetArraySlicesIndependently";
    case resource::TextureCreateFlags::Shared:
        return "Shared";
    case resource::TextureCreateFlags::NoFastClear:
        return "NoFastClear";
    case resource::TextureCreateFlags::DepthStencilResolveTarget:
        return "DepthStencilResolveTarget";
    case resource::TextureCreateFlags::Streamable:
        return "Streamable";
    case resource::TextureCreateFlags::NoFastClearFinalize:
        return "NoFastClearFinalize";
    case resource::TextureCreateFlags::Atomic64Compatible:
        return "Atomic64Compatible";
    case resource::TextureCreateFlags::ReduceMemoryWithTilingMode:
        return "ReduceMemoryWithTilingMode";
    case resource::TextureCreateFlags::AtomicCompatible:
        return "AtomicCompatible";
    case resource::TextureCreateFlags::External:
        return "External";
    case resource::TextureCreateFlags::MultiGPUGraphIgnore:
        return "MultiGPUGraphIgnore";
    case resource::TextureCreateFlags::ReservedResource:
        return "ReservedResource";
    case resource::TextureCreateFlags::ImmediateCommit:
        return "ImmediateCommit";
    case resource::TextureCreateFlags::ForceIntoNonStreamingMemoryTracking:
        return "ForceIntoNonStreamingMemoryTracking";
    case resource::TextureCreateFlags::Invalid:
        return "Invalid";
    case resource::TextureCreateFlags::LossyCompression:
        return "LossyCompression";
    case resource::TextureCreateFlags::LossyCompressionLowBitrate:
        return "LossyCompressionLowBitrate";
    default:
        return "";
    }
}

gp::String getTextureCreateFlagsName(const resource::TextureCreateFlags textureCreateFlags) noexcept
{
    return buildEnumNameBitList<resource::TextureCreateFlags>(textureCreateFlags, &getTextureCreateBitNameOrUnknown);
}

gp::String getRHIAccessName(const sync::ResourceAccess access) noexcept
{
    return buildEnumNameBitList<sync::ResourceAccess>(access, &getResourceAccessBitName);
}

gp::String getResourceTransitionFlagsName(const sync::ResourceTransitionFlags flags) noexcept
{
    return buildEnumNameBitList<sync::ResourceTransitionFlags>(flags, &getResourceTransitionBitName);
}

gp::String getRHIPipelineName(const sync::PipelineStage pipeline) noexcept
{
    return buildEnumNameBitList<sync::PipelineStage>(pipeline, &getPipelineStageBitName);
}

const char* getRenderQueryTypeString(const sync::RenderQueryType queryType) noexcept
{
    switch (queryType)
    {
    case sync::RenderQueryType::Occlusion:
        return "GPUOcclusion";
    case sync::RenderQueryType::AbsoluteTime:
        return "GPUAbsoluteTime";
    case sync::RenderQueryType::Undefined:
    default:
        return "Undefined";
    }
}

gp::Name getRenderQueryTypeName(const sync::RenderQueryType queryType) noexcept
{
    return gp::Name(getRenderQueryTypeString(queryType));
}

const char* getVertexElementTypeString(const pipeline::VertexElementType elementType) noexcept
{
    switch (elementType)
    {
    case pipeline::VertexElementType::None:
        return "None";
    case pipeline::VertexElementType::Float1:
        return "Float1";
    case pipeline::VertexElementType::Float2:
        return "Float2";
    case pipeline::VertexElementType::Float3:
        return "Float3";
    case pipeline::VertexElementType::Float4:
        return "Float4";
    case pipeline::VertexElementType::PackedNormal:
        return "PackedNormal";
    case pipeline::VertexElementType::UByte4:
        return "UByte4";
    case pipeline::VertexElementType::UByte4N:
        return "UByte4N";
    case pipeline::VertexElementType::Color:
        return "Color";
    case pipeline::VertexElementType::Short2:
        return "Short2";
    case pipeline::VertexElementType::Short4:
        return "Short4";
    case pipeline::VertexElementType::Short2N:
        return "Short2N";
    case pipeline::VertexElementType::Half2:
        return "Half2";
    case pipeline::VertexElementType::Half4:
        return "Half4";
    case pipeline::VertexElementType::Short4N:
        return "Short4N";
    case pipeline::VertexElementType::UShort2:
        return "UShort2";
    case pipeline::VertexElementType::UShort4:
        return "UShort4";
    case pipeline::VertexElementType::UShort2N:
        return "UShort2N";
    case pipeline::VertexElementType::UShort4N:
        return "UShort4N";
    case pipeline::VertexElementType::URGB10A2N:
        return "URGB10A2N";
    case pipeline::VertexElementType::UInt:
        return "UInt";
    case pipeline::VertexElementType::UInt2:
        return "UInt2";
    default:
        return "Unknown";
    }
}

const char* getPrimitiveTypeString(const pipeline::PrimitiveType primitiveType) noexcept
{
    switch (primitiveType)
    {
    case pipeline::PrimitiveType::TriangleList:
        return "TriangleList";
    case pipeline::PrimitiveType::TriangleStrip:
        return "TriangleStrip";
    case pipeline::PrimitiveType::LineList:
        return "LineList";
    case pipeline::PrimitiveType::QuadList:
        return "QuadList";
    case pipeline::PrimitiveType::PointList:
        return "PointList";
    case pipeline::PrimitiveType::RectList:
        return "RectList";
    default:
        return "Unknown";
    }
}

const char* getPixelFormatString(const PixelFormat format) noexcept
{
    return getPixelFormatInfo(format).name.data();
}

gp::String getPixelFormatCapabilitiesName(const PixelFormatCapabilities capabilities) noexcept
{
    return buildEnumNameBitList<PixelFormatCapabilities>(capabilities, &getPixelFormatCapabilityBitName);
}

const char* getUniformBufferBaseTypeString(const shader::UniformBufferBaseType baseType) noexcept
{
    switch (baseType)
    {
    case shader::UniformBufferBaseType::Invalid:
        return "UBMT_INVALID";
    case shader::UniformBufferBaseType::Bool:
        return "UBMT_BOOL";
    case shader::UniformBufferBaseType::Int32:
        return "UBMT_INT32";
    case shader::UniformBufferBaseType::UInt32:
        return "UBMT_UINT32";
    case shader::UniformBufferBaseType::Float32:
        return "UBMT_FLOAT32";
    case shader::UniformBufferBaseType::Texture:
        return "UBMT_TEXTURE";
    case shader::UniformBufferBaseType::SRV:
        return "UBMT_SRV";
    case shader::UniformBufferBaseType::UAV:
        return "UBMT_UAV";
    case shader::UniformBufferBaseType::Sampler:
        return "UBMT_SAMPLER";
    case shader::UniformBufferBaseType::NestedStruct:
        return "UBMT_NESTED_STRUCT";
    case shader::UniformBufferBaseType::IncludedStruct:
        return "UBMT_INCLUDED_STRUCT";
    case shader::UniformBufferBaseType::ReferencedStruct:
        return "UBMT_REFERENCED_STRUCT";
    case shader::UniformBufferBaseType::RenderTargetBindingSlots:
        return "UBMT_RENDER_TARGET_BINDING_SLOTS";
    case shader::UniformBufferBaseType::ResourceCollection:
        return "UBMT_RESOURCE_COLLECTION";
    case shader::UniformBufferBaseType::DescriptorRange:
        return "UBMT_DESCRIPTOR_RANGE";
    case shader::UniformBufferBaseType::UniformBlock:
        return "UBMT_UNIFORM_BLOCK";
    default:
        return "";
    }
}

const char* getShaderCodeResourceBindingTypeName(const shader::ShaderCodeResourceBindingType bindingType) noexcept
{
    switch (bindingType)
    {
    case shader::ShaderCodeResourceBindingType::Invalid:
        return "Invalid";
    case shader::ShaderCodeResourceBindingType::SamplerState:
        return "SamplerState";
    case shader::ShaderCodeResourceBindingType::Texture2D:
        return "Texture2D";
    case shader::ShaderCodeResourceBindingType::Texture2DArray:
        return "Texture2DArray";
    case shader::ShaderCodeResourceBindingType::Texture2DMS:
        return "Texture2DMS";
    case shader::ShaderCodeResourceBindingType::Texture3D:
        return "Texture3D";
    case shader::ShaderCodeResourceBindingType::TextureCube:
        return "TextureCube";
    case shader::ShaderCodeResourceBindingType::TextureCubeArray:
        return "TextureCubeArray";
    case shader::ShaderCodeResourceBindingType::TextureMetadata:
        return "TextureMetadata";
    case shader::ShaderCodeResourceBindingType::Buffer:
        return "Buffer";
    case shader::ShaderCodeResourceBindingType::StructuredBuffer:
        return "StructuredBuffer";
    case shader::ShaderCodeResourceBindingType::ByteAddressBuffer:
        return "ByteAddressBuffer";
    case shader::ShaderCodeResourceBindingType::RaytracingAccelerationStructure:
        return "RaytracingAccelerationStructure";
    case shader::ShaderCodeResourceBindingType::RWTexture2D:
        return "RWTexture2D";
    case shader::ShaderCodeResourceBindingType::RWTexture2DArray:
        return "RWTexture2DArray";
    case shader::ShaderCodeResourceBindingType::RWTexture3D:
        return "RWTexture3D";
    case shader::ShaderCodeResourceBindingType::RWTextureCube:
        return "RWTextureCube";
    case shader::ShaderCodeResourceBindingType::RWTextureMetadata:
        return "RWTextureMetadata";
    case shader::ShaderCodeResourceBindingType::RWBuffer:
        return "RWBuffer";
    case shader::ShaderCodeResourceBindingType::RWStructuredBuffer:
        return "RWStructuredBuffer";
    case shader::ShaderCodeResourceBindingType::RWByteAddressBuffer:
        return "RWByteAddressBuffer";
    case shader::ShaderCodeResourceBindingType::RasterizerOrderedTexture2D:
        return "RasterizerOrderedTexture2D";
    case shader::ShaderCodeResourceBindingType::ResourceCollection:
        return "ResourceCollection";
    case shader::ShaderCodeResourceBindingType::DescriptorRange:
        return "DescriptorRange";
    case shader::ShaderCodeResourceBindingType::Texture2DMultiView:
        return "Texture2DMultiView";
    case shader::ShaderCodeResourceBindingType::RWTexture2DMultiView:
        return "RWTexture2DMultiView";
    default:
        return "";
    }
}

}   // namespace gp::rhi
