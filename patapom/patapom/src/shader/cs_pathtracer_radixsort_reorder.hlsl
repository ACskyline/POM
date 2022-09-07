#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "PathTracerUtil.hlsli"

cbuffer PassUniformBuffer : register(b0, SPACE(PASS))
{
	PassUniformPathTracerBuildScene uPass;
};

StructuredBuffer<MeshPT> gMeshBufferPT : register(t0, SPACE(PASS));
StructuredBuffer<AabbProxy> gRadixSortEntryBufferIn : register(t1, SPACE(PASS));
StructuredBuffer<uint4> gRadixSortBitCountArray : register(t2, SPACE(PASS));
StructuredBuffer<uint4> gRadixSortPrefixSumAuxArray : register(t3, SPACE(PASS)); // element count = PT_RADIXSORT_DISPATCH_COUNT_MAX
StructuredBuffer<GlobalBvhSettings> gGlobalBvhSettingBuffer : register(t4, SPACE(PASS));
RWStructuredBuffer<AabbProxy> gRadixSortEntryBufferOut : register(u0, SPACE(PASS));

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
	uint newEntryIndexOffset = gRadixSortPrefixSumAuxArray[uPass.mRadixSortSweepThreadCount - 1].x + gGlobalBvhSettingBuffer[0].mRadixSortReorderOffset;

	for (uint i = 0; i < PT_BUILDBVH_ENTRY_PER_THREAD; i++)
	{
		uint entryIndex = GetEntryIndexPerRootBVH(i, threadIndex, PT_BUILDBVH_ENTRY_PER_THREAD);
		if (entryIndex < leafCount)
		{
			uint entryKey = gRadixSortEntryBufferIn[entryIndex].mMortonCode;
			uint bit = (entryKey & bucketMask) >> bucketShift;
			uint newEntryIndex = 0;
			// after up sweep and down sweep, prefix sum aux arry store the sum of set/unset bit count of the previsou thread which will be used as offset for the current thread
			if (bit == 0)
				newEntryIndex = gRadixSortBitCountArray[entryIndex].x + gRadixSortPrefixSumAuxArray[threadIndex].x;
			else
				newEntryIndex = gRadixSortBitCountArray[entryIndex].y + gRadixSortPrefixSumAuxArray[threadIndex].y + newEntryIndexOffset;
			gRadixSortEntryBufferOut[newEntryIndex] = gRadixSortEntryBufferIn[entryIndex];
		}
	}
}
