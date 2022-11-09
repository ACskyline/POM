#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

StructuredBuffer<WaterSimCellFace> gWaterSimCellFaceBufferSrc : register(t0, SPACE(PASS));
RWStructuredBuffer<WaterSimCell> gWaterSimCellBuffer : register(u0, SPACE(PASS));
RWStructuredBuffer<WaterSimCellFace> gWaterSimCellFaceBufferDest : register(u1, SPACE(PASS));

float ApplyGravity(float velocityY)
{
	return max(-10000.0f, min(10000.0f, velocityY - uPass.mGravitationalAccel * WaterSimTimeStep));
}

[numthreads(WATERSIM_THREAD_PER_THREADGROUP_X, WATERSIM_THREAD_PER_THREADGROUP_Y, WATERSIM_THREAD_PER_THREADGROUP_Z)]
void main(uint3 gGroupThreadID : SV_GroupThreadID, uint3 gGroupID : SV_GroupID, uint gGroupIndex : SV_GroupIndex)
{
	uint faceCellIndex = GetThreadIndex(gGroupID, gGroupIndex);
	if (FaceCellExist(faceCellIndex))
	{
		uint3 faceCellIndexXYZ = DeflattenFaceCellIndexXYZ(faceCellIndex);
		uint3 cellIndexXYZ = min(faceCellIndexXYZ, uPass.mCellCount - 1);
		uint cellIndex = FlattenCellIndexClamp(cellIndexXYZ);
		if (uPass.mWaterSimMode == 0 || uPass.mWaterSimMode == 1) // flip
		{
			if (CellExist(cellIndexXYZ) && gWaterSimCellBuffer[cellIndex].mType == 0.0f)
			{
				uint indexNegJ = FlattenCellIndexClamp(faceCellIndexXYZ - uint3(0, 1, 0));
				uint3 cellFaceIndices = FlattenCellFaceIndices(faceCellIndexXYZ, faceCellIndexXYZ, faceCellIndexXYZ);

				WaterSimCellFace cellFaceX = gWaterSimCellFaceBufferSrc[cellFaceIndices.x];
				WaterSimCellFace cellFaceY = gWaterSimCellFaceBufferSrc[cellFaceIndices.y];
				WaterSimCellFace cellFaceZ = gWaterSimCellFaceBufferSrc[cellFaceIndices.z];

				// advect cell face // nope - this is not plain grid based MAC

				// grid force
				if (uPass.mApplyForce)
				{
					cellFaceY.mVelocity = ApplyGravity(cellFaceY.mVelocity);
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
				gWaterSimCellFaceBufferDest[cellFaceIndices.x] = cellFaceX;
				gWaterSimCellFaceBufferDest[cellFaceIndices.y] = cellFaceY;
				gWaterSimCellFaceBufferDest[cellFaceIndices.z] = cellFaceZ;
			}
		}
		else if (uPass.mWaterSimMode == 2 || uPass.mWaterSimMode == 3) // mpm
		{
			if (CellExist(cellIndexXYZ))
			{
				WaterSimCell cell = gWaterSimCellBuffer[cellIndex];
				float mass = asfloat(cell.mMassU32);
				if (mass > 0.0f)
				{
					// convert momentum to velocity
					float3 velocity = float3(asfloat(cell.mVelocityXU32), asfloat(cell.mVelocityYU32), asfloat(cell.mVelocityZU32)) / mass;

					cell.mOldVelocity = velocity;

					// grid force
					if (uPass.mApplyForce)
					{
						velocity.y = ApplyGravity(velocity.y);
					}

					// boundary condition
					if (faceCellIndexXYZ.x == 0 || faceCellIndexXYZ.x == uPass.mCellCount.x)
						velocity.x = 0.0f;
					if (faceCellIndexXYZ.y == 0 || faceCellIndexXYZ.y == uPass.mCellCount.y)
						velocity.y = 0.0f;
					if (faceCellIndexXYZ.z == 0 || faceCellIndexXYZ.z == uPass.mCellCount.z)
						velocity.z = 0.0f;

					cell.mMassU32 = 0;
					cell.mVelocityXU32 = 0;
					cell.mVelocityYU32 = 0;
					cell.mVelocityZU32 = 0;
					cell.mMass = mass;
					cell.mVelocity = velocity;
					gWaterSimCellBuffer[cellIndex] = cell;
				}
			}
		}
	}
}
