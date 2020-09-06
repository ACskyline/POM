#define VS_OUTPUT_LITE

#ifndef PS_OUTPUT_COUNT
#define PS_OUTPUT_COUNT 1
#endif

#include "GlobalInclude.hlsl"
#include "Lighting.hlsl"

Texture2D gbuffer0 : register(t0, SPACE(PASS));
SamplerState gbufferSampler0 : register(s0, SPACE(PASS));

Texture2D gbuffer1 : register(t1, SPACE(PASS));
SamplerState gbufferSampler1 : register(s1, SPACE(PASS));

Texture2D gbuffer2 : register(t2, SPACE(PASS));
SamplerState gbufferSampler2 : register(s2, SPACE(PASS));

Texture2D dbuffer : register(t3, SPACE(PASS));
SamplerState dbufferSampler : register(s3, SPACE(PASS));

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
    sdi.albedo = gbuffer0.Sample(gbufferSampler0, TransformUV(uv)).rgb;
    sdi.norWorld = UnpackNormal(gbuffer1.Sample(gbufferSampler1, TransformUV(uv)).rgb);
    float depth = dbuffer.SampleLevel(dbufferSampler, TransformUV(uv), 0.0f).r;
    float zView = DequantizeDepth(gbuffer0.Sample(gbufferSampler0, TransformUV(uv)).a, uPass.pNearClipPlane, uPass.pFarClipPlane);
    float3 storedPosWorld_gbuffer = gbuffer2.Sample(gbufferSampler2, TransformUV(uv)).rgb;
    float3 restoredPosWorld_gbuffer = RestorePosFromViewZ(screenPos, float2(uPass.pWidth, uPass.pHeight), zView, uPass.pFov, uPass.pNearClipPlane, uPass.pFarClipPlane, uPass.pViewInv);
    float3 restoredPosWorld_dbuffer = RestorePosFromDepth(screenPos, float2(uPass.pWidth, uPass.pHeight), depth, uPass.pNearClipPlane, uPass.pFarClipPlane, uPass.pViewProjInv);
    sdi.posWorld = restoredPosWorld_gbuffer;
    
    if (uScene.sMode == 0) // default
    {
        output.col0 = DefaultShading(sdi);
    }
    else if (uScene.sMode == 1) // albedo
    {
        output.col0 = gbuffer0.Sample(gbufferSampler0, TransformUV(uv));
    }
    else if (uScene.sMode == 2) // normal
    {
        output.col0 = gbuffer1.Sample(gbufferSampler1, TransformUV(uv));
    }
    else if (uScene.sMode == 3) // uv
    {
        output.col0 = float4(input.uv, 0.0f, 1.0f);
    }
    else if (uScene.sMode == 4) // stored pos
    {
        output.col0 = float4(storedPosWorld_gbuffer, 1.0f);
    }
    else if (uScene.sMode == 5) // restored pos g
    {
        output.col0 = float4(restoredPosWorld_gbuffer, 1.0f);
    }
    else if (uScene.sMode == 6) // restored pos d
    {
        output.col0 = float4(restoredPosWorld_dbuffer, 1.0f);
    }
    else if (uScene.sMode == 7) // abs diff pos
    {
        output.col0 = float4(abs(restoredPosWorld_gbuffer - restoredPosWorld_dbuffer), 1.0f);
    }
    
    return output;
}
