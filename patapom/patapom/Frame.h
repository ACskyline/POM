#pragma once

#include "GlobalInclude.h"
#include "Renderer.h"

class Texture;

class Frame
{
public:
	Frame();
	~Frame();

	void AddTexture(Texture* texture);

	vector<Texture*>& GetTextureVec();
	int GetTextureCount();
	D3D12_GPU_VIRTUAL_ADDRESS GetUniformBufferGpuAddress();
	D3D12_GPU_DESCRIPTOR_HANDLE GetCbvSrvUavDescriptorHeapTableHandle();
	D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerDescriptorHeapTableHandle();

	void InitFrame(
		int index,
		Renderer* renderer,
		DescriptorHeap& cbvSrvUavDescriptorHeap,
		DescriptorHeap& samplerDescriptorHeap,
		DescriptorHeap& rtvDescriptorHeap,
		DescriptorHeap& dsvDescriptorHeap);
	void CreateUniformBuffer();
	void UpdateUniformBuffer();
	void Release(bool checkOnly = false);

	FrameUniform mFrameUniform;

private:
	int mIndex;
	wstring mDebugName;
	Renderer* mRenderer;
	vector<Texture*> mTextureVec;

	// TODO: Hide API specific implementation in Renderer
	ID3D12Resource* mUniformBuffer; 
	D3D12_GPU_DESCRIPTOR_HANDLE mCbvSrvUavDescriptorHeapTableHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE mSamplerDescriptorHeapTableHandle;
};
