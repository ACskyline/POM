#define VS_OUTPUT_LITE
#define VS_INPUT_INSTANCE_ID
#define VS_INPUT_VERTEX_ID

#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

StructuredBuffer<WaterSimCell> gWaterSimCellBuffer : register(t0, SPACE(PASS));
StructuredBuffer<WaterSimCellFace> gWaterSimCellFaceBuffer : register(t1, SPACE(PASS));

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	uint cellIndex = input.instanceID;
	float3 scale = uPass.mCellSize.xxx;
	uint3 cellIndexXYZ = DeflattenCellIndexXYZ(cellIndex);
	float3 translate = GetCellCenterPos(cellIndexXYZ);
	float4 pos = float4(translate, 1.0f);
	uint3 cellFaceIndices = FlattenCellFaceIndices(cellIndexXYZ, cellIndexXYZ, cellIndexXYZ);
	float3 velocity = normalize(float3(GetCellFaceVelocity(cellFaceIndices.x), GetCellFaceVelocity(cellFaceIndices.y), GetCellFaceVelocity(cellFaceIndices.z)));
	if (input.vertexID & 1)
		pos.xyz += velocity * scale;
	pos = mul(uPass.mProj, mul(uPass.mView, pos));
	output.col = input.vertexID & 1 ? float4(abs(velocity), 0.5f) : float4(0.0f, 0.0f, 0.0f, 0.5f);
	output.pos = pos;
	return output;
}
