#include "Scene.h"
#include "Pass.h"
#include "Texture.h"
#include "Light.h"

Scene::Scene(const wstring& debugName) : 
	mRenderer(nullptr), mDebugName(debugName)
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

vector<Pass*>& Scene::GetPasses()
{
	return mPasses;
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
	mSceneUniform.lightCount = mLights.size();
	for(int i = 0;i<mLights.size() && i<MAX_LIGHTS_PER_SCENE;i++)
	{
		mSceneUniform.lights[i].view = mLights[i]->GetViewMatrix();
		mSceneUniform.lights[i].viewInv = mLights[i]->GetViewInvMatrix();
		mSceneUniform.lights[i].proj = mLights[i]->GetProjMatrix();
		mSceneUniform.lights[i].projInv = mLights[i]->GetProjInvMatrix();
		mSceneUniform.lights[i].color = mLights[i]->GetColor();
		mSceneUniform.lights[i].position = mLights[i]->GetPosition();
		mSceneUniform.lights[i].nearClipPlane = mLights[i]->GetNearClipPlane();
		mSceneUniform.lights[i].farClipPlane = mLights[i]->GetFarClipPlane();
		mSceneUniform.lights[i].textureIndex = mLights[i]->GetTextureIndex();
	}

	CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU. (so end is less than or equal to begin)
	void* cpuAddress;
	mUniformBuffers[frameIndex]->Map(0, &readRange, &cpuAddress);
	memcpy(cpuAddress, &mSceneUniform, sizeof(mSceneUniform));
	mUniformBuffers[frameIndex]->Unmap(0, &readRange);
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
