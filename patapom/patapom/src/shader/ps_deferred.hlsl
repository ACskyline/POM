#define VS_OUTPUT_LITE
#define PS_COLOR_OUTPUT_COUNT 1

#include "ShaderInclude.hlsli"
#include "Lighting.hlsli"

Texture2D gbuffer0 : register(t0, SPACE(PASS));
Texture2D gbuffer1 : register(t1, SPACE(PASS));
Texture2D gbuffer2 : register(t2, SPACE(PASS));
Texture2D gbuffer3 : register(t3, SPACE(PASS));
Texture2D dbuffer : register(t4, SPACE(PASS));

float4 DefaultShading(SurfaceDataIn sdi)
{
    float3 output;
    output = Lighting(sdi);
    //return float4(output, 1.0f); // for debug
    return float4(LinearToGamma(output.rgb), 1.0f); // some old school gamma
}

PS_OUTPUT main(VS_OUTPUT input)
{
    PS_OUTPUT output;
    output.col0 = 0.0f.xxxx;
    SurfaceDataIn sdi;
    float2 screenPos = input.pos.xy;
    float2 uv = input.uv;
	float4 albedoQuantizedZView = gbuffer0.Sample(gSamplerPoint, TransformUV(uv));
	float4 packedNormalRoughness = gbuffer1.Sample(gSamplerPoint, TransformUV(uv));
    float4 posWorldFresnel = gbuffer2.Sample(gSamplerPoint, TransformUV(uv));
	float4 specularityMetallic = gbuffer3.Sample(gSamplerPoint, TransformUV(uv));
	sdi.mAlbedo = albedoQuantizedZView.rgb;
    sdi.mNorWorld = UnpackNormal(packedNormalRoughness.rgb);
	sdi.mRoughness = packedNormalRoughness.a;
	sdi.mFresnel = posWorldFresnel.a;
	sdi.mSpecularity = specularityMetallic.r;
	sdi.mMetallic = specularityMetallic.g;
    float zView = DequantizeDepth(albedoQuantizedZView.a, uPass.mNearClipPlane, uPass.mFarClipPlane);
	float depth = dbuffer.SampleLevel(gSamplerPoint, TransformUV(uv), 0.0f).r;
    float3 restoredPosWorld_gbuffer = RestorePosFromViewZ(screenPos, float2(uPass.mResolution), zView, uPass.mFov, uPass.mNearClipPlane, uPass.mFarClipPlane, uPass.mViewInv);
    float3 restoredPosWorld_dbuffer = RestorePosFromDepth(screenPos, float2(uPass.mResolution), depth, uPass.mNearClipPlane, uPass.mFarClipPlane, uPass.mViewProjInv);
    sdi.mPosWorld = restoredPosWorld_gbuffer;
    
    if (uScene.mMode == 0) // default
    {
        output.col0 = DefaultShading(sdi);
    }
    else if (uScene.mMode == 1) // albedo
    {
        output.col0 = gbuffer0.Sample(gSamplerPoint, TransformUV(uv));
    }
    else if (uScene.mMode == 2) // normal
    {
        output.col0 = gbuffer1.Sample(gSamplerPoint, TransformUV(uv));
    }
    else if (uScene.mMode == 3) // uv
    {
        output.col0 = float4(input.uv, 0.0f, 1.0f);
    }
    else if (uScene.mMode == 4) // stored pos
    {
        output.col0 = float4(posWorldFresnel.xyz, 1.0f);
    }
    else if (uScene.mMode == 5) // restored pos g
    {
        output.col0 = float4(restoredPosWorld_gbuffer, 1.0f);
    }
    else if (uScene.mMode == 6) // restored pos d
    {
        output.col0 = float4(restoredPosWorld_dbuffer, 1.0f);
    }
    else if (uScene.mMode == 7) // abs diff pos
    {
        output.col0 = float4(abs(restoredPosWorld_gbuffer - restoredPosWorld_dbuffer), 1.0f);
    }
    
    return output;
}
