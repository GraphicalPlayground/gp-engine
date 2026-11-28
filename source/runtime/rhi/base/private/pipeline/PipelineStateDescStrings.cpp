// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "pipeline/BlendStateDesc.hpp"
#include "pipeline/DepthStencilStateDesc.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/RasterizerStateDesc.hpp"
#include "pipeline/SamplerStateDesc.hpp"
#include "pipeline/VertexDeclarationDesc.hpp"
#include "RHIDefinitions.hpp"
#include "templates/NumericLimits.hpp"
#include <charconv>
#include <cmath>
#include <system_error>

namespace gp::rhi::pipeline
{

namespace
{

constexpr gp::USize kMaxDescriptorTokens = 64u;

[[nodiscard]] constexpr bool isDescriptorDelimiter(const char ch) noexcept
{
    return (ch == ' ') || (ch == '\t') || (ch == '\r') || (ch == '\n') || (ch == '<') || (ch == '>');
}

[[nodiscard]] bool tokenizeDescriptorString(
    const gp::StringView src, gp::FixedVector<gp::StringView, kMaxDescriptorTokens>& outTokens
) noexcept
{
    outTokens.clear();

    const gp::USize len = src.size();
    gp::USize pos = 0u;
    while (pos < len)
    {
        while (pos < len && isDescriptorDelimiter(src[pos]))
        {
            ++pos;
        }

        if (pos >= len)
        {
            break;
        }

        const gp::USize start = pos;
        while (pos < len && !isDescriptorDelimiter(src[pos]))
        {
            ++pos;
        }

        if (outTokens.size() >= kMaxDescriptorTokens)
        {
            return false;
        }

        outTokens.emplaceBack(src.substr(start, pos - start));
    }

    return true;
}

void appendUInt32(gp::String& outStr, const gp::UInt32 value) noexcept
{
    char buffer[16]{};
    const std::to_chars_result res = std::to_chars(buffer, buffer + sizeof(buffer), value, 10);
    if (res.ec == std::errc{})
    {
        outStr.append(buffer, static_cast<gp::USize>(res.ptr - buffer));
    }
}

void appendInt32(gp::String& outStr, const gp::Int32 value) noexcept
{
    char buffer[16]{};
    const std::to_chars_result res = std::to_chars(buffer, buffer + sizeof(buffer), value, 10);
    if (res.ec == std::errc{})
    {
        outStr.append(buffer, static_cast<gp::USize>(res.ptr - buffer));
    }
}

void appendFloat32(gp::String& outStr, const gp::Float32 value) noexcept
{
    const gp::Float32 normalized = (value == 0.0f) ? 0.0f : value;
    char buffer[32]{};
    const std::to_chars_result res = std::to_chars(buffer, buffer + sizeof(buffer), normalized);
    if (res.ec == std::errc{})
    {
        outStr.append(buffer, static_cast<gp::USize>(res.ptr - buffer));
    }
    else
    {
        outStr.pushBack('0');
    }
}

void appendBool(gp::String& outStr, const bool value) noexcept
{
    outStr.pushBack(value ? '1' : '0');
}

[[nodiscard]] bool parseUInt32(const gp::StringView token, gp::UInt32& outValue) noexcept
{
    if (token.isEmpty())
    {
        return false;
    }

    const char* const begin = token.data();
    const char* const end = begin + token.size();
    const std::from_chars_result res = std::from_chars(begin, end, outValue, 10);
    return (res.ec == std::errc{}) && (res.ptr == end);
}

[[nodiscard]] bool parseInt32(const gp::StringView token, gp::Int32& outValue) noexcept
{
    if (token.isEmpty())
    {
        return false;
    }

    const char* const begin = token.data();
    const char* const end = begin + token.size();
    const std::from_chars_result res = std::from_chars(begin, end, outValue, 10);
    return (res.ec == std::errc{}) && (res.ptr == end);
}

[[nodiscard]] bool parseFloat32(const gp::StringView token, gp::Float32& outValue) noexcept
{
    if (token.isEmpty())
    {
        return false;
    }

    const char* const begin = token.data();
    const char* const end = begin + token.size();
    gp::Float32 parsed = 0.0f;
    const std::from_chars_result res = std::from_chars(begin, end, parsed);
    if (res.ec != std::errc{} || res.ptr != end || !std::isfinite(parsed))
    {
        return false;
    }

    outValue = (parsed == 0.0f) ? 0.0f : parsed;
    return true;
}

[[nodiscard]] bool parseBool(const gp::StringView token, bool& outValue) noexcept
{
    if (token == "0" || token.equalsIgnoreCase("false"))
    {
        outValue = false;
        return true;
    }

    if (token == "1" || token.equalsIgnoreCase("true"))
    {
        outValue = true;
        return true;
    }

    return false;
}

}   // namespace

gp::String SamplerStateDesc::toString() const noexcept
{
    gp::String result;
    result.reserve(64u);
    result.pushBack('<');
    appendUInt32(result, static_cast<gp::UInt32>(filter));
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(addressU));
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(addressV));
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(addressW));
    result.pushBack(' ');
    appendFloat32(result, mipBias);
    result.pushBack(' ');
    appendFloat32(result, minMipLevel);
    result.pushBack(' ');
    appendFloat32(result, maxMipLevel);
    result.pushBack(' ');
    appendInt32(result, maxAnisotropy);
    result.pushBack(' ');
    appendUInt32(result, borderColor);
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(samplerComparisonFunction));
    result.pushBack('>');
    return result;
}

bool SamplerStateDesc::fromString(const gp::StringView src) noexcept
{
    gp::FixedVector<gp::StringView, kMaxDescriptorTokens> tokens{};
    if (!tokenizeDescriptorString(src, tokens) || tokens.size() != 10u)
    {
        return false;
    }

    gp::UInt32 filterRaw = 0u;
    gp::UInt32 addrURaw = 0u;
    gp::UInt32 addrVRaw = 0u;
    gp::UInt32 addrWRaw = 0u;
    gp::Float32 parsedMipBias = 0.0f;
    gp::Float32 parsedMinMip = 0.0f;
    gp::Float32 parsedMaxMip = 0.0f;
    gp::Int32 parsedAniso = 0;
    gp::UInt32 parsedBorderColor = 0u;
    gp::UInt32 compareRaw = 0u;

    if (!parseUInt32(tokens[0], filterRaw) || filterRaw >= kSamplerFilterCount || !parseUInt32(tokens[1], addrURaw) ||
        addrURaw >= kSamplerAddressModeCount || !parseUInt32(tokens[2], addrVRaw) ||
        addrVRaw >= kSamplerAddressModeCount || !parseUInt32(tokens[3], addrWRaw) ||
        addrWRaw >= kSamplerAddressModeCount || !parseFloat32(tokens[4], parsedMipBias) ||
        !parseFloat32(tokens[5], parsedMinMip) || !parseFloat32(tokens[6], parsedMaxMip) ||
        !parseInt32(tokens[7], parsedAniso) || parsedAniso < 0 || !parseUInt32(tokens[8], parsedBorderColor) ||
        !parseUInt32(tokens[9], compareRaw) || compareRaw > static_cast<gp::UInt32>(SamplerCompareFunction::Less))
    {
        return false;
    }

    filter = static_cast<SamplerFilter>(filterRaw);
    addressU = static_cast<SamplerAddressMode>(addrURaw);
    addressV = static_cast<SamplerAddressMode>(addrVRaw);
    addressW = static_cast<SamplerAddressMode>(addrWRaw);
    mipBias = parsedMipBias;
    minMipLevel = parsedMinMip;
    maxMipLevel = parsedMaxMip;
    maxAnisotropy = parsedAniso;
    borderColor = parsedBorderColor;
    samplerComparisonFunction = static_cast<SamplerCompareFunction>(compareRaw);
    return true;
}

gp::String RasterizerStateDesc::toString() const noexcept
{
    gp::String result;
    result.reserve(48u);
    result.pushBack('<');
    appendFloat32(result, depthBias);
    result.pushBack(' ');
    appendFloat32(result, slopeScaleDepthBias);
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(fillMode));
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(cullMode));
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(depthClipMode));
    result.pushBack(' ');
    appendBool(result, allowMSAA);
    result.pushBack('>');
    return result;
}

bool RasterizerStateDesc::fromString(const gp::StringView src) noexcept
{
    gp::FixedVector<gp::StringView, kMaxDescriptorTokens> tokens{};
    if (!tokenizeDescriptorString(src, tokens) || tokens.size() != 6u)
    {
        return false;
    }

    gp::Float32 parsedDepthBias = 0.0f;
    gp::Float32 parsedSlopeBias = 0.0f;
    gp::UInt32 fillRaw = 0u;
    gp::UInt32 cullRaw = 0u;
    gp::UInt32 clipRaw = 0u;
    bool parsedAllowMSAA = false;

    if (!parseFloat32(tokens[0], parsedDepthBias) || !parseFloat32(tokens[1], parsedSlopeBias) ||
        !parseUInt32(tokens[2], fillRaw) || fillRaw >= kRasterizerFillModeCount || !parseUInt32(tokens[3], cullRaw) ||
        cullRaw >= kRasterizerCullModeCount || !parseUInt32(tokens[4], clipRaw) ||
        clipRaw >= kRasterizerDepthClipModeCount || !parseBool(tokens[5], parsedAllowMSAA))
    {
        return false;
    }

    depthBias = parsedDepthBias;
    slopeScaleDepthBias = parsedSlopeBias;
    fillMode = static_cast<RasterizerFillMode>(fillRaw);
    cullMode = static_cast<RasterizerCullMode>(cullRaw);
    depthClipMode = static_cast<RasterizerDepthClipMode>(clipRaw);
    allowMSAA = parsedAllowMSAA;
    return true;
}

gp::String DepthStencilStateDesc::toString() const noexcept
{
    gp::String result;
    result.reserve(48u);
    result.pushBack('<');
    appendBool(result, enableDepthWrite);
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(depthTest));
    result.pushBack(' ');
    appendBool(result, enableFrontFaceStencil);
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(frontFaceStencilTest));
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(frontFaceStencilFailOp));
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(frontFaceDepthFailOp));
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(frontFacePassOp));
    result.pushBack(' ');
    appendBool(result, enableBackFaceStencil);
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(backFaceStencilTest));
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(backFaceStencilFailOp));
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(backFaceDepthFailOp));
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(backFacePassOp));
    result.pushBack(' ');
    appendUInt32(result, stencilReadMask);
    result.pushBack(' ');
    appendUInt32(result, stencilWriteMask);
    result.pushBack('>');
    return result;
}

bool DepthStencilStateDesc::fromString(const gp::StringView src) noexcept
{
    gp::FixedVector<gp::StringView, kMaxDescriptorTokens> tokens{};
    if (!tokenizeDescriptorString(src, tokens) || tokens.size() != 14u)
    {
        return false;
    }

    bool parsedDepthWrite = false;
    gp::UInt32 depthTestRaw = 0u;
    bool parsedFrontStencil = false;
    gp::UInt32 frontTestRaw = 0u;
    gp::UInt32 frontFailRaw = 0u;
    gp::UInt32 frontDepthFailRaw = 0u;
    gp::UInt32 frontPassRaw = 0u;
    bool parsedBackStencil = false;
    gp::UInt32 backTestRaw = 0u;
    gp::UInt32 backFailRaw = 0u;
    gp::UInt32 backDepthFailRaw = 0u;
    gp::UInt32 backPassRaw = 0u;
    gp::UInt32 readMaskRaw = 0u;
    gp::UInt32 writeMaskRaw = 0u;

    if (!parseBool(tokens[0], parsedDepthWrite) || !parseUInt32(tokens[1], depthTestRaw) ||
        depthTestRaw >= kCompareFunctionCount || !parseBool(tokens[2], parsedFrontStencil) ||
        !parseUInt32(tokens[3], frontTestRaw) || frontTestRaw >= kCompareFunctionCount ||
        !parseUInt32(tokens[4], frontFailRaw) || frontFailRaw >= kStencilOpCount ||
        !parseUInt32(tokens[5], frontDepthFailRaw) || frontDepthFailRaw >= kStencilOpCount ||
        !parseUInt32(tokens[6], frontPassRaw) || frontPassRaw >= kStencilOpCount ||
        !parseBool(tokens[7], parsedBackStencil) || !parseUInt32(tokens[8], backTestRaw) ||
        backTestRaw >= kCompareFunctionCount || !parseUInt32(tokens[9], backFailRaw) ||
        backFailRaw >= kStencilOpCount || !parseUInt32(tokens[10], backDepthFailRaw) ||
        backDepthFailRaw >= kStencilOpCount || !parseUInt32(tokens[11], backPassRaw) ||
        backPassRaw >= kStencilOpCount || !parseUInt32(tokens[12], readMaskRaw) ||
        readMaskRaw > static_cast<gp::UInt32>(gp::NumericLimits<gp::UInt8>::max()) ||
        !parseUInt32(tokens[13], writeMaskRaw) ||
        writeMaskRaw > static_cast<gp::UInt32>(gp::NumericLimits<gp::UInt8>::max()))
    {
        return false;
    }

    enableDepthWrite = parsedDepthWrite;
    depthTest = static_cast<CompareFunction>(depthTestRaw);
    enableFrontFaceStencil = parsedFrontStencil;
    frontFaceStencilTest = static_cast<CompareFunction>(frontTestRaw);
    frontFaceStencilFailOp = static_cast<StencilOp>(frontFailRaw);
    frontFaceDepthFailOp = static_cast<StencilOp>(frontDepthFailRaw);
    frontFacePassOp = static_cast<StencilOp>(frontPassRaw);
    enableBackFaceStencil = parsedBackStencil;
    backFaceStencilTest = static_cast<CompareFunction>(backTestRaw);
    backFaceStencilFailOp = static_cast<StencilOp>(backFailRaw);
    backFaceDepthFailOp = static_cast<StencilOp>(backDepthFailRaw);
    backFacePassOp = static_cast<StencilOp>(backPassRaw);
    stencilReadMask = static_cast<gp::UInt8>(readMaskRaw);
    stencilWriteMask = static_cast<gp::UInt8>(writeMaskRaw);
    return true;
}

gp::String RenderTargetBlendDesc::toString() const noexcept
{
    gp::String result;
    result.reserve(24u);
    appendUInt32(result, static_cast<gp::UInt32>(colorBlendOp));
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(colorSrcBlend));
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(colorDestBlend));
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(alphaBlendOp));
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(alphaSrcBlend));
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(alphaDestBlend));
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(colorWriteMask));
    result.pushBack(' ');
    return result;
}

bool RenderTargetBlendDesc::fromString(const gp::ConstVectorView<gp::StringView> parts) noexcept
{
    if (parts.size() != kNumStringFields)
    {
        return false;
    }

    gp::UInt32 colorOpRaw = 0u;
    gp::UInt32 colorSrcRaw = 0u;
    gp::UInt32 colorDstRaw = 0u;
    gp::UInt32 alphaOpRaw = 0u;
    gp::UInt32 alphaSrcRaw = 0u;
    gp::UInt32 alphaDstRaw = 0u;
    gp::UInt32 writeMaskRaw = 0u;

    if (!parseUInt32(parts[0], colorOpRaw) || colorOpRaw >= kBlendOperationCount ||
        !parseUInt32(parts[1], colorSrcRaw) || colorSrcRaw >= kBlendFactorCount ||
        !parseUInt32(parts[2], colorDstRaw) || colorDstRaw >= kBlendFactorCount || !parseUInt32(parts[3], alphaOpRaw) ||
        alphaOpRaw >= kBlendOperationCount || !parseUInt32(parts[4], alphaSrcRaw) || alphaSrcRaw >= kBlendFactorCount ||
        !parseUInt32(parts[5], alphaDstRaw) || alphaDstRaw >= kBlendFactorCount ||
        !parseUInt32(parts[6], writeMaskRaw) || writeMaskRaw > static_cast<gp::UInt32>(ColorWriteMask::RGBA))
    {
        return false;
    }

    colorBlendOp = static_cast<BlendOperation>(colorOpRaw);
    colorSrcBlend = static_cast<BlendFactor>(colorSrcRaw);
    colorDestBlend = static_cast<BlendFactor>(colorDstRaw);
    alphaBlendOp = static_cast<BlendOperation>(alphaOpRaw);
    alphaSrcBlend = static_cast<BlendFactor>(alphaSrcRaw);
    alphaDestBlend = static_cast<BlendFactor>(alphaDstRaw);
    colorWriteMask = static_cast<ColorWriteMask>(writeMaskRaw);
    return true;
}

bool RenderTargetBlendDesc::fromString(const gp::StringView src) noexcept
{
    gp::FixedVector<gp::StringView, kMaxDescriptorTokens> tokens{};
    if (!tokenizeDescriptorString(src, tokens) || tokens.size() != kNumStringFields)
    {
        return false;
    }

    return fromString(gp::ConstVectorView<gp::StringView>(tokens.data(), tokens.size()));
}

gp::String BlendStateDesc::toString() const noexcept
{
    gp::String result;
    result.reserve(160u);
    result.pushBack('<');
    for (gp::USize i = 0u; i < kMaxSimultaneousRenderTargets; ++i)
    {
        result.append(renderTargets[i].toString());
    }
    appendBool(result, useIndependentRenderTargetBlendStates);
    result.pushBack(' ');
    appendBool(result, useAlphaToCoverage);
    result.pushBack('>');
    return result;
}

bool BlendStateDesc::fromString(const gp::StringView src) noexcept
{
    constexpr gp::USize kRenderTargetTokenCount =
        kMaxSimultaneousRenderTargets * RenderTargetBlendDesc::kNumStringFields;
    constexpr gp::USize kLegacyTokenCount = kRenderTargetTokenCount + 1u;
    constexpr gp::USize kCurrentTokenCount = kRenderTargetTokenCount + 2u;

    gp::FixedVector<gp::StringView, kMaxDescriptorTokens> tokens{};
    if (!tokenizeDescriptorString(src, tokens) ||
        (tokens.size() != kCurrentTokenCount && tokens.size() != kLegacyTokenCount))
    {
        return false;
    }

    BlendStateDesc parsed{};
    for (gp::USize i = 0u; i < kMaxSimultaneousRenderTargets; ++i)
    {
        const gp::USize sliceOffset = i * RenderTargetBlendDesc::kNumStringFields;
        const gp::ConstVectorView<gp::StringView> slice(
            tokens.data() + sliceOffset, RenderTargetBlendDesc::kNumStringFields
        );
        if (!parsed.renderTargets[i].fromString(slice))
        {
            return false;
        }
    }

    if (!parseBool(tokens[kRenderTargetTokenCount], parsed.useIndependentRenderTargetBlendStates))
    {
        return false;
    }

    if (tokens.size() == kCurrentTokenCount)
    {
        if (!parseBool(tokens[kRenderTargetTokenCount + 1u], parsed.useAlphaToCoverage))
        {
            return false;
        }
    }
    else
    {
        parsed.useAlphaToCoverage = false;
    }

    *this = parsed;
    return true;
}

gp::String VertexElement::toString() const noexcept
{
    gp::String result;
    result.reserve(32u);
    result.pushBack('<');
    appendUInt32(result, streamIndex);
    result.pushBack(' ');
    appendUInt32(result, offset);
    result.pushBack(' ');
    appendUInt32(result, static_cast<gp::UInt32>(type));
    result.pushBack(' ');
    appendUInt32(result, attributeIndex);
    result.pushBack(' ');
    appendUInt32(result, stride);
    result.pushBack(' ');
    appendUInt32(result, useInstanceIndex);
    result.pushBack('>');
    return result;
}

bool VertexElement::fromString(const gp::StringView src) noexcept
{
    gp::FixedVector<gp::StringView, kMaxDescriptorTokens> tokens{};
    if (!tokenizeDescriptorString(src, tokens) || tokens.size() != 6u)
    {
        return false;
    }

    gp::UInt32 streamRaw = 0u;
    gp::UInt32 offsetRaw = 0u;
    gp::UInt32 typeRaw = 0u;
    gp::UInt32 attrRaw = 0u;
    gp::UInt32 strideRaw = 0u;
    gp::UInt32 instancedRaw = 0u;

    if (!parseUInt32(tokens[0], streamRaw) ||
        streamRaw > static_cast<gp::UInt32>(gp::NumericLimits<gp::UInt8>::max()) ||
        !parseUInt32(tokens[1], offsetRaw) ||
        offsetRaw > static_cast<gp::UInt32>(gp::NumericLimits<gp::UInt8>::max()) || !parseUInt32(tokens[2], typeRaw) ||
        typeRaw >= kVertexElementTypeCount || !parseUInt32(tokens[3], attrRaw) ||
        attrRaw > static_cast<gp::UInt32>(gp::NumericLimits<gp::UInt8>::max()) || !parseUInt32(tokens[4], strideRaw) ||
        strideRaw > static_cast<gp::UInt32>(gp::NumericLimits<gp::UInt16>::max()) ||
        !parseUInt32(tokens[5], instancedRaw) ||
        instancedRaw > static_cast<gp::UInt32>(gp::NumericLimits<gp::UInt16>::max()))
    {
        return false;
    }

    streamIndex = static_cast<gp::UInt8>(streamRaw);
    offset = static_cast<gp::UInt8>(offsetRaw);
    type = static_cast<VertexElementType>(typeRaw);
    attributeIndex = static_cast<gp::UInt8>(attrRaw);
    stride = static_cast<gp::UInt16>(strideRaw);
    useInstanceIndex = static_cast<gp::UInt16>(instancedRaw);
    return true;
}

}   // namespace gp::rhi::pipeline
