#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

RWStructuredBuffer<WaterSimCell> gWaterSimCellBuffer : register(u0, SPACE(PASS));
RWStructuredBuffer<WaterSimParticle> gWaterSimParticleBuffer : register(u1, SPACE(PASS));

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

			float density = 0.0f;
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
							WaterSimCell cell = gWaterSimCellBuffer[dIndex];
							float mass = asfloat(cell.mMassU32);
							float dWeight = weights[dx].x * weights[dy].y * weights[dz].z;
							density += dWeight * mass;
						}
					}
				}
			}

			particle.mVolume0 = 1.0f / max(density, 0.0001f);
			particle.mF = IDENTITY_3X3;
			gWaterSimParticleBuffer[particleIndex] = particle;
		} // (particle.mAlive)
	} // (particleIndex < uPass.mParticleCount)
}
