#include "Scene.h"
#include "Pass.h"
#include "Texture.h"

Scene::Scene(const wstring& debugName) : 
	mRenderer(nullptr), mDebugName(debugName)
{
}

Scene::~Scene()
{
	Release();
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

int Scene::GetTextureCount()
{
	return mTextures.size();
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
			&CD3DX12_RESOURCE_DESC::Buffer(sizeof(SceneUniform)), // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
			Renderer::TranslateResourceLayout(ResourceLayout::UPLOAD), // will be data that is read from so we keep it in the generic read state
			nullptr, // we do not have use an optimized clear value for constant buffers
			IID_PPV_ARGS(&mUniformBuffers[i]));

		fatalAssertf(SUCCEEDED(hr), "create scene uniform buffer failed");
		wstring name(L": scene uniform buffer ");
		mUniformBuffers[i]->SetName((mDebugName + name + to_wstring(i)).data());
	}
}

void Scene::UpdateUniformBuffer(int frameIndex)
{
	CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU. (so end is less than or equal to begin)
	void* cpuAddress;
	mUniformBuffers[frameIndex]->Map(0, &readRange, &cpuAddress);
	memcpy(cpuAddress, &mSceneUniform, sizeof(mSceneUniform));
	mUniformBuffers[frameIndex]->Unmap(0, &readRange);
}

void Scene::Release()
{
	for (auto uniformBuffer : mUniformBuffers)
		SAFE_RELEASE(uniformBuffer);
}

D3D12_GPU_VIRTUAL_ADDRESS Scene::GetUniformBufferGpuAddress(int frameIndex)
{
	return mUniformBuffers[frameIndex]->GetGPUVirtualAddress();
}
