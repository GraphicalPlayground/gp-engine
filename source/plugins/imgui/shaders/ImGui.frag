// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#version 450

// Dear ImGui pixel stage.
// Texture and sampler are bound as SEPARATE descriptors (SAMPLED_IMAGE + SAMPLER) rather than a combined image
// sampler: this matches the RHI's D3D-style `setShaderTexture()` / `setShaderSampler()` split, and lets every ImGui
// texture share the single bilinear-clamp sampler owned by the renderer.

layout(set = 1, binding = 0) uniform texture2D sTexture;
layout(set = 1, binding = 1) uniform sampler sSampler;

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = inColor * texture(sampler2D(sTexture, sSampler), inUV);
}
