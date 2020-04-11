#include "GlobalInclude.hlsli"

Texture2D gbuffer : register(t0, SPACE(PASS));
SamplerState gbufferSampler : register(s0, SPACE(PASS));

PS_OUTPUT main(VS_OUTPUT input)
{
    PS_OUTPUT output;
    float2 uv = input.uv;
    float3 posWorld = input.posWorld;
    float3 norWorld = normalize(input.norWorld);
    float3 tanWorld = normalize(input.tanWorld);
    float3 bitanWorld = normalize(input.bitanWorld);
    
    output.col0 = gbuffer.Sample(gbufferSampler, uv);
    return output;
}
