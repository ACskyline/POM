#include "GlobalInclude.hlsl"

#define SHADOW_BIAS 0.08

Texture2D lightTexes[] : register(t0, SPACE(SCENE));
SamplerState lightSamplers[] : register(s0, SPACE(SCENE));

//assuming z axis range [0, 1] in NDC maps to [near, far] in view space
float LinearizeDepth(float4 depth, float near, float far)
{
    return far * near / (far + (near - far) * depth);
}

float ShadowFeeler(float3 posWorld, LightData ld)
{
    float result = 1.0f;
    float4 posView = mul(ld.view, float4(posWorld, 1.0));
    float4 posProj = mul(ld.proj, posView);
    posProj /= posProj.w;
    float2 uv = posProj.xy * 0.5f + 0.5f;
    if(uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
        return 0.0f;
    float depth = lightTexes[ld.textureIndex].Sample(lightSamplers[ld.textureIndex], TransformUV(uv));
    float linearDepth = LinearizeDepth(depth, ld.nearClipPlane, ld.farClipPlane);
    // TODO: add support for PCF or PCSS
    result = posView.z > linearDepth + SHADOW_BIAS ? 0.0f : 1.0f;
    return result;
}

float3 Lighting(SurfaceData sd)
{
    float3 result = 0.0f.xxx;
    for (uint i = 0; i < sLightCount && i < MAX_LIGHTS_PER_SCENE; i++)
    {
        float3 lightDir = normalize(sLights[i].position - sd.posWorld);
        float facingLight = dot(lightDir, sd.normalWorld) < 0.0f ? 0.0f : 1.0f;
        result += sLights[i].color * ShadowFeeler(sd.posWorld, sLights[i]) * facingLight;
    }
    return result;
}
