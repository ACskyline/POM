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
			if (particle.mAlive != 1.0f && particleIndex < uPass.mAliveParticleCount)
			{
				particle = (WaterSimParticle)0;
				particle.mPos = uPass.mParticleSpawnSourcePos + frandom3once(particleIndex) * uPass.mParticleSpawnSourceSpan;
				particle.mCellIndexXYZ = uint4(particle.mPos / uPass.mCellSize, 0); // for debugging purpose
				particle.mVelocity = float3(0.0f, 0.0f, 0.0f);
				particle.mAlive = 1.0f;
				particle.mF = IDENTITY_3X3;
			}
		}
		else
		{
			particle = (WaterSimParticle)0;
		}
		gWaterSimParticleBuffer[particleIndex] = particle;
	}
}
