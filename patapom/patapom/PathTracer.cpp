#include "PathTracer.h"
#include "Scene.h"
#include "Store.h"

const int					PathTracer::sTriangleCountMax = PATH_TRACER_TRIANGLE_COUNT_MAX;
const int					PathTracer::sThreadGroupCountX = ceil(PATH_TRACER_BACKBUFFER_WIDTH / PATH_TRACER_THREAD_COUNT_X);
const int					PathTracer::sThreadGroupCountY = ceil(PATH_TRACER_BACKBUFFER_HEIGHT / PATH_TRACER_THREAD_COUNT_Y);
const int					PathTracer::sThreadGroupCountZ = PATH_TRACER_THREAD_COUNT_Z;
const int					PathTracer::sBackbufferWidth = PATH_TRACER_BACKBUFFER_WIDTH;
const int					PathTracer::sBackbufferHeight = PATH_TRACER_BACKBUFFER_HEIGHT;
vector<Triangle>			PathTracer::sTriangles;
PassPathTracer				PathTracer::sPathTracerPass(L"path tracer pass");
Shader						PathTracer::sPathTracerCS(Shader::ShaderType::COMPUTE_SHADER, L"cs_raytracer.hlsl");
Buffer						PathTracer::sTriangleBuffer(L"pt triangle buffer", sizeof(Triangle), sTriangleCountMax);
RenderTexture				PathTracer::sPtBackbuffer(TextureType::DEFAULT, L"path tracer backbuffer", sBackbufferWidth, sBackbufferHeight, 1, 1, ReadFrom::COLOR, gSamplerLinear, Format::R16G16B16A16_FLOAT, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));

void PathTracer::InitPathTracer(Store& store, Scene& scene)
{
	sPathTracerPass.AddShader(&sPathTracerCS);
	sPathTracerPass.AddBuffer(&sTriangleBuffer);
	sPathTracerPass.AddWriteTexture(&sPtBackbuffer, 0);

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
	store.AddTexture(&sPtBackbuffer);
}

void PathTracer::AddMeshesFromPass(Pass& pass)
{
	vector<Mesh*> meshes = pass.GetMeshVec();
	for (int i = 0; i < meshes.size(); i++)
	{
		vector<Triangle> triangles;
		meshes[i]->ConvertMeshToTriangles(triangles);
		sTriangles.insert(sTriangles.end(), triangles.begin(), triangles.end());
	}
}

void PathTracer::PreparePathTracer()
{
	sTriangleBuffer.SetBufferData(sTriangles.data(), sizeof(Triangle) * sTriangleCountMax);
}

void PathTracer::RunPathTracer(ID3D12GraphicsCommandList* commandList)
{
	sPtBackbuffer.MakeReadyToWrite(commandList);
	gRenderer.RecordComputePass(sPathTracerPass, commandList, sThreadGroupCountX, sThreadGroupCountY, sThreadGroupCountZ);
	sPtBackbuffer.MakeReadyToRead(commandList);
}

void PathTracer::Shutdown()
{

}
