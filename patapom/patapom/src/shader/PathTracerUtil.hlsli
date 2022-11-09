#ifndef PATHTRACER_UTIL_H
#define PATHTRACER_UTIL_H

#include "../engine/SharedHeader.h"

void GetBucketShiftAndMask(uint bitGroupIndex, out uint bucketShiftOut, out uint bucketMaskOut)
{
    bucketShiftOut = bitGroupIndex * PT_RADIXSORT_BIT_PER_BITGROUP;
    bucketMaskOut = ((1 << PT_RADIXSORT_BIT_PER_BITGROUP) - 1) << bucketShiftOut;
}

// bvh offset into the global bvh buffer is different from leaf offset into the global triangle/mesh buffer
// it has 1 less entry per bvh tree, because for n leaves you only need n-1 bvh nodes, and there are multiple bvh trees inside the global bvh tree
#define DEF_FUNC_GET_LEAF_COUNT_AND_BVH_OFFSET(meshBufferType) \
uint GetLeafCountAndOffsets(meshBufferType<MeshPT> meshBuffer, uint bvhType, uint meshIndex, uint meshCount, out uint leafCountOut, out uint leafOffsetOut) \
{ \
	uint bvhOffsetOut = 0; \
    if (bvhType == PT_BUILDBVH_TYPE_TRIANGLE) \
    { \
        leafCountOut = meshBuffer[meshIndex].mTriangleCount; \
		leafOffsetOut = meshBuffer[meshIndex].mTriangleIndexLocalToGlobalOffset; \
        bvhOffsetOut = meshBuffer[meshIndex].mTriangleBvhIndexLocalToGlobalOffset; \
    } \
    else if (bvhType == PT_BUILDBVH_TYPE_MESH) \
    { \
        leafCountOut = meshCount; \
        leafOffsetOut = 0; \
        bvhOffsetOut = 0; \
    } \
	return bvhOffsetOut; \
}

DEF_FUNC_GET_LEAF_COUNT_AND_BVH_OFFSET(StructuredBuffer)
DEF_FUNC_GET_LEAF_COUNT_AND_BVH_OFFSET(RWStructuredBuffer)

uint GetDispatchThreadIndexPerRootBVH(uint dispatchThreadIndexPerDispatch, uint dispatchIndex, uint threadPerDispatch)
{
    return dispatchThreadIndexPerDispatch + dispatchIndex * threadPerDispatch;
}

uint GetEntryIndexPerRootBVH(uint entryIndexPerThread, uint globalDispatchThreadIndex, uint entryPerThread)
{
    return entryIndexPerThread + globalDispatchThreadIndex * entryPerThread;
}

bool HitAnything(Ray ray)
{
    return ray.mHitLightIndex != INVALID_UINT32 ||
        ray.mHitTriangleIndex != INVALID_UINT32;
}

#endif // PATHTRACER_UTIL_H
