#include "PathTracer.h"
#include "Scene.h"
#include "Store.h"

const int					PathTracer::sTriangleCountMax = 3000;
const int					PathTracer::sThreadGroupCountX = 32;
const int					PathTracer::sThreadGroupCountY = 32;
const int					PathTracer::sThreadGroupCountZ = 32;
vector<Mesh::Triangle>		PathTracer::sTriangles;
PassPathTracer				PathTracer::sPathTracerPass(L"path tracer pass");
Shader						PathTracer::sPathTracerCS(Shader::ShaderType::COMPUTE_SHADER, L"cs_raytracer.hlsl");
Buffer						PathTracer::sTriangleBuffer(L"pt triangle buffer", sizeof(Mesh::Triangle), sTriangleCountMax);

void PathTracer::InitPathTracer(Store& store, Scene& scene)
{
	sPathTracerPass.AddShader(&sPathTracerCS);
	sPathTracerPass.AddBuffer(&sTriangleBuffer);

	vector<Pass*>& passes = scene.GetPasses();
	for (int i = 0;i<passes.size();i++)
	{
		if (passes[i]->ShareMeshesWithPathTracer())
			AddMeshesFromPass(*passes[i]);
	}

	scene.AddPass(&sPathTracerPass);
	store.AddPass(&sPathTracerPass);
	store.AddShader(&sPathTracerCS);
	store.AddBuffer(&sTriangleBuffer);
}

void PathTracer::AddMeshesFromPass(Pass& pass)
{
	vector<Mesh*> meshes = pass.GetMeshVec();
	for (int i = 0; i < meshes.size(); i++)
	{
		vector<Mesh::Triangle> triangles;
		meshes[i]->ConvertMeshToTriangles(triangles);
		sTriangles.insert(sTriangles.end(), triangles.begin(), triangles.end());
	}
}

void PathTracer::PreparePathTracer()
{
	sTriangleBuffer.SetBufferData(sTriangles.data(), sizeof(Mesh::Triangle) * sTriangleCountMax);
}

void PathTracer::RunPathTracer(ID3D12GraphicsCommandList* commandList)
{
	gRenderer.RecordComputePass(sPathTracerPass, commandList, sThreadGroupCountX, sThreadGroupCountY, sThreadGroupCountZ);
}

void PathTracer::Shutdown()
{

}
