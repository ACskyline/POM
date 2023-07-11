#include "PathTracer.h"
#include "Scene.h"
#include "Store.h"
#include "Light.h"
#include "DeferredLighting.h"
#include <algorithm>

CommandLineArg PARAM_printBvh("-printBvh");
CommandLineArg PARAM_buildBvhGpu("-buildBvhGpu");

const int							PathTracer::sLightDataCountMax = PT_LIGHT_PER_SCENE_MAX;
const int							PathTracer::sThreadGroupCountX = ceil(PT_BACKBUFFER_WIDTH / PT_THREAD_PER_THREADGROUP_X);
const int							PathTracer::sThreadGroupCountY = ceil(PT_BACKBUFFER_HEIGHT / PT_THREAD_PER_THREADGROUP_Y);
const int							PathTracer::sBackbufferWidth = PT_BACKBUFFER_WIDTH;
const int							PathTracer::sBackbufferHeight = PT_BACKBUFFER_HEIGHT;
u32									PathTracer::sMeshTextureCount = 0;
u32									PathTracer::sMeshBvhRootIndexGlobal = INVALID_UINT32;
vector<TrianglePT>					PathTracer::sTriangles;
vector<MeshPT>						PathTracer::sMeshes;
vector<LightData>					PathTracer::sLightData;
vector<BVH>							PathTracer::sTriangleModelBVHs;
vector<BVH>							PathTracer::sMeshWorldBVHs;
PassPathTracerBuildScene			PathTracer::sPathTracerRadixSortInitPass[BuildBvhType::BuildBvhTypeCount];
PassPathTracerBuildScene			PathTracer::sPathTracerRadixSortPollPass[BuildBvhType::BuildBvhTypeCount];
PassPathTracerBuildScene			PathTracer::sPathTracerRadixSortUpSweepPass[BuildBvhType::BuildBvhTypeCount];
PassPathTracerBuildScene			PathTracer::sPathTracerRadixSortDownSweepPass[BuildBvhType::BuildBvhTypeCount];
PassPathTracerBuildScene			PathTracer::sPathTracerRadixSortReorderPass[BuildBvhType::BuildBvhTypeCount];
PassPathTracerBuildScene			PathTracer::sPathTracerBuildBvhPass[BuildBvhType::BuildBvhTypeCount];
PassPathTracerBuildScene			PathTracer::sPathTracerBuildBvhUpdatePass[BuildBvhType::BuildBvhTypeCount];
PassPathTracer						PathTracer::sPathTracerPass("path tracer pass", false, false);
PassDefault							PathTracer::sPathTracerCopyDepthPass("path tracer copy depth pass", false, true, false);
PassDefault							PathTracer::sPathTracerDebugLinePass("path tracer debug line pass", true, true, false, PrimitiveType::LINE);
PassDefault							PathTracer::sPathTracerDebugCubePass("path tracer debug cube pass", true, false, false, PrimitiveType::TRIANGLE);
PassDefault							PathTracer::sPathTracerDebugFullscreenPass("path tracer debug fullscreen pass", true, false, false, PrimitiveType::TRIANGLE);
Mesh								PathTracer::sPathTracerDebugMeshLine("path tracer debug mesh line", Mesh::MeshType::LINE, XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1));
Mesh								PathTracer::sPathTracerDebugMeshCube("path tracer debug mesh cube", Mesh::MeshType::CUBE, XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1));
Shader								PathTracer::sPathTracerCS(Shader::ShaderType::COMPUTE_SHADER, "cs_pathtracer");
Shader								PathTracer::sPathTracerRadixSortInitCS(Shader::ShaderType::COMPUTE_SHADER, "cs_pathtracer_radixsort_init");
Shader								PathTracer::sPathTracerRadixSortPollCS(Shader::ShaderType::COMPUTE_SHADER, "cs_pathtracer_radixsort_poll");
Shader								PathTracer::sPathTracerRadixSortUpSweepCS(Shader::ShaderType::COMPUTE_SHADER, "cs_pathtracer_radixsort_upsweep");
Shader								PathTracer::sPathTracerRadixSortDownSweepCS(Shader::ShaderType::COMPUTE_SHADER, "cs_pathtracer_radixsort_downsweep");
Shader								PathTracer::sPathTracerRadixSortReorderCS(Shader::ShaderType::COMPUTE_SHADER, "cs_pathtracer_radixsort_reorder");
Shader								PathTracer::sPathTracerBuildBvhCS(Shader::ShaderType::COMPUTE_SHADER, "cs_pathtracer_buildbvh");
Shader								PathTracer::sPathTracerBuildBvhUpdateCS(Shader::ShaderType::COMPUTE_SHADER, "cs_pathtracer_buildbvh_update");
Shader								PathTracer::sPathTracerDebugLineVS(Shader::ShaderType::VERTEX_SHADER, "vs_pathtracer_debug_line");
Shader								PathTracer::sPathTracerDebugLinePS(Shader::ShaderType::PIXEL_SHADER, "ps_pathtracer_debug_line");
Shader								PathTracer::sPathTracerDebugCubeVS(Shader::ShaderType::VERTEX_SHADER, "vs_pathtracer_debug_cube");
Shader								PathTracer::sPathTracerDebugCubePS(Shader::ShaderType::PIXEL_SHADER, "ps_pathtracer_debug_cube");
Shader								PathTracer::sPathTracerDebugFullscreenPS(Shader::ShaderType::PIXEL_SHADER, "ps_pathtracer_debug_fullscreen");
Shader								PathTracer::sPathTracerCopyDepthVS(Shader::ShaderType::VERTEX_SHADER, "vs_pathtracer_copydepth");
Shader								PathTracer::sPathTracerCopyDepthPS(Shader::ShaderType::PIXEL_SHADER, "ps_pathtracer_copydepth");
Shader								PathTracer::sPathTracerBlitBackbufferPS(Shader::ShaderType::PIXEL_SHADER, "ps_pathtracer_blitbackbuffer");
Buffer								PathTracer::sLightDataBuffer("pt light data buffer", sizeof(LightData), sLightDataCountMax);
Buffer								PathTracer::sTriangleBuffer("pt triangle buffer", sizeof(TrianglePT), PT_TRIANGLE_PER_SCENE_MAX);
WriteBuffer							PathTracer::sMeshBuffer("pt mesh buffer", sizeof(MeshPT), PT_MESH_PER_SCENE_MAX);
WriteBuffer							PathTracer::sGlobalBvhSettingsBuffer("pt global bvh settings buffer", sizeof(GlobalBvhSettings), 1);
WriteBuffer							PathTracer::sTriangleBvhBuffer("pt triangle bvh buffer", sizeof(BVH), PT_TRIANGLE_BVH_PER_SCENE_MAX); // for each mesh we have, there is 1 fewer entry for bvh than for triangle, i.e. a[n] = 2^(n-1) then S[n] = 2^n-1, S[n-1]=a[n]-1
WriteBuffer							PathTracer::sMeshBvhBuffer("pt mesh bvh buffer", sizeof(BVH), PT_MESH_BVH_PER_SCENE_MAX);
WriteBuffer							PathTracer::sTempBvhBuffer("temporary bvh buffer", sizeof(BVH), PT_BVH_PER_SCENE_MAX); // temporary bvh buffer for building ONE bvh root
WriteBuffer							PathTracer::sRayBuffer("ray buffer", sizeof(Ray), sBackbufferWidth * sBackbufferHeight);
WriteBuffer							PathTracer::sDebugRayBuffer("debug ray buffer", sizeof(Ray), PT_MAXDEPTH_MAX);
WriteBuffer							PathTracer::sAabbProxyBuffer("aabb proxy buffer", sizeof(AabbProxy), PT_LEAF_PER_TREE_MAX); // largest triangle count per mesh is the total triangle count (e.g. when only 1 mesh is in the scene)
WriteBuffer							PathTracer::sSortedAabbProxyBuffer("aabb sorted proxy buffer", sizeof(AabbProxy), PT_LEAF_PER_TREE_MAX);
WriteBuffer							PathTracer::sRadixSortBitCountArrayBuffer("bit count array buffer", sizeof(XMUINT4), PT_LEAF_PER_TREE_MAX);
WriteBuffer							PathTracer::sRadixSortPrefixSumAuxArrayBuffer("prefix sum aux array buffer", sizeof(XMUINT4), PT_RADIXSORT_THREAD_COUNT_MAX);
RenderTexture						PathTracer::sBackbufferPT(TextureType::TEX_2D, "path tracer backbuffer", sBackbufferWidth, sBackbufferHeight, 1, 1, ReadFrom::COLOR, Format::R16G16B16A16_FLOAT, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture						PathTracer::sDebugBackbufferPT(TextureType::TEX_2D, "path tracer debug backbuffer", sBackbufferWidth, sBackbufferHeight, 1, 1, ReadFrom::COLOR, Format::R16G16B16A16_FLOAT, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture						PathTracer::sDepthbufferWritePT(TextureType::TEX_2D, "path tracer depthbuffer write", sBackbufferWidth, sBackbufferHeight, 1, 1, ReadFrom::COLOR, Format::R32_FLOAT, XMFLOAT4(DEPTH_FAR_REVERSED_Z_SWITCH, 0.0f, 0.0f, 0.0f));
RenderTexture						PathTracer::sDepthbufferRenderPT(TextureType::TEX_2D, "path tracer depthbuffer read", sBackbufferWidth, sBackbufferHeight, 1, 1, ReadFrom::DEPTH, Format::D32_FLOAT, DEPTH_FAR_REVERSED_Z_SWITCH, 0);

void PathTracer::InitPathTracer(Store& store, Scene& scene)
{
	fatalAssertf(PT_RADIXSORT_THREAD_COUNT_MAX <= 0x80000000, "We need power of 2 threads to do the sweeps, we can't find the next power of 2 with the current thread count %d!", PT_RADIXSORT_THREAD_COUNT_MAX);
	// maximum thread group per dispatch is 65535 for D3D
	fatalAssertf(PT_RADIXSORT_THREAD_COUNT_MAX <= PT_RADIXSORT_SWEEP_THREAD_PER_THREADGROUP * 65535, "Sweep operation can't be finished in 1 dispatch with the current thread count %d, we can only dispatch 65535 thread groups for each dispatch!", PT_RADIXSORT_THREAD_COUNT_MAX);

	for (u8 i = 0; i < BuildBvhType::BuildBvhTypeCount; i++)
	{
		string buildBvhType = i == (u8)BuildBvhType::BuildBvhTypeTriangle ? "Triangle" : "Mesh";

		sPathTracerRadixSortInitPass[i].CreatePass("path tracer radix sort init pass " + buildBvhType);
		sPathTracerRadixSortPollPass[i].CreatePass("path tracer radix sort poll pass " + buildBvhType);
		sPathTracerRadixSortUpSweepPass[i].CreatePass("path tracer radix sort up sweep pass " + buildBvhType);
		sPathTracerRadixSortDownSweepPass[i].CreatePass("path tracer radix sort down sweep pass " + buildBvhType);
		sPathTracerRadixSortReorderPass[i].CreatePass("path tracer radix sort reorder pass " + buildBvhType);
		sPathTracerBuildBvhPass[i].CreatePass("path tracer build bvh pass " + buildBvhType);
		sPathTracerBuildBvhUpdatePass[i].CreatePass("path tracer build bvh update pass " + buildBvhType);

		// build triangle bvh on GPU
		sPathTracerRadixSortInitPass[i].AddShader(&sPathTracerRadixSortInitCS);
		sPathTracerRadixSortInitPass[i].AddBuffer(&sTriangleBuffer);
		sPathTracerRadixSortInitPass[i].AddBuffer(&sMeshBuffer);
		sPathTracerRadixSortInitPass[i].AddWriteBuffer(&sAabbProxyBuffer);
		sPathTracerRadixSortInitPass[i].AddWriteBuffer(&sTriangleBvhBuffer);
		sPathTracerRadixSortInitPass[i].AddWriteBuffer(&sTempBvhBuffer);
		sPathTracerRadixSortPollPass[i].AddShader(&sPathTracerRadixSortPollCS);
		sPathTracerRadixSortPollPass[i].AddBuffer(&sMeshBuffer);
		sPathTracerRadixSortPollPass[i].AddWriteBuffer(&sAabbProxyBuffer);
		sPathTracerRadixSortPollPass[i].AddWriteBuffer(&sRadixSortBitCountArrayBuffer);
		sPathTracerRadixSortPollPass[i].AddWriteBuffer(&sRadixSortPrefixSumAuxArrayBuffer);
		sPathTracerRadixSortPollPass[i].AddWriteBuffer(&sGlobalBvhSettingsBuffer);
		sPathTracerRadixSortUpSweepPass[i].AddShader(&sPathTracerRadixSortUpSweepCS);
		sPathTracerRadixSortUpSweepPass[i].AddWriteBuffer(&sRadixSortPrefixSumAuxArrayBuffer);
		sPathTracerRadixSortDownSweepPass[i].AddShader(&sPathTracerRadixSortDownSweepCS);
		sPathTracerRadixSortDownSweepPass[i].AddWriteBuffer(&sRadixSortPrefixSumAuxArrayBuffer);
		sPathTracerRadixSortReorderPass[i].AddShader(&sPathTracerRadixSortReorderCS);
		sPathTracerRadixSortReorderPass[i].AddBuffer(&sMeshBuffer);
		sPathTracerRadixSortReorderPass[i].AddBuffer(&sAabbProxyBuffer);
		sPathTracerRadixSortReorderPass[i].AddBuffer(&sRadixSortBitCountArrayBuffer);
		sPathTracerRadixSortReorderPass[i].AddBuffer(&sRadixSortPrefixSumAuxArrayBuffer);
		sPathTracerRadixSortReorderPass[i].AddBuffer(&sGlobalBvhSettingsBuffer);
		sPathTracerRadixSortReorderPass[i].AddWriteBuffer(&sSortedAabbProxyBuffer);
		sPathTracerBuildBvhPass[i].AddShader(&sPathTracerBuildBvhCS);
		sPathTracerBuildBvhPass[i].AddBuffer(&sMeshBuffer);
		sPathTracerBuildBvhPass[i].AddWriteBuffer(&sSortedAabbProxyBuffer);
		sPathTracerBuildBvhPass[i].AddWriteBuffer(&sTempBvhBuffer);
		sPathTracerBuildBvhUpdatePass[i].AddShader(&sPathTracerBuildBvhUpdateCS);
		sPathTracerBuildBvhUpdatePass[i].AddBuffer(&sSortedAabbProxyBuffer);
		sPathTracerBuildBvhUpdatePass[i].AddWriteBuffer(&sMeshBuffer);
		sPathTracerBuildBvhUpdatePass[i].AddWriteBuffer(&sTempBvhBuffer);
		if (i == BuildBvhType::BuildBvhTypeTriangle)
			sPathTracerBuildBvhUpdatePass[i].AddWriteBuffer(&sTriangleBvhBuffer);
		else
			sPathTracerBuildBvhUpdatePass[i].AddWriteBuffer(&sMeshBvhBuffer);
		sPathTracerBuildBvhUpdatePass[i].AddWriteBuffer(&sGlobalBvhSettingsBuffer);
	}

	sPathTracerPass.AddShader(&sPathTracerCS);
	sPathTracerPass.AddBuffer(&sTriangleBuffer);
	sPathTracerPass.AddBuffer(&sMeshBuffer);
	sPathTracerPass.AddBuffer(&sLightDataBuffer);
	sPathTracerPass.AddBuffer(&sMeshBvhBuffer);
	sPathTracerPass.AddBuffer(&sTriangleBvhBuffer);
	sPathTracerPass.AddBuffer(&sGlobalBvhSettingsBuffer);
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

	sPathTracerDebugLinePass.AddMesh(&sPathTracerDebugMeshLine);
	sPathTracerDebugLinePass.AddShader(&sPathTracerDebugLineVS);
	sPathTracerDebugLinePass.AddShader(&sPathTracerDebugLinePS);
	sPathTracerDebugLinePass.AddBuffer(&sDebugRayBuffer);
	sPathTracerDebugLinePass.AddRenderTexture(&sDebugBackbufferPT, 0, 0, BlendState::NoBlend());
	sPathTracerDebugLinePass.AddRenderTexture(&sDepthbufferRenderPT, 0, 0, DS_REVERSED_Z_SWITCH);
	sPathTracerDebugLinePass.SetCamera(&gCameraMain);

	sPathTracerDebugCubePass.AddMesh(&sPathTracerDebugMeshCube);
	sPathTracerDebugCubePass.AddShader(&sPathTracerDebugCubeVS);
	sPathTracerDebugCubePass.AddShader(&sPathTracerDebugCubePS);
	sPathTracerDebugCubePass.AddBuffer(&sMeshBuffer);
	sPathTracerDebugCubePass.AddBuffer(&sMeshBvhBuffer);
	sPathTracerDebugCubePass.AddBuffer(&sTriangleBvhBuffer);
	sPathTracerDebugCubePass.AddRenderTexture(&sDebugBackbufferPT, 0, 0, BlendState::AlphaBlend());
	sPathTracerDebugCubePass.SetCamera(&gCameraMain);

	sPathTracerDebugFullscreenPass.AddMesh(&gFullscreenTriangle);
	sPathTracerDebugFullscreenPass.AddShader(&DeferredLighting::gDeferredVS);
	sPathTracerDebugFullscreenPass.AddShader(&sPathTracerDebugFullscreenPS);
	sPathTracerDebugFullscreenPass.AddRenderTexture(&sDebugBackbufferPT, 0, 0, BlendState::AlphaBlend());
	sPathTracerDebugFullscreenPass.SetCamera(&gCameraMain);
	
	// scene
	for (u8 i = 0; i < BuildBvhType::BuildBvhTypeCount; i++)
	{
		scene.AddPass(&sPathTracerRadixSortInitPass[i]);
		scene.AddPass(&sPathTracerRadixSortPollPass[i]);
		scene.AddPass(&sPathTracerRadixSortUpSweepPass[i]);
		scene.AddPass(&sPathTracerRadixSortDownSweepPass[i]);
		scene.AddPass(&sPathTracerRadixSortReorderPass[i]);
		scene.AddPass(&sPathTracerBuildBvhPass[i]);
		scene.AddPass(&sPathTracerBuildBvhUpdatePass[i]);
	}
	scene.AddPass(&sPathTracerPass);
	scene.AddPass(&sPathTracerCopyDepthPass);
	scene.AddPass(&sPathTracerDebugLinePass);
	scene.AddPass(&sPathTracerDebugCubePass);
	scene.AddPass(&sPathTracerDebugFullscreenPass);

	// store
	for (u8 i = 0; i < BuildBvhType::BuildBvhTypeCount; i++)
	{
		store.AddPass(&sPathTracerRadixSortInitPass[i]);
		store.AddPass(&sPathTracerRadixSortPollPass[i]);
		store.AddPass(&sPathTracerRadixSortUpSweepPass[i]);
		store.AddPass(&sPathTracerRadixSortDownSweepPass[i]);
		store.AddPass(&sPathTracerRadixSortReorderPass[i]);
		store.AddPass(&sPathTracerBuildBvhPass[i]);
		store.AddPass(&sPathTracerBuildBvhUpdatePass[i]);
	}
	store.AddPass(&sPathTracerPass);
	store.AddMesh(&sPathTracerDebugMeshLine);
	store.AddMesh(&sPathTracerDebugMeshCube);
	store.AddPass(&sPathTracerCopyDepthPass);
	store.AddPass(&sPathTracerDebugLinePass);
	store.AddPass(&sPathTracerDebugCubePass);
	store.AddPass(&sPathTracerDebugFullscreenPass);
	store.AddShader(&sPathTracerCS);
	store.AddShader(&sPathTracerRadixSortInitCS);
	store.AddShader(&sPathTracerRadixSortPollCS);
	store.AddShader(&sPathTracerRadixSortUpSweepCS);
	store.AddShader(&sPathTracerRadixSortDownSweepCS);
	store.AddShader(&sPathTracerRadixSortReorderCS);
	store.AddShader(&sPathTracerBuildBvhCS);
	store.AddShader(&sPathTracerBuildBvhUpdateCS);
	store.AddShader(&sPathTracerDebugLineVS);
	store.AddShader(&sPathTracerDebugLinePS);
	store.AddShader(&sPathTracerDebugCubeVS);
	store.AddShader(&sPathTracerDebugCubePS);
	store.AddShader(&sPathTracerDebugFullscreenPS);
	store.AddShader(&sPathTracerCopyDepthVS);
	store.AddShader(&sPathTracerCopyDepthPS);
	store.AddShader(&sPathTracerBlitBackbufferPS);
	store.AddBuffer(&sLightDataBuffer);
	store.AddBuffer(&sTriangleBuffer);
	store.AddBuffer(&sMeshBuffer);
	store.AddBuffer(&sGlobalBvhSettingsBuffer);
	store.AddBuffer(&sTriangleBvhBuffer);
	store.AddBuffer(&sMeshBvhBuffer);
	store.AddBuffer(&sTempBvhBuffer);
	store.AddBuffer(&sRayBuffer);
	store.AddBuffer(&sDebugRayBuffer);
	store.AddBuffer(&sAabbProxyBuffer);
	store.AddBuffer(&sSortedAabbProxyBuffer);
	store.AddBuffer(&sRadixSortBitCountArrayBuffer);
	store.AddBuffer(&sRadixSortPrefixSumAuxArrayBuffer);
	store.AddTexture(&sBackbufferPT);
	store.AddTexture(&sDebugBackbufferPT);
	store.AddTexture(&sDepthbufferWritePT);
	store.AddTexture(&sDepthbufferRenderPT);

	// scene uniform
	scene.mSceneUniform.mPathTracerCurrentTileIndex = 0;

	// cache meshes
	int trianglesPerMeshMax = 0;
	vector<Pass*>& passes = scene.GetPasses();
	for (int i = 0; i < passes.size(); i++)
	{
		if (passes[i]->ShareMeshesWithPathTracer() && passes[i] != &sPathTracerPass)
		{
			int trianglesPerMesh = AddMeshesFromPass(*passes[i]);
			trianglesPerMeshMax = max(trianglesPerMeshMax, trianglesPerMesh);
		}
	}

	// update BVH
	if (!PARAM_buildBvhGpu.Get())
	{
		UpdateBvhCpu(scene);
		if (PARAM_printBvh.Get())
			PrintBVH();
	}

	// cache lights
	vector<Light*>& lights = scene.GetLights();
	for (int i = 0; i < lights.size(); i++)
	{
		sLightData.push_back(lights[i]->CreateLightData());
	}

	int triangleBvhPerScene = sTriangles.size() - sMeshes.size();
	int meshBvhPerScene = sMeshes.size() - 1;
	int bvhPerSceneMax = max(triangleBvhPerScene, meshBvhPerScene);
	int leafPerTreeMax = max(trianglesPerMeshMax, sMeshes.size());
	sTriangleBuffer.SetElementSizeAndCount(sizeof(TrianglePT), sTriangles.size());
	sMeshBuffer.SetElementSizeAndCount(sizeof(MeshPT), sMeshes.size());
	sTriangleBvhBuffer.SetElementSizeAndCount(sizeof(BVH), triangleBvhPerScene);
	sMeshBvhBuffer.SetElementSizeAndCount(sizeof(BVH), meshBvhPerScene);
	sTempBvhBuffer.SetElementSizeAndCount(sizeof(BVH), bvhPerSceneMax);
	sAabbProxyBuffer.SetElementSizeAndCount(sizeof(AabbProxy), leafPerTreeMax);
	sSortedAabbProxyBuffer.SetElementSizeAndCount(sizeof(AabbProxy), leafPerTreeMax);
	sRadixSortBitCountArrayBuffer.SetElementSizeAndCount(sizeof(XMUINT4), leafPerTreeMax);
	sRadixSortPrefixSumAuxArrayBuffer.SetElementSizeAndCount(sizeof(XMUINT4), NextPowerOfTwo32(leafPerTreeMax / PT_BUILDBVH_ENTRY_PER_THREAD));
}

int PathTracer::AddMeshesFromPass(Pass& pass)
{
	int trianglePerMeshMax = 0;
	vector<Mesh*>& meshes = pass.GetMeshes();
	for (u32 i = 0; i < meshes.size(); i++)
	{
		vector<Texture*>& meshTextures = meshes[i]->GetTextures();
		u32 meshTextureCount = meshTextures.size();

		// triangles
		vector<TrianglePT> triangles;
		meshes[i]->ConvertMeshToTrianglesPT(triangles, sMeshes.size());

		if (triangles.size() > trianglePerMeshMax)
			trianglePerMeshMax = triangles.size();

		// meshes
		MeshPT mpt;
		mpt.mModel = meshes[i]->mObjectUniform.mModel;
		mpt.mModelInv = meshes[i]->mObjectUniform.mModelInv;
		mpt.mMaterialType = MATERIAL_TYPE_GGX; // TODO: change this to an actual per object attribute
		mpt.mEmissive = XMFLOAT3(0.0f, 0.0f, 0.0f);
		mpt.mRoughness = 0.0f;
		mpt.mFresnel = 0.0f;
		mpt.mMetallic = 0.0f;
		mpt.mTriangleCount = triangles.size();
		mpt.mTriangleIndexLocalToGlobalOffset = sTriangles.size();
		mpt.mTriangleBvhIndexLocalToGlobalOffset = sTriangles.size() - sMeshes.size(); // for each mesh we have, there is 1 fewer entry for bvh than for triangle
		mpt.mRootTriangleBvhIndexLocal = INVALID_UINT32;
		mpt.mTextureIndexOffset = sMeshTextureCount;
		mpt.mTextureCount = meshTextureCount;
		sMeshTextureCount += meshTextureCount;
		for (int j = 0; j < meshTextureCount; j++)
			sPathTracerPass.AddTexture(meshTextures[j]);

		// append
		sMeshes.push_back(mpt);
		sTriangles.insert(sTriangles.end(), triangles.begin(), triangles.end());
		fatalAssertf(sTriangles.size() < sTriangleBuffer.GetElementCount(), "triangle count %d exceeds structured buffer size %d, adjust PT_TOTAL_TRIANGLE_COUNT_MAX to a larger value", sTriangles.size(), PT_TRIANGLE_PER_SCENE_MAX);
		fatalAssertf(sTriangleModelBVHs.size() < sTriangleBvhBuffer.GetElementCount(), "triangle BVH count %d exceeds structured buffer size %d, adjust PT_TOTAL_TRIANGLE_COUNT_MAX to a larger value", sTriangleModelBVHs.size(), PT_TRIANGLE_PER_SCENE_MAX);
		fatalAssertf(sTriangles.size() < MAX_UINT32, "not sure how to index into a structured buffer which needs 64 bit integer to address"); // TODO: remove this limit
		fatalAssertf(sTriangleModelBVHs.size() < MAX_UINT32, "not sure how to index into a structured buffer which needs 64 bit integer to address"); // TODO: remove this limit
	}
	return trianglePerMeshMax;
}

inline void CentroidOfAABB(const AABB& aabb, XMFLOAT3& centroid)
{
	XMStoreFloat3(&centroid, (XMLoadFloat3(&aabb.mMax) + XMLoadFloat3(&aabb.mMin)) / 2);
}

inline void TransformAABB(const XMFLOAT4X4& mat, const AABB& in, AABB& out)
{
	// transformed AABB is not necessarily an AABB anymore, so we need to recalculate the AABB
	XMVECTOR A = XMVector4Transform(XMVectorSet(in.mMin.x, in.mMin.y, in.mMin.z, 1.0f), XMLoadFloat4x4(&mat));
	XMVECTOR B = XMVector4Transform(XMVectorSet(in.mMin.x, in.mMin.y, in.mMax.z, 1.0f), XMLoadFloat4x4(&mat));
	XMVECTOR C = XMVector4Transform(XMVectorSet(in.mMin.x, in.mMax.y, in.mMin.z, 1.0f), XMLoadFloat4x4(&mat));
	XMVECTOR D = XMVector4Transform(XMVectorSet(in.mMin.x, in.mMax.y, in.mMax.z, 1.0f), XMLoadFloat4x4(&mat));
	XMVECTOR E = XMVector4Transform(XMVectorSet(in.mMax.x, in.mMin.y, in.mMin.z, 1.0f), XMLoadFloat4x4(&mat));
	XMVECTOR F = XMVector4Transform(XMVectorSet(in.mMax.x, in.mMin.y, in.mMax.z, 1.0f), XMLoadFloat4x4(&mat));
	XMVECTOR G = XMVector4Transform(XMVectorSet(in.mMax.x, in.mMax.y, in.mMin.z, 1.0f), XMLoadFloat4x4(&mat));
	XMVECTOR H = XMVector4Transform(XMVectorSet(in.mMax.x, in.mMax.y, in.mMax.z, 1.0f), XMLoadFloat4x4(&mat));
	XMStoreFloat3(&out.mMin, XMVectorMin(A, XMVectorMin(B, XMVectorMin(C, XMVectorMin(D, XMVectorMin(E, XMVectorMin(F, XMVectorMin(G, H))))))));
	XMStoreFloat3(&out.mMax, XMVectorMax(A, XMVectorMax(B, XMVectorMax(C, XMVectorMax(D, XMVectorMax(E, XMVectorMax(F, XMVectorMax(G, H))))))));
}

void PathTracer::UpdateBvhCpu(Scene& scene)
{
	sTriangleModelBVHs.clear();
	sMeshWorldBVHs.clear();

	// process triangle BVH
	for (int i = 0; i < sMeshes.size(); i++)
	{
		MeshPT& mesh = sMeshes[i];
		vector<TrianglePT> triangles(sTriangles.begin() + mesh.mTriangleIndexLocalToGlobalOffset, sTriangles.begin() + mesh.mTriangleIndexLocalToGlobalOffset + mesh.mTriangleCount);
		fatalAssertf(mesh.mTriangleBvhIndexLocalToGlobalOffset == sTriangleModelBVHs.size(), "this should already be set");
		mesh.mTriangleBvhIndexLocalToGlobalOffset = sTriangleModelBVHs.size();
		mesh.mRootTriangleBvhIndexLocal = BuildBVH(triangles, sTriangleModelBVHs, i);
		copy(triangles.begin(), triangles.end(), sTriangles.begin() + mesh.mTriangleIndexLocalToGlobalOffset);
	}

	// process mesh BVH
	vector<AabbProxy> meshProxies;
	for (int i = 0; i < sMeshes.size(); i++)
	{
		AabbProxy meshProxy;
		XMFLOAT3 centroid;
		TransformAABB(sMeshes[i].mModel, sTriangleModelBVHs[sMeshes[i].mRootTriangleBvhIndexLocal + sMeshes[i].mTriangleBvhIndexLocalToGlobalOffset].mAABB, meshProxy.mAABB);
		CentroidOfAABB(meshProxy.mAABB, centroid);
		meshProxy.mOriginalIndex = i;
		meshProxy.mMortonCode = Mesh::GenerateMortonCode(centroid);
		meshProxies.push_back(meshProxy);
	}
	sMeshBvhRootIndexGlobal = BuildBVH(meshProxies, sMeshWorldBVHs); // we look up actual indices through proxies so no need to overwrite mesh vector like we do for triangle vector to keep the new order

	// correct mesh index for mesh BVH
	for (int i = 0; i < sMeshWorldBVHs.size(); i++)
	{
		if (sMeshWorldBVHs[i].mLeftIsLeaf)
			sMeshWorldBVHs[i].mLeftIndexLocal = meshProxies[sMeshWorldBVHs[i].mLeftIndexLocal].mOriginalIndex;
		if (sMeshWorldBVHs[i].mRightIsLeaf)
			sMeshWorldBVHs[i].mRightIndexLocal = meshProxies[sMeshWorldBVHs[i].mRightIndexLocal].mOriginalIndex;
	}

	scene.mSceneUniform.mPathTracerMeshBvhCount = sMeshWorldBVHs.size();
	scene.mSceneUniform.mPathTracerTriangleBvhCount = sTriangleModelBVHs.size();
	scene.mSceneUniform.mPathTracerMeshBvhRootIndex = sMeshBvhRootIndexGlobal;
	scene.SetUniformDirty();
}

void PathTracer::UpdateBvhGpuCommon(CommandList commandList, Scene& scene, BuildBvhType buildBvhType)
{
	SET_UNIFORM_VAR(sPathTracerRadixSortInitPass[buildBvhType], mPassUniform, mBuildBvhMeshCount, sMeshes.size());
	SET_UNIFORM_VAR(sPathTracerRadixSortPollPass[buildBvhType], mPassUniform, mBuildBvhMeshCount, sMeshes.size());
	SET_UNIFORM_VAR(sPathTracerRadixSortUpSweepPass[buildBvhType], mPassUniform, mBuildBvhMeshCount, sMeshes.size());
	SET_UNIFORM_VAR(sPathTracerRadixSortDownSweepPass[buildBvhType], mPassUniform, mBuildBvhMeshCount, sMeshes.size());
	SET_UNIFORM_VAR(sPathTracerRadixSortReorderPass[buildBvhType], mPassUniform, mBuildBvhMeshCount, sMeshes.size());
	SET_UNIFORM_VAR(sPathTracerBuildBvhPass[buildBvhType], mPassUniform, mBuildBvhMeshCount, sMeshes.size());
	SET_UNIFORM_VAR(sPathTracerBuildBvhUpdatePass[buildBvhType], mPassUniform, mBuildBvhMeshCount, sMeshes.size());

	SET_UNIFORM_VAR(sPathTracerRadixSortInitPass[buildBvhType], mPassUniform, mBuildBvhType, buildBvhType);
	SET_UNIFORM_VAR(sPathTracerRadixSortPollPass[buildBvhType], mPassUniform, mBuildBvhType, buildBvhType);
	SET_UNIFORM_VAR(sPathTracerRadixSortUpSweepPass[buildBvhType], mPassUniform, mBuildBvhType, buildBvhType);
	SET_UNIFORM_VAR(sPathTracerRadixSortDownSweepPass[buildBvhType], mPassUniform, mBuildBvhType, buildBvhType);
	SET_UNIFORM_VAR(sPathTracerRadixSortReorderPass[buildBvhType], mPassUniform, mBuildBvhType, buildBvhType);
	SET_UNIFORM_VAR(sPathTracerBuildBvhPass[buildBvhType], mPassUniform, mBuildBvhType, buildBvhType);
	SET_UNIFORM_VAR(sPathTracerBuildBvhUpdatePass[buildBvhType], mPassUniform, mBuildBvhType, buildBvhType);

	// build bvh on GPU
	int iterationCount = buildBvhType == BuildBvhType::BuildBvhTypeTriangle ? sMeshes.size() : 1;
	for (int i = 0; i < iterationCount; i++)
	{
		GPU_LABEL(commandList, "UpdateBvhGpu for entry %d", i);

		int leafCount = buildBvhType == BuildBvhType::BuildBvhTypeTriangle ? sMeshes[i].mTriangleCount : sMeshes.size();
		const int buildBvhDispatchCount = ceil((float)leafCount / PT_BUILDBVH_ENTRY_PER_DISPATCH);
		const int radixSortDispatchCount = ceil((float)leafCount / PT_BUILDBVH_ENTRY_PER_DISPATCH);
		const int radixSortSweepThreadCount = NextPowerOfTwo32(leafCount / PT_BUILDBVH_ENTRY_PER_THREAD);
		const int radixSortSweepDepthCount = log2(radixSortSweepThreadCount);
		fatalAssert(buildBvhDispatchCount > 0);
		fatalAssert(radixSortDispatchCount > 0);
		fatalAssert(radixSortSweepThreadCount > 0);
		fatalAssert(radixSortSweepDepthCount > 0);

		SET_UNIFORM_VAR(sPathTracerRadixSortInitPass[buildBvhType], mPassUniform, mBuildBvhMeshIndex, i);
		SET_UNIFORM_VAR(sPathTracerRadixSortPollPass[buildBvhType], mPassUniform, mBuildBvhMeshIndex, i);
		SET_UNIFORM_VAR(sPathTracerRadixSortUpSweepPass[buildBvhType], mPassUniform, mBuildBvhMeshIndex, i);
		SET_UNIFORM_VAR(sPathTracerRadixSortDownSweepPass[buildBvhType], mPassUniform, mBuildBvhMeshIndex, i);
		SET_UNIFORM_VAR(sPathTracerRadixSortReorderPass[buildBvhType], mPassUniform, mBuildBvhMeshIndex, i);
		SET_UNIFORM_VAR(sPathTracerBuildBvhPass[buildBvhType], mPassUniform, mBuildBvhMeshIndex, i);
		SET_UNIFORM_VAR(sPathTracerBuildBvhUpdatePass[buildBvhType], mPassUniform, mBuildBvhMeshIndex, i);
		
		SET_UNIFORM_VAR(sPathTracerRadixSortInitPass[buildBvhType], mPassUniform, mRadixSortSweepThreadCount, radixSortSweepThreadCount);
		SET_UNIFORM_VAR(sPathTracerRadixSortPollPass[buildBvhType], mPassUniform, mRadixSortSweepThreadCount, radixSortSweepThreadCount);
		SET_UNIFORM_VAR(sPathTracerRadixSortUpSweepPass[buildBvhType], mPassUniform, mRadixSortSweepThreadCount, radixSortSweepThreadCount);
		SET_UNIFORM_VAR(sPathTracerRadixSortDownSweepPass[buildBvhType], mPassUniform, mRadixSortSweepThreadCount, radixSortSweepThreadCount);
		SET_UNIFORM_VAR(sPathTracerRadixSortReorderPass[buildBvhType], mPassUniform, mRadixSortSweepThreadCount, radixSortSweepThreadCount);
		SET_UNIFORM_VAR(sPathTracerBuildBvhPass[buildBvhType], mPassUniform, mRadixSortSweepThreadCount, radixSortSweepThreadCount);
		SET_UNIFORM_VAR(sPathTracerBuildBvhUpdatePass[buildBvhType], mPassUniform, mRadixSortSweepThreadCount, radixSortSweepThreadCount);

		SET_UNIFORM_VAR(sPathTracerRadixSortInitPass[buildBvhType], mPassUniform, mRadixSortSweepDepthCount, radixSortSweepDepthCount);
		SET_UNIFORM_VAR(sPathTracerRadixSortPollPass[buildBvhType], mPassUniform, mRadixSortSweepDepthCount, radixSortSweepDepthCount);
		SET_UNIFORM_VAR(sPathTracerRadixSortUpSweepPass[buildBvhType], mPassUniform, mRadixSortSweepDepthCount, radixSortSweepDepthCount);
		SET_UNIFORM_VAR(sPathTracerRadixSortDownSweepPass[buildBvhType], mPassUniform, mRadixSortSweepDepthCount, radixSortSweepDepthCount);
		SET_UNIFORM_VAR(sPathTracerRadixSortReorderPass[buildBvhType], mPassUniform, mRadixSortSweepDepthCount, radixSortSweepDepthCount);
		SET_UNIFORM_VAR(sPathTracerBuildBvhPass[buildBvhType], mPassUniform, mRadixSortSweepDepthCount, radixSortSweepDepthCount);
		SET_UNIFORM_VAR(sPathTracerBuildBvhUpdatePass[buildBvhType], mPassUniform, mRadixSortSweepDepthCount, radixSortSweepDepthCount);

		// init
		{
			GPU_LABEL(commandList, "init");
			sMeshBuffer.MakeReadyToRead(commandList);
			sTriangleBvhBuffer.MakeReadyToWrite(commandList);
			for (int j = 0; j < radixSortDispatchCount; j++)
			{
				SET_UNIFORM_VAR(sPathTracerRadixSortInitPass[buildBvhType], mPassUniform, mDispatchIndex, j);
				gRenderer.RecordComputePass(
					sPathTracerRadixSortInitPass[buildBvhType],
					commandList,
					PT_BUILDBVH_THREADGROUP_PER_DISPATCH,
					1,
					1);
			}
		}

		// sort the proxy array
		GPU_LABEL_BEGIN(commandList, "sort the proxy array");
		WriteBuffer* aabbProxyIn = &sAabbProxyBuffer;
		WriteBuffer* aabbProxyOut = &sSortedAabbProxyBuffer;
		for (int j = 0; j < PT_RADIXSORT_BITGROUP_COUNT; j++)
		{
			GPU_LABEL(commandList, "bit group %d", j);

			const int bitGroupIndex = j;
			SET_UNIFORM_VAR(sPathTracerRadixSortPollPass[buildBvhType], mPassUniform, mRadixSortBitGroupIndex, bitGroupIndex);
			
			// poll
			GPU_LABEL_BEGIN(commandList, "Poll");
			aabbProxyIn->MakeReadyToWrite(commandList);
			aabbProxyIn->MakeReadyToWriteAgain(commandList);
			sPathTracerRadixSortPollPass[buildBvhType].SetWriteBuffer(0, aabbProxyIn);
			for (int k = 0; k < radixSortDispatchCount; k++)
			{
				GPU_MARKER(commandList, "poll dispatch %d", k);
				SET_UNIFORM_VAR(sPathTracerRadixSortPollPass[buildBvhType], mPassUniform, mDispatchIndex, k);
				gRenderer.RecordComputePass(
					sPathTracerRadixSortPollPass[buildBvhType],
					commandList,
					PT_BUILDBVH_THREADGROUP_PER_DISPATCH,
					1,
					1);
			}
			GPU_LABEL_END(commandList);

			// up sweep
			GPU_LABEL_BEGIN(commandList, "Up Sweep");
			sRadixSortPrefixSumAuxArrayBuffer.MakeReadyToWrite(commandList);
			for (int k = 0; k < radixSortSweepDepthCount; k++)
			{
				GPU_MARKER(commandList, "up sweep dispatch %d", k);
				sRadixSortPrefixSumAuxArrayBuffer.MakeReadyToWriteAgain(commandList);
				SET_UNIFORM_VAR(sPathTracerRadixSortUpSweepPass[buildBvhType], mPassUniform, mDispatchIndex, k);
				gRenderer.RecordComputePass(
					sPathTracerRadixSortUpSweepPass[buildBvhType],
					commandList,
					CEIL_DIVIDE(radixSortSweepThreadCount, PT_RADIXSORT_SWEEP_THREAD_PER_THREADGROUP),
					1,
					1);
			}
			GPU_LABEL_END(commandList);

			// down sweep
			GPU_LABEL_BEGIN(commandList, "Down Sweep");
			for (int k = 0; k < radixSortSweepDepthCount; k++)
			{
				GPU_MARKER(commandList, "down sweep dispatch %d", k);
				sRadixSortPrefixSumAuxArrayBuffer.MakeReadyToWriteAgain(commandList);
				SET_UNIFORM_VAR(sPathTracerRadixSortDownSweepPass[buildBvhType], mPassUniform, mDispatchIndex, k);
				gRenderer.RecordComputePass(
					sPathTracerRadixSortDownSweepPass[buildBvhType],
					commandList,
					CEIL_DIVIDE(radixSortSweepThreadCount, PT_RADIXSORT_SWEEP_THREAD_PER_THREADGROUP),
					1,
					1);
			}
			GPU_LABEL_END(commandList);

			// reorder
			GPU_LABEL_BEGIN(commandList, "Reorder");
			aabbProxyIn->MakeReadyToRead(commandList);
			sRadixSortBitCountArrayBuffer.MakeReadyToRead(commandList);
			sRadixSortPrefixSumAuxArrayBuffer.MakeReadyToWriteAgain(commandList);
			sRadixSortPrefixSumAuxArrayBuffer.MakeReadyToRead(commandList);
			sGlobalBvhSettingsBuffer.MakeReadyToRead(commandList);
			aabbProxyOut->MakeReadyToWrite(commandList);
			SET_UNIFORM_VAR(sPathTracerRadixSortReorderPass[buildBvhType], mPassUniform, mRadixSortBitGroupIndex, bitGroupIndex);
			sPathTracerRadixSortReorderPass[buildBvhType].SetBuffer(1, aabbProxyIn);
			sPathTracerRadixSortReorderPass[buildBvhType].SetWriteBuffer(0, aabbProxyOut);
			for (int k = 0; k < radixSortDispatchCount; k++)
			{
				GPU_MARKER(commandList, "reorder dispatch %d", k);
				SET_UNIFORM_VAR(sPathTracerRadixSortReorderPass[buildBvhType], mPassUniform, mDispatchIndex, k);
				gRenderer.RecordComputePass(
					sPathTracerRadixSortReorderPass[buildBvhType],
					commandList,
					PT_BUILDBVH_THREADGROUP_PER_DISPATCH,
					1,
					1);
			}
			sGlobalBvhSettingsBuffer.MakeReadyToWrite(commandList);
			aabbProxyIn->MakeReadyToWrite(commandList);
			sRadixSortBitCountArrayBuffer.MakeReadyToWrite(commandList);
			sRadixSortPrefixSumAuxArrayBuffer.MakeReadyToWrite(commandList);
			sRadixSortPrefixSumAuxArrayBuffer.MakeReadyToWriteAgain(commandList);
			GPU_LABEL_END(commandList);

			WriteBuffer* tmp = aabbProxyIn;
			aabbProxyIn = aabbProxyOut;
			aabbProxyOut = tmp;
		}
		GPU_LABEL_END(commandList);

		// build the bvh
		GPU_LABEL_BEGIN(commandList, "build the bvh");
		aabbProxyIn->MakeReadyToWrite(commandList);
		sPathTracerBuildBvhPass[buildBvhType].SetWriteBuffer(0, aabbProxyIn);
		for (int j = 0; j < buildBvhDispatchCount; j++)
		{
			GPU_MARKER(commandList, "dispatch %d", j);
			SET_UNIFORM_VAR(sPathTracerBuildBvhPass[buildBvhType], mPassUniform, mDispatchIndex, j);
			gRenderer.RecordComputePass(
				sPathTracerBuildBvhPass[buildBvhType],
				commandList,
				PT_BUILDBVH_THREADGROUP_PER_DISPATCH,
				1,
				1);
		}
		GPU_LABEL_END(commandList);

		// update the bvh
		GPU_LABEL_BEGIN(commandList, "update the bvh");
		sMeshBuffer.MakeReadyToWrite(commandList);
		aabbProxyIn->MakeReadyToRead(commandList);
		sPathTracerBuildBvhUpdatePass[buildBvhType].SetBuffer(0, aabbProxyIn);
		for (int j = 0; j < buildBvhDispatchCount; j++)
		{
			GPU_MARKER(commandList, "dispatch %d", j);
			SET_UNIFORM_VAR(sPathTracerBuildBvhUpdatePass[buildBvhType], mPassUniform, mDispatchIndex, j);
			gRenderer.RecordComputePass(
				sPathTracerBuildBvhUpdatePass[buildBvhType],
				commandList,
				PT_BUILDBVH_THREADGROUP_PER_DISPATCH,
				1,
				1);
		}
		sMeshBuffer.MakeReadyToRead(commandList);
		sGlobalBvhSettingsBuffer.MakeReadyToRead(commandList);
		sTriangleBvhBuffer.MakeReadyToRead(commandList);
		sMeshBvhBuffer.MakeReadyToRead(commandList);
		GPU_LABEL_END(commandList);
	}
}

void PathTracer::UpdateTriangleBvhGpu(CommandList commandList, Scene& scene)
{
	GPU_LABEL(commandList, "UpdateTriangleBvhGpu");
	UpdateBvhGpuCommon(commandList, scene, BuildBvhType::BuildBvhTypeTriangle);
}

void PathTracer::UpdateMeshBvhGpu(CommandList commandList, Scene& scene)
{
	GPU_LABEL(commandList, "UpdateMeshBvhGpu");
	UpdateBvhGpuCommon(commandList, scene, BuildBvhType::BuildBvhTypeMesh);
}

void PathTracer::UpdateBvhGpu(CommandList commandList, Scene& scene)
{
	UpdateTriangleBvhGpu(commandList, scene);
	UpdateMeshBvhGpu(commandList, scene);

	scene.mSceneUniform.mPathTracerMeshBvhCount = sMeshes.size() - 1;
	scene.mSceneUniform.mPathTracerTriangleBvhCount = sTriangles.size() - sMeshes.size();
	scene.mSceneUniform.mPathTracerMeshBvhRootIndex = INVALID_UINT32;
	scene.SetUniformDirty();
}

inline void PrintSpaces(int spaceCount)
{
	if (spaceCount < 1)
		return;
	string str;
	str.resize(spaceCount * 4, ' ');
	displayf("%s", str.c_str());
}

inline void PrintAABB(const AABB& aabb)
{
	displayf("AABB: min(%f, %f, %f) max(%f, %f, %f)",
		aabb.mMin.x, aabb.mMin.y, aabb.mMin.z,
		aabb.mMax.x, aabb.mMax.y, aabb.mMax.z);
}

inline void PrintLeaf(const TrianglePT& tri)
{
	displayf("mesh index: %u, morton code: %u, v0(%f, %f, %f) v1(%f, %f, %f) v2(%f, %f, %f)", 
		tri.mMeshIndex, tri.mMortonCode, 
		tri.mVertices[0].pos.x, tri.mVertices[0].pos.y, tri.mVertices[0].pos.z,
		tri.mVertices[1].pos.x, tri.mVertices[1].pos.y, tri.mVertices[1].pos.z, 
		tri.mVertices[2].pos.x, tri.mVertices[2].pos.y, tri.mVertices[2].pos.z);
}

inline void PrintLeaf(const MeshPT& mesh)
{
	displayf("root bvh global: %u,  bvh offset: %u, triangle offset: %u, triangle count: %u",
		mesh.mRootTriangleBvhIndexLocal,
		mesh.mTriangleBvhIndexLocalToGlobalOffset,
		mesh.mTriangleIndexLocalToGlobalOffset,
		mesh.mTriangleCount);
}

template<class T_LeafNode>
void PrintBvhNode(const vector<BVH>& bvhsGlobal, const vector<T_LeafNode>& leafNodesGlobal, i64 localIndex, i64 bvhLocalToGlobalOffset, i64 leafLocalToGlobalOffset)
{
	static int depth = 0;
	const i64 bvhGlobalIndex = localIndex + bvhLocalToGlobalOffset;
	const BVH& bvh = bvhsGlobal[bvhGlobalIndex];
	PrintSpaces(depth); 
	displayf("bvh global index: %ld, ", bvhGlobalIndex); 
	PrintAABB(bvh.mAABB);
	displayf("\n");
	depth++;

	// left child
	if (bvh.mLeftIsLeaf)
	{
		const i64 leftLeafGlobalIndex = bvh.mLeftIndexLocal + leafLocalToGlobalOffset;
		PrintSpaces(depth);
		displayf("leaf index: %lld, ", leftLeafGlobalIndex);
		PrintLeaf(leafNodesGlobal[leftLeafGlobalIndex]);
		displayf("\n");
	}
	else if (bvh.mLeftIndexLocal != INVALID_UINT32)
		PrintBvhNode(bvhsGlobal, leafNodesGlobal, bvh.mLeftIndexLocal, bvhLocalToGlobalOffset, leafLocalToGlobalOffset);
	
	// right child
	if (bvh.mRightIsLeaf)
	{
		const i64 rightLeafGlobalIndex = bvh.mRightIndexLocal + leafLocalToGlobalOffset;
		PrintSpaces(depth);
		displayf("leaf index: %lld, ", rightLeafGlobalIndex);
		PrintLeaf(leafNodesGlobal[rightLeafGlobalIndex]);
		displayf("\n");
	}
	else if(bvh.mRightIndexLocal != INVALID_UINT32)
		PrintBvhNode(bvhsGlobal, leafNodesGlobal, bvh.mRightIndexLocal, bvhLocalToGlobalOffset, leafLocalToGlobalOffset);

	depth--;
}

void PathTracer::PrintBVH()
{
	displayfln(">>> Mesh BVH <<<");
	PrintBvhNode(sMeshWorldBVHs, sMeshes, sMeshBvhRootIndexGlobal, 0, 0);
	displayfln("================");
	displayfln(">>> Triangle BVH <<<");
	for (int i = 0; i < sMeshes.size(); i++)
	{
		const MeshPT& mesh = sMeshes[i];
		PrintBvhNode(sTriangleModelBVHs, sTriangles, mesh.mRootTriangleBvhIndexLocal, mesh.mTriangleBvhIndexLocalToGlobalOffset, mesh.mTriangleIndexLocalToGlobalOffset);
	}
}

void PathTracer::PreparePathTracer(CommandList commandList, Scene& scene)
{
	GPU_LABEL_BEGIN(commandList, "Prepare PathTracer");

	sPathTracerPass.mPassUniform.mTriangleCountPT = sTriangles.size();
	sPathTracerPass.mPassUniform.mMeshCountPT = sMeshes.size();
	sPathTracerPass.mPassUniform.mLightCountPT = sLightData.size();

	sPathTracerPass.UpdateAllUniformBuffers();

	sTriangleBuffer.SetBufferData(sTriangles.data(), sizeof(TrianglePT) * sTriangles.size());
	sMeshBuffer.SetBufferData(sMeshes.data(), sizeof(MeshPT) * sMeshes.size());
	sTriangleBvhBuffer.SetBufferData(sTriangleModelBVHs.data(), sizeof(BVH) * sTriangleModelBVHs.size());
	sMeshBvhBuffer.SetBufferData(sMeshWorldBVHs.data(), sizeof(BVH) * sMeshWorldBVHs.size());
	sLightDataBuffer.SetBufferData(sLightData.data(), sizeof(LightData) * sLightData.size());
	sRayBuffer.RecordSetBufferData(commandList, nullptr, sizeof(Ray) * sBackbufferWidth * sBackbufferHeight);
	sDebugRayBuffer.RecordSetBufferData(commandList, nullptr, sizeof(Ray) * PT_MAXDEPTH_MAX);
	sRadixSortPrefixSumAuxArrayBuffer.SetBufferData(nullptr, 0);

	if (PARAM_buildBvhGpu.Get())
	{
		UpdateBvhGpu(commandList, scene);
	}

	GPU_LABEL_END(commandList);
}

void PathTracer::RunPathTracer(CommandList commandList)
{
	sBackbufferPT.MakeReadyToWrite(commandList);
	sDepthbufferWritePT.MakeReadyToWrite(commandList);
	sDebugRayBuffer.MakeReadyToWrite(commandList);
	gRenderer.RecordComputePass(
		sPathTracerPass, 
		commandList, 
		gSceneDefault.mSceneUniform.mPathTracerThreadGroupPerTileX,
		gSceneDefault.mSceneUniform.mPathTracerThreadGroupPerTileY,
		1);
}

void PathTracer::CopyDepthBuffer(CommandList commandList)
{
	sDepthbufferWritePT.MakeReadyToRead(commandList);
	sDepthbufferRenderPT.MakeReadyToRender(commandList);
	gRenderer.RecordGraphicsPass(sPathTracerCopyDepthPass, commandList, false, true, true);
}

void PathTracer::DebugDraw(CommandList commandList)
{
	// always render debug passes for easier renderdoc capture
	PathTracer::sDebugRayBuffer.MakeReadyToRead(commandList);
	PathTracer::sDebugBackbufferPT.MakeReadyToRender(commandList);
	PathTracer::sDepthbufferRenderPT.MakeReadyToRender(commandList);
	gRenderer.RecordGraphicsPassInstanced(PathTracer::sPathTracerDebugLinePass, commandList, PT_MAXDEPTH_MAX * PT_DEBUG_LINE_COUNT_PER_RAY, true);
	gRenderer.RecordGraphicsPassInstanced(PathTracer::sPathTracerDebugCubePass, commandList, 2);
	gRenderer.RecordGraphicsPass(PathTracer::sPathTracerDebugFullscreenPass, commandList);
}

void PathTracer::Shutdown()
{

}

void PathTracer::Restart(Scene& scene)
{
	scene.mSceneUniform.mPathTracerCurrentTileIndex = 0;
	scene.mSceneUniform.mPathTracerCurrentSampleIndex = 0;
	scene.mSceneUniform.mPathTracerCurrentDepth = 0;
}

template<class T_LeafNode>
inline int LongestCommonPrefix(const vector<T_LeafNode>& triangles, i64 left, i64 right)
{
	fatalAssert((left >= 0 && left < triangles.size()) || (right >= 0 && right < triangles.size()));
	if (left < 0 || left >= triangles.size() || right < 0 || right >= triangles.size())
		return -1;
	u32 lkey = triangles[left].mMortonCode;
	u32 rkey = triangles[right].mMortonCode;
	int lcp = __lzcnt(lkey ^ rkey);
	if (lkey == rkey)
	{
		// if the keys of the nodes are the same, nodes which are at closer indices stay close to eachother
		lcp += __lzcnt64((u64)left ^ (u64)right);
	}
	return lcp;
}

inline void MergeAABB(const AABB& aIn, const AABB& bIn, AABB& cOut)
{
	XMStoreFloat3(&cOut.mMin, XMVectorMin(XMLoadFloat3(&aIn.mMin), XMLoadFloat3(&bIn.mMin)));
	XMStoreFloat3(&cOut.mMax, XMVectorMax(XMLoadFloat3(&aIn.mMax), XMLoadFloat3(&bIn.mMax)));
}

inline void InitBVH(BVH& bvh, u32 meshIndex)
{
	bvh.mParentIndexLocal = INVALID_UINT32;
	bvh.mLeftIndexLocal = INVALID_UINT32;
	bvh.mRightIndexLocal = INVALID_UINT32;
	bvh.mLeftIsLeaf = 0;
	bvh.mRightIsLeaf = 0;
	bvh.mVisited = 0;
	bvh.mMeshIndex = meshIndex;
}

inline void InitAABB(const Triangle& triangleIn, AABB& aabbOut)
{
	XMVECTOR p0 = XMLoadFloat3(&triangleIn.mVertices[0].pos);
	XMVECTOR p1 = XMLoadFloat3(&triangleIn.mVertices[1].pos);
	XMVECTOR p2 = XMLoadFloat3(&triangleIn.mVertices[2].pos);
	XMStoreFloat3(&aabbOut.mMin, XMVectorMin(p0, XMVectorMin(p1, p2)));
	XMStoreFloat3(&aabbOut.mMax, XMVectorMax(p0, XMVectorMax(p1, p2)));
}

inline void InitAABB(const AabbProxy& meshProxyIn, AABB& aabbOut)
{
	aabbOut = meshProxyIn.mAABB;
}

template<class T_LeafNode>
i64 PathTracer::BuildBVH(vector<T_LeafNode>& leafLocal, vector<BVH>& bvhGlobal, u32 meshIndex)
{
	// algorithm based on https://developer.nvidia.com/blog/parallelforall/wp-content/uploads/2012/11/karras2012hpg_paper.pdf
	fatalAssertf(leafLocal.size() > 1 && leafLocal.size() < MAX_UINT32, "too many or too few triangles");
	
	// local bvh
	vector<BVH> bvhLocal;

	// 1. sort
	std::sort(leafLocal.begin(), leafLocal.end(), [](const T_LeafNode& a, const T_LeafNode& b)->bool { return a.mMortonCode > b.mMortonCode; });

	// 2. build tree
	bvhLocal.resize(leafLocal.size() - 1);
	for (int i = 0; i < bvhLocal.size(); i++)
		InitBVH(bvhLocal[i], meshIndex);
	for (i64 i = 0; i < bvhLocal.size(); i++)
	{
		// Determine direction of the range (+1 or -1)
		const i64 d = LongestCommonPrefix(leafLocal, i, i + 1) - LongestCommonPrefix(leafLocal, i, i - 1) > 0 ? 1 : -1;
		// Compute upper bound for the length of the range
		const int lcpMin = LongestCommonPrefix(leafLocal, i, i - d);
		i64 lMax = 2;
		while (LongestCommonPrefix(leafLocal, i, i + lMax * d) > lcpMin)
			lMax *= 2;
		// Find the other end using binary search
		i64 l = 0;
		i64 div = 2;
		i64 t = lMax / div;
		while (t >= 1)
		{
			if (LongestCommonPrefix(leafLocal, i, i + (l + t) * d) > lcpMin)
				l = l + t;
			if (t == 1)
				break;
			div *= 2;
			t = lMax / div;
		}
		i64 j = i + l * d;
		// Find the split position using binary search
		int lcpNode = LongestCommonPrefix(leafLocal, i, j);
		i64 s = 0;
		div = 2;
		t = RoundUpDivide(l, div);
		while (t >= 1)
		{
			if (LongestCommonPrefix(leafLocal, i, i + (s + t) * d) > lcpNode)
				s = s + t;
			if (t == 1)
				break;
			div *= 2;
			t = RoundUpDivide(l, div);
		}
		const i64 split = i + s * d + MIN(d, 0);
		fatalAssert(split >= 0 && split < bvhLocal.size());
		// Output child pointers
		const i64 leftIndex = split;
		const i64 rightIndex = split + 1;
		bvhLocal[i].mLeftIndexLocal = leftIndex;
		bvhLocal[i].mLeftIsLeaf = leftIndex == MIN(i, j);
		bvhLocal[i].mRightIndexLocal = rightIndex;
		bvhLocal[i].mRightIsLeaf = rightIndex == MAX(i, j);
		if (bvhLocal[i].mLeftIsLeaf) // left child is leaf
			leafLocal[leftIndex].mBvhIndexLocal = i;
		else
			bvhLocal[leftIndex].mParentIndexLocal = i;
		if (bvhLocal[i].mRightIsLeaf) // right child is leaf
			leafLocal[rightIndex].mBvhIndexLocal = i;
		else
			bvhLocal[rightIndex].mParentIndexLocal = i;
	}

	// 3. update bvh
	i64 rootBvhIndexLocal = -1;
	i64 maxHeight = -1;
	for (i64 i = 0; i < leafLocal.size(); i++)
	{
		i64 current = leafLocal[i].mBvhIndexLocal;
		fatalAssert(current >= 0 && current < bvhLocal.size());
		i64 height = 0;
		while (current != INVALID_UINT32)
		{
			height++;
			u32 original = bvhLocal[current].mVisited++;

			if (original < 1) // if this is the first time trying to visit current node, stop
				break;
			else
			{
				i64 parent = bvhLocal[current].mParentIndexLocal;
				if (parent == INVALID_UINT32)
				{
					if (rootBvhIndexLocal != -1)
						fatalAssert(rootBvhIndexLocal == current);
					rootBvhIndexLocal = current;
				}
				const u32 left = bvhLocal[current].mLeftIndexLocal;
				const u32 right = bvhLocal[current].mRightIndexLocal;
				AABB lAABB;
				AABB rAABB;
				if (bvhLocal[current].mLeftIsLeaf)
					InitAABB(leafLocal[left], lAABB);
				else
					lAABB = bvhLocal[left].mAABB;
				if (bvhLocal[current].mRightIsLeaf)
					InitAABB(leafLocal[right], rAABB);
				else
					rAABB = bvhLocal[right].mAABB;
				MergeAABB(lAABB, rAABB, bvhLocal[current].mAABB);
				current = parent;
			}
		}
		if (height > maxHeight)
			maxHeight = height;
	}
	displayfln("build bvh tree height = %ld", maxHeight);
	fatalAssert(rootBvhIndexLocal >= 0 && rootBvhIndexLocal < bvhLocal.size());
	fatalAssert(rootBvhIndexLocal < INVALID_UINT32);
	bvhGlobal.insert(bvhGlobal.end(), bvhLocal.begin(), bvhLocal.end());
	return rootBvhIndexLocal;
}
