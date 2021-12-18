#ifndef LIGHTING_H
#define LIGHTING_H

#include "ShaderInclude.hlsli"

#define SHADOW_BIAS 0.08
#define SUN_LIGHTING_SCALING_FACTOR 0.15

Texture2D gShadowMaps[] : register(t0, SPACE(SCENE));
Texture2D gEnvMap : register(t5, SPACE(PASS));
TextureCube gPrefileteredEnvMap : register(t6, SPACE(PASS));
Texture2D gLUT : register(t7, SPACE(PASS));

float ShadowFeeler(float3 posWorld, LightData ld)
{
    float result = 1.0f;
    float4 posView = mul(ld.mView, float4(posWorld, 1.0));
    float4 posProj = mul(ld.mProj, posView);
    posProj /= posProj.w;
    float2 uv = posProj.xy * 0.5f + 0.5f;
    if(uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
        return 0.0f;
    float depth = gShadowMaps[ld.mTextureIndex].Sample(gSamplerPoint, TransformUV(uv)).r;
    float linearDepth = LinearizeDepth(depth, ld.mNearClipPlane, ld.mFarClipPlane);
    // TODO: add support for PCF or PCSS
    result = posView.z > linearDepth + SHADOW_BIAS ? 0.0f : 1.0f;
    return result;
}

float3 SpecularIBL(float3 SpecularColor, float roughness, float3 N, float3 V)
{
    float3 SpecularLighting = 0;
    const uint NumSamples = uScene.mSampleNumIBL;
    for (uint i = 0; i < NumSamples; i++)
    {
        float2 Xi = Hammersley(i, NumSamples);

        float3 H = ImportanceSampleGGX(Xi, roughness, N);
        float3 L = 2 * dot(V, H) * H - V;
        float NoL = saturate(dot(N, L));
        if (NoL > 0)
        {
            float3 ProbeColor = gEnvMap.SampleLevel(gSamplerLinear, TransformUV(DirToUV(L)), 0).rgb;
            SpecularLighting += SpecularColor * ProbeColor * GGX(V, L, H, N, roughness) * saturate(CosTheta(L, N));
        }
    }
    return SpecularLighting / NumSamples;
}

float3 ApproximateSpecularIBL(float3 SpecularColor, float roughness, float3 N, float3 V)
{
    float NoV = saturate(dot(N, V));
    float3 L = 2 * dot(V, N) * N - V;
    float4 PrefilteredColor = gPrefileteredEnvMap.SampleLevel(gSamplerIBL, L, roughness * uScene.mPrefilteredEnvMapMipLevelCount);
    float4 EnvBRDF = gLUT.SampleLevel(gSamplerIBL, TransformUV(float2(NoV, roughness)), 0);
    return PrefilteredColor.rgb * (SpecularColor * EnvBRDF.x + EnvBRDF.y);
}

float3 Lighting(SurfaceDataIn sdi)
{
    float3 result = 0.0f.xxx;
    float3 eyeDir = normalize(uPass.mEyePos - sdi.mPosWorld);
    
    // light sources
    float3 sceneLight = 0.0f.xxx;
    uint lightOffset = uScene.mLightDebugOffset > 0 ? uScene.mLightDebugOffset : 0;
    uint lightCount = uScene.mLightDebugCount > 0 ? uScene.mLightDebugCount : uScene.mLightCount;
    for (uint i = 0; i < lightCount && i < LIGHT_PER_SCENE_MAX; i++)
    {
        uint j = i + lightOffset;
        if (j < uScene.mLightCount)
        {
            float3 lightDir = normalize(uScene.mLightData[j].mPosWorld - sdi.mPosWorld);
            float facingLight = saturate(dot(lightDir, sdi.mNorWorld));
            if (facingLight > 0)
            {
                float shadow = ShadowFeeler(sdi.mPosWorld, uScene.mLightData[j]);
                float3 brdf = BRDF_GGX(sdi, lightDir, eyeDir);
                sceneLight += brdf * uScene.mLightData[j].mColor * shadow;
            }
        }
    }
    
    // sun
	// TODO: implement CSM for sun light, currently we don't have any shadow for sun light
    float3 sunLight = 0.0f.xxx;
    float3 sunDir = GetSunDir(uScene.mSunAzimuth, uScene.mSunAltitude);
    float facingSun = saturate(dot(sunDir, sdi.mNorWorld));
    if (facingSun)
        sunLight += BRDF_GGX(sdi, sunDir, eyeDir) * facingSun * uScene.mSunRadiance * SUN_LIGHTING_SCALING_FACTOR;
    
    // IBL
    float3 IBL = 0.0f.xxx;
    if (uScene.mShowReferenceIBL)
        IBL += SpecularIBL(sdi.mAlbedo, sdi.mRoughness, sdi.mNorWorld, eyeDir); // enumerate
    else
        IBL += ApproximateSpecularIBL(sdi.mAlbedo, sdi.mRoughness, sdi.mNorWorld, eyeDir); // Split Sum
    
    // Similar to Unreal's specularity, add fake specularity to diffuse material
    if (sdi.mMetallic == 0.0f)
        IBL *= sdi.mSpecularity;
    
    result = uScene.mUseSceneLight * sceneLight + uScene.mUseSunLight * sunLight + uScene.mUseIBL * IBL;
    
    return result;
}

#endif
