// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::math
{

/// @section Color & Color Management Enumerations

enum class GammaSpace : gp::UInt8;
enum class ColorSpaceType : gp::UInt8;
enum class WhitePoint : gp::UInt8;
enum class ChromaticAdaptationMethod : gp::UInt8;
enum class ColorEncoding : gp::UInt8;

/// @section Core Color Primitives & Palettes

struct Color;
struct LinearColor;
class ColorList;

/// @section CIE Colorimetry & Color Space Management

struct Chromaticities;
class ColorSpace;
class ColorSpaceTransform;

}   // namespace gp::math

namespace gp
{

using GammaSpace = math::GammaSpace;
using ColorSpaceType = math::ColorSpaceType;
using WhitePoint = math::WhitePoint;
using ChromaticAdaptationMethod = math::ChromaticAdaptationMethod;
using ColorEncoding = math::ColorEncoding;

using Color = math::Color;
using LinearColor = math::LinearColor;
using ColorList = math::ColorList;
using Chromaticities = math::Chromaticities;
using ColorSpace = math::ColorSpace;
using ColorSpaceTransform = math::ColorSpaceTransform;

}   // namespace gp
