#ifndef SHARED_MACROS
#define SHARED_MACROS

#define PASTE(x, y) x ## y

#ifdef SHARED_HEADER_CPP
#define SHARED_HEADER_CPP_ONLY(x) x
#else
#define SHARED_HEADER_CPP_ONLY(x)
#endif

#ifdef SHARED_HEADER_HLSL
#define SHARED_HEADER_HLSL_ONLY(x) x
#else
#define SHARED_HEADER_HLSL_ONLY(x)
#endif

#define EPSILON 0.0001f
#define FLOAT_MAX 3.402823466e+38F
#define EQUALF(a, b) (abs(a - b) < EPSILON)
#define INVALID_UINT32 0xffffffff

#define USE_REVERSED_Z
#ifdef USE_REVERSED_Z
#define REVERSED_Z_SWITCH(x, y) (x)
#else
#define REVERSED_Z_SWITCH(x, y) (y)
#endif
#define DEPTH_FAR_REVERSED_Z_SWITCH REVERSED_Z_SWITCH(0.0f, 1.0f)
#define DEPTH_NEAR_REVERSED_Z_SWITCH REVERSED_Z_SWITCH(1.0f, 0.0f)

#define LIGHT_COUNT_PER_SCENE_MAX	10
#define LIGHT_TYPE_INVALID			0 // TODO: add support to directional light
#define LIGHT_TYPE_QUAD				1
#define LIGHT_TYPE_POINT			2
#define LIGHT_TYPE_SPHERE			3

#define MATERIAL_TYPE_INVALID					0x00000000
#define MATERIAL_TYPE_GGX						0x00000001
#define MATERIAL_TYPE_SPECULAR_REFLECTIIVE		0x00000002
#define MATERIAL_TYPE_SPECULAR_TRANSMISSIVE		0x00000003
#define MATERIAL_TYPE_EMISSIVE					0x00000004

#define PATH_TRACER_TRIANGLE_COUNT_MAX					10000
#define PATH_TRACER_MESH_COUNT_MAX						10
#define PATH_TRACER_LIGHT_COUNT_MAX						LIGHT_COUNT_PER_SCENE_MAX
#define PATH_TRACER_THREAD_COUNT_X						8
#define PATH_TRACER_THREAD_COUNT_Y						8
#define PATH_TRACER_THREAD_COUNT_Z						1
#define PATH_TRACER_BACKBUFFER_WIDTH					960
#define PATH_TRACER_BACKBUFFER_HEIGHT					960
#define PATH_TRACER_MIN_DEPTH_MAX						5
#define PATH_TRACER_MAX_DEPTH_MAX						10
#define PATH_TRACER_DEBUG_LINE_COUNT_PER_RAY			3

#define PATH_TRACER_MODE_OFF							0
#define PATH_TRACER_MODE_DEFAULT						1
#define PATH_TRACER_MODE_PROGRESSIVE					2
#define PATH_TRACER_MODE_DEBUG_TRIANGLE_INTERSECTION	3
#define PATH_TRACER_MODE_DEBUG_LIGHT_INTERSECTION		4
#define PATH_TRACER_MODE_DEBUG_MESH_INTERSECTION		5
#define PATH_TRACER_MODE_DEBUG_ALBEDO					6
#define PATH_TRACER_MODE_DEBUG_NORMAL					7

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
	FLOAT3 mCentroid;
	UINT mMeshIndex;
	UINT mMortonCode;
};

struct TriangleBVH 
{
	FLOAT3 mMin;
	UINT mLeftIndex;
	FLOAT3 mMax;
	UINT mRightIndex;
	UINT mTriangleIndex;
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
	int mTextureStartIndex;
	int mTextureCount;
};

struct MeshBVH
{
	FLOAT3 mMin;
	UINT mLeftIndex;
	FLOAT3 mMax;
	UINT mRightIndex;
	FLOAT3 mCentroid;
	UINT mTriangleBvhIndex;
	UINT mMortonCode;
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
	UINT mPathTracerEnableDebugSampleRay;
	float mPathTracerDebugDirLength;
	UINT mPathTracerUpdateDebug;
	UINT PADDING0;
	//
	LightData mLightData[LIGHT_COUNT_PER_SCENE_MAX];
};

struct FrameUniform
{
	UINT mFrameCountSinceGameStart;
	UINT mResetRays;
	UINT PADDING1;
	UINT PADDING2;
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
