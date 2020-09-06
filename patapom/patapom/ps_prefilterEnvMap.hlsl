#define VS_OUTPUT_LITE

#ifndef PS_OUTPUT_COUNT
#define PS_OUTPUT_COUNT 1
#endif

#define ENV_MAP_NUM_SAMPLES 128
#define CUSTOM_PASS_UNIFORM

#include "GlobalInclude.hlsl"

struct PassUniformIBL : PassUniformDefault
{
    float pRoughness;
};

cbuffer PassUniformBuffer : register(b0, SPACE(PASS))
{
    PassUniformIBL uPass;
};

Texture2D envMap : register(t0, SPACE(PASS));
SamplerState envMapSampler : register(s0, SPACE(PASS));

PS_OUTPUT main(VS_OUTPUT input)
{
    PS_OUTPUT output;
    float3 N = UVtoDir(input.uv);
    float3 V = N;
    
    float3 SpecularLighting = 0;
    float TotalWeight = 0;
    const int NumSamples = ENV_MAP_NUM_SAMPLES;
    for (int i = 0; i < NumSamples; i++)
    {
        float2 Xi = Hammersley(i, NumSamples);
        float3 H = ImportanceSampleGGX(Xi, uPass.pRoughness, N);
        float3 L = 2 * dot(V, H) * H - V;
        float NoL = saturate(dot(N, L));
        if (NoL > 0)
        {
            float3 ProbeColor = envMap.SampleLevel(envMapSampler, TransformUV(DirToUV(L)), 0).rgb;
            SpecularLighting += ProbeColor * NoL;
            TotalWeight += NoL;
        }
    }
    SpecularLighting /= TotalWeight;
    
    output.col0 = float4(SpecularLighting, 0);
    return output;
}
