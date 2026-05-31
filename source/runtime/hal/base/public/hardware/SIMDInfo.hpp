// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

namespace gp::hal
{

/// @brief Contains information about the SIMD capabilities of the CPU.
struct SIMDInfo
{
    bool hasSSE{ false };
    bool hasSSE2{ false };
    bool hasSSE3{ false };
    bool hasSSSE3{ false };
    bool hasSSE41{ false };
    bool hasSSE42{ false };
    bool hasAVX{ false };
    bool hasFMA3{ false };
    bool hasAVX2{ false };
    bool hasAVX512F{ false };
    bool hasNEON{ false };
};

}   // namespace gp::hal
