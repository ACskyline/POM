#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "PathTracerUtil.hlsli"

cbuffer PassUniformBuffer : register(b0, SPACE(PASS))
{
	PassUniformPathTracerBuildScene uPass;
};

StructuredBuffer<TrianglePT> gTriangleBufferPT : register(t0, SPACE(PASS));
StructuredBuffer<MeshPT> gMeshBufferPT : register(t1, SPACE(PASS));
RWStructuredBuffer<AabbProxy> gProxyBuffer : register(u0, SPACE(PASS));
RWStructuredBuffer<BVH> gTriangleBvhBuffer : register(u1, SPACE(PASS));
RWStructuredBuffer<BVH> gTempBvhBuffer : register(u2, SPACE(PASS));

AABB TransformAABB(float4x4 mat, AABB aabbIn)
{
	AABB aabbOut;
	float4 A = mul(mat, float4(aabbIn.mMin.x, aabbIn.mMin.y, aabbIn.mMin.z, 1.0f));
	float4 B = mul(mat, float4(aabbIn.mMin.x, aabbIn.mMin.y, aabbIn.mMax.z, 1.0f));
	float4 C = mul(mat, float4(aabbIn.mMin.x, aabbIn.mMax.y, aabbIn.mMin.z, 1.0f));
	float4 D = mul(mat, float4(aabbIn.mMin.x, aabbIn.mMax.y, aabbIn.mMax.z, 1.0f));
	float4 E = mul(mat, float4(aabbIn.mMax.x, aabbIn.mMin.y, aabbIn.mMin.z, 1.0f));
	float4 F = mul(mat, float4(aabbIn.mMax.x, aabbIn.mMin.y, aabbIn.mMax.z, 1.0f));
	float4 G = mul(mat, float4(aabbIn.mMax.x, aabbIn.mMax.y, aabbIn.mMin.z, 1.0f));
	float4 H = mul(mat, float4(aabbIn.mMax.x, aabbIn.mMax.y, aabbIn.mMax.z, 1.0f));
	aabbOut.mMin = min(A, min(B, min(C, min(D, min(E, min(F, min(G, H)))))));
	aabbOut.mMax = max(A, max(B, max(C, max(D, max(E, max(F, max(G, H)))))));
	return aabbOut;
}

float3 CentroidOfAABB(AABB aabb)
{
	return (aabb.mMin + aabb.mMax) / 2.0f;
}

uint LeftShift3(float fx)
{
	uint x = asuint(fx);
	if (x == (1 << 10)) --x;
	// 0b 0000 0011 0000 0000 0000 0000 1111 1111
	x = (x | (x << 16)) & 0x030000ff;
	// 0b 0000 0011 0000 0000 1111 0000 0000 1111
	x = (x | (x << 8)) & 0x0300f00f;
	// 0b 0000 0011 0000 1100 0011 0000 1100 0011
	x = (x | (x << 4)) & 0x030c30c3;
	// 0b 0000 1001 0010 0100 1001 0010 0100 1001
	x = (x | (x << 2)) & 0x09249249;
	return x;
}

uint GenerateMortonCode(float3 position)
{
	return LeftShift3(position.z) << 2 | LeftShift3(position.y) << 1 | LeftShift3(position.x);
}

AABB InitAABB(Vertex vertices[3])
{
	AABB aabbOut;
	aabbOut.mMin = min(vertices[0].pos, min(vertices[1].pos, vertices[2].pos));
	aabbOut.mMax = max(vertices[0].pos, max(vertices[1].pos, vertices[2].pos));
	return aabbOut;
}

BVH InitBVH(uint meshIndex)
{
	BVH bvh = (BVH)0;
	bvh.mParentIndexLocal = INVALID_UINT32;
	bvh.mLeftIndexLocal = INVALID_UINT32;
	bvh.mRightIndexLocal = INVALID_UINT32;
	bvh.mLeftIsLeaf = 0;
	bvh.mRightIsLeaf = 0;
	bvh.mVisited = 0;
	bvh.mMeshIndex = meshIndex;
	return bvh;
}

[numthreads(PT_BUILDBVH_THREAD_PER_THREADGROUP, 1, 1)]
void main(uint3 gDispatchThreadID : SV_DispatchThreadID)
{
	uint leafCount = 0;
	uint leafOffset = 0;
	uint threadIndex = GetDispatchThreadIndexPerRootBVH(gDispatchThreadID.x, uPass.mDispatchIndex, PT_BUILDBVH_THREAD_PER_DISPATCH);
	GetLeafCountAndOffsets(gMeshBufferPT, uPass.mBuildBvhType, uPass.mBuildBvhMeshIndex, uPass.mBuildBvhMeshCount, leafCount, leafOffset);
	
	for (uint i = 0; i < PT_BUILDBVH_ENTRY_PER_THREAD; i++)
	{
		uint entryIndex = GetEntryIndexPerRootBVH(i, threadIndex, PT_BUILDBVH_ENTRY_PER_THREAD);
		// parallelize leaf node n
		if (entryIndex < leafCount)
		{
			if (entryIndex < leafCount - 1)
				gTempBvhBuffer[entryIndex] = InitBVH(uPass.mBuildBvhMeshIndex);
			uint leafIndex = entryIndex + leafOffset;
			if (uPass.mBuildBvhType == PT_BUILDBVH_TYPE_TRIANGLE)
			{
				gProxyBuffer[entryIndex].mAABB = InitAABB(gTriangleBufferPT[leafIndex].mVertices);
				gProxyBuffer[entryIndex].mMortonCode = gTriangleBufferPT[leafIndex].mMortonCode;
			}
			else if (uPass.mBuildBvhType == PT_BUILDBVH_TYPE_MESH)
			{
				uint rootTriangleBvhIndex = gMeshBufferPT[leafIndex].mRootTriangleBvhIndexLocal + gMeshBufferPT[leafIndex].mTriangleBvhIndexLocalToGlobalOffset;
				gProxyBuffer[entryIndex].mAABB = TransformAABB(gMeshBufferPT[leafIndex].mModel, gTriangleBvhBuffer[rootTriangleBvhIndex].mAABB);
				gProxyBuffer[entryIndex].mMortonCode = GenerateMortonCode(CentroidOfAABB(gProxyBuffer[entryIndex].mAABB));
			}
			gProxyBuffer[entryIndex].mOriginalIndex = entryIndex;
			gProxyBuffer[entryIndex].mBvhIndexLocal = leafCount;
		}
	}
}
