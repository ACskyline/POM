#define VS_OUTPUT_LITE
#define PS_COLOR_OUTPUT_COUNT 1

#define LUT_NUM_SAMPLES 128
#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"

cbuffer PassUniformBuffer : register(b0, SPACE(PASS))
{
    PassUniformIBL uPass;
};

Texture2D gLUT : register(t0, SPACE(PASS));

PS_OUTPUT main(VS_OUTPUT input)
{
    PS_OUTPUT output;
    float NoV = input.uv.x;
    float roughness = input.uv.y;
    float3 N = float3(0, 1, 0);
    float3 V; // only one quater of a full circle
    V.x = sqrt(1.0f - NoV * NoV);
    V.y = NoV;
    V.z = 0;
    
    float A = 0;
    float B = 0;
    
    const int NumSamples = LUT_NUM_SAMPLES;
    for (int i = 0; i < NumSamples; i++)
    {
        float2 Xi = Hammersley(i, NumSamples);
        float3 H = ImportanceSampleGGX(Xi, roughness, N);
        float3 L = 2 * dot(V, H) * H - V;
        float NoL = saturate(dot(N, L));
        float NoH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));
        if (NoL > 0)
        {
            float G = GGX_NoFresnel(L, V, H, N, roughness);
            float Fresnel = pow(1 - VoH, 5);
            A += (1 - Fresnel) * G;
            B += Fresnel * G;
        }
    }
    
    output.col0 = float4(A, B, 0, 0) / NumSamples;
	return output;
}
