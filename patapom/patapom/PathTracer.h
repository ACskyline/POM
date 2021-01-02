#pragma once

#include "Buffer.h"
#include "Texture.h"
#include "Pass.h"
#include "Mesh.h"

struct PassUniformPathTracer : PassUniformDefault
{
	u32 PADDING_0;
	u32 PADDING_1;
	u32 PADDING_2;
	u32 PADDING_3;
};

typedef PassCommon<PassUniformPathTracer> PassPathTracer;

class PathTracer
{
public:
	static const int sTriangleCountMax;
	static const int sThreadGroupCountX;
	static const int sThreadGroupCountY;
	static const int sThreadGroupCountZ;
	static vector<Mesh::Triangle> sTriangles;
	static PassPathTracer sPathTracerPass;
	static Shader sPathTracerCS;
	static Buffer sTriangleBuffer;
	static void InitPathTracer(Store& store, Scene& scene);
	static void AddMeshesFromPass(Pass& pass);
	static void PreparePathTracer();
	static void RunPathTracer(ID3D12GraphicsCommandList* commandList);
	static void Shutdown();
};
