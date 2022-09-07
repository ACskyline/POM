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
		// TODO: avoid spawning particles in solid cells
		WaterSimParticle particle = gWaterSimParticleBuffer[particleIndex];
		if (uPass.mAliveParticleCount > 0)
		{
			if (particleIndex < uPass.mAliveParticleCount && particle.mAlive != 1.0f)
			{
				particle.mPos = uPass.mParticleSpawnSourcePos + frandom3once(particleIndex) * uPass.mParticleSpawnSourceSpan;
				particle.mCellIndexXYZ = uint3(particle.mPos / uPass.mCellSize); // for debugging purpose
				particle.mVelocity = float3(0.0f, 0.0f, 0.0f);
				particle.mAlive = 1.0f;
			}
		}
		else
		{
			particle = (WaterSimParticle)0;
		}
		gWaterSimParticleBuffer[particleIndex] = particle;
	}
}
