#include "GlobalInclude.hlsli"

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float4 worldPos = mul(model, float4(input.pos, 1.0f));
    output.pos = mul(viewProj, worldPos);
    output.uv = TransformUV(input.uv);
    output.posWorld = worldPos.xyz;
    float3 bitan = normalize(cross(input.nor, input.tan.xyz)) * input.tan.w;
    output.norWorld = mul(float4(input.nor, 0.0f), modelInv).xyz;
    output.tanWorld = mul(model, float4(input.tan.xyz, 0.0f)).xyz;
    output.bitanWorld = mul(model, float4(bitan, 0.0f)).xyz;
    return output;
}
