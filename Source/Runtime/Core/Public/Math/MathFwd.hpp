// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreTypes.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"

namespace GP
{

template <CFloatingPoint T>
struct TVector2;
template <CFloatingPoint T>
struct TVector3;
template <CFloatingPoint T>
struct TVector4;
template <CFloatingPoint T>
struct TMatrix2x2;
template <CFloatingPoint T>
struct TMatrix3x3;
template <CFloatingPoint T>
struct TMatrix3x4;
template <CFloatingPoint T>
struct TMatrix4x4;
template <CFloatingPoint T>
struct TQuaternion;
template <CFloatingPoint T>
struct TDualQuaternion;
template <CFloatingPoint T>
struct TRotator;
template <CFloatingPoint T>
struct TRotator2D;
template <CFloatingPoint T>
struct TBoundingBox;
template <CFloatingPoint T>
struct TOrientedBoundingBox;
template <CFloatingPoint T>
struct TBoundingSphere;
template <CFloatingPoint T>
struct TFrustum;
template <CFloatingPoint T>
struct TCapsule;
template <CFloatingPoint T>
struct TCone;
template <CFloatingPoint T>
struct TCylinder;
template <CFloatingPoint T>
struct TCurve;
template <CFloatingPoint T>
struct TNURBS;
template <CFloatingPoint T>
struct TPlane;
template <CFloatingPoint T>
struct TPolyhedron;
template <CFloatingPoint T>
struct TRay;
template <CFloatingPoint T>
struct TSphere;
template <CFloatingPoint T>
struct TSpline;
template <CFloatingPoint T>
struct TSurface;
template <CFloatingPoint T>
struct TTorus;
template <CFloatingPoint T>
struct TVolume;
template <CFloatingPoint T>
struct TTransform;

struct FBezier;
struct FCatmullRom;
struct FEasing;
struct FHermite;
struct FInterpolation;
struct FLerp;
struct FSlerp;
struct FColor;
struct FLinearColor;
struct FColorGrading;
struct FColorSpace;

}   // namespace GP
