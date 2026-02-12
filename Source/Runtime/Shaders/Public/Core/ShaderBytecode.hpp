// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/Array.hpp"
#include "Container/BasicString.hpp"
#include "Container/ContainerForward.hpp"
#include "Core/ShaderHash.hpp"
#include "Core/ShaderTypes.hpp"
#include "ShadersBuild.hpp"

namespace GP::Shaders
{

/// \brief FShaderBytecode encapsulates the compiled shader bytecode along with metadata such as the target shading
/// language, shader stage, source hash, and entry point. This structure is used to represent the output of shader
/// compilation and can be used for caching, debugging, and runtime shader management.
struct FShaderBytecode
{
public:
    Container::TArray<Byte> data;                      //<! Raw bytecode data for the shader
    EShaderTarget target = EShaderTarget::SPIRV_1_5;   //<! Target shading language or intermediate representation
    EShaderStage stage = EShaderStage::None;           //<! Shader stage this bytecode is intended for
    FShaderHash sourceHash;   //<! Hash of the original shader source code, useful for caching and debugging
    FString entryPoint;       //<! Entry point function name in the shader, typically "main"

public:
    GP_NODISCARD bool IsValid() const noexcept;
    GP_NODISCARD SizeT GetSize() const noexcept;
};

}   // namespace GP::Shaders
