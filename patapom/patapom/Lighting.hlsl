#ifndef LIGHTING_H
#define LIGHTING_H

#include "GlobalInclude.hlsl"

#define SHADOW_BIAS 0.08
#define SUN_LIGHTING_SCALING_FACTOR 0.15

Texture2D lightTexes[] : register(t0, SPACE(SCENE));
SamplerState lightSamplers[] : register(s0, SPACE(SCENE));

Texture2D envMap : register(t4, SPACE(PASS));
SamplerState envMapSampler : register(s4, SPACE(PASS));

TextureCube prefileteredEnvMap : register(t5, SPACE(PASS));
SamplerState prefileteredEnvMapSampler : register(s5, SPACE(PASS));

Texture2D lut : register(t6, SPACE(PASS));
SamplerState lutSampler : register(s6, SPACE(PASS));

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

// Schlick approximation
float Fresnel(float vDotH, float F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - vDotH, 5.0f);
}

// wi = lightDir, wo = eyeDir
float GGX(float3 wi, float3 wo, float3 wh, float3 N, float roughness)
{
    float fresnel = Fresnel(dot(wh, wo), uScene.sFresnel);
    return saturate(fresnel * GGX_NoFresnel(wi, wo, wh, N, roughness));
}

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
    
    float specular = GGX(wi, wo, wh, sdi.norWorld, uScene.sRoughness);
    float diffuse = ONE_OVER_PI; // TODO: use Disney diffuse
    return sdi.albedo * (diffuse * (1.0f - uScene.sMetallic) + specular * uScene.sMetallic) * saturate(CosTheta(wi, sdi.norWorld));
}

float3 SpecularIBL(float3 SpecularColor, float roughness, float3 N, float3 V)
{
    float3 SpecularLighting = 0;
    const uint NumSamples = uScene.sSampleNumIBL;
    for (uint i = 0; i < NumSamples; i++)
    {
        float2 Xi = Hammersley(i, NumSamples);

        float3 H = ImportanceSampleGGX(Xi, roughness, N);
        float3 L = 2 * dot(V, H) * H - V;
        float NoL = saturate(dot(N, L));
        if (NoL > 0)
        {
            float3 ProbeColor = envMap.SampleLevel(envMapSampler, TransformUV(DirToUV(L)), 0).rgb;
            SpecularLighting += SpecularColor * ProbeColor * GGX(V, L, H, N, roughness) * saturate(CosTheta(L, N));
        }
    }
    return SpecularLighting / NumSamples;
}

float3 ApproximateSpecularIBL(float3 SpecularColor, float roughness, float3 N, float3 V)
{
    float NoV = saturate(dot(N, V));
    float3 L = 2 * dot(V, N) * N - V;
    float4 PrefilteredColor = prefileteredEnvMap.SampleLevel(prefileteredEnvMapSampler, L, roughness * uScene.sPrefilteredEnvMapMipLevelCount);
    float4 EnvBRDF = lut.SampleLevel(lutSampler, TransformUV(float2(NoV, roughness)), 0);
    return PrefilteredColor.rgb * (SpecularColor * EnvBRDF.x + EnvBRDF.y);
}

float3 Lighting(SurfaceDataIn sdi)
{
    float3 result = 0.0f.xxx;
    float3 eyeDir = normalize(uPass.pEyePos - sdi.posWorld);
    
    // light sources
    float3 sceneLight = 0.0f.xxx;
    uint lightOffset = uScene.sLightDebugOffset > 0 ? uScene.sLightDebugOffset : 0;
    uint lightCount = uScene.sLightDebugCount > 0 ? uScene.sLightDebugCount : uScene.sLightCount;
    for (uint i = 0; i < lightCount && i < MAX_LIGHTS_PER_SCENE; i++)
    {
        uint j = i + lightOffset;
        if (j < uScene.sLightCount)
        {
            float3 lightDir = normalize(uScene.sLights[j].position - sdi.posWorld);
            float facingLight = saturate(dot(lightDir, sdi.norWorld));
            if (facingLight > 0)
            {
                float shadow = ShadowFeeler(sdi.posWorld, uScene.sLights[j]);
                float3 brdf = BRDF(sdi, lightDir, eyeDir);
                sceneLight += brdf * uScene.sLights[j].color * shadow;
            }
        }
    }
    
    // sun
    float3 sunLight = 0.0f.xxx;
    float3 sunDir = GetSunDir(uScene.sSunAzimuth, uScene.sSunAltitude);
    float facingSun = saturate(dot(sunDir, sdi.norWorld));
    if (facingSun)
        sunLight += BRDF(sdi, sunDir, eyeDir) * facingSun * uScene.sSunRadiance * SUN_LIGHTING_SCALING_FACTOR;
    
    // IBL
    float3 IBL = 0.0f.xxx;
    if (uScene.sShowReferenceIBL)
        IBL += SpecularIBL(sdi.albedo, uScene.sRoughness, sdi.norWorld, eyeDir);
    else
        IBL += ApproximateSpecularIBL(sdi.albedo, uScene.sRoughness, sdi.norWorld, eyeDir);
    
    // Similar to Unreal's specularity, add fake specularity to diffuse material
    if (uScene.sMetallic == 0.0f)
        IBL *= uScene.sSpecularity;
    
    result = uScene.sUseSceneLight * sceneLight + uScene.sUseSunLight * sunLight + uScene.sUseIBL * IBL;
    
    return result;
}

#endif
