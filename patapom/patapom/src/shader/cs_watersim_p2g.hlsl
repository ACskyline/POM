#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

RWStructuredBuffer<WaterSimParticle> gWaterSimParticleBuffer : register(u0, SPACE(PASS));
RWStructuredBuffer<WaterSimCell> gWaterSimCellBuffer : register(u1, SPACE(PASS));
RWStructuredBuffer<WaterSimCellFace> gWaterSimCellFaceBuffer : register(u2, SPACE(PASS));

void InterLockedAddCellFaceVelocity(uint faceCellIndex, float velocity)
{
	while(true)
	{
		uint oldVelocityU32 = gWaterSimCellFaceBuffer[faceCellIndex].mVelocityU32;
		float oldVelocity = asfloat(oldVelocityU32);
		uint originalVelocityU32;
		InterlockedCompareExchange(gWaterSimCellFaceBuffer[faceCellIndex].mVelocityU32, oldVelocityU32, asuint(oldVelocity + velocity), originalVelocityU32);
		if (originalVelocityU32 == oldVelocityU32) // chosen
			break;
	}
}

void InterLockedAddCellFaceWeight(uint faceCellIndex, float weight)
{
	while (true)
	{
		uint oldWeightU32 = gWaterSimCellFaceBuffer[faceCellIndex].mWeightU32;
		float oldWeight = asfloat(oldWeightU32);
		uint originalWeightU32;
		InterlockedCompareExchange(gWaterSimCellFaceBuffer[faceCellIndex].mWeightU32, oldWeightU32, asuint(oldWeight + weight), originalWeightU32);
		if (originalWeightU32 == oldWeightU32) // chosen
			break;
	}
}

float3 ApplyExplosion(float3 particlePos, float3 explosionPos, float3 explosionScale)
{
	float3 dir = normalize(particlePos - explosionPos);
	float maxDistance = uPass.mCellSize * length(uPass.mCellCount);
	float distanceScale = length(explosionPos - particlePos) / maxDistance;
	return max(-10000.0f, min(10000.0f, dir * distanceScale * explosionScale * WaterSimTimeStep));
}

[numthreads(WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_X, WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_Y, WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_Z)]
void main(uint3 gGroupID : SV_GroupID, uint gGroupIndex : SV_GroupIndex)
{
	uint particleIndex = GetParticleThreadIndex(gGroupID, gGroupIndex);
	if (particleIndex < uPass.mParticleCount)
	{
		// the following is the FLIP method
		// 1. P2G
		WaterSimParticle particle = gWaterSimParticleBuffer[particleIndex];
		if (particle.mAlive)
		{
			// debug
			//particle.mVelocity.y -= uPass.mGravitationalAccel * WaterSimTimeStep;
			
			// apply explosion force
			if (uPass.mApplyForce)
			{
				if (uPass.mApplyExplosion)
				{
					particle.mVelocity += ApplyExplosion(particle.mPos, uPass.mExplosionPos, uPass.mExplosionForceScale);
				}
			}

			uint3 minFaceFlattenedIndices, maxFaceFlattenedIndices;
			float3 minFaceWeights, maxFaceWeights;
			uint cellIndex = GetCellFaceIndicesAndWeight(particle.mPos, minFaceFlattenedIndices, maxFaceFlattenedIndices, minFaceWeights, maxFaceWeights);
			uint3 cellIndexXYZ = DeflattenCellIndexXYZ(cellIndex);
			if (gWaterSimCellBuffer[cellIndex].mType != 1.0f)
			{
				uint indexPosI = FlattenCellIndex(cellIndexXYZ + uint3(1, 0, 0));
				uint indexNegI = FlattenCellIndex(cellIndexXYZ - uint3(1, 0, 0));
				uint indexPosJ = FlattenCellIndex(cellIndexXYZ + uint3(0, 1, 0));
				uint indexNegJ = FlattenCellIndex(cellIndexXYZ - uint3(0, 1, 0));
				uint indexPosK = FlattenCellIndex(cellIndexXYZ + uint3(0, 0, 1));
				uint indexNegK = FlattenCellIndex(cellIndexXYZ - uint3(0, 0, 1));

				bool solidCellPosI = gWaterSimCellBuffer[indexPosI].mType == 1;
				bool solidCellNegI = gWaterSimCellBuffer[indexNegI].mType == 1;
				bool solidCellPosJ = gWaterSimCellBuffer[indexPosJ].mType == 1;
				bool solidCellNegJ = gWaterSimCellBuffer[indexNegJ].mType == 1;
				bool solidCellPosK = gWaterSimCellBuffer[indexPosK].mType == 1;
				bool solidCellNegK = gWaterSimCellBuffer[indexNegK].mType == 1;

				if (solidCellPosI || solidCellNegI)
				{
					if (solidCellPosI && solidCellNegI)
					{
						maxFaceWeights.x = 0.0f;
						minFaceWeights.x = 0.0f;
					}
					else if (solidCellPosI)
					{
						maxFaceWeights.x = 0.0f;
						minFaceWeights.x = 1.0f;
					}
					else
					{
						maxFaceWeights.x = 1.0f;
						minFaceWeights.x = 0.0f;
					}
				}

				if (solidCellPosJ || solidCellNegJ)
				{
					if (solidCellPosJ && solidCellNegJ)
					{
						maxFaceWeights.y = 0.0f;
						minFaceWeights.y = 0.0f;
					}
					else if (solidCellPosJ)
					{
						maxFaceWeights.y = 0.0f;
						minFaceWeights.y = 1.0f;
					}
					else
					{
						maxFaceWeights.y = 1.0f;
						minFaceWeights.y = 0.0f;
					}
				}

				if (solidCellPosK || solidCellNegK)
				{
					if (solidCellPosK && solidCellNegK)
					{
						maxFaceWeights.z = 0.0f;
						minFaceWeights.z = 0.0f;
					}
					else if (solidCellPosK)
					{
						maxFaceWeights.z = 0.0f;
						minFaceWeights.z = 1.0f;
					}
					else
					{
						maxFaceWeights.z = 1.0f;
						minFaceWeights.z = 0.0f;
					}
				}

				uint oldParticleCount = gWaterSimCellBuffer[cellIndex].mParticleCount;
				while (oldParticleCount < WATERSIM_PARTICLE_COUNT_PER_CELL)
				{
					uint originalParticleCount;
					InterlockedCompareExchange(gWaterSimCellBuffer[cellIndex].mParticleCount, oldParticleCount, oldParticleCount + 1, originalParticleCount);
					if (originalParticleCount == oldParticleCount)
					{
						float3 minFaceVelocity = minFaceWeights * particle.mVelocity;
						float3 maxFaceVelocity = maxFaceWeights * particle.mVelocity;

						if (!solidCellNegI)
						{
							InterLockedAddCellFaceVelocity(minFaceFlattenedIndices.x, minFaceVelocity.x);
							InterLockedAddCellFaceWeight(minFaceFlattenedIndices.x, minFaceWeights.x);
						}
						if (!solidCellNegJ)
						{
							InterLockedAddCellFaceVelocity(minFaceFlattenedIndices.y, minFaceVelocity.y);
							InterLockedAddCellFaceWeight(minFaceFlattenedIndices.y, minFaceWeights.y);
						}
						if (!solidCellNegK)
						{
							InterLockedAddCellFaceVelocity(minFaceFlattenedIndices.z, minFaceVelocity.z);
							InterLockedAddCellFaceWeight(minFaceFlattenedIndices.z, minFaceWeights.z);
						}
						//if (!solidCellPosI)
						//{
						//	InterLockedAddCellFaceVelocity(maxFaceFlattenedIndices.x, maxFaceVelocity.x);
						//	InterLockedAddCellFaceWeight(maxFaceFlattenedIndices.x, maxFaceWeights.x);
						//}
						//if (!solidCellPosJ)
						//{
						//	InterLockedAddCellFaceVelocity(maxFaceFlattenedIndices.y, maxFaceVelocity.y);
						//	InterLockedAddCellFaceWeight(maxFaceFlattenedIndices.y, maxFaceWeights.y);
						//}
						//if (!solidCellPosK)
						//{
						//	InterLockedAddCellFaceVelocity(maxFaceFlattenedIndices.z, maxFaceVelocity.z);
						//	InterLockedAddCellFaceWeight(maxFaceFlattenedIndices.z, maxFaceWeights.z);
						//}
						break;
					}

					oldParticleCount = gWaterSimCellBuffer[cellIndex].mParticleCount;
				}

				if (oldParticleCount >= WATERSIM_PARTICLE_COUNT_PER_CELL)
				{
					particle.mAlive = 0.0f;
					gWaterSimParticleBuffer[particleIndex] = particle;
				}
			}
			// these are done separately in a different dispatch
			// 2. solving on the grid
			// 3. G2P - interpolated delta velocity
			// 4. move particles
		}
	}
}
