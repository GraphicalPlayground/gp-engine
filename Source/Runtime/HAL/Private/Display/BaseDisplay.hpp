// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Display/IDisplay.hpp"

namespace GP
{

/// @brief
class FBaseDisplay : public IDisplay
{
public:
    virtual ~FBaseDisplay() = default;

public:
    ///
    /// @section Identity
    ///

    GP_NODISCARD virtual SizeT GetId() const noexcept override;
    GP_NODISCARD virtual FStringView GetName() const noexcept override;
    GP_NODISCARD virtual void* GetNativeHandle() const noexcept override;
    GP_NODISCARD virtual bool IsPrimary() const noexcept override;

    ///
    /// @section Screen Geometry
    ///

    GP_NODISCARD virtual FIntPoint2D GetPosition() const noexcept override;
    GP_NODISCARD virtual FIntExtent2D GetSize() const noexcept override;
    GP_NODISCARD virtual FIntPoint2D GetWorkAreaPosition() const noexcept override;
    GP_NODISCARD virtual FIntExtent2D GetWorkAreaSize() const noexcept override;

    ///
    /// @section Display Modes
    ///

    GP_NODISCARD virtual FDisplayMode GetCurrentMode() const noexcept override;
    GP_NODISCARD virtual TArrayView<const FDisplayMode> GetSupportedModes() const noexcept override;
    GP_NODISCARD virtual bool SupportsMode(const FDisplayMode& mode) const noexcept override;

    ///
    /// @section Physical Properties
    ///

    GP_NODISCARD virtual Float32 GetDPI() const noexcept override;
    GP_NODISCARD virtual FIntExtent2D GetRawDPI() const noexcept override;
    GP_NODISCARD virtual Float32 GetContentScale() const noexcept override;
    GP_NODISCARD virtual FIntExtent2D GetPhysicalSizeMillimeters() const noexcept override;
    GP_NODISCARD virtual EDisplayOrientation GetOrientation() const noexcept override;

    ///
    /// @section Capabilities
    ///

    GP_NODISCARD virtual EDisplayColorSpace GetColorSpace() const noexcept override;
    GP_NODISCARD virtual bool SupportsHDR() const noexcept override;
    GP_NODISCARD virtual bool IsHDREnabled() const noexcept override;
    GP_NODISCARD virtual Float32 GetPeakLuminance() const noexcept override;
    GP_NODISCARD virtual Float32 GetMinLuminance() const noexcept override;
};

}   // namespace GP
