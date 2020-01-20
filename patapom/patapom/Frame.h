#pragma once

#include "Texture.h"

class Frame
{
public:
	struct FrameUniform
	{
		uint32_t frameIndex;
	};

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
	void Release();

private:
	int mIndex;
	wstring mDebugName;
	Renderer* mRenderer;
	FrameUniform mFrameUniform;
	vector<Texture*> mTextureVec;

	// TODO: hide API specific implementation in Renderer
	ID3D12Resource* mUniformBuffer; 
	D3D12_GPU_DESCRIPTOR_HANDLE mCbvSrvUavDescriptorHeapTableHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE mSamplerDescriptorHeapTableHandle;
};
