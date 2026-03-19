// Copyright (c) - Graphical Playground. All rights reserved.

#include "Display/BaseDisplay.hpp"

namespace GP
{

///
/// @section Identity
///

GP_NODISCARD SizeT FBaseDisplay::GetId() const noexcept { return 0; }

GP_NODISCARD FStringView FBaseDisplay::GetName() const noexcept { return ""; }

GP_NODISCARD void* FBaseDisplay::GetNativeHandle() const noexcept { return nullptr; }

GP_NODISCARD bool FBaseDisplay::IsPrimary() const noexcept { return false; }

///
/// @section Screen Geometry
///

GP_NODISCARD FIntPoint2D FBaseDisplay::GetPosition() const noexcept { return { 0, 0 }; }

GP_NODISCARD FIntExtent2D FBaseDisplay::GetSize() const noexcept { return { 0, 0 }; }

GP_NODISCARD FIntPoint2D FBaseDisplay::GetWorkAreaPosition() const noexcept { return { 0, 0 }; }

GP_NODISCARD FIntExtent2D FBaseDisplay::GetWorkAreaSize() const noexcept { return { 0, 0 }; }

///
/// @section Display Modes
///

GP_NODISCARD FDisplayMode FBaseDisplay::GetCurrentMode() const noexcept { return {}; }

GP_NODISCARD TArrayView<const FDisplayMode> FBaseDisplay::GetSupportedModes() const noexcept { return {}; }

GP_NODISCARD bool FBaseDisplay::SupportsMode(const FDisplayMode& mode) const noexcept { return false; }

///
/// @section Physical Properties
///

GP_NODISCARD Float32 FBaseDisplay::GetDPI() const noexcept { return 0.0f; }

GP_NODISCARD FIntExtent2D FBaseDisplay::GetRawDPI() const noexcept { return { 0, 0 }; }

GP_NODISCARD Float32 FBaseDisplay::GetContentScale() const noexcept { return 1.0f; }

GP_NODISCARD FIntExtent2D FBaseDisplay::GetPhysicalSizeMillimeters() const noexcept { return { 0, 0 }; }

GP_NODISCARD EDisplayOrientation FBaseDisplay::GetOrientation() const noexcept
{
    return EDisplayOrientation::Landscape;
}

///
/// @section Capabilities
///

GP_NODISCARD EDisplayColorSpace FBaseDisplay::GetColorSpace() const noexcept { return EDisplayColorSpace::sRGB; }

GP_NODISCARD bool FBaseDisplay::SupportsHDR() const noexcept { return false; }

GP_NODISCARD bool FBaseDisplay::IsHDREnabled() const noexcept { return false; }

GP_NODISCARD Float32 FBaseDisplay::GetPeakLuminance() const noexcept { return 0.0f; }

GP_NODISCARD Float32 FBaseDisplay::GetMinLuminance() const noexcept { return 0.0f; }

}   // namespace GP
