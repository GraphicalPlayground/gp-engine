// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#version 450

// Dear ImGui vertex stage.
// Input layout mirrors `ImDrawVert` (20 bytes): float2 pos, float2 uv, UByte4N RGBA color.
// The orthographic projection is folded into a scale + translate pair (16 bytes of packed globals) instead of a full
// 4x4 matrix: ImGui geometry is 2D, so a mat4 would waste 48 bytes of uniform bandwidth per draw list.

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;

layout(set = 0, binding = 0) uniform Globals
{
    vec2 uScale;
    vec2 uTranslate;
};

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUV;

void main()
{
    outColor = inColor;
    outUV = inUV;
    gl_Position = vec4(inPosition * uScale + uTranslate, 0.0, 1.0);
}
