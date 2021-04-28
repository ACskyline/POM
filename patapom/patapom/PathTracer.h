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
	static const int sThreadGroupCountZ;
	static const int sBackbufferWidth;
	static const int sBackbufferHeight;
	static int sPassTextureCount;
	static vector<TrianglePT> sTriangles;
	static vector<MeshPT> sMeshes;
	static vector<LightData> sLightData;
	static vector<TriangleBVH> sTriangleBVH;
	static vector<MeshBVH> sMeshBVH;
	static PassPathTracer sPathTracerPass;
	static PassDefault sPathTracerCopyDepthPass;
	static PassDefault sPathTracerDebugPass;
	static PassDefault sPathTracerDebugFullscreenPass;
	static Mesh sPathTracerDebugMeshLine;
	static Shader sPathTracerCS;
	static Shader sPathTracerDebugVS;
	static Shader sPathTracerDebugPS;
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
	static void PreparePathTracer(ID3D12GraphicsCommandList* commandList, Scene& scene);
	static void RunPathTracer(ID3D12GraphicsCommandList* commandList);
	static void Shutdown();
	static void BuildBVH();
};
