#define VS_INPUT_INSTANCE_ID

#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

StructuredBuffer<WaterSimParticle> gWaterSimParticleBuffer : register(t0, SPACE(PASS));

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output = (VS_OUTPUT)0;
	uint particleIndex = input.instanceID;
	WaterSimParticle particle = gWaterSimParticleBuffer[particleIndex];
	if (particle.mAlive == 0.0f)
	{
		DiscardVertex(output);
		return output;
	}
	float3 particleOffset = particle.mPos * uPass.mGridRenderScale + uPass.mGridRenderOffset;
	float4 posWorld = float4(input.pos * 0.006 + particleOffset, 1.0f);
	float4 posView = mul(uPass.mView, posWorld);
	output.pos = mul(uPass.mProj, posView);
	output.uv = input.uv;
	output.posView = posView.xyz;
	output.posWorld = posWorld.xyz;
	float3 bitan = normalize(cross(input.nor, input.tan.xyz)) * input.tan.w;
	output.norWorld = normalize(mul(float4(input.nor, 0.0f), uObject.mModelInv).xyz); // post multiply because normal needs to be transformed by the transpose of the inverse model matrix
	output.tanWorld = normalize(mul(uObject.mModel, float4(input.tan.xyz, 0.0f)).xyz);
	output.bitanWorld = normalize(mul(uObject.mModel, float4(bitan, 0.0f)).xyz);
	return output;
}