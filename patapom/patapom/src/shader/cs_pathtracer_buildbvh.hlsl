#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "cs_pathtracer_util.hlsli"

cbuffer PassUniformBuffer : register(b0, SPACE(PASS))
{
	PassUniformPathTracerBuildScene uPass;
};

StructuredBuffer<MeshPT> gMeshBufferPT : register(t0, SPACE(PASS));
RWStructuredBuffer<AabbProxy> gProxyBuffer : register(u0, SPACE(PASS));
RWStructuredBuffer<BVH> gTempBvhBuffer : register(u1, SPACE(PASS));

int LongestCommonPrefix(int left, int right, int leafCount)
{
	if (left < 0 || left >= leafCount || right < 0 || right >= leafCount)
		return -1;
	uint lkey = gProxyBuffer[left].mMortonCode;
	uint rkey = gProxyBuffer[right].mMortonCode;
	int lcp = 31 - firstbithigh(lkey ^ rkey);
	if (lkey == rkey)
	{
		lcp += 31 - firstbithigh(left ^ right);
	}
	return lcp;
}

[numthreads(PT_BUILDBVH_THREAD_PER_THREADGROUP, 1, 1)]
void main(uint3 gDispatchThreadID : SV_DispatchThreadID)
{
	uint leafCount = 0;
	uint leafOffset = 0;
	uint threadIndex = GetDispatchThreadIndexPerRootBVH(gDispatchThreadID.x, uPass.mDispatchIndex, PT_BUILDBVH_THREAD_PER_DISPATCH);
	uint bvhOffset = GetLeafCountAndOffsets(gMeshBufferPT, uPass.mBuildBvhType, uPass.mBuildBvhMeshIndex, uPass.mBuildBvhMeshCount, leafCount, leafOffset);

	for (uint i = 0; i < PT_BUILDBVH_ENTRY_PER_THREAD; i++)
	{
		uint entryIndex = GetEntryIndexPerRootBVH(i, threadIndex, PT_BUILDBVH_ENTRY_PER_THREAD);
		// parallelize bvh node n - 1
		if (entryIndex < leafCount - 1)
		{
			int d = LongestCommonPrefix(entryIndex, entryIndex + 1, leafCount) - LongestCommonPrefix(entryIndex, entryIndex - 1, leafCount) > 0 ? 1 : -1;
			int lcpMin = LongestCommonPrefix(entryIndex, entryIndex - d, leafCount);
			int lMax = 2;
			while (LongestCommonPrefix(entryIndex, entryIndex + lMax * d, leafCount) > lcpMin)
				lMax *= 2;
			int l = 0;
			int div = 2;
			int t = lMax / div;
			while (t >= 1)
			{
				if (LongestCommonPrefix(entryIndex, entryIndex + (l + t) * d, leafCount) > lcpMin)
					l = l + t;
				if (t == 1)
					break;
				div *= 2;
				t = lMax / div;
			}
			int j = entryIndex + l * d;
			int lcpNode = LongestCommonPrefix(entryIndex, j, leafCount);
			int s = 0;
			div = 2;
			t = RoundUpDivide(l, div);
			while (t >= 1)
			{
				if (LongestCommonPrefix(entryIndex, entryIndex + (s + t) * d, leafCount) > lcpNode)
					s = s + t;
				if (t == 1)
					break;
				div *= 2;
				t = RoundUpDivide(l, div);
			}
			int split = entryIndex + s * d + min(d, 0);
			int leftIndex = split;
			int rightIndex = split + 1;
			gTempBvhBuffer[entryIndex].mLeftIndexLocal = leftIndex;
			gTempBvhBuffer[entryIndex].mLeftIsLeaf = leftIndex == min(entryIndex, j);
			gTempBvhBuffer[entryIndex].mRightIndexLocal = rightIndex;
			gTempBvhBuffer[entryIndex].mRightIsLeaf = rightIndex == max(entryIndex, j);
			if (gTempBvhBuffer[entryIndex].mLeftIsLeaf)
				gProxyBuffer[leftIndex].mBvhIndexLocal = entryIndex;
			else
				gTempBvhBuffer[leftIndex].mParentIndexLocal = entryIndex;
			if (gTempBvhBuffer[entryIndex].mRightIsLeaf)
				gProxyBuffer[rightIndex].mBvhIndexLocal = entryIndex;
			else
				gTempBvhBuffer[rightIndex].mParentIndexLocal = entryIndex;
		}
	}
}
