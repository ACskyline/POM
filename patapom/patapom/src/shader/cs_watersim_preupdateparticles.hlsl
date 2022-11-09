#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

RWStructuredBuffer<WaterSimCell> gWaterSimCellBuffer : register(u0, SPACE(PASS));
RWStructuredBuffer<WaterSimParticle> gWaterSimParticleBuffer : register(u1, SPACE(PASS));

#include "WaterSimCellResourceUtil.hlsli"

[numthreads(WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_X, WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_Y, WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_Z)]
void main(uint3 gGroupID : SV_GroupID, uint gGroupIndex : SV_GroupIndex)
{
	uint particleIndex = GetParticleThreadIndex(gGroupID, gGroupIndex);
	if (particleIndex < uPass.mParticleCount)
	{
		WaterSimParticle particle = gWaterSimParticleBuffer[particleIndex];
		if (particle.mAlive)
		{
			// mls-mpm
			uint index;
			uint3 indexXYZ = GetFloorCellMinIndex(particle.mPos, index);
			float3 cellDiff = particle.mPos / uPass.mCellSize - indexXYZ - 0.5f;
			float3 weights[3];
			weights[0] = 0.5f * pow(0.5f - cellDiff, 2.0f);
			weights[1] = 0.75f - pow(cellDiff, 2.0f); // is this correct ?
			weights[2] = 0.5f * pow(0.5f + cellDiff, 2.0f);

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
							float3 Q = mul(particle.mC, dCellDiff);
							float massContrib = dWeight; // assume mass is 1
							float3 weightedVelocity = massContrib * (particle.mVelocity + Q); // TODO: adding Q makes particles go up for some reason
							InterLockedAddCellMass(dIndex, massContrib);
							// TODO: rearrange the sequence based on particle index to avoid potential wait when writing to the same component of the cell's velocity
							InterLockedAddCellVelocityX(dIndex, weightedVelocity.x);
							InterLockedAddCellVelocityY(dIndex, weightedVelocity.y);
							InterLockedAddCellVelocityZ(dIndex, weightedVelocity.z);
							InterlockedAdd(gWaterSimCellBuffer[dIndex].mParticleCount, 1);
						}
					}
				}
			}

			// debug
			particle.mCellIndexXYZ = uint4(indexXYZ, index);
			gWaterSimParticleBuffer[particleIndex] = particle;
		} // (particle.mAlive)
	} // (particleIndex < uPass.mParticleCount)
}
