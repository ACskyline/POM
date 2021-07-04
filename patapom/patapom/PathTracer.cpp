#include "PathTracer.h"
#include "Scene.h"
#include "Store.h"
#include "Light.h"
#include <algorithm>

const int					PathTracer::sTriangleCountMax = PATH_TRACER_TOTAL_TRIANGLE_COUNT_MAX;
const int					PathTracer::sMeshCountMax = PATH_TRACER_MESH_COUNT_MAX;
const int					PathTracer::sLightDataCountMax = PATH_TRACER_LIGHT_COUNT_MAX;
const int					PathTracer::sThreadGroupCountX = ceil(PATH_TRACER_BACKBUFFER_WIDTH / PATH_TRACER_THREAD_COUNT_X);
const int					PathTracer::sThreadGroupCountY = ceil(PATH_TRACER_BACKBUFFER_HEIGHT / PATH_TRACER_THREAD_COUNT_Y);
const int					PathTracer::sBackbufferWidth = PATH_TRACER_BACKBUFFER_WIDTH;
const int					PathTracer::sBackbufferHeight = PATH_TRACER_BACKBUFFER_HEIGHT;
u32							PathTracer::sMeshTextureCount = 0;
u32							PathTracer::sMeshBvhRootIndexGlobal = INVALID_UINT32;
vector<TrianglePT>			PathTracer::sTriangles;
vector<MeshPT>				PathTracer::sMeshes;
vector<LightData>			PathTracer::sLightData;
vector<BVH>					PathTracer::sTriangleModelBVHs;
vector<BVH>					PathTracer::sMeshWorldBVHs;
PassPathTracer				PathTracer::sPathTracerPass(L"path tracer pass");
PassDefault					PathTracer::sPathTracerCopyDepthPass(L"path tracer copy depth pass", false, true, false);
PassDefault					PathTracer::sPathTracerDebugLinePass(L"path tracer debug line pass", true, true, false, PrimitiveType::LINE);
PassDefault					PathTracer::sPathTracerDebugCubePass(L"path tracer debug cube pass", true, false, false, PrimitiveType::TRIANGLE);
PassDefault					PathTracer::sPathTracerDebugFullscreenPass(L"path tracer debug fullscreen pass", true, false, false, PrimitiveType::TRIANGLE);
Mesh						PathTracer::sPathTracerDebugMeshLine(L"path tracer debug mesh line", Mesh::MeshType::LINE, XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1));
Mesh						PathTracer::sPathTracerDebugMeshCube(L"path tracer debug mesh cube", Mesh::MeshType::CUBE, XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1));
Shader						PathTracer::sPathTracerCS(Shader::ShaderType::COMPUTE_SHADER, L"cs_pathtracer.hlsl");
Shader						PathTracer::sPathTracerDebugLineVS(Shader::ShaderType::VERTEX_SHADER, L"vs_pathtracer_debug_line.hlsl");
Shader						PathTracer::sPathTracerDebugLinePS(Shader::ShaderType::PIXEL_SHADER, L"ps_pathtracer_debug_line.hlsl");
Shader						PathTracer::sPathTracerDebugCubeVS(Shader::ShaderType::VERTEX_SHADER, L"vs_pathtracer_debug_cube.hlsl");
Shader						PathTracer::sPathTracerDebugCubePS(Shader::ShaderType::PIXEL_SHADER, L"ps_pathtracer_debug_cube.hlsl");
Shader						PathTracer::sPathTracerDebugFullscreenPS(Shader::ShaderType::PIXEL_SHADER, L"ps_pathtracer_debug_fullscreen.hlsl");
Shader						PathTracer::sPathTracerCopyDepthVS(Shader::ShaderType::VERTEX_SHADER, L"vs_pathtracer_copydepth.hlsl");
Shader						PathTracer::sPathTracerCopyDepthPS(Shader::ShaderType::PIXEL_SHADER, L"ps_pathtracer_copydepth.hlsl");
Shader						PathTracer::sPathTracerBlitPS(Shader::ShaderType::PIXEL_SHADER, L"ps_pathtracer_blit.hlsl");
Buffer						PathTracer::sTriangleBuffer(L"pt triangle buffer", sizeof(TrianglePT), sTriangleCountMax);
Buffer						PathTracer::sMeshBuffer(L"pt mesh buffer", sizeof(MeshPT), sMeshCountMax);
Buffer						PathTracer::sLightDataBuffer(L"pt light data buffer", sizeof(LightData), sLightDataCountMax);
Buffer						PathTracer::sTriangleBvhBuffer(L"pt triangle bvh buffer", sizeof(BVH), sTriangleCountMax - 1); // if a[n] = 2^(n-1) then S[n] = 2^n-1, S[n-1]=a[n]-1
Buffer						PathTracer::sMeshBvhBuffer(L"pt mesh bvh buffer", sizeof(BVH), sMeshCountMax);
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
	sPathTracerDebugCubePass.AddRenderTexture(&sDebugBackbufferPT, 0, 0, BlendState::PremultipliedAlphaBlend());
	sPathTracerDebugCubePass.SetCamera(&gCameraMain);

	sPathTracerDebugFullscreenPass.AddMesh(&gFullscreenTriangle);
	sPathTracerDebugFullscreenPass.AddShader(&gDeferredVS);
	sPathTracerDebugFullscreenPass.AddShader(&sPathTracerDebugFullscreenPS);
	sPathTracerDebugFullscreenPass.AddRenderTexture(&sDebugBackbufferPT, 0, 0, BlendState::AdditiveBlend());
	sPathTracerDebugFullscreenPass.SetCamera(&gCameraMain);

	scene.AddPass(&sPathTracerPass);
	scene.AddPass(&sPathTracerCopyDepthPass);
	scene.AddPass(&sPathTracerDebugLinePass);
	scene.AddPass(&sPathTracerDebugCubePass);
	scene.AddPass(&sPathTracerDebugFullscreenPass);
	
	store.AddMesh(&sPathTracerDebugMeshLine);
	store.AddMesh(&sPathTracerDebugMeshCube);
	store.AddPass(&sPathTracerPass);
	store.AddPass(&sPathTracerCopyDepthPass);
	store.AddPass(&sPathTracerDebugLinePass);
	store.AddPass(&sPathTracerDebugCubePass);
	store.AddPass(&sPathTracerDebugFullscreenPass);
	store.AddShader(&sPathTracerCS);
	store.AddShader(&sPathTracerDebugLineVS);
	store.AddShader(&sPathTracerDebugLinePS);
	store.AddShader(&sPathTracerDebugCubeVS);
	store.AddShader(&sPathTracerDebugCubePS);
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

	// scene uniform
	scene.mSceneUniform.mPathTracerCurrentTileIndex = 0;

	// cache meshes
	vector<Pass*>& passes = scene.GetPasses();
	for (int i = 0; i < passes.size(); i++)
	{
		if (passes[i]->ShareMeshesWithPathTracer() && passes[i] != &sPathTracerPass)
			AddMeshesFromPass(*passes[i]);
	}

	// update BVH
	UpdateBVH(scene);
	// PrintBVH();

	// cache lights
	vector<Light*>& lights = scene.GetLights();
	for (int i = 0; i < lights.size(); i++)
	{
		sLightData.push_back(lights[i]->CreateLightData());
	}
}

void PathTracer::AddMeshesFromPass(Pass& pass)
{
	vector<Mesh*>& meshes = pass.GetMeshVec();
	for (u32 i = 0; i < meshes.size(); i++)
	{
		vector<Texture*>& meshTextures = meshes[i]->GetTextures();
		u32 meshTextureCount = meshTextures.size();

		// triangles
		vector<TrianglePT> triangles;
		meshes[i]->ConvertMeshToTrianglesPT(triangles, sMeshes.size());

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
		mpt.mTriangleBvhIndexLocalToGlobalOffset = INVALID_UINT32;
		mpt.mRootTriangleBvhIndexLocal = INVALID_UINT32;
		mpt.mTextureIndexOffset = sMeshTextureCount;
		mpt.mTextureCount = meshTextureCount;
		sMeshTextureCount += meshTextureCount;
		for (int j = 0; j < meshTextureCount; j++)
			sPathTracerPass.AddTexture(meshTextures[j]);

		// append
		sMeshes.push_back(mpt);
		sTriangles.insert(sTriangles.end(), triangles.begin(), triangles.end());
		fatalAssertf(sTriangles.size() < sTriangleBuffer.GetElementCount(), "triangle count %d exceeds structured buffer size %d, adjust PATH_TRACER_TOTAL_TRIANGLE_COUNT_MAX to a larger value", sTriangles.size(), PATH_TRACER_TOTAL_TRIANGLE_COUNT_MAX);
		fatalAssertf(sTriangleModelBVHs.size() < sTriangleBvhBuffer.GetElementCount(), "triangle BVH count %d exceeds structured buffer size %d, adjust PATH_TRACER_TOTAL_TRIANGLE_COUNT_MAX to a larger value", sTriangleModelBVHs.size(), PATH_TRACER_TOTAL_TRIANGLE_COUNT_MAX);
		fatalAssertf(sTriangles.size() < MAX_UINT32, "not sure how to index into a structured buffer which needs 64 bit integer to address"); // TODO: remove this limit
		fatalAssertf(sTriangleModelBVHs.size() < MAX_UINT32, "not sure how to index into a structured buffer which needs 64 bit integer to address"); // TODO: remove this limit
	}
}

inline void CentroidOfAABB(const AABB& aabb, XMFLOAT3& centroid)
{
	XMStoreFloat3(&centroid, (XMLoadFloat3(&aabb.mMax) + XMLoadFloat3(&aabb.mMin)) / 2);
}

struct MeshProxy
{
	AABB mWorldAABB;
	UINT mMortonCode;
	UINT mMeshOriginalIndex;
	UINT mBvhIndexLocal;
};

inline void TransformAABB(const XMFLOAT4X4& mat, const AABB& in, AABB& out)
{
	// transformed AABB is not necessarily an AABB anymore
	XMVECTOR A = XMVector4Transform(XMLoadFloat4(&XMFLOAT4(in.mMin.x, in.mMin.y, in.mMin.z, 1.0f)), XMLoadFloat4x4(&mat));
	XMVECTOR B = XMVector4Transform(XMLoadFloat4(&XMFLOAT4(in.mMin.x, in.mMin.y, in.mMax.z, 1.0f)), XMLoadFloat4x4(&mat));
	XMVECTOR C = XMVector4Transform(XMLoadFloat4(&XMFLOAT4(in.mMin.x, in.mMax.y, in.mMin.z, 1.0f)), XMLoadFloat4x4(&mat));
	XMVECTOR D = XMVector4Transform(XMLoadFloat4(&XMFLOAT4(in.mMin.x, in.mMax.y, in.mMax.z, 1.0f)), XMLoadFloat4x4(&mat));
	XMVECTOR E = XMVector4Transform(XMLoadFloat4(&XMFLOAT4(in.mMax.x, in.mMin.y, in.mMin.z, 1.0f)), XMLoadFloat4x4(&mat));
	XMVECTOR F = XMVector4Transform(XMLoadFloat4(&XMFLOAT4(in.mMax.x, in.mMin.y, in.mMax.z, 1.0f)), XMLoadFloat4x4(&mat));
	XMVECTOR G = XMVector4Transform(XMLoadFloat4(&XMFLOAT4(in.mMax.x, in.mMax.y, in.mMin.z, 1.0f)), XMLoadFloat4x4(&mat));
	XMVECTOR H = XMVector4Transform(XMLoadFloat4(&XMFLOAT4(in.mMax.x, in.mMax.y, in.mMax.z, 1.0f)), XMLoadFloat4x4(&mat));
	XMStoreFloat3(&out.mMin, XMVectorMin(A, XMVectorMin(B, XMVectorMin(C, XMVectorMin(D, XMVectorMin(E, XMVectorMin(F, XMVectorMin(G, H))))))));
	XMStoreFloat3(&out.mMax, XMVectorMax(A, XMVectorMax(B, XMVectorMax(C, XMVectorMax(D, XMVectorMax(E, XMVectorMax(F, XMVectorMax(G, H))))))));
}

void PathTracer::UpdateBVH(Scene& scene)
{
	sTriangleModelBVHs.clear();
	sMeshWorldBVHs.clear();

	// process triangle BVH
	for (int i = 0; i < sMeshes.size(); i++)
	{
		MeshPT& mesh = sMeshes[i];
		vector<TrianglePT> triangles(sTriangles.begin() + mesh.mTriangleIndexLocalToGlobalOffset, sTriangles.begin() + mesh.mTriangleIndexLocalToGlobalOffset + mesh.mTriangleCount);
		mesh.mTriangleBvhIndexLocalToGlobalOffset = sTriangleModelBVHs.size();
		mesh.mRootTriangleBvhIndexLocal = BuildBVH(triangles, sTriangleModelBVHs, i);
	}

	// process mesh BVH
	vector<MeshProxy> meshProxies;
	for (int i = 0; i < sMeshes.size(); i++)
	{
		MeshProxy meshProxy;
		XMFLOAT3 centroid;
		TransformAABB(sMeshes[i].mModel, sTriangleModelBVHs[sMeshes[i].mRootTriangleBvhIndexLocal + sMeshes[i].mTriangleBvhIndexLocalToGlobalOffset].mAABB, meshProxy.mWorldAABB);
		CentroidOfAABB(meshProxy.mWorldAABB, centroid);
		meshProxy.mMeshOriginalIndex = i;
		meshProxy.mMortonCode = Mesh::GenerateMortonCode(centroid);
		meshProxies.push_back(meshProxy);
	}
	sMeshBvhRootIndexGlobal = BuildBVH(meshProxies, sMeshWorldBVHs);

	// correct mesh index for mesh BVH
	for (int i = 0; i < sMeshWorldBVHs.size(); i++)
	{
		if (sMeshWorldBVHs[i].mLeftIsLeaf)
			sMeshWorldBVHs[i].mLeftIndexLocal = meshProxies[sMeshWorldBVHs[i].mLeftIndexLocal].mMeshOriginalIndex;
		if (sMeshWorldBVHs[i].mRightIsLeaf)
			sMeshWorldBVHs[i].mRightIndexLocal = meshProxies[sMeshWorldBVHs[i].mRightIndexLocal].mMeshOriginalIndex;
	}

	scene.mSceneUniform.mPathTracerMeshBvhCount = sMeshWorldBVHs.size();
	scene.mSceneUniform.mPathTracerTriangleBvhCount = sTriangleModelBVHs.size();
	scene.mSceneUniform.mPathTracerMeshBvhRootIndex = sMeshBvhRootIndexGlobal;
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
		displayf("leaf index: %ld, ", leftLeafGlobalIndex);
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
		displayf("leaf index: %ld, ", rightLeafGlobalIndex);
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

void PathTracer::PreparePathTracer(ID3D12GraphicsCommandList* commandList, Scene& scene)
{
	sPathTracerPass.mPassUniform.mTriangleCountPT = sTriangles.size();
	sPathTracerPass.mPassUniform.mMeshCountPT = sMeshes.size();
	sPathTracerPass.mPassUniform.mLightCountPT = sLightData.size();

	sPathTracerPass.UpdateAllUniformBuffers();

	sTriangleBuffer.SetBufferData(sTriangles.data(), sizeof(TrianglePT) * sTriangles.size());
	sMeshBuffer.SetBufferData(sMeshes.data(), sizeof(MeshPT) * sMeshCountMax);
	sTriangleBvhBuffer.SetBufferData(sTriangleModelBVHs.data(), sizeof(BVH) * sTriangleModelBVHs.size());
	sMeshBvhBuffer.SetBufferData(sMeshWorldBVHs.data(), sizeof(BVH) * sMeshWorldBVHs.size());
	sLightDataBuffer.SetBufferData(sLightData.data(), sizeof(LightData) * sLightData.size());
	sRayBuffer.RecordSetBufferData(commandList, nullptr, sizeof(Ray) * sBackbufferWidth * sBackbufferHeight);
	sDebugRayBuffer.RecordSetBufferData(commandList, nullptr, sizeof(Ray) * PATH_TRACER_MAX_DEPTH_MAX);
}

void PathTracer::RunPathTracer(ID3D12GraphicsCommandList* commandList)
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

void PathTracer::CopyDepthBuffer(ID3D12GraphicsCommandList* commandList)
{
	sDepthbufferWritePT.MakeReadyToRead(commandList);
	sDepthbufferRenderPT.MakeReadyToRender(commandList);
	gRenderer.RecordGraphicsPass(sPathTracerCopyDepthPass, commandList, false, true);
}

void PathTracer::DebugDraw(ID3D12GraphicsCommandList* commandList)
{
	// always render debug passes for easier renderdoc capture
	PathTracer::sDebugRayBuffer.MakeReadyToRead(commandList);
	PathTracer::sDebugBackbufferPT.MakeReadyToRender(commandList);
	PathTracer::sDepthbufferRenderPT.MakeReadyToRender(commandList);
	gRenderer.RecordGraphicsPassInstanced(PathTracer::sPathTracerDebugLinePass, commandList, PATH_TRACER_MAX_DEPTH_MAX * PATH_TRACER_DEBUG_LINE_COUNT_PER_RAY, true, false, false);
	gRenderer.RecordGraphicsPassInstanced(PathTracer::sPathTracerDebugCubePass, commandList, sTriangleModelBVHs.size() + sMeshWorldBVHs.size(), false, false, false);
	gRenderer.RecordGraphicsPass(PathTracer::sPathTracerDebugFullscreenPass, commandList, false, false, false);
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
		lcp += __lzcnt64((u64)left ^ (u64)right);
	}
	return lcp;
}

inline i64 RoundUpDivide(i64 dividend, i64 divisor)
{
	return dividend / divisor + (dividend % divisor != 0);
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

inline void InitAABB(const MeshProxy& meshProxyIn, AABB& aabbOut)
{
	aabbOut = meshProxyIn.mWorldAABB;
}

template<class T_LeafNode>
i64 PathTracer::BuildBVH(vector<T_LeafNode>& leafLocal, vector<BVH>& bvhGlobal, u32 meshIndex)
{
	// algorithm based on https://developer.nvidia.com/blog/parallelforall/wp-content/uploads/2012/11/karras2012hpg_paper.pdf
	fatalAssertf(leafLocal.size() > 1 && leafLocal.size() < MAX_UINT32, "too many or too few triangles");
	
	// local bvh
	vector<BVH> triangleBVHsLocal;

	// 1. sort
	// TODO: change to radix sort if run on GPU
	std::sort(leafLocal.begin(), leafLocal.end(), [](const T_LeafNode& a, const T_LeafNode& b)->bool { return a.mMortonCode > b.mMortonCode; });

	// 2. build tree
	triangleBVHsLocal.resize(leafLocal.size() - 1);
	for (int i = 0; i < triangleBVHsLocal.size(); i++)
		InitBVH(triangleBVHsLocal[i], meshIndex);
	for (i64 i = 0; i < triangleBVHsLocal.size(); i++)
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
		fatalAssert(split >= 0 && split < triangleBVHsLocal.size());
		// Output child pointers
		const i64 leftIndex = split;
		const i64 rightIndex = split + 1;
		triangleBVHsLocal[i].mLeftIndexLocal = leftIndex;
		triangleBVHsLocal[i].mLeftIsLeaf = leftIndex == MIN(i, j);
		triangleBVHsLocal[i].mRightIndexLocal = rightIndex;
		triangleBVHsLocal[i].mRightIsLeaf = rightIndex == MAX(i, j);
		if (triangleBVHsLocal[i].mLeftIsLeaf) // left child is leaf
			leafLocal[leftIndex].mBvhIndexLocal = i;
		else
			triangleBVHsLocal[leftIndex].mParentIndexLocal = i;
		if (triangleBVHsLocal[i].mRightIsLeaf) // right child is leaf
			leafLocal[rightIndex].mBvhIndexLocal = i;
		else
			triangleBVHsLocal[rightIndex].mParentIndexLocal = i;
	}

	// 3. update bvh
	i64 rootBvhIndexLocal = -1;
	i64 maxHeight = -1;
	for (i64 i = 0; i < leafLocal.size(); i++)
	{
		i64 current = leafLocal[i].mBvhIndexLocal;
		fatalAssert(current >= 0 && current < triangleBVHsLocal.size());
		i64 height = 0;
		while (current != INVALID_UINT32)
		{
			height++;
			// TODO: use atomic operation (InterlockedAdd) if this loop is run in parallel
			u32 original = triangleBVHsLocal[current].mVisited++;

			if (original < 1) // if this is the first time trying to visit current node, stop
				break;
			else
			{
				i64 parent = triangleBVHsLocal[current].mParentIndexLocal;
				if (parent == INVALID_UINT32)
				{
					if (rootBvhIndexLocal != -1)
						fatalAssert(rootBvhIndexLocal == current);
					rootBvhIndexLocal = current;
				}
				const u32 left = triangleBVHsLocal[current].mLeftIndexLocal;
				const u32 right = triangleBVHsLocal[current].mRightIndexLocal;
				AABB lAABB;
				AABB rAABB;
				if (triangleBVHsLocal[current].mLeftIsLeaf)
					InitAABB(leafLocal[left], lAABB);
				else
					lAABB = triangleBVHsLocal[left].mAABB;
				if (triangleBVHsLocal[current].mRightIsLeaf)
					InitAABB(leafLocal[right], rAABB);
				else
					rAABB = triangleBVHsLocal[right].mAABB;
				MergeAABB(lAABB, rAABB, triangleBVHsLocal[current].mAABB);
				current = parent;
			}
		}
		if (height > maxHeight)
			maxHeight = height;
	}
	displayfln("build bvh tree height = %ld", maxHeight);
	fatalAssert(rootBvhIndexLocal >= 0 && rootBvhIndexLocal < triangleBVHsLocal.size());
	fatalAssert(rootBvhIndexLocal < INVALID_UINT32);
	bvhGlobal.insert(bvhGlobal.end(), triangleBVHsLocal.begin(), triangleBVHsLocal.end());
	return rootBvhIndexLocal;
}
