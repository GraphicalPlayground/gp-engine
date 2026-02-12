// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/Array.hpp"
#include "Container/BasicString.hpp"
#include "Container/ContainerForward.hpp"
#include "Core/ShaderDefine.hpp"
#include "Core/ShaderHash.hpp"
#include "Core/ShaderTypes.hpp"
#include "ShadersBuild.hpp"

namespace GP::Shaders
{

struct FShaderSource
{
public:
    FString filePath;
    FString code;
    FString entryPoint;
    EShaderStage stage = EShaderStage::None;
    EShaderLanguage language = EShaderLanguage::GLSL;
    Container::TArray<FShaderDefine> defines;

public:
    GP_NODISCARD FShaderHash ComputeHash() const;
};

}   // namespace GP::Shaders
