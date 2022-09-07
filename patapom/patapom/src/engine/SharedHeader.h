#ifndef SHARED_HEADER
#define SHARED_HEADER

#define PASTE(x, y) x ## y

#ifdef SHARED_HEADER_CPP
#define SHARED_HEADER_CPP_ONLY(x) x
#define SHARED_HEADER_SWITCH_CPP_HLSL(x, y) x
#else
#define SHARED_HEADER_CPP_ONLY(x)
#endif

#ifdef SHARED_HEADER_HLSL
#define SHARED_HEADER_HLSL_ONLY(x) x
#define SHARED_HEADER_SWITCH_CPP_HLSL(x, y) y
#else
#define SHARED_HEADER_HLSL_ONLY(x)
#endif

#define SHARED_HEADER_CPP_STATIC_INLINE		SHARED_HEADER_CPP_ONLY(static inline)
#define SHARED_HEADER_CPP_TEMPLATE_T		SHARED_HEADER_CPP_ONLY(template <class T>)
#define TEMPLATE_UINT						SHARED_HEADER_SWITCH_CPP_HLSL(T, UINT)
	
#define PI									3.1415926535f
#define ONE_OVER_PI							0.3183098861f
#define HALF_PI								1.5707963267f
#define TWO_PI								6.2831853071f
#define EPSILON								0.0001f
#define FLOAT_MAX							3.402823466e+38F
#define FLOAT_MIN							1.175494351e-38F
#define EQUALF(a, b)						(abs(a - b) < EPSILON)
#define CEIL_DIVIDE(dividend, divisor)		(dividend / divisor + (dividend % divisor != 0))
#define NTH_POWER_OF_TWO(x)					(1 << x)
#define MAX_UINT32							0xffffffff
#define INVALID_UINT32						MAX_UINT32

#define USE_REVERSED_Z
#ifdef USE_REVERSED_Z
#define REVERSED_Z_SWITCH(x, y) (x)
#else
#define REVERSED_Z_SWITCH(x, y) (y)
#endif
#define DEPTH_FAR_REVERSED_Z_SWITCH REVERSED_Z_SWITCH(0.0f, 1.0f)
#define DEPTH_NEAR_REVERSED_Z_SWITCH REVERSED_Z_SWITCH(1.0f, 0.0f)

#define LIGHT_PER_SCENE_MAX			10
#define LIGHT_TYPE_INVALID			0 // TODO: add support to directional light
#define LIGHT_TYPE_QUAD				1
#define LIGHT_TYPE_POINT			2
#define LIGHT_TYPE_SPHERE			3

#define MATERIAL_TYPE_INVALID					0x00000000
#define MATERIAL_TYPE_GGX						0x00000001
#define MATERIAL_TYPE_SPECULAR_REFLECTIIVE		0x00000002
#define MATERIAL_TYPE_SPECULAR_TRANSMISSIVE		0x00000003
#define MATERIAL_TYPE_EMISSIVE					0x00000004

// path tracer
#define PT_TRIANGLE_BVH_STACK_SIZE						256 // 256 uint array so it's actually 256 * 4 bytes per thread
#define PT_TRIANGLE_BVH_TREE_HEIGHT_MAX					PT_TRIANGLE_BVH_STACK_SIZE
#define PT_TRIANGLE_PER_SCENE_MAX						300000
#define PT_TRIANGLE_PER_MESH_MAX						9000
#define PT_MESH_PER_SCENE_MAX							10
#if PT_TRIANGLE_PER_SCENE_MAX > PT_MESH_PER_SCENE_MAX
	#define PT_LEAF_PER_TREE_MAX						PT_TRIANGLE_PER_MESH_MAX
#else
	#define PT_LEAF_PER_TREE_MAX						PT_MESH_PER_SCENE_MAX
#endif
#define PT_TRIANGLE_BVH_PER_SCENE_MAX					PT_TRIANGLE_PER_SCENE_MAX - 1
#define PT_MESH_BVH_PER_SCENE_MAX						PT_MESH_PER_SCENE_MAX - 1
#define PT_BVH_PER_SCENE_MAX							MAX(PT_TRIANGLE_BVH_PER_SCENE_MAX, PT_MESH_BVH_PER_SCENE_MAX)
#define PT_LIGHT_PER_SCENE_MAX							LIGHT_PER_SCENE_MAX
#define PT_THREAD_PER_THREADGROUP_X						8
#define PT_THREAD_PER_THREADGROUP_Y						8
#define PT_BUILDBVH_TYPE_TRIANGLE						0
#define PT_BUILDBVH_TYPE_MESH							1
#define PT_BUILDBVH_TYPE_COUNT							2
#define PT_BUILDBVH_THREADGROUP_PER_DISPATCH			1024
#define PT_BUILDBVH_THREAD_PER_THREADGROUP				256
#define PT_BUILDBVH_ENTRY_PER_THREAD					64
#define PT_BUILDBVH_ENTRY_PER_DISPATCH					(PT_BUILDBVH_ENTRY_PER_THREAD * PT_BUILDBVH_THREAD_PER_THREADGROUP * PT_BUILDBVH_THREADGROUP_PER_DISPATCH)
#define PT_BUILDBVH_THREAD_PER_DISPATCH					(PT_BUILDBVH_THREAD_PER_THREADGROUP * PT_BUILDBVH_THREADGROUP_PER_DISPATCH)
#define PT_RADIXSORT_BIT_PER_ENTRY						32 // radix sort only works on 32 bit entry at the moment
#define PT_RADIXSORT_BIT_PER_BITGROUP					1
#define PT_RADIXSORT_BITGROUP_COUNT						(PT_RADIXSORT_BIT_PER_ENTRY / PT_RADIXSORT_BIT_PER_BITGROUP)
#define GET_PT_RADIXSORT_DISPATCH_COUNT(leafPerTree)	CEIL_DIVIDE(leafPerTree, PT_BUILDBVH_ENTRY_PER_DISPATCH)
#define PT_RADIXSORT_DISPATCH_COUNT_MAX					GET_PT_RADIXSORT_DISPATCH_COUNT(PT_LEAF_PER_TREE_MAX)
#define GET_PT_RADIXSORT_THREAD_COUNT(leafPerTree)		(PT_BUILDBVH_THREAD_PER_DISPATCH * GET_PT_RADIXSORT_DISPATCH_COUNT(leafPerTree))
#define PT_RADIXSORT_THREAD_COUNT_MAX					GET_PT_RADIXSORT_THREAD_COUNT(PT_LEAF_PER_TREE_MAX)
#define PT_RADIXSORT_SWEEP_THREAD_PER_THREADGROUP		512
#define PT_BACKBUFFER_WIDTH								960
#define PT_BACKBUFFER_HEIGHT							960
#define PT_MINDEPTH_MAX									5
#define PT_MAXDEPTH_MAX									10
#define PT_DEBUG_LINE_COUNT_PER_RAY						3

#define PT_MODE_OFF										0
#define PT_MODE_DEFAULT									1
#define PT_MODE_PROGRESSIVE								2
#define PT_MODE_DEBUG_TRIANGLE_INTERSECTION				3
#define PT_MODE_DEBUG_LIGHT_INTERSECTION				4
#define PT_MODE_DEBUG_MESH_INTERSECTION					5
#define PT_MODE_DEBUG_ALBEDO							6
#define PT_MODE_DEBUG_NORMAL							7

// water sim
#define WATERSIM_CELL_COUNT_X							32
#define WATERSIM_CELL_COUNT_Y							32
#define WATERSIM_CELL_COUNT_Z							32
#define WATERSIM_CELLFACE_COUNT_X						(WATERSIM_CELL_COUNT_X + 1)
#define WATERSIM_CELLFACE_COUNT_Y						(WATERSIM_CELL_COUNT_Y + 1)
#define WATERSIM_CELLFACE_COUNT_Z						(WATERSIM_CELL_COUNT_Z + 1)
#define WATERSIM_THREAD_PER_THREADGROUP_X				8
#define WATERSIM_THREAD_PER_THREADGROUP_Y				8
#define WATERSIM_THREAD_PER_THREADGROUP_Z				8
#define WATERSIM_PARTICLE_COUNT_PER_CELL				64
#define WATERSIM_PARTICLE_COUNT_MAX						WATERSIM_CELL_COUNT_X * WATERSIM_CELL_COUNT_Y * WATERSIM_CELL_COUNT_Z * WATERSIM_PARTICLE_COUNT_PER_CELL
#define WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_X		8
#define WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_Y		8
#define WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_Z		8
#define WATERSIM_BACKBUFFER_WIDTH						960
#define WATERSIM_BACKBUFFER_HEIGHT						960

SHARED_HEADER_CPP_TEMPLATE_T
SHARED_HEADER_CPP_STATIC_INLINE TEMPLATE_UINT RoundUpDivide(TEMPLATE_UINT dividend, TEMPLATE_UINT divisor)
{
	// this method is less likely to overflow than
	// (dividend + divisor - 1) / divisor
	// return dividend / divisor + (dividend % divisor != 0);
	return CEIL_DIVIDE(dividend, divisor);
}

SHARED_HEADER_CPP_STATIC_INLINE UINT NextPowerOfTwo32(UINT x)
{
	// return the input if it happens to be a power of 2
	// return 2 if input is smaller than 2
	if (x < 2)
		return 2;
	x = x - 1;
	x = x | (x >> 1);
	x = x | (x >> 2);
	x = x | (x >> 4);
	x = x | (x >> 8);
	x = x | (x >> 16);
	return x + 1;
}

struct Vertex {
	FLOAT3 pos;
	FLOAT2 uv;
	FLOAT3 nor;
	FLOAT4 tan;
};

struct Triangle
{
	Vertex mVertices[3];
};

struct TrianglePT : Triangle
{
	UINT mMeshIndex;
	UINT mMortonCode;
	UINT mBvhIndexLocal;
};

struct AABB
{
	FLOAT3 mMin;
	FLOAT3 mMax;
};

struct RadixSortEntry
{
	UINT key;
	UINT val;
};

struct BVH 
{
	AABB mAABB; // this is in model space for triangle bvh, but in world space for mesh bvh
	UINT mParentIndexLocal;
	UINT mLeftIndexLocal;
	UINT mRightIndexLocal;
	UINT mLeftIsLeaf;
	UINT mRightIsLeaf;
	UINT mVisited;
	UINT mMeshIndex;
};

struct GlobalBvhSettings
{
	UINT mMeshBvhRootIndex;
	UINT mRadixSortReorderOffset;
};

struct MeshPT
{
	FLOAT4X4 mModel;
	FLOAT4X4 mModelInv;
	UINT mMaterialType;
	FLOAT3 mEmissive;
	float mRoughness;
	float mFresnel;
	float mMetallic;
	UINT mTriangleCount;
	UINT mTriangleIndexLocalToGlobalOffset;
	UINT mTriangleBvhIndexLocalToGlobalOffset;
	UINT mRootTriangleBvhIndexLocal;
	UINT mTextureIndexOffset;
	UINT mTextureCount;
};

struct AabbProxy
{
	AABB mAABB;
	UINT mMortonCode;
	UINT mOriginalIndex;
	UINT mBvhIndexLocal;
};

struct Ray
{
	FLOAT3 mOri;
	UINT mReadyForDebug;
	FLOAT3 mDir;
	UINT mTerminated;
	FLOAT3 mEnd;
	UINT mResultWritten;
	FLOAT3 mThroughput;
	UINT mIsLastBounceSpecular;
	FLOAT3 mResult;
	UINT mRemainingDepth;
	FLOAT3 mNextOri;
	UINT mSeed;
	FLOAT3 mNextDir;
	UINT mHitMeshIndex;
	FLOAT4 mLightSampleRayEnd; // if hit it's a point, otherwise it's a direction
	FLOAT4 mMaterialSampleRayEnd; // if hit it's a point, otherwise it's a direction
	UINT mHitTriangleIndex;
	UINT mHitLightIndex;
	UINT mRayAabbTestCount;
};

struct WaterSimParticle
{
	FLOAT3 mPos;
	FLOAT3 mVelocity;
	FLOAT3 mMinFaceWeights;
	FLOAT3 mMaxFaceWeights;
	UINT3 mCellIndexXYZ;
	UINT3 mCellFaceMinIndexXYZ;
	UINT3 mCellFaceMaxIndexXYZ;
	float mAlive;
};

struct WaterSimCell
{
	int mParticleCount;
	float mPressure; // TODO: turn this into groupshared
	float mType; // 0 - fluid/air, 1 - solid
	FLOAT4 mPosPressureAndNonSolidCount; // debug
	FLOAT4 mNegPressureAndDivergence; // debug
	FLOAT3 mSolidVelocity;
	UINT mIteration; // not used
	UINT mReadByNeighbor; // not used
	UINT mNonSolidNeighborCount; // not used TODO: utilize this instead storing in aux buffer
	UINT4 mIndicesXYZ_Total;
	float mResidual; // not used
};

struct WaterSimCellFace
{
	UINT4 mIndices;
	UINT4 mDebugGroupThreadIdGroupIndex; // compute indices
	FLOAT4 mPosPressureAndP; // debug 
	FLOAT4 mNegPressureAndDivergence; // debug
	UINT mVelocityU32;
	UINT mWeightU32;
	float mVelocity;
	float mOldVelocity;
	float mWeight;
};

struct WaterSimCellAux
{
	FLOAT3 mIsNotSolidPos;
	FLOAT3 mIsNotSolidNeg;
	float mDivergence;
	float mNonSolidNeighborCount;
};

struct LightData
{
	FLOAT4X4 mView;
	FLOAT4X4 mViewInv;
	FLOAT4X4 mProj;
	FLOAT4X4 mProjInv;
	FLOAT3 mColor;
	float mNearClipPlane;
	FLOAT3 mPosWorld;
	float mFarClipPlane;
	FLOAT3 mScale;
	float mArea;
	FLOAT3 mDirWorld;
	float mAngle;
	int mTextureIndex;
	UINT mLightType;
	UINT PADDING0;
	UINT PADDING1;
};

struct SceneUniform
{
	UINT mMode;
	UINT mPomMarchStep;
	float mPomScale;
	float mPomBias;
	//
	float mSkyScatterG;
	UINT mSkyMarchStep;
	UINT mSkyMarchStepTr;
	float mSunAzimuth;
	//
	float mSunAltitude;
	FLOAT3 mSunRadiance;
	//
	UINT mLightCount;
	UINT mLightDebugOffset;
	UINT mLightDebugCount;
	float mFresnel;
	//
	FLOAT4 mStandardColor;
	//
	float mRoughness;
	UINT mUsePerPassTextures;
	float mMetallic;
	float mSpecularity;
	//
	UINT mSampleNumIBL;
	UINT mShowReferenceIBL;
	UINT mUseSceneLight;
	UINT mUseSunLight;
	//
	UINT mUseIBL;
	UINT mPrefilteredEnvMapMipLevelCount;
	UINT mPathTracerMaxSampleCountPerPixel;
	UINT mPathTracerCurrentSampleIndex; // starting from 0
	//
	UINT mPathTracerMinDepth;
	UINT mPathTracerMaxDepth;
	UINT mPathTracerMode;
	UINT mPathTracerCurrentDepth; // starting from 0
	//
	UINT mPathTracerEnableRussianRoulette;
	UINT mPathTracerDebugPixelX;
	UINT mPathTracerDebugPixelY;
	UINT mPathTracerEnableDebug;
	//
	UINT mPathTracerDebugSampleRay;
	float mPathTracerDebugDirLength;
	UINT mPathTracerUpdateDebug;
	UINT mPathTracerMeshBvhCount;
	//
	UINT mPathTracerTriangleBvhCount;
	UINT mPathTracerDebugMeshBVH;
	UINT mPathTracerDebugTriangleBVH;
	UINT mPathTracerMeshBvhRootIndex;
	//
	UINT mPathTracerTileCountX SHARED_HEADER_CPP_ONLY(= 1);
	UINT mPathTracerTileCountY SHARED_HEADER_CPP_ONLY(= 1);
	UINT mPathTracerThreadGroupPerTileX;
	UINT mPathTracerThreadGroupPerTileY;
	//
	UINT mPathTracerTileCount;
	UINT mPathTracerCurrentTileIndex;
	UINT mPathTracerUseBVH;
	UINT mPathTracerDebugTriangleBvhIndex;
	//
	UINT mPathTracerDebugMeshBvhIndex;
	UINT PADDING_0;
	UINT PADDING_1;
	UINT PADDING_2;
	//
	LightData mLightData[LIGHT_PER_SCENE_MAX];
};

struct FrameUniform
{
	UINT mFrameCountSinceGameStart;
	UINT mResetRays;
	float mLastFrameTimeInSecond;
	UINT PADDING0;
};

SHARED_HEADER_CPP_ONLY(class Camera;)
struct PassUniformDefault
{
	FLOAT4X4 mViewProj;
	FLOAT4X4 mViewProjInv;
	FLOAT4X4 mView;
	FLOAT4X4 mViewInv;
	FLOAT4X4 mProj;
	FLOAT4X4 mProjInv;
	UINT mPassIndex;
	FLOAT3 mEyePos;
	float mNearClipPlane;
	float mFarClipPlane;
	float mWidth;
	float mHeight;
	float mFov;
	UINT PADDING_0;
	UINT PADDING_1;
	UINT PADDING_2;
	SHARED_HEADER_CPP_ONLY(void Update(Camera* camera);)
};

struct PassUniformIBL : PassUniformDefault
{
	float mRoughness;
	UINT mFaceIndex;
	UINT PADDING_0;
	UINT PADDING_1;
};

struct PassUniformPathTracer : PassUniformDefault
{
	UINT mTriangleCountPT;
	UINT mMeshCountPT;
	UINT mLightCountPT;
	UINT PADDING_0;
};

struct PassUniformPathTracerBuildScene : PassUniformDefault
{
	UINT mDispatchIndex;
	UINT mRadixSortBitGroupIndex;
	UINT mBuildBvhMeshIndex;
	UINT mBuildBvhMeshCount;
	UINT mBuildBvhType;
	UINT mRadixSortSweepThreadCount;
	UINT mRadixSortSweepDepthCount;
	UINT PADDING_0;
};

struct PassUniformWaterSim : PassUniformDefault
{
	UINT3 mCellCount;
	UINT mParticleCountPerCell;
	FLOAT3 mGridOffset; // all pos in simulation are local, add this offset during rasterization if needed
	UINT mParticleCount;
	float mTimeStepScale;
	float mCellSize;
	UINT2 mWaterSimBackbufferSize;
	UINT mJacobiIterationCount;
	UINT mCurrentJacobiIteration;
	float mApplyForce;
	float mGravitationalAccel;
	float mWaterDensity;
	FLOAT3 mParticleSpawnSourcePos;
	FLOAT3 mParticleSpawnSourceSpan;
	int mAliveParticleCount;
	FLOAT3 mExplosionPos;
	float mWarmStart;
	FLOAT3 mExplosionForceScale;
	float mFlipScale;
	float mApplyExplosion;
	UINT PADDING_0;
	UINT PADDING_1;
	UINT PADDING_2;
};

struct ObjectUniform
{
	FLOAT4X4 mModel;
	FLOAT4X4 mModelInv;
	float mRoughness;
	float mFresnel;
	float mMetallic;
	UINT PADDING_0;
};

#endif
