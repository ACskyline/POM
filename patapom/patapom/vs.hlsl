#include "GlobalInclude.hlsl"

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float4 posWorld = mul(oModel, float4(input.pos, 1.0f));
    float4 posView = mul(pView, posWorld);
    float4x4 projMatrix = pProj;
    output.pos = mul(pProj, posView);
    output.uv = TransformUV(input.uv);
    output.posView = posView.xyz;
    output.posWorld = posWorld.xyz;
    float3 bitan = normalize(cross(input.nor, input.tan.xyz)) * input.tan.w;
    output.norWorld = mul(float4(input.nor, 0.0f), oModelInv).xyz; // pos multiply because normal needs to be transformed by the transpose of the inverse model matrix
    output.tanWorld = mul(oModel, float4(input.tan.xyz, 0.0f)).xyz;
    output.bitanWorld = mul(oModel, float4(bitan, 0.0f)).xyz;
    return output;
}
