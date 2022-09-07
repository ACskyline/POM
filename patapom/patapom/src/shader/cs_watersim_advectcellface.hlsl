#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

StructuredBuffer<WaterSimCell> gWaterSimCellBuffer : register(t0, SPACE(PASS));
StructuredBuffer<WaterSimCellFace> gWaterSimCellFaceBuffer : register(t1, SPACE(PASS));
RWStructuredBuffer<WaterSimCellFace> gWaterSimCellFaceBufferTemp : register(u0, SPACE(PASS));

#include "WaterSimResourceUtil.hlsli"

void AdvectCellFace(uint3 cellIndexXYZ, inout WaterSimCellFace cellXFace, inout WaterSimCellFace cellYFace, inout WaterSimCellFace cellZFace)
{
	float3 xFacePos = GetCellXFaceCenterPos(cellIndexXYZ);
	float3 yFacePos = GetCellYFaceCenterPos(cellIndexXYZ);
	float3 zFacePos = GetCellZFaceCenterPos(cellIndexXYZ);
	float3 xFaceVelocity = InterpolateVelocity(xFacePos);
	float3 yFaceVelocity = InterpolateVelocity(yFacePos);
	float3 zFaceVelocity = InterpolateVelocity(zFacePos);
	float3 xMidPos = xFacePos - 0.5f * WaterSimTimeStep * xFaceVelocity;
	float3 yMidPos = yFacePos - 0.5f * WaterSimTimeStep * yFaceVelocity;
	float3 zMidPos = zFacePos - 0.5f * WaterSimTimeStep * zFaceVelocity;
	float3 xParticlePos = xFacePos - WaterSimTimeStep * InterpolateVelocity(xMidPos);
	float3 yParticlePos = yFacePos - WaterSimTimeStep * InterpolateVelocity(yMidPos);
	float3 zParticlePos = zFacePos - WaterSimTimeStep * InterpolateVelocity(zMidPos);
	cellXFace = InterpolateCellFace(X, xParticlePos);
	cellYFace = InterpolateCellFace(Y, yParticlePos);
	cellZFace = InterpolateCellFace(Z, zParticlePos);
}

void ApplyGravity(inout WaterSimCellFace cellFace)
{
	cellFace.mVelocity = max(-10000.0f, min(10000.0f, cellFace.mVelocity - uPass.mGravitationalAccel * WaterSimTimeStep));
}

[numthreads(WATERSIM_THREAD_PER_THREADGROUP_X, WATERSIM_THREAD_PER_THREADGROUP_Y, WATERSIM_THREAD_PER_THREADGROUP_Z)]
void main(uint3 gGroupThreadID : SV_GroupThreadID, uint3 gGroupID : SV_GroupID, uint gGroupIndex : SV_GroupIndex)
{
	uint faceCellIndex = GetThreadIndex(gGroupID, gGroupIndex);

	// 2. solving on the grid
	if (FaceCellExist(faceCellIndex))
	{
		uint3 faceCellIndexXYZ = DeflattenFaceCellIndexXYZ(faceCellIndex);
		uint cellIndex = FlattenCellIndex(min(faceCellIndexXYZ, uPass.mCellCount - 1));

		if (CellExist(cellIndex) && gWaterSimCellBuffer[cellIndex].mType == 0.0f)
		{
			uint indexNegJ = FlattenCellIndex(faceCellIndexXYZ - uint3(0, 1, 0));
			uint3 cellFaceIndices = FlattenCellFaceIndices(faceCellIndexXYZ, faceCellIndexXYZ, faceCellIndexXYZ);
			
			WaterSimCellFace cellFaceX = gWaterSimCellFaceBuffer[cellFaceIndices.x];
			WaterSimCellFace cellFaceY = gWaterSimCellFaceBuffer[cellFaceIndices.y];
			WaterSimCellFace cellFaceZ = gWaterSimCellFaceBuffer[cellFaceIndices.z];

			// 2b. advect cell face // nope - this is not MAC
			// AdvectCellFace(faceCellIndexXYZ, cellFaceX, cellFaceY, cellFaceZ);

			// 2c. force
			if (uPass.mApplyForce)
			{
				ApplyGravity(cellFaceY);
			}

			// boundary condition
			if (faceCellIndexXYZ.x == 0 || faceCellIndexXYZ.x == uPass.mCellCount.x)
				cellFaceX.mVelocity = 0.0f;
			if (faceCellIndexXYZ.y == 0 || faceCellIndexXYZ.y == uPass.mCellCount.y)
				cellFaceY.mVelocity = 0.0f;
			if (faceCellIndexXYZ.z == 0 || faceCellIndexXYZ.z == uPass.mCellCount.z)
				cellFaceZ.mVelocity = 0.0f;

			cellFaceX.mIndices = uint4(faceCellIndexXYZ, faceCellIndex);
			cellFaceX.mDebugGroupThreadIdGroupIndex = uint4(gGroupThreadID, gGroupIndex);
			cellFaceY.mIndices = uint4(faceCellIndexXYZ, faceCellIndex);
			cellFaceY.mDebugGroupThreadIdGroupIndex = uint4(gGroupThreadID, gGroupIndex);
			cellFaceZ.mIndices = uint4(faceCellIndexXYZ, faceCellIndex);
			cellFaceZ.mDebugGroupThreadIdGroupIndex = uint4(gGroupThreadID, gGroupIndex);
			gWaterSimCellFaceBufferTemp[cellFaceIndices.x] = cellFaceX;
			gWaterSimCellFaceBufferTemp[cellFaceIndices.y] = cellFaceY;
			gWaterSimCellFaceBufferTemp[cellFaceIndices.z] = cellFaceZ;
		}
	}
}
