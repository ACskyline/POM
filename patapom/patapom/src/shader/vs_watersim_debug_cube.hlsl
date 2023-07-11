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
	float3 pos = input.pos * scale + translate;
	float4 renderPos = float4(pos * uPass.mGridRenderScale + uPass.mGridRenderOffset, 1.0f);
	renderPos = mul(uPass.mProj, mul(uPass.mView, renderPos));
	output.col = float4(input.uv, 0.0f, 0.0f);
	output.pos = renderPos;
	return output;
}
