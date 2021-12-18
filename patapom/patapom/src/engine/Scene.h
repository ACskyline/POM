#pragma once

#include "GlobalInclude.h"

class Pass;
class Texture;
class Light;
class DescriptorHeap;

class Scene
{
public:
	Scene(const string& debugName = "unnamed scene");
	~Scene();
	void AddPass(Pass* pass);
	void AddTexture(Texture* texture);
	void AddSampler(Sampler* sampler);
	void AddLight(Light* light);
	void SetUniformDirty(int frameIndex = -1);
	void ResetUniformDirty(int frameIndex = -1);

	vector<Pass*>& GetPasses();
	vector<Light*>& GetLights();
	int GetSamplerCount();
	int GetTextureCount();
	int GetLightCount();
	D3D12_GPU_VIRTUAL_ADDRESS GetUniformBufferGpuAddress(int frameIndex);
	DescriptorHeap::Handle GetCbvSrvUavDescriptorHeapTableHandle(int frameIndex);
	DescriptorHeap::Handle GetSamplerDescriptorHeapTableHandle(int frameIndex);
	bool IsUniformDirty(int frameIndex);

	void InitScene(
		Renderer* renderer, 
		int frameCount,
		DescriptorHeap& cbvSrvUavDescriptorHeap,
		DescriptorHeap& samplerDescriptorHeap);
	void CreateUniformBuffer(int frameCount);
	void UpdateUniformBuffer(int frameIndex);
	void UpdateUniform();
	void Release(bool checkOnly = false);
	
	SceneUniform mSceneUniform;

private:
	string mDebugName;
	Renderer* mRenderer;
	vector<Pass*> mPasses;
	vector<Texture*> mTextures;
	vector<Sampler*> mSamplers;
	vector<Light*> mLights;
	u8 mUniformDirtyFlag;

	// one for each frame
	// TODO: hide API specific implementation in Renderer
	vector<ID3D12Resource*> mUniformBuffers;
	vector<ID3D12Resource*> mLightBuffers;
	vector<DescriptorHeap::Handle> mCbvSrvUavDescriptorHeapTableHandles;
	vector<DescriptorHeap::Handle> mSamplerDescriptorHeapTableHandles;
};
