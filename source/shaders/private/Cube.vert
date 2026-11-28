#version 450

// TEMPORARY FOR THE HELLO CUBE

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

layout(set = 0, binding = 0) uniform Globals
{
    mat4 uMVP;
    mat4 uModel;
    vec4 uLightPosAndIntensity;
    vec4 uCameraPosAndMode;
};

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec3 outWorldPos;
layout(location = 2) out vec3 outWorldNormal;
layout(location = 3) out vec4 outLightPosAndIntensity;
layout(location = 4) out vec4 outCameraPosAndMode;

void main()
{
    vec4 worldPos = uModel * vec4(inPosition, 1.0);
    gl_Position = uMVP * vec4(inPosition, 1.0);
    outColor = inColor;
    outWorldPos = worldPos.xyz;
    outWorldNormal = mat3(uModel) * inNormal;
    outLightPosAndIntensity = uLightPosAndIntensity;
    outCameraPosAndMode = uCameraPosAndMode;
}
