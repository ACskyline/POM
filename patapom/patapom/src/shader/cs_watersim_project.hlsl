#define CUSTOM_PASS_UNIFORM

#define FULL_SIM 1
#if FULL_SIM
#define FULL_SIM_ONLY(x) x
#define FULL_SIM_SWITCH(x,y) x
#else
#define FULL_SIM_ONLY(x)
#define FULL_SIM_SWITCH(x,y) y
#endif

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

RWStructuredBuffer<WaterSimCell> gWaterSimCellBuffer : register(u0, SPACE(PASS));
RWStructuredBuffer<WaterSimCell> gWaterSimCellBufferTemp : register(u1, SPACE(PASS));
RWStructuredBuffer<WaterSimCellFace> gWaterSimCellFaceBuffer : register(u2, SPACE(PASS));
RWStructuredBuffer<WaterSimCellAux> gWaterSimCellAuxBuffer : register(u3, SPACE(PASS));

#include "WaterSimResourceUtil.hlsli"

float GetPressure(uint index, uint iteration)
{
	bool temp = (iteration & 0x1) == 1;
	if (temp)
		return gWaterSimCellBufferTemp[index].mPressure;
	else
		return gWaterSimCellBuffer[index].mPressure;
}

WaterSimCell GetCell(uint index, uint iteration)
{
	bool temp = (iteration & 0x1) == 1;
	if (temp)
		return gWaterSimCellBufferTemp[index];
	else
		return gWaterSimCellBuffer[index];
}

void SetCell(uint index, uint iteration, WaterSimCell cell)
{
	bool temp = (iteration & 0x1) == 0;
	if (temp)
		gWaterSimCellBufferTemp[index] = cell;
	else
		gWaterSimCellBuffer[index] = cell;
}

//void SetPressure(uint index, uint iteration, float pressure)
//{
//	bool temp = (iteration & 0x1) == 0;
//	if (temp)
//		gWaterSimCellBufferTemp[index].mPressure = pressure;
//	else
//		gWaterSimCellBuffer[index].mPressure = pressure;
//}
//
//void SetDebug(uint index, uint iteration, float4 posPressureAndNonSolidCount, float4 negPressureAndDivergence)
//{
//	bool temp = (iteration & 0x1) == 0;
//	if (temp)
//	{
//		gWaterSimCellBufferTemp[index].mPosPressureAndNonSolidCount = posPressureAndNonSolidCount;
//		gWaterSimCellBufferTemp[index].mNegPressureAndDivergence = negPressureAndDivergence;
//	}
//	else
//	{
//		gWaterSimCellBuffer[index].mPosPressureAndNonSolidCount = posPressureAndNonSolidCount;
//		gWaterSimCellBuffer[index].mNegPressureAndDivergence = negPressureAndDivergence;
//	}
//}

void Project(uint3 cellIndexXYZ)
{
	// Jacobi
	uint index = FlattenCellIndex(cellIndexXYZ);
	uint curIteration = uPass.mCurrentJacobiIteration;
	WaterSimCell cell = GetCell(index, curIteration);
	bool isWater = cell.mType == 0;
	bool isSolid = cell.mType == 1;

	float cellSize = uPass.mCellSize;
	uint indexPosI = FlattenCellIndex(cellIndexXYZ + uint3(1, 0, 0));
	uint indexNegI = FlattenCellIndex(cellIndexXYZ - uint3(1, 0, 0));
	uint indexPosJ = FlattenCellIndex(cellIndexXYZ + uint3(0, 1, 0));
	uint indexNegJ = FlattenCellIndex(cellIndexXYZ - uint3(0, 1, 0));
	uint indexPosK = FlattenCellIndex(cellIndexXYZ + uint3(0, 0, 1));
	uint indexNegK = FlattenCellIndex(cellIndexXYZ - uint3(0, 0, 1));
	
	float3 isNotSolidPos = 0.0f;
	float3 isNotSolidNeg = 0.0f;
	float divergence = 0.0f;
	float nonSolidNeighborCount = 0.0f;

	if (curIteration == 0 || curIteration > uPass.mJacobiIterationCount)
	{
		float3 vPos = GetCellVelocityPos(cellIndexXYZ);
		float3 vNeg = GetCellVelocityNeg(cellIndexXYZ);
		divergence = dot(vPos - vNeg, 1.0f) / cellSize;

		WaterSimCell cellPosI = GetCell(indexPosI, curIteration);
		WaterSimCell cellNegI = GetCell(indexNegI, curIteration);
		WaterSimCell cellPosJ = GetCell(indexPosJ, curIteration);
		WaterSimCell cellNegJ = GetCell(indexNegJ, curIteration);
		WaterSimCell cellPosK = GetCell(indexPosK, curIteration);
		WaterSimCell cellNegK = GetCell(indexNegK, curIteration);

		//isNotSolidPos = float3(1.0f, 1.0f, 1.0f);
		//isNotSolidNeg = float3(1.0f, 1.0f, 1.0f);
		isNotSolidPos = float3(uint3(cellPosI.mType, cellPosJ.mType, cellPosK.mType) != 1);
		isNotSolidNeg = float3(uint3(cellNegI.mType, cellNegJ.mType, cellNegK.mType) != 1);
		nonSolidNeighborCount = max(1.0f, dot(isNotSolidPos, 1.0f) + dot(isNotSolidNeg, 1.0f));
#if FULL_SIM
		divergence += (-(1.0f - isNotSolidPos.x) * (vPos.x - cellPosI.mSolidVelocity.x)
			+ (1.0f - isNotSolidNeg.x) * (vNeg.x - cellNegI.mSolidVelocity.x)
			- (1.0f - isNotSolidPos.y) * (vPos.y - cellPosJ.mSolidVelocity.y)
			+ (1.0f - isNotSolidNeg.y) * (vNeg.y - cellNegJ.mSolidVelocity.y)
			- (1.0f - isNotSolidPos.z) * (vPos.z - cellPosK.mSolidVelocity.z)
			+ (1.0f - isNotSolidNeg.z) * (vNeg.z - cellNegK.mSolidVelocity.z)) / cellSize;
#endif
		// TODO: account for frame time and density
		divergence = divergence * /*max(1.0, cell.mParticleCount) **/ uPass.mWaterDensity / WaterSimTimeStep;

		gWaterSimCellAuxBuffer[index].mIsNotSolidPos = isNotSolidPos;
		gWaterSimCellAuxBuffer[index].mIsNotSolidNeg = isNotSolidNeg;
		gWaterSimCellAuxBuffer[index].mNonSolidNeighborCount = nonSolidNeighborCount;
		gWaterSimCellAuxBuffer[index].mDivergence = divergence;
	}
	else
	{
		isNotSolidPos = gWaterSimCellAuxBuffer[index].mIsNotSolidPos;
		isNotSolidNeg = gWaterSimCellAuxBuffer[index].mIsNotSolidNeg;
		nonSolidNeighborCount = gWaterSimCellAuxBuffer[index].mNonSolidNeighborCount;
		divergence = gWaterSimCellAuxBuffer[index].mDivergence;
	}

	if (curIteration < uPass.mJacobiIterationCount)
	{
		float cellSize2 = cellSize * cellSize;
		float pPosI = FULL_SIM_ONLY(isNotSolidPos.x *) GetPressure(indexPosI, curIteration);
		float pNegI = FULL_SIM_ONLY(isNotSolidNeg.x *) GetPressure(indexNegI, curIteration);
		float pPosJ = FULL_SIM_ONLY(isNotSolidPos.y *) GetPressure(indexPosJ, curIteration);
		float pNegJ = FULL_SIM_ONLY(isNotSolidNeg.y *) GetPressure(indexNegJ, curIteration);
		float pPosK = FULL_SIM_ONLY(isNotSolidPos.z *) GetPressure(indexPosK, curIteration);
		float pNegK = FULL_SIM_ONLY(isNotSolidNeg.z *) GetPressure(indexNegK, curIteration);
		float p = (pPosI + pNegI + pPosJ + pNegJ + pPosK + pNegK - cellSize * cellSize * divergence) / FULL_SIM_SWITCH(nonSolidNeighborCount, 6.0f);
		float Ax = -FULL_SIM_SWITCH(nonSolidNeighborCount, 6.0f) / cellSize2 * cell.mPressure + pPosI + pNegI + pPosJ + pNegJ + pPosK + pNegK;
		cell.mResidual = abs((divergence - Ax) / divergence);
		cell.mPressure = isWater ? p : 0.0f;
		cell.mPosPressureAndNonSolidCount = float4(pPosI, pPosJ, pPosK, nonSolidNeighborCount);
		cell.mNegPressureAndDivergence = float4(pNegI, pNegJ, pNegK, divergence);
		
		SetCell(index, curIteration, cell);
	}
	else if (curIteration == uPass.mJacobiIterationCount)
	{
		// TODO: can store necessary fields in aux buffer
		WaterSimCell cellPosI = GetCell(indexPosI, curIteration);
		WaterSimCell cellNegI = GetCell(indexNegI, curIteration);
		WaterSimCell cellPosJ = GetCell(indexPosJ, curIteration);
		WaterSimCell cellNegJ = GetCell(indexNegJ, curIteration);
		WaterSimCell cellPosK = GetCell(indexPosK, curIteration);
		WaterSimCell cellNegK = GetCell(indexNegK, curIteration);

		// update velocity
		float pPosI = GetPressure(indexPosI, curIteration);
		float pNegI = GetPressure(indexNegI, curIteration);
		float pPosJ = GetPressure(indexPosJ, curIteration);
		float pNegJ = GetPressure(indexNegJ, curIteration);
		float pPosK = GetPressure(indexPosK, curIteration);
		float pNegK = GetPressure(indexNegK, curIteration);
		float p = GetPressure(index, curIteration);
		// subtract pressure gradient from velocity
		uint3 cellFaceIndices = FlattenCellFaceIndices(cellIndexXYZ, cellIndexXYZ, cellIndexXYZ);
		float3 newVelocity = cell.mSolidVelocity;
		
		if (isNotSolidNeg.x || !isSolid) // if any non-solid cell
		{
			if (!isNotSolidNeg.x) // if neg is solid
				newVelocity.x = cellNegI.mSolidVelocity.x;
			else if (isSolid) // if self is solid
				newVelocity.x = cell.mSolidVelocity.x;
			else // if both are non-solid cell
				newVelocity.x = GetCellFaceVelocity(cellFaceIndices.x) - (p - pNegI) * WaterSimTimeStep / (uPass.mWaterDensity * cellSize);
		}
		else // if both solid
			newVelocity.x = 0.0f;

		if (isNotSolidNeg.y || !isSolid) // if any non-solid cell
		{
			if (!isNotSolidNeg.y) // if neg is solid
				newVelocity.y = cellNegJ.mSolidVelocity.y;
			else if (isSolid) // if self is solid
				newVelocity.y = cell.mSolidVelocity.y;
			else // if both are non-solid cell
				newVelocity.y = GetCellFaceVelocity(cellFaceIndices.y) - (p - pNegJ) * WaterSimTimeStep / (uPass.mWaterDensity * cellSize);
		}
		else // if both solid
			newVelocity.y = 0.0f;

		if (isNotSolidNeg.z || !isSolid) // if any non-solid cell
		{
			if (!isNotSolidNeg.z) // if negX is solid
				newVelocity.z = cellNegK.mSolidVelocity.z;
			else if (isSolid) // if self is solid
				newVelocity.z = cell.mSolidVelocity.z;
			else // if both are non-solid cell
				newVelocity.z = GetCellFaceVelocity(cellFaceIndices.z) - (p - pNegK) * WaterSimTimeStep / (uPass.mWaterDensity * cellSize);
		}
		else // if both solid
			newVelocity.z = 0.0f;

		//if (isNotSolidNeg.x && !isSolid) // if both of the adjacent cells are NOT solid
		//	newVelocity.x = GetCellFaceVelocity(cellFaceIndices.x) - (p - pNegI) * WaterSimTimeStep / (uPass.mWaterDensity * cellSize);
		//if (isNotSolidNeg.y && !isSolid) // if both of the adjacent cells are NOT solid
		//	newVelocity.y = GetCellFaceVelocity(cellFaceIndices.y) - (p - pNegJ) * WaterSimTimeStep / (uPass.mWaterDensity * cellSize);
		//if (isNotSolidNeg.z && !isSolid) // if both of the adjacent cells are NOT solid
		//	newVelocity.z = GetCellFaceVelocity(cellFaceIndices.z) - (p - pNegK) * WaterSimTimeStep / (uPass.mWaterDensity * cellSize);
		UpdateCellFaceOldVelocity(cellFaceIndices.x);
		UpdateCellFaceOldVelocity(cellFaceIndices.y);
		UpdateCellFaceOldVelocity(cellFaceIndices.z);
		SetCellFaceVelocity(cellFaceIndices.x, newVelocity.x);
		SetCellFaceVelocity(cellFaceIndices.y, newVelocity.y);
		SetCellFaceVelocity(cellFaceIndices.z, newVelocity.z);
		// DEBUG
		gWaterSimCellFaceBuffer[cellFaceIndices.x].mPosPressureAndP = float4(pPosI, pPosJ, pPosK, p);
		gWaterSimCellFaceBuffer[cellFaceIndices.y].mPosPressureAndP = float4(pPosI, pPosJ, pPosK, p);
		gWaterSimCellFaceBuffer[cellFaceIndices.z].mPosPressureAndP = float4(pPosI, pPosJ, pPosK, p);
		gWaterSimCellFaceBuffer[cellFaceIndices.x].mNegPressureAndDivergence = float4(pNegI, pNegJ, pNegK, divergence);
		gWaterSimCellFaceBuffer[cellFaceIndices.y].mNegPressureAndDivergence = float4(pNegI, pNegJ, pNegK, divergence);
		gWaterSimCellFaceBuffer[cellFaceIndices.z].mNegPressureAndDivergence = float4(pNegI, pNegJ, pNegK, divergence);
	}
}

[numthreads(WATERSIM_THREAD_PER_THREADGROUP_X, WATERSIM_THREAD_PER_THREADGROUP_Y, WATERSIM_THREAD_PER_THREADGROUP_Z)]
void main(uint3 gGroupID : SV_GroupID, uint gGroupIndex : SV_GroupIndex)
{
	uint cellIndex = GetThreadIndex(gGroupID, gGroupIndex);

	// 2. solving on the grid
	// 2d. project
	if (CellExist(cellIndex))
	{
		uint3 cellIndexXYZ = DeflattenCellIndexXYZ(cellIndex);
		Project(cellIndexXYZ);
	}
}
