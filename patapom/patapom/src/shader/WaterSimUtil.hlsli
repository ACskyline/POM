#ifndef WATERSIM_UTIL_H
#define WATERSIM_UTIL_H

#include "../engine/SharedHeader.h"

cbuffer PassUniformBuffer : register(b0, SPACE(PASS))
{
	PassUniformWaterSim uPass;
};

#define YOUNGS_MODULUS	uPass.mYoungModulus
#define POISSON_RATIO	uPass.mPoissonRatio
#if WATERSIM_STABLE_NEO_HOOKEAN
#define ELASTIC_MU		(4.0f / 3.0f * YOUNGS_MODULUS / (2.0f * (1.0f + POISSON_RATIO)))
#define ELASTIC_LAMDA	(5.0f / 6.0f * ELASTIC_MU + YOUNGS_MODULUS * POISSON_RATIO / ((1.0f + POISSON_RATIO) * (1.0f - 2.0f * POISSON_RATIO)))
#else
#define ELASTIC_MU		(YOUNGS_MODULUS / (2.0f * (1.0f + POISSON_RATIO)))
#define ELASTIC_LAMDA	(YOUNGS_MODULUS * POISSON_RATIO / ((1.0f + POISSON_RATIO) * (1.0f - 2.0f * POISSON_RATIO)))
#endif

bool ShouldApplyForce()
{
	// time is not correct for the very first frame so we don't apply force
	return uPass.mApplyForce > 0 && uFrame.mFrameCountSinceGameStart > 0;
}

struct VS_OUTPUT_P2G
{
	float4 pos : SV_POSITION;
	nointerpolation uint particleIndex : PARTICLE;
	nointerpolation uint3 dxyz : DXYZ;
	nointerpolation uint cellIndex : CELL;
};

// (0,0) ------------ (width,0)
//   |  0, 1, 2, 3, 4,  ... |
//   |  width, width+1, ... |
//   |                      |
//   |                      |
//   |                      |
//   |                      |
// (0,height) ------- (width,height)
uint2 CellIndexToPixelIndices(uint cellIndex)
{
	return uint2(cellIndex % uPass.mCellRenderTextureSize.x, cellIndex / uPass.mCellRenderTextureSize.x);
}

// (-1,1) --------------- (1,1)
//    |  0, 1, 2, 3, 4, ... |
//    |  width, width+1, ...|
//    |                     |
//    |                     |
//    |                     |
//    |                     |
// (-1,-1) -------------- (1,-1)
float4 CellIndexToPixelCoords(uint cellIndex)
{
	return float4((CellIndexToPixelIndices(cellIndex) + 0.5f) / (float)uPass.mCellRenderTextureSize * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.5, 1.0f);
}

#define WaterSimTimeStep (uPass.mTimeStepScale * uFrame.mLastFrameTimeInSecond)
#define WATERSIM_VELOCITY_MAX 1000000.0f

float3 ApplyExplosion(float3 particlePos, float3 explosionPos, float3 explosionScale)
{
	float3 dir = normalize(particlePos - explosionPos);
	float distanceScale = 1.0f / max(1.0f, length(particlePos - explosionPos));
	return max(-WATERSIM_VELOCITY_MAX, min(WATERSIM_VELOCITY_MAX, dir * distanceScale * explosionScale * WaterSimTimeStep));
}

#define GetCellFaceVar(cellFaceIndex, var) (gWaterSimCellFaceBuffer[cellFaceIndex].var)
#define SetCellFaceVar(cellFaceIndex, var, val) gWaterSimCellFaceBuffer[cellFaceIndex].var = val
#define GetCellFaceVelocity(cellFaceIndex) GetCellFaceVar(cellFaceIndex, mVelocity)
#define SetCellFaceVelocity(cellFaceIndex, val) SetCellFaceVar(cellFaceIndex, mVelocity, val)
#define UpdateCellFaceOldVelocity(cellFaceIndex) SetCellFaceVar(cellFaceIndex, mOldVelocity, GetCellFaceVelocity(cellFaceIndex))

#define GetParticleThreadIndex(groupID, groupIndex) \
(groupID.x * WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_X * WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_Y * WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_Z + groupIndex)

#define GetThreadIndex(groupID, groupIndex) \
(groupID.x * WATERSIM_THREAD_PER_THREADGROUP_X * WATERSIM_THREAD_PER_THREADGROUP_Y * WATERSIM_THREAD_PER_THREADGROUP_Z + groupIndex)

// y
// |     / z
// |    /
// |   /
// |  /
// | /
// |--------------- x

// x * y * z = n cells
// |------------------------x0------------------------|------------------------x1------------------------|....|------------------------xn-1------------------------|
// |----------y0----------|----------y1----------|....|----------y0----------|----------y1----------|....|
// |---z0---|---z1---|....|---z0---|---z1---|....|....|---z0---|---z1---|....|---z0---|---z1---|....|....|
uint GetCellCount()
{
	return uPass.mCellCount.x * uPass.mCellCount.y * uPass.mCellCount.z;
}

bool CellExist(uint cellIndex)
{
	return cellIndex < GetCellCount();
}

bool CellExistXYZ(uint3 cellIndexXYZ)
{
	return all(cellIndexXYZ < uPass.mCellCount);
}

uint FlattenCellIndexClamp(uint3 indexXYZ)
{
	indexXYZ = clamp(indexXYZ, 0, uPass.mCellCount - 1);
	return indexXYZ.x * uPass.mCellCount.y * uPass.mCellCount.z +
		indexXYZ.y * uPass.mCellCount.z +
		indexXYZ.z;
}

uint3 DeflattenCellIndexXYZ(uint index)
{
	uint3 cellCount = uPass.mCellCount;
	uint x = index / (cellCount.y * cellCount.z);
	uint y = (index - x * (cellCount.y * cellCount.z)) / cellCount.z;
	uint z = index - x * (cellCount.y * cellCount.z) - y * cellCount.z;
	return uint3(x, y, z);
}

uint3 DeflattenFaceCellIndexXYZ(uint index)
{
	uint3 faceCellCount = uPass.mCellCount + 1;
	uint x = index / (faceCellCount.y * faceCellCount.z);
	uint y = (index - x * (faceCellCount.y * faceCellCount.z)) / faceCellCount.z;
	uint z = index - x * (faceCellCount.y * faceCellCount.z) - y * faceCellCount.z;
	return uint3(x, y, z);
}

uint3 GetFloorCellMinIndex(float3 pos, out uint cellIndex)
{
	uint3 indexXYZ = uint3(pos / uPass.mCellSize);
	indexXYZ = clamp(indexXYZ, 0, uPass.mCellCount - 1);
	cellIndex = FlattenCellIndexClamp(indexXYZ);
	return indexXYZ;
}

// help function to find the closest 8 cell centers
uint3 GetFloorCellCenterIndex(float3 pos, out uint closestFloorCellIndex)
{
	// offset by half cell size to get closest floor cell
	float halfCellSize = 0.5f * uPass.mCellSize;
	uint3 closestFloorCellIndexXYZ = uint3(max(0.0f, pos - halfCellSize) / uPass.mCellSize);
	closestFloorCellIndex = FlattenCellIndexClamp(closestFloorCellIndexXYZ);
	return closestFloorCellIndexXYZ;
}

// x * y * z = 3 * (n + 1) cell faces
// |------------------------x0------------------------|------------------------x1------------------------|....|------------------------xn------------------------|
// |----------y0----------|----------y1----------|....|----------y0----------|----------y1----------|....|
// |---z0---|---z1---|....|---z0---|---z1---|....|....|---z0---|---z1---|....|---z0---|---z1---|....|....|
// |fx|fy|fz|fx|fy|fz|....|fx|fy|fz|fx|fy|fz|....|....|fx|fy|fz|fx|fy|fz|....|fx|fy|fz|fx|fy|fz|....|....|
uint GetFaceCellCount()
{
	return (uPass.mCellCount.x + 1) * (uPass.mCellCount.y + 1) * (uPass.mCellCount.z + 1);
}

bool FaceCellExist(uint cellIndex)
{
	return cellIndex < GetFaceCellCount();
}

uint FlattenCellXFaceIndex(uint3 cellIndexXYZforFaceX)
{
	cellIndexXYZforFaceX = clamp(cellIndexXYZforFaceX, 0, uPass.mCellCount);
	return 3 * (cellIndexXYZforFaceX.x * (uPass.mCellCount.y + 1) * (uPass.mCellCount.z + 1) + cellIndexXYZforFaceX.y * (uPass.mCellCount.z + 1) + cellIndexXYZforFaceX.z);
}

uint FlattenCellYFaceIndex(uint3 cellIndexXYZforFaceY)
{
	cellIndexXYZforFaceY = clamp(cellIndexXYZforFaceY, 0, uPass.mCellCount);
	return 3 * (cellIndexXYZforFaceY.x * (uPass.mCellCount.y + 1) * (uPass.mCellCount.z + 1) + cellIndexXYZforFaceY.y * (uPass.mCellCount.z + 1) + cellIndexXYZforFaceY.z) + 1;
}

uint FlattenCellZFaceIndex(uint3 cellIndexXYZforFaceZ)
{
	cellIndexXYZforFaceZ = clamp(cellIndexXYZforFaceZ, 0, uPass.mCellCount);
	return 3 * (cellIndexXYZforFaceZ.x * (uPass.mCellCount.y + 1) * (uPass.mCellCount.z + 1) + cellIndexXYZforFaceZ.y * (uPass.mCellCount.z + 1) + cellIndexXYZforFaceZ.z) + 2;
}

uint3 FlattenCellFaceIndices(uint3 cellIndexXYZforFaceX, uint3 cellIndexXYZforFaceY, uint3 cellIndexXYZforFaceZ)
{
	return uint3(
		FlattenCellXFaceIndex(cellIndexXYZforFaceX),
		FlattenCellYFaceIndex(cellIndexXYZforFaceY),
		FlattenCellZFaceIndex(cellIndexXYZforFaceZ));
}

// helper function to find the closest 8 cell face centers
uint3 GetClosestFloorCellXFaceIndex(float3 pos, out uint closestFloorCellXFaceIndex)
{
	float halfCellSize = 0.5f * uPass.mCellSize;
	// the cell face is defined at the center of the face, so we subtract half cell size on the other 2 axis
	uint3 closestFloorCellXFaceIndexXYZ = uint3(max(0.0f, pos - float3(0.0f, halfCellSize, halfCellSize)) / uPass.mCellSize);
	closestFloorCellXFaceIndex = FlattenCellXFaceIndex(closestFloorCellXFaceIndexXYZ);
	return closestFloorCellXFaceIndexXYZ;
}

uint3 GetClosestFloorCellYFaceIndex(float3 pos, out uint closestFloorCellYFaceIndex)
{
	float halfCellSize = 0.5f * uPass.mCellSize;
	// the cell face is defined at the center of the face, so we subtract half cell size on the other 2 axis
	uint3 closestFloorCellYFaceIndexXYZ = uint3(max(0.0f, pos - float3(halfCellSize, 0.0f, halfCellSize)) / uPass.mCellSize);
	closestFloorCellYFaceIndex = FlattenCellYFaceIndex(closestFloorCellYFaceIndexXYZ);
	return closestFloorCellYFaceIndexXYZ;
}

uint3 GetClosestFloorCellZFaceIndex(float3 pos, out uint closestFloorCellZFaceIndex)
{
	float halfCellSize = 0.5f * uPass.mCellSize;
	// the cell face is defined at the center of the face, so we subtract half cell size on the other 2 axis
	uint3 closestFloorCellZFaceIndexXYZ = uint3(max(0.0f, pos - float3(halfCellSize, halfCellSize, 0.0f)) / uPass.mCellSize);
	closestFloorCellZFaceIndex = FlattenCellZFaceIndex(closestFloorCellZFaceIndexXYZ);
	return closestFloorCellZFaceIndexXYZ;
}

float3 GetCellCenterPos(uint3 cellIndexXYZ)
{
	float halfCellSize = 0.5f * uPass.mCellSize;
	return cellIndexXYZ * uPass.mCellSize + halfCellSize;
}

float3 GetCellXFaceCenterPos(uint3 cellIndexXYZ)
{
	float halfCellSize = 0.5f * uPass.mCellSize;
	return cellIndexXYZ * uPass.mCellSize + float3(0.0f, halfCellSize, halfCellSize);
}

float3 GetCellYFaceCenterPos(uint3 cellIndexXYZ)
{
	float halfCellSize = 0.5f * uPass.mCellSize;
	return cellIndexXYZ * uPass.mCellSize + float3(halfCellSize, 0.0f, halfCellSize);
}

float3 GetCellZFaceCenterPos(uint3 cellIndexXYZ)
{
	float halfCellSize = 0.5f * uPass.mCellSize;
	return cellIndexXYZ * uPass.mCellSize + float3(halfCellSize, halfCellSize, 0.0f);
}

float3 GetClosestCellFaceIndicesWithWeight(uint3 cellIndexXYZ, float3 pos, out uint3 cellFaceIndices)
{
	float3 cellPosCenter = GetCellCenterPos(cellIndexXYZ);
	float3 halfCellSize = 0.5f * uPass.mCellSize;
	float3 cellPosMin = cellPosCenter - halfCellSize;
	float3 cellPosMax = cellPosCenter + halfCellSize;
	// weights are linearly interpolated based on closet cell face, accounted for cell size and particle count
	// when particle is closer to ceil face, sign > 0
	// when particle is closer to floor face, sign < 0
	float3 faceSign = sign(pos - cellPosCenter);
	// when particle is closer to face, weight is larger
	// weights are normalized by the density of particles, so that using denser particles doesn't scale the simulation unexpectedly
	float3 weights = (1 - (saturate(faceSign) * abs(pos - cellPosMax) + saturate(-faceSign) * abs(pos - cellPosMin)) / halfCellSize) / uPass.mParticleCountPerCell;
	uint3 closestCellFaceIndexX = cellIndexXYZ + uint3(saturate(faceSign.x), 0, 0);
	uint3 closestCellFaceIndexY = cellIndexXYZ + uint3(0, saturate(faceSign.y), 0);
	uint3 closestCellFaceIndexZ = cellIndexXYZ + uint3(0, 0, saturate(faceSign.z));
	cellFaceIndices = FlattenCellFaceIndices(closestCellFaceIndexX, closestCellFaceIndexY, closestCellFaceIndexZ);
	return weights;
}

uint GetCellFaceIndicesAndWeight(float3 pos, out uint3 minFaceFlattenedIndices, out uint3 maxFaceFlattenedIndices, out float3 minFaceWeights, out float3 maxFaceWeights)
{
	uint cellIndex;
	uint3 minFaceIndicesXYZ = GetFloorCellMinIndex(pos, cellIndex);
	//maxFaceWeights = (pos - minFaceIndicesXYZ * uPass.mCellSize) / uPass.mCellSize;
	//minFaceWeights = 1.0f - maxFaceWeights;
	float halfSize = 0.5f * uPass.mCellSize;
	minFaceWeights = saturate((halfSize - abs(pos - (minFaceIndicesXYZ + 0.5f) * uPass.mCellSize)) / halfSize);
	maxFaceWeights = 1.0f - minFaceWeights;
	minFaceFlattenedIndices = FlattenCellFaceIndices(minFaceIndicesXYZ, minFaceIndicesXYZ, minFaceIndicesXYZ);
	maxFaceFlattenedIndices = FlattenCellFaceIndices(minFaceIndicesXYZ + uint3(1,0,0), minFaceIndicesXYZ + uint3(0, 1, 0), minFaceIndicesXYZ + uint3(0, 0, 1));
	return cellIndex;
}

WaterSimCellFace LerpCellFace(WaterSimCellFace A, WaterSimCellFace B, float weight)
{
	WaterSimCellFace result = (WaterSimCellFace)0;
	result.mVelocity = lerp(A.mVelocity, B.mVelocity, weight);
	return result;
}

uint PackWaterSimParticleDepth(float viewDepth)
{
	return saturate(viewDepth / WATERSIM_VIEWDEPTH_MAX) * (float)WATERSIM_DEPTHBUFFER_MAX;
}

float UnpackWaterSimParticleDepth(uint packed)
{
	return packed / (float)WATERSIM_DEPTHBUFFER_MAX * WATERSIM_VIEWDEPTH_MAX;
}

#endif
