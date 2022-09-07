#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

RWTexture2D<uint> gWaterSimDepthBufferMax : register(u0, SPACE(PASS));
RWTexture2D<uint> gWaterSimDepthBufferMin : register(u1, SPACE(PASS));
RWTexture2D<float4> gWaterSimDebugBuffer : register(u2, SPACE(PASS));

[numthreads(WATERSIM_THREAD_PER_THREADGROUP_X, WATERSIM_THREAD_PER_THREADGROUP_Y, 1)]
void main(uint3 gDispatchThreadID : SV_DispatchThreadID)
{
	uint2 screenPos = gDispatchThreadID.xy;
	if (any(screenPos < uPass.mWaterSimBackbufferSize))
	{
		gWaterSimDepthBufferMax[screenPos] = 0;
		gWaterSimDepthBufferMin[screenPos] = 255;
		gWaterSimDebugBuffer[screenPos] = 0.0f.xxxx;
	}
}
