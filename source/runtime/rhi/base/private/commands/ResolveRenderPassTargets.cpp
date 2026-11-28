// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/ResolveRenderPassTargets.hpp"
#include "commands/RenderPassInfo.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureDesc.hpp"
#include "RHIDefinitions.hpp"
#include "RHIPixelFormat.hpp"

namespace gp::rhi::command
{

void resolveRenderPassTargets(
    const RenderPassInfo& info, const gp::FunctionRef<void(ResolveTextureInfo)> resolveFunction
) noexcept
{
    const auto resolveTexture = [&resolveFunction](const ResolveTextureInfo& resolveInfo) noexcept -> void
    {
        if (resolveInfo.sourceTexture == nullptr || resolveInfo.destTexture == nullptr ||
            resolveInfo.sourceTexture == resolveInfo.destTexture)
        {
            return;
        }

        const resource::TextureDesc& sourceDesc = resolveInfo.sourceTexture->getDesc();
        const resource::TextureDesc& destDesc = resolveInfo.destTexture->getDesc();

        GP_CHECK(sourceDesc.format == destDesc.format);
        GP_CHECK(sourceDesc.extent == destDesc.extent);
        GP_CHECK(sourceDesc.isMultisample() && !destDesc.isMultisample());
        GP_CHECK(!isDepthOrStencilFormat(sourceDesc.format) || (sourceDesc.isTexture2D() && destDesc.isTexture2D()));
        GP_CHECK(!sourceDesc.isTexture3D() && !destDesc.isTexture3D());

        resolveFunction(resolveInfo);
    };

    for (gp::UInt32 index = 0u; index < kMaxSimultaneousRenderTargets; ++index)
    {
        const RenderPassInfo::ColorEntry& rtv = info.colorRenderTargets[index];
        resolveTexture(
            ResolveTextureInfo(rtv.renderTarget, rtv.resolveTarget, rtv.mipIndex, rtv.arraySlice, info.resolveRect)
        );
    }

    const RenderPassInfo::DepthStencilEntry& dsv = info.depthStencilRenderTarget;
    resolveTexture(ResolveTextureInfo(dsv.depthStencilTarget, dsv.resolveTarget, 0u, 0, info.resolveRect));
}

}   // namespace gp::rhi::command
