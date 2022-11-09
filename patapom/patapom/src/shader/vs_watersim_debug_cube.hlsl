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
	float3 translate = GetCellCenterPos(DeflattenCellIndexXYZ(cellIndex));
	float4 pos = float4(input.pos * scale + translate, 1.0f);
	pos = mul(uPass.mProj, mul(uPass.mView, pos));
	output.col = gWaterSimCellBuffer[cellIndex].mType == 1 ? float4(1.0f, 0.0f, 0.0f, 0.5) : float4(0.0f, 1.0f, 0.0f, 0.5);
	output.pos = pos;
	return output;
}
