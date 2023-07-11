#define VS_OUTPUT_LITE
#define VS_INPUT_INSTANCE_ID
#define VS_INPUT_VERTEX_ID

#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

StructuredBuffer<WaterSimCell> gWaterSimCellBuffer : register(t0, SPACE(PASS));
StructuredBuffer<WaterSimCellFace> gWaterSimCellFaceBuffer : register(t1, SPACE(PASS));
Texture2D<float4> gWaterSimCellRT : register(t2, SPACE(PASS));

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	uint cellIndex = input.instanceID;
	uint3 cellIndexXYZ = DeflattenCellIndexXYZ(cellIndex);
	float4 pos = float4(GetCellCenterPos(cellIndexXYZ) * uPass.mGridRenderScale + uPass.mGridRenderOffset, 1.0f);
	float3 velocity = 0.0f.xxx;
	if (uPass.mWaterSimMode == 0 || uPass.mWaterSimMode == 1)
	{
		uint3 cellFaceIndices = FlattenCellFaceIndices(cellIndexXYZ, cellIndexXYZ, cellIndexXYZ);
		velocity = normalize(float3(GetCellFaceVelocity(cellFaceIndices.x), GetCellFaceVelocity(cellFaceIndices.y), GetCellFaceVelocity(cellFaceIndices.z)));
	}
	else if (uPass.mWaterSimMode == 2 || uPass.mWaterSimMode == 3)
	{
		if (CellExist(cellIndex))
		{
			if (uPass.mUseRasterizerP2G)
				velocity = gWaterSimCellRT.Load(uint3(CellIndexToPixelIndices(cellIndex), 0)).rgb;
			else
				velocity = gWaterSimCellBuffer[cellIndex].mVelocity;
			if (length(velocity) > 0.0f)
				velocity = normalize(velocity);
		}
	}
	if (input.vertexID & 1)
		pos.xyz += velocity * uPass.mCellSize.xxx * uPass.mGridRenderScale;
	pos = mul(uPass.mProj, mul(uPass.mView, pos));
	output.col = input.vertexID & 1 ? float4(abs(velocity), 0.5f) : float4(0.0f, 0.0f, 0.0f, 0.5f);
	output.pos = pos;
	return output;
}
