#include "GlobalInclude.hlsl"

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float4 posWorld = mul(oModel, float4(input.pos, 1.0f));
    output.pos = mul(pViewProj, posWorld);
    output.uv = TransformUV(input.uv);
    output.posWorld = posWorld.xyz;
    float3 bitan = normalize(cross(input.nor, input.tan.xyz)) * input.tan.w;
    output.norWorld = mul(float4(input.nor, 0.0f), oModelInv).xyz;
    output.tanWorld = mul(oModel, float4(input.tan.xyz, 0.0f)).xyz;
    output.bitanWorld = mul(oModel, float4(bitan, 0.0f)).xyz;
    return output;
}
