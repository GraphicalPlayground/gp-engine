// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "translation/VulkanPipelineEnums.hpp"
#include "commands/CommandEnums.hpp"
#include "CoreMinimal.hpp"        // IWYU pragma: keep
#include "pipeline/PipelineEnums.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

static_assert(primitiveTypeToVkTopology(pipeline::PrimitiveType::TriangleList) == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
static_assert(
    primitiveTopologyTypeToVkTopology(pipeline::PrimitiveTopologyType::Patch) == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
);
static_assert(
    compareFunctionToVkCompareOp(pipeline::CompareFunction::DepthNearOrEqual) == VK_COMPARE_OP_GREATER_OR_EQUAL
);
static_assert(stencilOpToVkStencilOp(pipeline::StencilOp::SaturatedIncrement) == VK_STENCIL_OP_INCREMENT_AND_CLAMP);
static_assert(blendOperationToVkBlendOp(pipeline::BlendOperation::ReverseSubtract) == VK_BLEND_OP_REVERSE_SUBTRACT);
static_assert(
    blendFactorToVkBlendFactor(pipeline::BlendFactor::InverseSourceAlpha) == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
);
static_assert(
    colorWriteMaskToVkColorComponentFlags(pipeline::ColorWriteMask::RGBA) ==
    (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)
);
static_assert(rasterizerFillModeToVkPolygonMode(pipeline::RasterizerFillMode::Wireframe) == VK_POLYGON_MODE_LINE);
static_assert(rasterizerCullModeToVkCullMode(pipeline::RasterizerCullMode::CCW) == VK_CULL_MODE_BACK_BIT);
static_assert(
    vrsRateCombinerToVkCombinerOp(pipeline::VRSRateCombiner::Override) ==
    VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR
);
static_assert(vrsShadingRateToVkExtent2D(pipeline::VRSShadingRate::Rate2x4).width == 2u);
static_assert(vrsShadingRateToVkExtent2D(pipeline::VRSShadingRate::Rate2x4).height == 4u);
static_assert(renderTargetLoadActionToVkLoadOp(command::RenderTargetLoadAction::Clear) == VK_ATTACHMENT_LOAD_OP_CLEAR);
static_assert(
    renderTargetStoreActionToVkStoreOp(command::RenderTargetStoreAction::MultisampleResolve, true) ==
    VK_ATTACHMENT_STORE_OP_DONT_CARE
);

}   // namespace

TEST(VulkanPipelineEnumsTest, PrimitiveTopologyConversions)
{
    EXPECT_EQ(primitiveTypeToVkTopology(pipeline::PrimitiveType::PointList), VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
    EXPECT_EQ(primitiveTypeToVkTopology(pipeline::PrimitiveType::LineList), VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
    EXPECT_EQ(primitiveTypeToVkTopology(pipeline::PrimitiveType::TriangleList), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    EXPECT_EQ(primitiveTypeToVkTopology(pipeline::PrimitiveType::TriangleStrip), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
    EXPECT_EQ(primitiveTypeToVkTopology(pipeline::PrimitiveType::QuadList), VK_PRIMITIVE_TOPOLOGY_MAX_ENUM);
    EXPECT_EQ(primitiveTypeToVkTopology(pipeline::PrimitiveType::RectList), VK_PRIMITIVE_TOPOLOGY_MAX_ENUM);
    EXPECT_EQ(primitiveTypeToVkTopology(static_cast<pipeline::PrimitiveType>(255u)), VK_PRIMITIVE_TOPOLOGY_MAX_ENUM);

    EXPECT_EQ(
        primitiveTopologyTypeToVkTopology(pipeline::PrimitiveTopologyType::Triangle),
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    );
    EXPECT_EQ(
        primitiveTopologyTypeToVkTopology(pipeline::PrimitiveTopologyType::Patch), VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
    );
    EXPECT_EQ(
        primitiveTopologyTypeToVkTopology(pipeline::PrimitiveTopologyType::Line), VK_PRIMITIVE_TOPOLOGY_LINE_LIST
    );
    EXPECT_EQ(
        primitiveTopologyTypeToVkTopology(pipeline::PrimitiveTopologyType::Point), VK_PRIMITIVE_TOPOLOGY_POINT_LIST
    );
    EXPECT_EQ(
        primitiveTopologyTypeToVkTopology(static_cast<pipeline::PrimitiveTopologyType>(255u)),
        VK_PRIMITIVE_TOPOLOGY_MAX_ENUM
    );
}

TEST(VulkanPipelineEnumsTest, CompareAndStencilOpConversions)
{
    EXPECT_EQ(compareFunctionToVkCompareOp(pipeline::CompareFunction::Less), VK_COMPARE_OP_LESS);
    EXPECT_EQ(compareFunctionToVkCompareOp(pipeline::CompareFunction::LessEqual), VK_COMPARE_OP_LESS_OR_EQUAL);
    EXPECT_EQ(compareFunctionToVkCompareOp(pipeline::CompareFunction::Greater), VK_COMPARE_OP_GREATER);
    EXPECT_EQ(compareFunctionToVkCompareOp(pipeline::CompareFunction::GreaterEqual), VK_COMPARE_OP_GREATER_OR_EQUAL);
    EXPECT_EQ(compareFunctionToVkCompareOp(pipeline::CompareFunction::Equal), VK_COMPARE_OP_EQUAL);
    EXPECT_EQ(compareFunctionToVkCompareOp(pipeline::CompareFunction::NotEqual), VK_COMPARE_OP_NOT_EQUAL);
    EXPECT_EQ(compareFunctionToVkCompareOp(pipeline::CompareFunction::Never), VK_COMPARE_OP_NEVER);
    EXPECT_EQ(compareFunctionToVkCompareOp(pipeline::CompareFunction::Always), VK_COMPARE_OP_ALWAYS);
    EXPECT_EQ(compareFunctionToVkCompareOp(pipeline::CompareFunction::DepthNear), VK_COMPARE_OP_GREATER);
    EXPECT_EQ(
        compareFunctionToVkCompareOp(pipeline::CompareFunction::DepthNearOrEqual), VK_COMPARE_OP_GREATER_OR_EQUAL
    );
    EXPECT_EQ(compareFunctionToVkCompareOp(pipeline::CompareFunction::DepthFarther), VK_COMPARE_OP_LESS);
    EXPECT_EQ(
        compareFunctionToVkCompareOp(pipeline::CompareFunction::DepthFartherOrEqual), VK_COMPARE_OP_LESS_OR_EQUAL
    );
    EXPECT_EQ(compareFunctionToVkCompareOp(static_cast<pipeline::CompareFunction>(255u)), VK_COMPARE_OP_MAX_ENUM);

    EXPECT_EQ(stencilOpToVkStencilOp(pipeline::StencilOp::Keep), VK_STENCIL_OP_KEEP);
    EXPECT_EQ(stencilOpToVkStencilOp(pipeline::StencilOp::Zero), VK_STENCIL_OP_ZERO);
    EXPECT_EQ(stencilOpToVkStencilOp(pipeline::StencilOp::Replace), VK_STENCIL_OP_REPLACE);
    EXPECT_EQ(stencilOpToVkStencilOp(pipeline::StencilOp::SaturatedIncrement), VK_STENCIL_OP_INCREMENT_AND_CLAMP);
    EXPECT_EQ(stencilOpToVkStencilOp(pipeline::StencilOp::SaturatedDecrement), VK_STENCIL_OP_DECREMENT_AND_CLAMP);
    EXPECT_EQ(stencilOpToVkStencilOp(pipeline::StencilOp::Invert), VK_STENCIL_OP_INVERT);
    EXPECT_EQ(stencilOpToVkStencilOp(pipeline::StencilOp::Increment), VK_STENCIL_OP_INCREMENT_AND_WRAP);
    EXPECT_EQ(stencilOpToVkStencilOp(pipeline::StencilOp::Decrement), VK_STENCIL_OP_DECREMENT_AND_WRAP);
    EXPECT_EQ(stencilOpToVkStencilOp(static_cast<pipeline::StencilOp>(255u)), VK_STENCIL_OP_MAX_ENUM);
}

TEST(VulkanPipelineEnumsTest, BlendOpFactorAndColorWriteMaskConversions)
{
    EXPECT_EQ(blendOperationToVkBlendOp(pipeline::BlendOperation::Add), VK_BLEND_OP_ADD);
    EXPECT_EQ(blendOperationToVkBlendOp(pipeline::BlendOperation::Subtract), VK_BLEND_OP_SUBTRACT);
    EXPECT_EQ(blendOperationToVkBlendOp(pipeline::BlendOperation::Min), VK_BLEND_OP_MIN);
    EXPECT_EQ(blendOperationToVkBlendOp(pipeline::BlendOperation::Max), VK_BLEND_OP_MAX);
    EXPECT_EQ(blendOperationToVkBlendOp(pipeline::BlendOperation::ReverseSubtract), VK_BLEND_OP_REVERSE_SUBTRACT);
    EXPECT_EQ(blendOperationToVkBlendOp(static_cast<pipeline::BlendOperation>(255u)), VK_BLEND_OP_MAX_ENUM);

    EXPECT_EQ(blendFactorToVkBlendFactor(pipeline::BlendFactor::Zero), VK_BLEND_FACTOR_ZERO);
    EXPECT_EQ(blendFactorToVkBlendFactor(pipeline::BlendFactor::One), VK_BLEND_FACTOR_ONE);
    EXPECT_EQ(blendFactorToVkBlendFactor(pipeline::BlendFactor::SourceColor), VK_BLEND_FACTOR_SRC_COLOR);
    EXPECT_EQ(
        blendFactorToVkBlendFactor(pipeline::BlendFactor::InverseSourceColor), VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR
    );
    EXPECT_EQ(blendFactorToVkBlendFactor(pipeline::BlendFactor::SourceAlpha), VK_BLEND_FACTOR_SRC_ALPHA);
    EXPECT_EQ(
        blendFactorToVkBlendFactor(pipeline::BlendFactor::InverseSourceAlpha), VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
    );
    EXPECT_EQ(blendFactorToVkBlendFactor(pipeline::BlendFactor::DestAlpha), VK_BLEND_FACTOR_DST_ALPHA);
    EXPECT_EQ(blendFactorToVkBlendFactor(pipeline::BlendFactor::InverseDestAlpha), VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA);
    EXPECT_EQ(blendFactorToVkBlendFactor(pipeline::BlendFactor::DestColor), VK_BLEND_FACTOR_DST_COLOR);
    EXPECT_EQ(blendFactorToVkBlendFactor(pipeline::BlendFactor::InverseDestColor), VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR);
    EXPECT_EQ(blendFactorToVkBlendFactor(pipeline::BlendFactor::ConstantBlendFactor), VK_BLEND_FACTOR_CONSTANT_COLOR);
    EXPECT_EQ(
        blendFactorToVkBlendFactor(pipeline::BlendFactor::InverseConstantBlendFactor),
        VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR
    );
    EXPECT_EQ(blendFactorToVkBlendFactor(pipeline::BlendFactor::Source1Color), VK_BLEND_FACTOR_SRC1_COLOR);
    EXPECT_EQ(
        blendFactorToVkBlendFactor(pipeline::BlendFactor::InverseSource1Color), VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR
    );
    EXPECT_EQ(blendFactorToVkBlendFactor(pipeline::BlendFactor::Source1Alpha), VK_BLEND_FACTOR_SRC1_ALPHA);
    EXPECT_EQ(
        blendFactorToVkBlendFactor(pipeline::BlendFactor::InverseSource1Alpha), VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA
    );
    EXPECT_EQ(blendFactorToVkBlendFactor(static_cast<pipeline::BlendFactor>(255u)), VK_BLEND_FACTOR_MAX_ENUM);

    EXPECT_EQ(colorWriteMaskToVkColorComponentFlags(pipeline::ColorWriteMask::None), 0u);
    EXPECT_EQ(colorWriteMaskToVkColorComponentFlags(pipeline::ColorWriteMask::Red), VK_COLOR_COMPONENT_R_BIT);
    EXPECT_EQ(colorWriteMaskToVkColorComponentFlags(pipeline::ColorWriteMask::Green), VK_COLOR_COMPONENT_G_BIT);
    EXPECT_EQ(colorWriteMaskToVkColorComponentFlags(pipeline::ColorWriteMask::Blue), VK_COLOR_COMPONENT_B_BIT);
    EXPECT_EQ(colorWriteMaskToVkColorComponentFlags(pipeline::ColorWriteMask::Alpha), VK_COLOR_COMPONENT_A_BIT);
    EXPECT_EQ(
        colorWriteMaskToVkColorComponentFlags(pipeline::ColorWriteMask::RGB),
        static_cast<VkColorComponentFlags>(
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
        )
    );
    EXPECT_EQ(
        colorWriteMaskToVkColorComponentFlags(pipeline::ColorWriteMask::RGBA),
        static_cast<VkColorComponentFlags>(
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        )
    );
    EXPECT_EQ(
        colorWriteMaskToVkColorComponentFlags(pipeline::ColorWriteMask::RG),
        static_cast<VkColorComponentFlags>(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT)
    );
    EXPECT_EQ(
        colorWriteMaskToVkColorComponentFlags(pipeline::ColorWriteMask::BA),
        static_cast<VkColorComponentFlags>(VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)
    );
}

TEST(VulkanPipelineEnumsTest, RasterizerVrsAndAttachmentOpConversions)
{
    EXPECT_EQ(rasterizerFillModeToVkPolygonMode(pipeline::RasterizerFillMode::Point), VK_POLYGON_MODE_POINT);
    EXPECT_EQ(rasterizerFillModeToVkPolygonMode(pipeline::RasterizerFillMode::Wireframe), VK_POLYGON_MODE_LINE);
    EXPECT_EQ(rasterizerFillModeToVkPolygonMode(pipeline::RasterizerFillMode::Solid), VK_POLYGON_MODE_FILL);
    EXPECT_EQ(
        rasterizerFillModeToVkPolygonMode(static_cast<pipeline::RasterizerFillMode>(255u)), VK_POLYGON_MODE_MAX_ENUM
    );

    EXPECT_EQ(rasterizerCullModeToVkCullMode(pipeline::RasterizerCullMode::None), VK_CULL_MODE_NONE);
    EXPECT_EQ(rasterizerCullModeToVkCullMode(pipeline::RasterizerCullMode::CW), VK_CULL_MODE_FRONT_BIT);
    EXPECT_EQ(rasterizerCullModeToVkCullMode(pipeline::RasterizerCullMode::CCW), VK_CULL_MODE_BACK_BIT);
    EXPECT_EQ(rasterizerCullModeToVkCullMode(static_cast<pipeline::RasterizerCullMode>(255u)), VK_CULL_MODE_NONE);

    EXPECT_EQ(
        vrsRateCombinerToVkCombinerOp(pipeline::VRSRateCombiner::Passthrough),
        VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR
    );
    EXPECT_EQ(
        vrsRateCombinerToVkCombinerOp(pipeline::VRSRateCombiner::Override),
        VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR
    );
    EXPECT_EQ(
        vrsRateCombinerToVkCombinerOp(pipeline::VRSRateCombiner::Min), VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MIN_KHR
    );
    EXPECT_EQ(
        vrsRateCombinerToVkCombinerOp(pipeline::VRSRateCombiner::Max), VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR
    );
    EXPECT_EQ(
        vrsRateCombinerToVkCombinerOp(pipeline::VRSRateCombiner::Sum), VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR
    );
    EXPECT_EQ(
        vrsRateCombinerToVkCombinerOp(static_cast<pipeline::VRSRateCombiner>(255u)),
        VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR
    );

    EXPECT_EQ(vrsAxisShadingRateToExtentDimension(pipeline::VRSAxisShadingRate::Rate1X), 1u);
    EXPECT_EQ(vrsAxisShadingRateToExtentDimension(pipeline::VRSAxisShadingRate::Rate2X), 2u);
    EXPECT_EQ(vrsAxisShadingRateToExtentDimension(pipeline::VRSAxisShadingRate::Rate4X), 4u);
    EXPECT_EQ(vrsAxisShadingRateToExtentDimension(static_cast<pipeline::VRSAxisShadingRate>(3u)), 1u);

    const VkExtent2D rate1x1 = vrsShadingRateToVkExtent2D(pipeline::VRSShadingRate::Rate1x1);
    EXPECT_EQ(rate1x1.width, 1u);
    EXPECT_EQ(rate1x1.height, 1u);

    const VkExtent2D rate1x2 = vrsShadingRateToVkExtent2D(pipeline::VRSShadingRate::Rate1x2);
    EXPECT_EQ(rate1x2.width, 1u);
    EXPECT_EQ(rate1x2.height, 2u);

    const VkExtent2D rate2x1 = vrsShadingRateToVkExtent2D(pipeline::VRSShadingRate::Rate2x1);
    EXPECT_EQ(rate2x1.width, 2u);
    EXPECT_EQ(rate2x1.height, 1u);

    const VkExtent2D rate2x2 = vrsShadingRateToVkExtent2D(pipeline::VRSShadingRate::Rate2x2);
    EXPECT_EQ(rate2x2.width, 2u);
    EXPECT_EQ(rate2x2.height, 2u);

    const VkExtent2D rate2x4 = vrsShadingRateToVkExtent2D(pipeline::VRSShadingRate::Rate2x4);
    EXPECT_EQ(rate2x4.width, 2u);
    EXPECT_EQ(rate2x4.height, 4u);

    const VkExtent2D rate4x2 = vrsShadingRateToVkExtent2D(pipeline::VRSShadingRate::Rate4x2);
    EXPECT_EQ(rate4x2.width, 4u);
    EXPECT_EQ(rate4x2.height, 2u);

    const VkExtent2D rate4x4 = vrsShadingRateToVkExtent2D(pipeline::VRSShadingRate::Rate4x4);
    EXPECT_EQ(rate4x4.width, 4u);
    EXPECT_EQ(rate4x4.height, 4u);

    EXPECT_EQ(renderTargetLoadActionToVkLoadOp(command::RenderTargetLoadAction::Load), VK_ATTACHMENT_LOAD_OP_LOAD);
    EXPECT_EQ(renderTargetLoadActionToVkLoadOp(command::RenderTargetLoadAction::Clear), VK_ATTACHMENT_LOAD_OP_CLEAR);
    EXPECT_EQ(
        renderTargetLoadActionToVkLoadOp(command::RenderTargetLoadAction::NoAction), VK_ATTACHMENT_LOAD_OP_DONT_CARE
    );
    EXPECT_EQ(
        renderTargetLoadActionToVkLoadOp(static_cast<command::RenderTargetLoadAction>(255u)),
        VK_ATTACHMENT_LOAD_OP_MAX_ENUM
    );

    EXPECT_EQ(
        renderTargetStoreActionToVkStoreOp(command::RenderTargetStoreAction::Store), VK_ATTACHMENT_STORE_OP_STORE
    );
    EXPECT_EQ(
        renderTargetStoreActionToVkStoreOp(command::RenderTargetStoreAction::Store, true), VK_ATTACHMENT_STORE_OP_STORE
    );
    EXPECT_EQ(
        renderTargetStoreActionToVkStoreOp(command::RenderTargetStoreAction::NoAction, false),
        VK_ATTACHMENT_STORE_OP_DONT_CARE
    );
    EXPECT_EQ(
        renderTargetStoreActionToVkStoreOp(command::RenderTargetStoreAction::NoAction, true),
        VK_ATTACHMENT_STORE_OP_DONT_CARE
    );
    EXPECT_EQ(
        renderTargetStoreActionToVkStoreOp(command::RenderTargetStoreAction::MultisampleResolve),
        VK_ATTACHMENT_STORE_OP_STORE
    );
    EXPECT_EQ(
        renderTargetStoreActionToVkStoreOp(command::RenderTargetStoreAction::MultisampleResolve, true),
        VK_ATTACHMENT_STORE_OP_DONT_CARE
    );
    EXPECT_EQ(
        renderTargetStoreActionToVkStoreOp(static_cast<command::RenderTargetStoreAction>(255u), false),
        VK_ATTACHMENT_STORE_OP_MAX_ENUM
    );
}

}   // namespace gp::rhi::vulkan::tests
