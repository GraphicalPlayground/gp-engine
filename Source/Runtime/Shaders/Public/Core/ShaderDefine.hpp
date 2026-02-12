// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/BasicString.hpp"
#include "Container/ContainerForward.hpp"
#include "ShadersBuild.hpp"

namespace GP::Shaders
{

/// \brief Represents a shader define, which consists of a name and a value.
struct FShaderDefine
{
public:
    FString name;    //<! The name of the shader define.
    FString value;   //<! The value of the shader define.

public:
    /// \brief Equality operator for shader define.
    /// \param other The other shader define to compare with.
    /// \return True if both shader defines are equal, false otherwise.
    GP_NODISCARD bool operator==(const FShaderDefine&) const = default;
};

}   // namespace GP::Shaders
