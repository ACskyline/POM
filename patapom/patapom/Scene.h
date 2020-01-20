#pragma once

#include "Pass.h"
#include "Shader.h"

class Renderer;

class Scene
{
public:
	struct SceneUniform
	{
		uint32_t sceneMode;
	};

	Scene();
	~Scene();
	void AddPass(Pass* pass);
	void AddTexture(Texture* texture);
	
	int GetTextureCount();
	D3D12_GPU_VIRTUAL_ADDRESS GetUniformBufferGpuAddress(int frameIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE GetCbvSrvUavDescriptorHeapTableHandle(int frameIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerDescriptorHeapTableHandle(int frameIndex);

	void InitScene(
		Renderer* renderer, 
		int frameCount,
		DescriptorHeap& cbvSrvUavDescriptorHeap,
		DescriptorHeap& samplerDescriptorHeap);
	void CreateUniformBuffer(int frameCount);
	void UpdateUniformBuffer(int frameIndex);
	void Release();
	
	SceneUniform mSceneUniform;

private:
	Renderer* mRenderer;
	vector<Pass*> mPasses;
	vector<Texture*> mTextures;

	// one for each frame
	// TODO: hide API specific implementation in Renderer
	vector<ID3D12Resource*> mUniformBuffers;
	vector<D3D12_GPU_DESCRIPTOR_HANDLE> mCbvSrvUavDescriptorHeapTableHandles;
	vector<D3D12_GPU_DESCRIPTOR_HANDLE> mSamplerDescriptorHeapTableHandles;
};
