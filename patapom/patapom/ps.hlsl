#ifndef PS_OUTPUT_COUNT
#define PS_OUTPUT_COUNT 2
#endif

#include "GlobalInclude.hlsl"
#include "Lighting.hlsl"

Texture2D albedoTex : register(t0, SPACE(PASS));
SamplerState albedoSampler : register(s0, SPACE(PASS));

Texture2D normalTex : register(t1, SPACE(PASS));
SamplerState normalSampler : register(s1, SPACE(PASS));

PS_OUTPUT main(VS_OUTPUT input)
{
    SurfaceDataOut sdo;
    sdo.uv = input.uv;
    sdo.posWorld = input.posWorld;
    sdo.norWorld = normalize(input.norWorld);
    sdo.tanWorld = normalize(input.tanWorld);
    sdo.bitanWorld = normalize(input.bitanWorld);
    sdo.eyeDirWorld = normalize(pEyePos - sdo.posWorld);
    sdo.albedo = sStandardColor.rgb;
    
#ifdef USE_POM
    float3 newPosWorld;
    float2 newUV;
    newUV = POM(sdo, newPosWorld);
    sdo.uv = newUV;
    sdo.posWorld = newPosWorld;
#endif
    
    // recalculate depth after pom
    float zView = mul(pView, float4(sdo.posWorld, 1.0f)).z;
    sdo.depth = QuantizeDepth(zView, pNearClipPlane, pFarClipPlane);
    
    // sample textures
#ifndef USE_POM
    if (sUseStandardTextures)
#endif
    {
        sdo.albedo = albedoTex.Sample(albedoSampler, sdo.uv).rgb;
        float4 normalCol = normalTex.Sample(normalSampler, sdo.uv);
        float3x3 tanToWorld = float3x3(sdo.tanWorld, sdo.bitanWorld, sdo.norWorld); // constructed row by row
        float3 paintedNormal = UnpackNormal(normalCol.rgb);
        sdo.norWorld = normalize(mul(paintedNormal, tanToWorld)); // post multiply because tanToWorld is constructed row by row
    }
    
    return PackGbuffer(sdo);
}
