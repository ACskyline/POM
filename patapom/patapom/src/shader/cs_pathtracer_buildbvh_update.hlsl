#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "cs_pathtracer_util.hlsli"

cbuffer PassUniformBuffer : register(b0, SPACE(PASS))
{
	PassUniformPathTracerBuildScene uPass;
};

StructuredBuffer<AabbProxy> gProxyBuffer : register(t0, SPACE(PASS));
RWStructuredBuffer<MeshPT> gMeshBufferPT : register(u0, SPACE(PASS));
RWStructuredBuffer<BVH> gTempBvhBuffer : register(u1, SPACE(PASS));
RWStructuredBuffer<BVH> gBvhBuffer : register(u2, SPACE(PASS));
RWStructuredBuffer<GlobalBvhSettings> gGlobalBvhSettingBuffer : register(u3, SPACE(PASS));

AABB MergeAABB(AABB aIn, AABB bIn)
{
	AABB aabbOut;
	aabbOut.mMin = min(aIn.mMin, bIn.mMin);
	aabbOut.mMax = max(aIn.mMax, bIn.mMax);
	return aabbOut;
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
		// parallelize leaf node n
		if (entryIndex < leafCount)
		{
			uint current = gProxyBuffer[entryIndex].mBvhIndexLocal;
			while (current != INVALID_UINT32)
			{
				uint original = 0;
				InterlockedAdd(gTempBvhBuffer[current].mVisited, 1, original);
				// a non-leaf bvh node will only be ready to update when both of its child nodes are finished updating
				if (original < 1)
					break;
				else
				{
					uint parent = gTempBvhBuffer[current].mParentIndexLocal;
					if (parent == INVALID_UINT32)
					{
						if (uPass.mBuildBvhType == PT_BUILDBVH_TYPE_TRIANGLE)
						{
							gMeshBufferPT[uPass.mBuildBvhMeshIndex].mRootTriangleBvhIndexLocal = current; // local, we apply offsets in other places
						}
						else if (uPass.mBuildBvhType == PT_BUILDBVH_TYPE_MESH)
						{
							gGlobalBvhSettingBuffer[0].mMeshBvhRootIndex = current + bvhOffset; // offset is 0 anyways for meshes
						}
					}
					uint left = gTempBvhBuffer[current].mLeftIndexLocal;
					uint right = gTempBvhBuffer[current].mRightIndexLocal;
					AABB lAABB;
					AABB rAABB;
					if (gTempBvhBuffer[current].mLeftIsLeaf)
						lAABB = gProxyBuffer[left].mAABB;
					else
						lAABB = gTempBvhBuffer[left].mAABB;
					if (gTempBvhBuffer[current].mRightIsLeaf)
						rAABB = gProxyBuffer[right].mAABB;
					else
						rAABB = gTempBvhBuffer[right].mAABB;
					gTempBvhBuffer[current].mAABB = MergeAABB(lAABB, rAABB);
					
					// fill in corrected original global leaf index according to proxy tree
					if (gTempBvhBuffer[current].mLeftIsLeaf)
						gTempBvhBuffer[current].mLeftIndexLocal = gProxyBuffer[left].mOriginalIndex;
					if (gTempBvhBuffer[current].mRightIsLeaf)
						gTempBvhBuffer[current].mRightIndexLocal = gProxyBuffer[right].mOriginalIndex;

					// update from temporary buffer
					gBvhBuffer[current + bvhOffset] = gTempBvhBuffer[current];
					
					// going up to the root
					current = parent;
				}
			}
		}
	}
}
