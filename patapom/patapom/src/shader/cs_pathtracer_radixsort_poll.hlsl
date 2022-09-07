#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "PathTracerUtil.hlsli"

cbuffer PassUniformBuffer : register(b0, SPACE(PASS))
{
	PassUniformPathTracerBuildScene uPass;
};

StructuredBuffer<MeshPT> gMeshBufferPT : register(t0, SPACE(PASS));
RWStructuredBuffer<AabbProxy> gProxyBuffer : register(u0, SPACE(PASS));
RWStructuredBuffer<uint4> gRadixSortBitCountArray : register(u1, SPACE(PASS));
RWStructuredBuffer<uint4> gRadixSortPrefixSumAuxArray : register(u2, SPACE(PASS)); // element count = NextPowerOfTwo32(PT_RADIXSORT_THREAD_COUNT_MAX) need to be initialized to 0 to handle non power of 2 element count
RWStructuredBuffer<GlobalBvhSettings> gGlobalBvhSettingBuffer : register(u3, SPACE(PASS));

[numthreads(PT_BUILDBVH_THREAD_PER_THREADGROUP, 1, 1)]
void main(uint3 gDispatchThreadID : SV_DispatchThreadID)
{
	uint leafCount = 0;
	uint leafOffset = 0;
	uint bucketShift = 0;
	uint bucketMask = 0;
	uint threadIndex = GetDispatchThreadIndexPerRootBVH(gDispatchThreadID.x, uPass.mDispatchIndex, PT_BUILDBVH_THREAD_PER_DISPATCH);
	GetBucketShiftAndMask(uPass.mRadixSortBitGroupIndex, bucketShift, bucketMask);
	GetLeafCountAndOffsets(gMeshBufferPT, uPass.mBuildBvhType, uPass.mBuildBvhMeshIndex, uPass.mBuildBvhMeshCount, leafCount, leafOffset);

	uint2 sum = uint2(0, 0);
	for (uint i = 0; i < PT_BUILDBVH_ENTRY_PER_THREAD; i++)
	{
		uint entryIndex = GetEntryIndexPerRootBVH(i, threadIndex, PT_BUILDBVH_ENTRY_PER_THREAD);
		if (entryIndex < leafCount)
		{
			uint entryKey = gProxyBuffer[entryIndex].mMortonCode;
			uint bit = (entryKey & bucketMask) >> bucketShift;
			uint2 tmp = bit == 0 ? uint2(1, 0) : uint2(0, 1);
			gRadixSortBitCountArray[entryIndex] = uint4(sum, tmp);
			sum += tmp;
		}
	}
	if (threadIndex < uPass.mRadixSortSweepThreadCount)
		gRadixSortPrefixSumAuxArray[threadIndex] = uint4(sum, 0, 0);
	if (threadIndex == uPass.mRadixSortSweepThreadCount - 1)
		gGlobalBvhSettingBuffer[0].mRadixSortReorderOffset = sum.x;
}
