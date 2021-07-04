#pragma once

#include "Buffer.h"
#include "Texture.h"
#include "Pass.h"
#include "Mesh.h"

typedef PassCommon<PassUniformPathTracer> PassPathTracer;

class PathTracer
{
public:
	static const int sTriangleCountMax;
	static const int sMeshCountMax;
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
	static PassDefault sPathTracerCopyDepthPass;
	static PassDefault sPathTracerDebugLinePass;
	static PassDefault sPathTracerDebugCubePass;
	static PassDefault sPathTracerDebugFullscreenPass;
	static Mesh sPathTracerDebugMeshLine;
	static Mesh sPathTracerDebugMeshCube;
	static Shader sPathTracerCS;
	static Shader sPathTracerDebugLineVS;
	static Shader sPathTracerDebugLinePS;
	static Shader sPathTracerDebugCubeVS;
	static Shader sPathTracerDebugCubePS;
	static Shader sPathTracerDebugFullscreenPS;
	static Shader sPathTracerCopyDepthVS;
	static Shader sPathTracerCopyDepthPS;
	static Shader sPathTracerBlitPS;
	static Buffer sTriangleBuffer;
	static Buffer sMeshBuffer;
	static Buffer sLightDataBuffer;
	static Buffer sTriangleBvhBuffer;
	static Buffer sMeshBvhBuffer;
	static WriteBuffer sRayBuffer;
	static WriteBuffer sDebugRayBuffer;
	static RenderTexture sBackbufferPT;
	static RenderTexture sDebugBackbufferPT;
	static RenderTexture sDepthbufferWritePT;
	static RenderTexture sDepthbufferRenderPT;
	static Camera sCameraDummyPT;
	static void InitPathTracer(Store& store, Scene& scene);
	static void AddMeshesFromPass(Pass& pass);
	static void UpdateBVH(Scene& scene);
	static void PrintBVH();
	static void PreparePathTracer(ID3D12GraphicsCommandList* commandList, Scene& scene);
	static void RunPathTracer(ID3D12GraphicsCommandList* commandList);
	static void CopyDepthBuffer(ID3D12GraphicsCommandList* commandList);
	static void DebugDraw(ID3D12GraphicsCommandList* commandList);
	static void Shutdown();
	static void Restart(Scene& scene);

private:
	template<class T_LeafNode>
	static i64 BuildBVH(vector<T_LeafNode>& leafLocal, vector<BVH>& bvhGlobal, u32 meshIndex = INVALID_UINT32);
};
