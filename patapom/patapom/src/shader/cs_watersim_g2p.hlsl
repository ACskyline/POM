#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

StructuredBuffer<WaterSimCellFace> gWaterSimCellFaceBuffer : register(t0, SPACE(PASS));
RWStructuredBuffer<WaterSimCell> gWaterSimCellBuffer : register(u0, SPACE(PASS));
RWStructuredBuffer<WaterSimParticle> gWaterSimParticleBuffer : register(u1, SPACE(PASS));

#include "WaterSimCellFaceResourceUtil.hlsli"

void MovePosition(inout float3 pos, inout float3 velocity, float timeStep, float boundaryCellCount)
{
	float3 newPos = pos + velocity * timeStep;
	// outside
	float3 outside = float3((newPos < boundaryCellCount * uPass.mCellSize) || (newPos > (uPass.mCellCount - boundaryCellCount) * uPass.mCellSize));
	if (any(outside))
	{
		velocity = saturate(1.0f - outside) * velocity;
		newPos = clamp(newPos, (2.0f.xxx + EPSILON) * uPass.mCellSize, (uPass.mCellCount - (2.0f.xxx + EPSILON)) * uPass.mCellSize);
	}
	pos = newPos;
}

[numthreads(WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_X, WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_Y, WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_Z)]
void main(uint3 gGroupID : SV_GroupID, uint gGroupIndex : SV_GroupIndex)
{
	uint particleIndex = GetParticleThreadIndex(gGroupID, gGroupIndex);
	if (particleIndex < uPass.mParticleCount)
	{
		WaterSimParticle particle = gWaterSimParticleBuffer[particleIndex];
		if (particle.mAlive)
		{
			if (uPass.mWaterSimMode == 0 || uPass.mWaterSimMode == 1) // flip
			{
				uint3 minFaceFlattenedIndices, maxFaceFlattenedIndices;
				float3 minFaceWeights, maxFaceWeights;
				uint cellIndex = GetCellFaceIndicesAndWeight(particle.mPos, minFaceFlattenedIndices, maxFaceFlattenedIndices, minFaceWeights, maxFaceWeights);
				uint3 cellIndexXYZ = DeflattenCellIndexXYZ(cellIndex);

				uint indexPosI = FlattenCellIndexClamp(cellIndexXYZ + uint3(1, 0, 0));
				uint indexNegI = FlattenCellIndexClamp(cellIndexXYZ - uint3(1, 0, 0));
				uint indexPosJ = FlattenCellIndexClamp(cellIndexXYZ + uint3(0, 1, 0));
				uint indexNegJ = FlattenCellIndexClamp(cellIndexXYZ - uint3(0, 1, 0));
				uint indexPosK = FlattenCellIndexClamp(cellIndexXYZ + uint3(0, 0, 1));
				uint indexNegK = FlattenCellIndexClamp(cellIndexXYZ - uint3(0, 0, 1));

				bool solidCellPosI = gWaterSimCellBuffer[indexPosI].mType == 1;
				bool solidCellNegI = gWaterSimCellBuffer[indexNegI].mType == 1;
				bool solidCellPosJ = gWaterSimCellBuffer[indexPosJ].mType == 1;
				bool solidCellNegJ = gWaterSimCellBuffer[indexNegJ].mType == 1;
				bool solidCellPosK = gWaterSimCellBuffer[indexPosK].mType == 1;
				bool solidCellNegK = gWaterSimCellBuffer[indexNegK].mType == 1;

				InterlockedAdd(gWaterSimCellBuffer[cellIndex].mParticleCount, -1);
				float3 velocity =
					minFaceWeights * float3(gWaterSimCellFaceBuffer[minFaceFlattenedIndices.x].mVelocity,
						gWaterSimCellFaceBuffer[minFaceFlattenedIndices.y].mVelocity,
						gWaterSimCellFaceBuffer[minFaceFlattenedIndices.z].mVelocity) +
					maxFaceWeights * float3(gWaterSimCellFaceBuffer[maxFaceFlattenedIndices.x].mVelocity,
						gWaterSimCellFaceBuffer[maxFaceFlattenedIndices.y].mVelocity,
						gWaterSimCellFaceBuffer[maxFaceFlattenedIndices.z].mVelocity);
				float3 oldVelocity =
					minFaceWeights * float3(gWaterSimCellFaceBuffer[minFaceFlattenedIndices.x].mOldVelocity,
						gWaterSimCellFaceBuffer[minFaceFlattenedIndices.y].mOldVelocity,
						gWaterSimCellFaceBuffer[minFaceFlattenedIndices.z].mOldVelocity) +
					maxFaceWeights * float3(gWaterSimCellFaceBuffer[maxFaceFlattenedIndices.x].mOldVelocity,
						gWaterSimCellFaceBuffer[maxFaceFlattenedIndices.y].mOldVelocity,
						gWaterSimCellFaceBuffer[maxFaceFlattenedIndices.z].mOldVelocity);

				particle.mVelocity = (1.0f - uPass.mFlipScale) * velocity + uPass.mFlipScale * (particle.mVelocity + velocity - oldVelocity);

				// for debug purposes
				particle.mMinFaceWeights = minFaceWeights;
				particle.mMaxFaceWeights = maxFaceWeights;
				particle.mCellFaceMinIndexXYZ = minFaceFlattenedIndices;
				particle.mCellFaceMaxIndexXYZ = maxFaceFlattenedIndices;
				particle.mCellIndexXYZ = uint4(cellIndexXYZ, cellIndex);

				// move particles
				MovePosition(particle.mPos, particle.mVelocity, WaterSimTimeStep, 1.0f);

				// write back
				gWaterSimParticleBuffer[particleIndex] = particle;
			}
			else if (uPass.mWaterSimMode == 2 || uPass.mWaterSimMode == 3) // mpm
			{
				// mls-mpm
				uint index;
				uint3 indexXYZ = GetFloorCellMinIndex(particle.mPos, index);
				float3 cellDiff = particle.mPos / uPass.mCellSize - indexXYZ - 0.5f;
				float3 weights[3];
				weights[0] = 0.5f * pow(0.5f - cellDiff, 2.0f);
				weights[1] = 0.75f - pow(cellDiff, 2.0f); // is this correct ?
				weights[2] = 0.5f * pow(0.5f + cellDiff, 2.0f);

				float3x3 B = 0;
				float3 velocity = 0.0f.xxx;
				for (uint dx = 0; dx < 3; dx++)
				{
					for (uint dy = 0; dy < 3; dy++)
					{
						for (uint dz = 0; dz < 3; dz++)
						{
							uint3 dIndexXYZ = indexXYZ + uint3(dx, dy, dz) - 1;
							uint dIndex = FlattenCellIndexClamp(dIndexXYZ);
							if (CellExistXYZ(dIndexXYZ))
							{
								float dWeight = weights[dx].x * weights[dy].y * weights[dz].z;
								float3 dCellDiff = (dIndexXYZ + 0.5f) * uPass.mCellSize - particle.mPos;
								float3 weightedVelocity = dWeight * gWaterSimCellBuffer[dIndex].mVelocity;
								float3x3 b;
								b._m00_m01_m02 = weightedVelocity.x * dCellDiff; // row 0
								b._m10_m11_m12 = weightedVelocity.y * dCellDiff; // row 1
								b._m20_m21_m22 = weightedVelocity.z * dCellDiff; // row 2
								B += b;
								velocity += weightedVelocity;
							}
						}
					}
				}
				particle.mOldVelocity = particle.mVelocity;
				particle.mVelocity = velocity;
				particle.mC = B * 4.0f / (uPass.mCellSize * uPass.mCellSize);
				MovePosition(particle.mPos, particle.mVelocity, WaterSimTimeStep, 2.0f);
				particle.mCellIndexXYZ = uint4(indexXYZ, index);
				particle.mF = mul(IDENTITY_3X3 + particle.mC * WaterSimTimeStep, particle.mF);

				// write back
				gWaterSimParticleBuffer[particleIndex] = particle;
			}
		} // (particle.mAlive)
	} // (particleIndex < uPass.mParticleCount)
}
