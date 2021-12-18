#pragma once

#include "Buffer.h"
#include "Texture.h"
#include "Pass.h"
#include "Mesh.h"

typedef PassCommon<PassUniformPathTracer> PassPathTracer;
typedef PassCommon<PassUniformPathTracerBuildScene> PassPathTracerBuildScene;

class PathTracer
{
public:
	enum BuildBvhType : u8
	{ 
		BuildBvhTypeTriangle = PT_BUILDBVH_TYPE_TRIANGLE, 
		BuildBvhTypeMesh = PT_BUILDBVH_TYPE_MESH, 
		BuildBvhTypeCount = PT_BUILDBVH_TYPE_COUNT 
	};
	static const int sLightDataCountMax;
	static const int sThreadGroupCountX;
	static const int sThreadGroupCountY;
	static const int sBackbufferWidth;
	static const int sBackbufferHeight;
	static u32 sMeshTextureCount;
	static u32 sMeshBvhRootIndexGlobal;
	static vector<TrianglePT> sTriangles;
	static vector<MeshPT> sMeshes;
	static vector<LightData> sLightData;
	static vector<BVH> sTriangleModelBVHs;
	static vector<BVH> sMeshWorldBVHs;
	static PassPathTracer sPathTracerPass;
	static PassPathTracerBuildScene sPathTracerRadixSortInitPass[BuildBvhType::BuildBvhTypeCount];
	static PassPathTracerBuildScene sPathTracerRadixSortPollPass[BuildBvhType::BuildBvhTypeCount];
	static PassPathTracerBuildScene sPathTracerRadixSortUpSweepPass[BuildBvhType::BuildBvhTypeCount];
	static PassPathTracerBuildScene sPathTracerRadixSortDownSweepPass[BuildBvhType::BuildBvhTypeCount];
	static PassPathTracerBuildScene sPathTracerRadixSortReorderPass[BuildBvhType::BuildBvhTypeCount];
	static PassPathTracerBuildScene sPathTracerBuildBvhPass[BuildBvhType::BuildBvhTypeCount];
	static PassPathTracerBuildScene sPathTracerBuildBvhUpdatePass[BuildBvhType::BuildBvhTypeCount];
	static PassDefault sPathTracerCopyDepthPass;
	static PassDefault sPathTracerDebugLinePass;
	static PassDefault sPathTracerDebugCubePass;
	static PassDefault sPathTracerDebugFullscreenPass;
	static Mesh sPathTracerDebugMeshLine;
	static Mesh sPathTracerDebugMeshCube;
	static Shader sPathTracerCS;
	static Shader sPathTracerRadixSortInitCS;
	static Shader sPathTracerRadixSortPollCS;
	static Shader sPathTracerRadixSortUpSweepCS;
	static Shader sPathTracerRadixSortDownSweepCS;
	static Shader sPathTracerRadixSortReorderCS;
	static Shader sPathTracerBuildBvhCS;
	static Shader sPathTracerBuildBvhUpdateCS;
	static Shader sPathTracerDebugLineVS;
	static Shader sPathTracerDebugLinePS;
	static Shader sPathTracerDebugCubeVS;
	static Shader sPathTracerDebugCubePS;
	static Shader sPathTracerDebugFullscreenPS;
	static Shader sPathTracerCopyDepthVS;
	static Shader sPathTracerCopyDepthPS;
	static Shader sPathTracerBlitPS;
	static Buffer sLightDataBuffer;
	static Buffer sTriangleBuffer;
	static WriteBuffer sMeshBuffer;
	static WriteBuffer sGlobalBvhSettingsBuffer;
	static WriteBuffer sTriangleBvhBuffer;
	static WriteBuffer sMeshBvhBuffer;
	static WriteBuffer sTempBvhBuffer;
	static WriteBuffer sRayBuffer;
	static WriteBuffer sDebugRayBuffer;
	static WriteBuffer sAabbProxyBuffer;
	static WriteBuffer sSortedAabbProxyBuffer;
	static WriteBuffer sRadixSortBitCountArrayBuffer;
	static WriteBuffer sRadixSortPrefixSumAuxArrayBuffer;
	static RenderTexture sBackbufferPT;
	static RenderTexture sDebugBackbufferPT;
	static RenderTexture sDepthbufferWritePT;
	static RenderTexture sDepthbufferRenderPT;
	static Camera sCameraDummyPT;
	static void InitPathTracer(Store& store, Scene& scene);
	static int AddMeshesFromPass(Pass& pass);
	static void UpdateBvhCpu(Scene& scene);
	static void UpdateBvhGpu(CommandList commandList, Scene& scene);
	static void PrintBVH();
	static void PreparePathTracer(CommandList commandList, Scene& scene);
	static void RunPathTracer(CommandList commandList);
	static void CopyDepthBuffer(CommandList commandList);
	static void DebugDraw(CommandList commandList);
	static void Shutdown();
	static void Restart(Scene& scene);

private:
	template<class T_LeafNode>
	static i64 BuildBVH(vector<T_LeafNode>& leafLocal, vector<BVH>& bvhGlobal, u32 meshIndex = INVALID_UINT32);
	static void UpdateBvhGpuCommon(CommandList commandList, Scene& scene, BuildBvhType buildBvhType);
	static void UpdateTriangleBvhGpu(CommandList commandList, Scene& scene);
	static void UpdateMeshBvhGpu(CommandList commandList, Scene& scene);
};
