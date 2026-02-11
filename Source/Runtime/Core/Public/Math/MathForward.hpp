// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Math
{

template <Concepts::IsFloatingPoint T>
struct TVector2;
template <Concepts::IsFloatingPoint T>
struct TVector3;
template <Concepts::IsFloatingPoint T>
struct TVector4;

template <Concepts::IsFloatingPoint T>
struct TMatrix2x2;
template <Concepts::IsFloatingPoint T>
struct TMatrix3x3;
template <Concepts::IsFloatingPoint T>
struct TMatrix3x4;
template <Concepts::IsFloatingPoint T>
struct TMatrix4x4;

template <Concepts::IsFloatingPoint T>
struct TQuaternion;
template <Concepts::IsFloatingPoint T>
struct TDualQuaternion;
template <Concepts::IsFloatingPoint T>
struct TRotator;
template <Concepts::IsFloatingPoint T>
struct TRotator2D;

template <Concepts::IsFloatingPoint T>
struct TTransform;

template <Concepts::IsFloatingPoint T>
struct TSphere;
template <Concepts::IsFloatingPoint T>
struct TRay;
template <Concepts::IsFloatingPoint T>
struct TBox;
template <Concepts::IsFloatingPoint T>
struct TRectangle;
template <Concepts::IsFloatingPoint T>
struct TPlane;
template <Concepts::IsFloatingPoint T>
struct TFrustum;
template <Concepts::IsFloatingPoint T>
struct TAABB;
template <Concepts::IsFloatingPoint T>
struct TOBB;

struct FColor;
struct FLinearColor;

}   // namespace GP::Math

using FVector2 = GP::Math::TVector2<GP::Real>;
using FVector3 = GP::Math::TVector3<GP::Real>;
using FVector4 = GP::Math::TVector4<GP::Real>;
using FMatrix2x2 = GP::Math::TMatrix2x2<GP::Real>;
using FMatrix3x3 = GP::Math::TMatrix3x3<GP::Real>;
using FMatrix3x4 = GP::Math::TMatrix3x4<GP::Real>;
using FMatrix4x4 = GP::Math::TMatrix4x4<GP::Real>;
using FQuaternion = GP::Math::TQuaternion<GP::Real>;
using FDualQuaternion = GP::Math::TDualQuaternion<GP::Real>;
using FRotator = GP::Math::TRotator<GP::Real>;
using FRotator2D = GP::Math::TRotator2D<GP::Real>;
using FTransform = GP::Math::TTransform<GP::Real>;
using FSphere = GP::Math::TSphere<GP::Real>;
using FRay = GP::Math::TRay<GP::Real>;
using FBox = GP::Math::TBox<GP::Real>;
using FRectangle = GP::Math::TRectangle<GP::Real>;
using FPlane = GP::Math::TPlane<GP::Real>;
using FFrustum = GP::Math::TFrustum<GP::Real>;
using FAABB = GP::Math::TAABB<GP::Real>;
using FOBB = GP::Math::TOBB<GP::Real>;
using FColor = GP::Math::FColor;
using FLinearColor = GP::Math::FLinearColor;
