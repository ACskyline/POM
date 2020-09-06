#include "GlobalInclude.hlsl"

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float4 posWorld = mul(uObject.oModel, float4(input.pos, 1.0f));
    float4 posView = mul(uPass.pView, posWorld);
    float4x4 projMatrix = uPass.pProj;
    output.pos = mul(uPass.pProj, posView);
    output.uv = input.uv;
    output.posView = posView.xyz;
    output.posWorld = posWorld.xyz;
    float3 bitan = normalize(cross(input.nor, input.tan.xyz)) * input.tan.w;
    output.norWorld = mul(float4(input.nor, 0.0f), uObject.oModelInv).xyz; // post multiply because normal needs to be transformed by the transpose of the inverse model matrix
    output.tanWorld = mul(uObject.oModel, float4(input.tan.xyz, 0.0f)).xyz;
    output.bitanWorld = mul(uObject.oModel, float4(bitan, 0.0f)).xyz;
    return output;
}
