#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

StructuredBuffer<WaterSimParticle> gWaterSimParticleBuffer : register(t0, SPACE(PASS));
RWTexture2D<uint> gWaterSimDepthBufferMax : register(u0, SPACE(PASS));
RWTexture2D<uint> gWaterSimDepthBufferMin : register(u1, SPACE(PASS));

[numthreads(WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_X, WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_Y, WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_Z)]
void main(uint3 gGroupID : SV_GroupID, uint gGroupIndex : SV_GroupIndex)
{
	uint particleIndex = GetParticleThreadIndex(gGroupID, gGroupIndex);
	if (particleIndex < uPass.mParticleCount)
	{
		WaterSimParticle particle = gWaterSimParticleBuffer[particleIndex];
		if (particle.mAlive)
		{
			float4 posWorld = float4(particle.mPos * uPass.mGridRenderScale + uPass.mGridRenderOffset, 1.0f);
			float4 posView = mul(uPass.mView, posWorld);
			float4 pos = mul(uPass.mProj, posView);
			pos /= pos.w;
			pos.xy = ((pos.xy + 1.0f.xx) * float2(0.5f, -0.5f) + float2(0.0f, 1.0f)) * uPass.mWaterSimBackbufferSize;
			uint depth = PackWaterSimParticleDepth(posView.z);
			for (uint i = 0; i < 5; i++)
			{
				for (uint j = 0; j < 5; j++)
				{
					uint2 screenPos = pos.xy + uint2(i, j);
					if (screenPos.x > 0 && screenPos.x < uPass.mWaterSimBackbufferSize.x &&
						screenPos.y > 0 && screenPos.y < uPass.mWaterSimBackbufferSize.y)
					{
						if (depth > gWaterSimDepthBufferMax[screenPos])
							InterlockedMax(gWaterSimDepthBufferMax[screenPos], depth);
						if (depth < gWaterSimDepthBufferMin[screenPos])
							InterlockedMin(gWaterSimDepthBufferMin[screenPos], depth);
					}
				}
			}
		}
	}
}
