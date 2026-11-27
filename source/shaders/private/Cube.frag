#version 450

// TEMPORARY FOR THE HELLO CUBE

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inWorldPos;
layout(location = 2) in vec3 inWorldNormal;
layout(location = 3) in vec4 inLightPosAndIntensity;
layout(location = 4) in vec4 inCameraPosAndMode;

layout(location = 0) out vec4 outFragColor;

void main()
{
    if (inCameraPosAndMode.w > 0.5)
    {
        vec3 wireColor = mix(inColor, vec3(0.90, 0.96, 1.0), 0.45);
        outFragColor = vec4(wireColor, 1.0);
        return;
    }

    vec3 N = normalize(inWorldNormal);
    vec3 toLight = inLightPosAndIntensity.xyz - inWorldPos;
    float dist = length(toLight);
    vec3 L = toLight / max(dist, 0.0001);
    vec3 V = normalize(inCameraPosAndMode.xyz - inWorldPos);
    vec3 H = normalize(L + V);

    float attenuation = inLightPosAndIntensity.w / (1.0 + 0.10 * dist * dist);
    float nDotL = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 48.0) * 0.55;

    vec3 hemiAmbient = mix(vec3(0.06, 0.07, 0.12), vec3(0.18, 0.22, 0.30), N.y * 0.5 + 0.5);
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    vec3 rim = vec3(0.35, 0.65, 1.0) * fresnel * 0.28;

    vec3 lit = inColor * (hemiAmbient + nDotL * attenuation) + vec3(1.0, 0.96, 0.88) * spec * attenuation + rim;

    // Subtle depth-based atmospheric perspective
    float depthFactor = clamp((gl_FragCoord.z - 0.85) * 4.0, 0.0, 0.35);
    lit = mix(lit, vec3(0.05, 0.07, 0.11), depthFactor);

    // Soft Reinhard tone mapping
    lit = lit / (lit + vec3(1.0));
    outFragColor = vec4(lit, 1.0);
}
