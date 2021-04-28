#include "PathTracer.h"
#include "Scene.h"
#include "Store.h"
#include "Light.h"

const int					PathTracer::sTriangleCountMax = PATH_TRACER_TRIANGLE_COUNT_MAX;
const int					PathTracer::sMeshCountMax = PATH_TRACER_MESH_COUNT_MAX;
const int					PathTracer::sLightDataCountMax = PATH_TRACER_LIGHT_COUNT_MAX;
const int					PathTracer::sThreadGroupCountX = ceil(PATH_TRACER_BACKBUFFER_WIDTH / PATH_TRACER_THREAD_COUNT_X);
const int					PathTracer::sThreadGroupCountY = ceil(PATH_TRACER_BACKBUFFER_HEIGHT / PATH_TRACER_THREAD_COUNT_Y);
const int					PathTracer::sThreadGroupCountZ = PATH_TRACER_THREAD_COUNT_Z;
const int					PathTracer::sBackbufferWidth = PATH_TRACER_BACKBUFFER_WIDTH;
const int					PathTracer::sBackbufferHeight = PATH_TRACER_BACKBUFFER_HEIGHT;
int							PathTracer::sPassTextureCount = 0;
vector<TrianglePT>			PathTracer::sTriangles;
vector<MeshPT>				PathTracer::sMeshes;
vector<LightData>			PathTracer::sLightData;
vector<TriangleBVH>			PathTracer::sTriangleBVH;
vector<MeshBVH>				PathTracer::sMeshBVH;
PassPathTracer				PathTracer::sPathTracerPass(L"path tracer pass");
PassDefault					PathTracer::sPathTracerCopyDepthPass(L"path tracer copy depth pass", false, true, false);
PassDefault					PathTracer::sPathTracerDebugPass(L"path tracer debug pass", true, true, false, PrimitiveType::LINE);
PassDefault					PathTracer::sPathTracerDebugFullscreenPass(L"path tracer debug fullscreen pass", true, true, false, PrimitiveType::TRIANGLE);
Mesh						PathTracer::sPathTracerDebugMeshLine(L"path tracer debug mesh line", Mesh::MeshType::LINE, XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1));
Shader						PathTracer::sPathTracerCS(Shader::ShaderType::COMPUTE_SHADER, L"cs_pathtracer.hlsl");
Shader						PathTracer::sPathTracerDebugVS(Shader::ShaderType::VERTEX_SHADER, L"vs_pathtracer_debug.hlsl");
Shader						PathTracer::sPathTracerDebugPS(Shader::ShaderType::PIXEL_SHADER, L"ps_pathtracer_debug.hlsl");
Shader						PathTracer::sPathTracerDebugFullscreenPS(Shader::ShaderType::PIXEL_SHADER, L"ps_pathtracer_debug_fullscreen.hlsl");
Shader						PathTracer::sPathTracerCopyDepthVS(Shader::ShaderType::VERTEX_SHADER, L"vs_pathtracer_copydepth.hlsl");
Shader						PathTracer::sPathTracerCopyDepthPS(Shader::ShaderType::PIXEL_SHADER, L"ps_pathtracer_copydepth.hlsl");
Shader						PathTracer::sPathTracerBlitPS(Shader::ShaderType::PIXEL_SHADER, L"ps_pathtracer_blit.hlsl");
Buffer						PathTracer::sTriangleBuffer(L"pt triangle buffer", sizeof(TrianglePT), sTriangleCountMax);
Buffer						PathTracer::sMeshBuffer(L"pt mesh buffer", sizeof(MeshPT), sMeshCountMax);
Buffer						PathTracer::sLightDataBuffer(L"pt light data buffer", sizeof(LightData), sLightDataCountMax);
Buffer						PathTracer::sTriangleBvhBuffer(L"pt triangle bvh buffer", sizeof(TriangleBVH), sTriangleCountMax);
Buffer						PathTracer::sMeshBvhBuffer(L"pt mesh bvh buffer", sizeof(MeshBVH), sMeshCountMax);
WriteBuffer					PathTracer::sRayBuffer(L"ray buffer", sizeof(Ray), sBackbufferWidth * sBackbufferHeight);
WriteBuffer					PathTracer::sDebugRayBuffer(L"debug ray buffer", sizeof(Ray), PATH_TRACER_MAX_DEPTH_MAX);
RenderTexture				PathTracer::sBackbufferPT(TextureType::TEX_2D, L"path tracer backbuffer", sBackbufferWidth, sBackbufferHeight, 1, 1, ReadFrom::COLOR, gSamplerLinear, Format::R16G16B16A16_FLOAT, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture				PathTracer::sDebugBackbufferPT(TextureType::TEX_2D, L"path tracer debug backbuffer", sBackbufferWidth, sBackbufferHeight, 1, 1, ReadFrom::COLOR, gSamplerLinear, Format::R16G16B16A16_FLOAT, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture				PathTracer::sDepthbufferWritePT(TextureType::TEX_2D, L"path tracer depthbuffer write", sBackbufferWidth, sBackbufferHeight, 1, 1, ReadFrom::COLOR, gSamplerLinear, Format::R32_FLOAT, XMFLOAT4(DEPTH_FAR_REVERSED_Z_SWITCH, 0.0f, 0.0f, 0.0f));
RenderTexture				PathTracer::sDepthbufferRenderPT(TextureType::TEX_2D, L"path tracer depthbuffer read", sBackbufferWidth, sBackbufferHeight, 1, 1, ReadFrom::DEPTH, gSamplerLinear, Format::D32_FLOAT, DEPTH_FAR_REVERSED_Z_SWITCH, 0);
Camera						PathTracer::sCameraDummyPT(XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), sBackbufferWidth, sBackbufferHeight, 45.0f, 100.0f, 0.1f);

void PathTracer::InitPathTracer(Store& store, Scene& scene)
{
	sPathTracerPass.AddShader(&sPathTracerCS);
	sPathTracerPass.AddBuffer(&sTriangleBuffer);
	sPathTracerPass.AddBuffer(&sMeshBuffer);
	sPathTracerPass.AddBuffer(&sLightDataBuffer);
	sPathTracerPass.AddBuffer(&sMeshBvhBuffer);
	sPathTracerPass.AddBuffer(&sTriangleBvhBuffer);
	sPathTracerPass.AddWriteBuffer(&sRayBuffer);
	sPathTracerPass.AddWriteTexture(&sBackbufferPT, 0);
	sPathTracerPass.AddWriteBuffer(&sDebugRayBuffer);
	sPathTracerPass.AddWriteTexture(&sDepthbufferWritePT, 0);
	sPathTracerPass.SetCamera(&gCameraMain);

	sPathTracerCopyDepthPass.AddMesh(&gFullscreenTriangle);
	sPathTracerCopyDepthPass.AddShader(&sPathTracerCopyDepthVS);
	sPathTracerCopyDepthPass.AddShader(&sPathTracerCopyDepthPS);
	sPathTracerCopyDepthPass.AddTexture(&sDepthbufferWritePT);
	sPathTracerCopyDepthPass.AddRenderTexture(&sDepthbufferRenderPT, 0, 0, DS_REVERSED_Z_SWITCH);
	sPathTracerCopyDepthPass.SetCamera(&sCameraDummyPT);

	sPathTracerDebugPass.AddMesh(&sPathTracerDebugMeshLine);
	sPathTracerDebugPass.AddShader(&sPathTracerDebugVS);
	sPathTracerDebugPass.AddShader(&sPathTracerDebugPS);
	sPathTracerDebugPass.AddBuffer(&sDebugRayBuffer);
	sPathTracerDebugPass.AddRenderTexture(&sDebugBackbufferPT, 0, 0, BlendState::NoBlend());
	sPathTracerDebugPass.AddRenderTexture(&sDepthbufferRenderPT, 0, 0, DS_REVERSED_Z_SWITCH);
	sPathTracerDebugPass.SetCamera(&gCameraMain);

	sPathTracerDebugFullscreenPass.AddMesh(&gFullscreenTriangle);
	sPathTracerDebugFullscreenPass.AddShader(&gDeferredVS);
	sPathTracerDebugFullscreenPass.AddShader(&sPathTracerDebugFullscreenPS);
	sPathTracerDebugFullscreenPass.AddRenderTexture(&sDebugBackbufferPT, 0, 0, BlendState::AdditiveBlend());
	sPathTracerDebugFullscreenPass.AddRenderTexture(&sDepthbufferRenderPT, 0, 0, DS_REVERSED_Z_SWITCH);
	sPathTracerDebugFullscreenPass.SetCamera(&gCameraMain);

	scene.AddPass(&sPathTracerPass);
	scene.AddPass(&sPathTracerCopyDepthPass);
	scene.AddPass(&sPathTracerDebugPass);
	scene.AddPass(&sPathTracerDebugFullscreenPass);
	
	store.AddMesh(&sPathTracerDebugMeshLine);
	store.AddPass(&sPathTracerPass);
	store.AddPass(&sPathTracerCopyDepthPass);
	store.AddPass(&sPathTracerDebugPass);
	store.AddPass(&sPathTracerDebugFullscreenPass);
	store.AddShader(&sPathTracerCS);
	store.AddShader(&sPathTracerDebugVS);
	store.AddShader(&sPathTracerDebugPS);
	store.AddShader(&sPathTracerDebugFullscreenPS);
	store.AddShader(&sPathTracerCopyDepthVS);
	store.AddShader(&sPathTracerCopyDepthPS);
	store.AddShader(&sPathTracerBlitPS);
	store.AddBuffer(&sTriangleBuffer);
	store.AddBuffer(&sMeshBuffer);
	store.AddBuffer(&sLightDataBuffer);
	store.AddBuffer(&sMeshBvhBuffer);
	store.AddBuffer(&sTriangleBvhBuffer);
	store.AddBuffer(&sRayBuffer);
	store.AddTexture(&sBackbufferPT);
	store.AddTexture(&sDebugBackbufferPT);
	store.AddBuffer(&sDebugRayBuffer);
	store.AddTexture(&sDepthbufferWritePT);
	store.AddTexture(&sDepthbufferRenderPT);
	store.AddCamera(&sCameraDummyPT);

	// cache meshes
	vector<Pass*>& passes = scene.GetPasses();
	for (int i = 0; i < passes.size(); i++)
	{
		if (passes[i]->ShareMeshesWithPathTracer() && passes[i] != &sPathTracerPass)
			AddMeshesFromPass(*passes[i]);
	}

	// cache lights
	vector<Light*>& lights = scene.GetLights();
	for (int i = 0; i < lights.size(); i++)
	{
		sLightData.push_back(lights[i]->CreateLightData());
	}

	BuildBVH();
}

void PathTracer::AddMeshesFromPass(Pass& pass)
{
	vector<Mesh*>& meshes = pass.GetMeshVec();
	for (u32 i = 0; i < meshes.size(); i++)
	{
		vector<Texture*>& meshTextures = meshes[i]->GetTextures();
		int meshTextureCount = meshTextures.size();

		// meshes
		MeshPT mpt;
		mpt.mModel = meshes[i]->mObjectUniform.mModel;
		mpt.mModelInv = meshes[i]->mObjectUniform.mModelInv;
		mpt.mMaterialType = MATERIAL_TYPE_GGX; // TODO: change this to an actual per object attribute
		mpt.mEmissive = XMFLOAT3(0.0f, 0.0f, 0.0f);
		mpt.mRoughness = 0.0f;
		mpt.mFresnel = 0.0f;
		mpt.mMetallic = 0.0f;
		mpt.mTextureStartIndex = sPassTextureCount;
		mpt.mTextureCount = meshTextureCount;
		sPassTextureCount += meshTextureCount;
		for (int j = 0; j < meshTextureCount; j++)
		{
			sPathTracerPass.AddTexture(meshTextures[j]);
		}

		sMeshes.push_back(mpt);

		// triangles
		vector<TrianglePT> triangles;
		meshes[i]->ConvertMeshToTrianglesPT(triangles, sMeshes.size() - 1);
		sTriangles.insert(sTriangles.end(), triangles.begin(), triangles.end());
	}
}

void PathTracer::PreparePathTracer(ID3D12GraphicsCommandList* commandList, Scene& scene)
{
	sPathTracerPass.mPassUniform.mTriangleCountPT = sTriangles.size();
	sPathTracerPass.mPassUniform.mMeshCountPT = sMeshes.size();
	sPathTracerPass.mPassUniform.mLightCountPT = sLightData.size();

	sPathTracerPass.UpdateAllUniformBuffers();

	sTriangleBuffer.SetBufferData(sTriangles.data(), sizeof(TrianglePT) * sTriangles.size());
	sMeshBuffer.SetBufferData(sMeshes.data(), sizeof(MeshPT) * sMeshCountMax);
	sLightDataBuffer.SetBufferData(sLightData.data(), sizeof(LightData) * sLightData.size());
	sRayBuffer.RecordSetBufferData(commandList, nullptr, sizeof(Ray) * sBackbufferWidth * sBackbufferHeight);
	sDebugRayBuffer.RecordSetBufferData(commandList, nullptr, sizeof(Ray) * PATH_TRACER_MAX_DEPTH_MAX);
}

void PathTracer::RunPathTracer(ID3D12GraphicsCommandList* commandList)
{
	sBackbufferPT.MakeReadyToWrite(commandList);
	sDepthbufferWritePT.MakeReadyToWrite(commandList);
	sDebugRayBuffer.MakeReadyToWrite(commandList);
	gRenderer.RecordComputePass(sPathTracerPass, commandList, sThreadGroupCountX, sThreadGroupCountY, sThreadGroupCountZ);
	
	sDepthbufferWritePT.MakeReadyToRead(commandList);
	sDepthbufferRenderPT.MakeReadyToRender(commandList);
	gRenderer.RecordGraphicsPass(sPathTracerCopyDepthPass, commandList, false, true);
}

void PathTracer::Shutdown()
{

}

void PathTracer::BuildBVH()
{
	// 1. preprocess trianlges to generate top level bounding box, morton code
	

	// 2. build low level accelerate structure - order triangles for each mesh, gather triangles with the same n-bits morton code

	// 3. build high level accelerate structure - order meshes
}