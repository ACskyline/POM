#ifndef LIGHTING_H
#define LIGHTING_H

#include "GlobalInclude.hlsl"

#define SHADOW_BIAS 0.08
#define SUN_LIGHTING_SCALING_FACTOR 0.15
#define IBL_LIGHTING_SCALING_FACTOR 5.00

Texture2D lightTexes[] : register(t0, SPACE(SCENE));
SamplerState lightSamplers[] : register(s0, SPACE(SCENE));

Texture2D probeTex : register(t4, SPACE(PASS));
SamplerState probeSampler : register(s4, SPACE(PASS));

float ShadowFeeler(float3 posWorld, LightData ld)
{
    float result = 1.0f;
    float4 posView = mul(ld.view, float4(posWorld, 1.0));
    float4 posProj = mul(ld.proj, posView);
    posProj /= posProj.w;
    float2 uv = posProj.xy * 0.5f + 0.5f;
    if(uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
        return 0.0f;
    float depth = lightTexes[ld.textureIndex].Sample(lightSamplers[ld.textureIndex], TransformUV(uv)).r;
    float linearDepth = LinearizeDepth(depth, ld.nearClipPlane, ld.farClipPlane);
    // TODO: add support for PCF or PCSS
    result = posView.z > linearDepth + SHADOW_BIAS ? 0.0f : 1.0f;
    return result;
}

// Trowbridge-Reitz micro-facet distribution function (GGX)
float GGX_D(float cosThetaH, float alpha)
{
    float temp = cosThetaH * cosThetaH * (alpha * alpha - 1.0f) + 1.0f;
    return alpha * alpha / (PI * temp * temp);
}

// Trowbridge-Reitz masking shadowing function
float GGX_G(float tanThetaI, float tanThetaO, float alpha)
{
    float alpha2 = alpha * alpha;
    float tanThetaI2 = tanThetaI * tanThetaI;
    float tanThetaO2 = tanThetaO * tanThetaO;
    float AI = (-1.0f + sqrt(1.0f + alpha2 * tanThetaI2)) / 2.0f;
    float AO = (-1.0f + sqrt(1.0f + alpha2 * tanThetaO2)) / 2.0f;
    return 1.0f / (1.0f + AI + AO);
}

// Schlick approximation
float Fresnel(float vDotH, float F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - vDotH, 5.0f);
}

float TanTheta(float3 w, float3 nor)
{
    float b = dot(w, nor);
    float a = length(w - b * nor);
    return a / b;
}

float CosTheta(float3 w, float3 nor)
{
    return dot(w, nor);
}

// wi = lightDir, wo = eyeDir
float3 BRDF(SurfaceDataIn sdi, float3 wi, float3 wo)
{
    if (dot(wi, sdi.norWorld) < 0.0f || dot(wo, sdi.norWorld) < 0.0f)
        return 0.0f.xxx;
    
    float3 wh = (wo + wi);
    if (length(wh) == 0.0f)
    {
        float3 tangent = normalize(cross(sdi.norWorld, wi));
        wh = normalize(cross(wi, tangent));
    }
    else
        wh = normalize(wh);
    
    float fresnel = Fresnel(dot(wh, wo), sFresnel);
    float specular = saturate(fresnel * GGX_D(CosTheta(wh, sdi.norWorld), sRoughness) * GGX_G(TanTheta(wi, sdi.norWorld), TanTheta(wo, sdi.norWorld), sRoughness) / (4.0f * CosTheta(wi, sdi.norWorld) * CosTheta(wo, sdi.norWorld)));
    float diffuse = saturate(dot(wi, sdi.norWorld) / PI); // TODO: use Disney diffuse
    return sdi.albedo * (diffuse * (1.0f - sMetallic) + specular * sMetallic) * saturate(CosTheta(wi, sdi.norWorld));
}

float3 SampleProbe(float3 dir)
{
    float tan = dir.z / dir.x;
    float u = atan(tan);
    if (dir.x < 0)
        u += PI;
    u = u / TWO_PI;
    float v = dir.y * 0.5f + 0.5f;
    return probeTex.Sample(probeSampler, TransformUV(float2(u, v))).rgb;
}

float3 Lighting(SurfaceDataIn sdi)
{
    float3 result = 0.0f.xxx;
    float3 eyeDir = normalize(pEyePos - sdi.posWorld);
    uint lightOffset = sLightDebugOffset > 0 ? sLightDebugOffset : 0;
    uint lightCount = sLightDebugCount > 0 ? sLightDebugCount : sLightCount;
    for (uint i = 0; i < lightCount && i < MAX_LIGHTS_PER_SCENE; i++)
    {
        uint j = i + lightOffset;
        if(j < sLightCount)
        {
            float3 lightDir = normalize(sLights[j].position - sdi.posWorld);
            float facingLight = dot(lightDir, sdi.norWorld) < 0.0f ? 0.0f : 1.0f;
            float3 brdf = BRDF(sdi, lightDir, eyeDir);
            float shadow = facingLight * ShadowFeeler(sdi.posWorld, sLights[j]);
            result += brdf * sLights[j].color * shadow;
        }
    }
    
    float3 probeDir = reflect(-eyeDir, sdi.norWorld);
    float3 IBL = SampleProbe(probeDir);
    float3 sunDir = GetSunDir(sSunAzimuth, sSunAltitude);
    float facingSun = dot(sunDir, sdi.norWorld) < 0.0f ? 0.0f : 1.0f;
    result += BRDF(sdi, sunDir, eyeDir) * facingSun * sSunRadiance * SUN_LIGHTING_SCALING_FACTOR;
    result += BRDF(sdi, probeDir, eyeDir) * IBL * sReflection * IBL_LIGHTING_SCALING_FACTOR;
    return result;
}

#endif
