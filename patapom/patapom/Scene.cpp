#include "Scene.h"
#include "Pass.h"
#include "Texture.h"
#include "Light.h"
#include "PathTracer.h"

Scene::Scene(const wstring& debugName) : 
	mRenderer(nullptr), mDebugName(debugName), mUniformDirtyFlag(0)
{
}

Scene::~Scene()
{
	Release(true);
}

void Scene::AddPass(Pass* pass)
{
	mPasses.push_back(pass);
	pass->SetScene(this);
}

void Scene::AddTexture(Texture* texture)
{
	mTextures.push_back(texture);
}

void Scene::AddLight(Light* light)
{
	light->SetScene(this);
	mLights.push_back(light);
	if (light->GetRenderTexture() != nullptr)
	{
		light->SetTextureIndex(static_cast<int32_t>(mTextures.size()));
		mTextures.push_back(light->GetRenderTexture());
	}
}

void Scene::SetUniformDirty(int frameIndex)
{
	if (frameIndex < 0)
		mUniformDirtyFlag = ~0;
	else
		mUniformDirtyFlag |= 1 << frameIndex;
}

void Scene::ResetUniformDirty(int frameIndex)
{
	if (frameIndex < 0)
		mUniformDirtyFlag = 0;
	else
		mUniformDirtyFlag &= ~(1 << frameIndex);
}

vector<Pass*>& Scene::GetPasses()
{
	return mPasses;
}

vector<Light*>& Scene::GetLights()
{
	return mLights;
}

int Scene::GetTextureCount()
{
	return mTextures.size();
}

int Scene::GetLightCount()
{
	return mLights.size();
}

D3D12_GPU_DESCRIPTOR_HANDLE Scene::GetCbvSrvUavDescriptorHeapTableHandle(int frameIndex)
{
	return mCbvSrvUavDescriptorHeapTableHandles[frameIndex];
}

D3D12_GPU_DESCRIPTOR_HANDLE Scene::GetSamplerDescriptorHeapTableHandle(int frameIndex)
{
	return mSamplerDescriptorHeapTableHandles[frameIndex];
}

bool Scene::IsUniformDirty(int frameIndex)
{
	if (frameIndex < 0)
		return mUniformDirtyFlag;
	else
		return mUniformDirtyFlag & (1 << frameIndex);
}

void Scene::InitScene(
	Renderer* renderer,
	int frameCount,
	DescriptorHeap& cbvSrvUavDescriptorHeap,
	DescriptorHeap& samplerDescriptorHeap)
{
	mRenderer = renderer;

	// create uniform buffers 
	CreateUniformBuffer(frameCount);
	for (int i = 0; i < frameCount; i++)
		UpdateUniformBuffer(i);

	// bind textures to heaps
	mCbvSrvUavDescriptorHeapTableHandles.resize(frameCount);
	mSamplerDescriptorHeapTableHandles.resize(frameCount);
	for (int i = 0; i < frameCount; i++)
	{
		// scene texture table
		mCbvSrvUavDescriptorHeapTableHandles[i] = cbvSrvUavDescriptorHeap.GetCurrentFreeGpuAddress();
		for (auto texture : mTextures)
			cbvSrvUavDescriptorHeap.AllocateSrv(texture->GetTextureBuffer(), texture->GetSrvDesc(), 1);
		
		// scene sampler table
		mSamplerDescriptorHeapTableHandles[i] = samplerDescriptorHeap.GetCurrentFreeGpuAddress();
		for (auto texture : mTextures)
			samplerDescriptorHeap.AllocateSampler(texture->GetSamplerDesc(), 1);
	}
}

void Scene::CreateUniformBuffer(int frameCount)
{
	mUniformBuffers.resize(frameCount);
	for (int i = 0; i < frameCount; i++)
	{
		HRESULT hr = mRenderer->mDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // this heap will be used to upload the constant buffer data
			D3D12_HEAP_FLAG_NONE, // no flags
			&CD3DX12_RESOURCE_DESC::Buffer(sizeof(SceneUniform)),
			Renderer::TranslateResourceLayout(ResourceLayout::UPLOAD), // will be data that is read from so we keep it in the generic read state
			nullptr, // we do not use an optimized clear value for constant buffers
			IID_PPV_ARGS(&mUniformBuffers[i]));

		fatalAssertf(SUCCEEDED(hr), "create scene uniform buffer failed");
		wstring name(L": scene uniform buffer ");
		mUniformBuffers[i]->SetName((mDebugName + name + to_wstring(i)).c_str());
	}
}

void Scene::UpdateUniformBuffer(int frameIndex)
{
	UpdateUniform();
	CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU. (so end is less than or equal to begin)
	void* cpuAddress;
	mUniformBuffers[frameIndex]->Map(0, &readRange, &cpuAddress);
	memcpy(cpuAddress, &mSceneUniform, sizeof(mSceneUniform));
	mUniformBuffers[frameIndex]->Unmap(0, nullptr);
	ResetUniformDirty(frameIndex);
}

void Scene::UpdateUniform()
{
	mSceneUniform.mPathTracerTileCount = mSceneUniform.mPathTracerTileCountX * mSceneUniform.mPathTracerTileCountY;
	mSceneUniform.mPathTracerThreadGroupPerTileX = ROUNDUP_DIVISION(PathTracer::sThreadGroupCountX, mSceneUniform.mPathTracerTileCountX),
	mSceneUniform.mPathTracerThreadGroupPerTileY = ROUNDUP_DIVISION(PathTracer::sThreadGroupCountY, mSceneUniform.mPathTracerTileCountY),
	mSceneUniform.mLightCount = mLights.size();
	for (int i = 0; i < mLights.size() && i < LIGHT_COUNT_PER_SCENE_MAX; i++)
		mSceneUniform.mLightData[i] = mLights[i]->CreateLightData();
}

void Scene::Release(bool checkOnly)
{
	for (auto& uniformBuffer : mUniformBuffers)
		SAFE_RELEASE(uniformBuffer, checkOnly);
}

D3D12_GPU_VIRTUAL_ADDRESS Scene::GetUniformBufferGpuAddress(int frameIndex)
{
	return mUniformBuffers[frameIndex]->GetGPUVirtualAddress();
}
