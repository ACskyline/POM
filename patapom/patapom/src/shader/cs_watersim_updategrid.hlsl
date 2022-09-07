#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

RWStructuredBuffer<WaterSimCell> gWaterSimCellBuffer : register(u0, SPACE(PASS));
RWStructuredBuffer<WaterSimCellFace> gWaterSimCellFaceBuffer : register(u1, SPACE(PASS));
RWStructuredBuffer<WaterSimCellFace> gWaterSimCellFaceBufferTemp : register(u2, SPACE(PASS));

#include "WaterSimResourceUtil.hlsli"

void AddNonSolidNeighborCount(uint cellIndex, uint3 cellIndexXYZ)
{
	uint indexPosI = FlattenCellIndex(cellIndexXYZ + uint3(1, 0, 0));
	uint indexNegI = FlattenCellIndex(cellIndexXYZ - uint3(1, 0, 0));
	uint indexPosJ = FlattenCellIndex(cellIndexXYZ + uint3(0, 1, 0));
	uint indexNegJ = FlattenCellIndex(cellIndexXYZ - uint3(0, 1, 0));
	uint indexPosK = FlattenCellIndex(cellIndexXYZ + uint3(0, 0, 1));
	uint indexNegK = FlattenCellIndex(cellIndexXYZ - uint3(0, 0, 1));
	if (indexPosI != cellIndex && gWaterSimCellBuffer[indexPosI].mType != 1.0f)
		InterlockedAdd(gWaterSimCellBuffer[cellIndex].mNonSolidNeighborCount, 1);
	if (indexNegI != cellIndex && gWaterSimCellBuffer[indexNegI].mType != 1.0f)
		InterlockedAdd(gWaterSimCellBuffer[cellIndex].mNonSolidNeighborCount, 1);
	if (indexPosJ != cellIndex && gWaterSimCellBuffer[indexPosJ].mType != 1.0f)
		InterlockedAdd(gWaterSimCellBuffer[cellIndex].mNonSolidNeighborCount, 1);
	if (indexNegJ != cellIndex && gWaterSimCellBuffer[indexNegJ].mType != 1.0f)
		InterlockedAdd(gWaterSimCellBuffer[cellIndex].mNonSolidNeighborCount, 1);
	if (indexPosK != cellIndex && gWaterSimCellBuffer[indexPosK].mType != 1.0f)
		InterlockedAdd(gWaterSimCellBuffer[cellIndex].mNonSolidNeighborCount, 1);
	if (indexNegK != cellIndex && gWaterSimCellBuffer[indexNegK].mType != 1.0f)
		InterlockedAdd(gWaterSimCellBuffer[cellIndex].mNonSolidNeighborCount, 1);
}

[numthreads(WATERSIM_THREAD_PER_THREADGROUP_X, WATERSIM_THREAD_PER_THREADGROUP_Y, WATERSIM_THREAD_PER_THREADGROUP_Z)]
void main(uint3 gGroupID : SV_GroupID, uint gGroupIndex : SV_GroupIndex)
{
	uint faceCellIndex = GetThreadIndex(gGroupID, gGroupIndex);

	// 2. solving on the grid
	if (FaceCellExist(faceCellIndex))
	{
		// update velocity from the u32 version
		// put this in here because advectcell happens right before advectcellface
		// so that we can slip in a write again barrier to make sure all cell's velocity is updated
		uint3 faceCellIndexXYZ = DeflattenFaceCellIndexXYZ(faceCellIndex);
		uint3 cellIndexXYZ = faceCellIndexXYZ;
		float particleCountInCell = 0.0f;
		if (CellExistXYZ(cellIndexXYZ))
		{
			uint cellIndex = FlattenCellIndex(faceCellIndexXYZ);
			particleCountInCell = gWaterSimCellBuffer[cellIndex].mParticleCount; 
			AddNonSolidNeighborCount(cellIndex, cellIndexXYZ);
			// mark air cell
			if (gWaterSimCellBuffer[cellIndex].mType != 1.0f)
				gWaterSimCellBuffer[cellIndex].mType = particleCountInCell ? 0.0f : 2.0f;
		}
		// particleCountInCell = max(1.0f, particleCountInCell); // clamp to 1 to avoid nan
		if (particleCountInCell)
		{
			uint3 cellFaceIndices = FlattenCellFaceIndices(faceCellIndexXYZ, faceCellIndexXYZ, faceCellIndexXYZ);
			float3 velocity = asfloat(uint3(gWaterSimCellFaceBuffer[cellFaceIndices.x].mVelocityU32,
				gWaterSimCellFaceBuffer[cellFaceIndices.y].mVelocityU32,
				gWaterSimCellFaceBuffer[cellFaceIndices.z].mVelocityU32));
			float3 weight = asfloat(uint3(gWaterSimCellFaceBuffer[cellFaceIndices.x].mWeightU32,
				gWaterSimCellFaceBuffer[cellFaceIndices.y].mWeightU32,
				gWaterSimCellFaceBuffer[cellFaceIndices.z].mWeightU32));

			gWaterSimCellFaceBuffer[cellFaceIndices.x].mVelocity = weight.x ? velocity.x / weight.x : 0.0f;
			gWaterSimCellFaceBuffer[cellFaceIndices.y].mVelocity = weight.y ? velocity.y / weight.y : 0.0f;
			gWaterSimCellFaceBuffer[cellFaceIndices.z].mVelocity = weight.z ? velocity.z / weight.z : 0.0f;
			gWaterSimCellFaceBufferTemp[cellFaceIndices.x] = gWaterSimCellFaceBuffer[cellFaceIndices.x];
			gWaterSimCellFaceBufferTemp[cellFaceIndices.y] = gWaterSimCellFaceBuffer[cellFaceIndices.y];
			gWaterSimCellFaceBufferTemp[cellFaceIndices.z] = gWaterSimCellFaceBuffer[cellFaceIndices.z];
		}
	}
}
