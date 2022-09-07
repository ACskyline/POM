#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "PathTracerUtil.hlsli"

cbuffer PassUniformBuffer : register(b0, SPACE(PASS))
{
	PassUniformPathTracerBuildScene uPass;
};

RWStructuredBuffer<uint4> gRadixSortPrefixSumAuxArray : register(u0, SPACE(PASS)); // element count = PT_RADIXSORT_DISPATCH_COUNT_MAX

[numthreads(PT_RADIXSORT_SWEEP_THREAD_PER_THREADGROUP, 1, 1)]
void main(uint3 gDispatchThreadID : SV_DispatchThreadID)
{
	uint k = gDispatchThreadID.x;
	uint d = uPass.mDispatchIndex; // counting upwards from 0 to log2(n) - 1
	uint n = NextPowerOfTwo32(uPass.mRadixSortSweepThreadCount);
	if ((k >= 0) && (k < n) && (k % NTH_POWER_OF_TWO(d + 1) == 0))
	{
		uint selfIndex = k + NTH_POWER_OF_TWO(d + 1) - 1;
		uint siblingIndex = k + NTH_POWER_OF_TWO(d) - 1;
		uint2 self = gRadixSortPrefixSumAuxArray[selfIndex].xy;
		uint2 sibling = gRadixSortPrefixSumAuxArray[siblingIndex].xy;
		gRadixSortPrefixSumAuxArray[selfIndex] = uint4(self + sibling, selfIndex, siblingIndex);
		gRadixSortPrefixSumAuxArray[siblingIndex] = uint4(sibling, self);
	}
}
