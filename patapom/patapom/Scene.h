#pragma once

#include "GlobalInclude.h"

class Pass;
class Texture;
class Light;
class DescriptorHeap;

class Scene
{
public:
	Scene(const wstring& debugName = L"unnamed scene");
	~Scene();
	void AddPass(Pass* pass);
	void AddTexture(Texture* texture);
	void AddLight(Light* light);
	void SetUniformDirty(int frameIndex = -1);
	void ResetUniformDirty(int frameIndex = -1);

	vector<Pass*>& GetPasses();
	vector<Light*>& GetLights();
	int GetTextureCount();
	int GetLightCount();
	D3D12_GPU_VIRTUAL_ADDRESS GetUniformBufferGpuAddress(int frameIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE GetCbvSrvUavDescriptorHeapTableHandle(int frameIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerDescriptorHeapTableHandle(int frameIndex);
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
	wstring mDebugName;
	Renderer* mRenderer;
	vector<Pass*> mPasses;
	vector<Texture*> mTextures;
	vector<Light*> mLights;
	u8 mUniformDirtyFlag;

	// one for each frame
	// TODO: hide API specific implementation in Renderer
	vector<ID3D12Resource*> mUniformBuffers;
	vector<ID3D12Resource*> mLightBuffers;
	vector<D3D12_GPU_DESCRIPTOR_HANDLE> mCbvSrvUavDescriptorHeapTableHandles;
	vector<D3D12_GPU_DESCRIPTOR_HANDLE> mSamplerDescriptorHeapTableHandles;
};
