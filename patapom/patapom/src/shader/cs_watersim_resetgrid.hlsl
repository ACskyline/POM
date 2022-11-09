#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

RWStructuredBuffer<WaterSimCell> gWaterSimCellBuffer : register(u0, SPACE(PASS));
RWStructuredBuffer<WaterSimCell> gWaterSimCellBufferTemp : register(u1, SPACE(PASS));
RWStructuredBuffer<WaterSimCellFace> gWaterSimCellFaceBuffer : register(u2, SPACE(PASS));
RWStructuredBuffer<WaterSimCellFace> gWaterSimCellFaceBufferTemp : register(u3, SPACE(PASS));

[numthreads(WATERSIM_THREAD_PER_THREADGROUP_X, WATERSIM_THREAD_PER_THREADGROUP_Y, WATERSIM_THREAD_PER_THREADGROUP_Z)]
void main(uint3 gGroupID : SV_GroupID, uint gGroupIndex : SV_GroupIndex)
{
	uint faceCellIndex = GetThreadIndex(gGroupID, gGroupIndex);

	if (FaceCellExist(faceCellIndex))
	{
		uint3 faceCellIndexXYZ = DeflattenFaceCellIndexXYZ(faceCellIndex);
		uint3 cellIndexXYZ = faceCellIndexXYZ;
		if (CellExistXYZ(cellIndexXYZ))
		{
			uint cellIndex = FlattenCellIndexClamp(cellIndexXYZ);
			WaterSimCell cell = (WaterSimCell)0;
			cell.mIndicesXYZ_Total = uint4(cellIndexXYZ, cellIndex);
			if (any(cellIndexXYZ == 0) || any(cellIndexXYZ == uPass.mCellCount - 1) // walls
				/*|| cellIndexXYZ.y < 0.3f * cellIndexXYZ.z*/)
			{
				cell.mType = 1; // set solid boundaries
			}
			if (uPass.mWarmStart)
			{
				cell.mPressure = gWaterSimCellBuffer[cellIndex].mPressure;
			}
			gWaterSimCellBuffer[cellIndex] = cell;
			gWaterSimCellBufferTemp[cellIndex] = cell;
		}

		uint3 cellFaceIndices = FlattenCellFaceIndices(faceCellIndexXYZ, faceCellIndexXYZ, faceCellIndexXYZ);
		WaterSimCellFace cellFace = (WaterSimCellFace)0;
		gWaterSimCellFaceBuffer[cellFaceIndices.x] = cellFace;
		gWaterSimCellFaceBuffer[cellFaceIndices.y] = cellFace;
		gWaterSimCellFaceBuffer[cellFaceIndices.z] = cellFace;
		gWaterSimCellFaceBufferTemp[cellFaceIndices.x] = gWaterSimCellFaceBuffer[cellFaceIndices.x];
		gWaterSimCellFaceBufferTemp[cellFaceIndices.y] = gWaterSimCellFaceBuffer[cellFaceIndices.y];
		gWaterSimCellFaceBufferTemp[cellFaceIndices.z] = gWaterSimCellFaceBuffer[cellFaceIndices.z];
	}
}
