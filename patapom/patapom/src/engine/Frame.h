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
	DescriptorHeap::Handle GetCbvSrvUavDescriptorHeapTableHandle();

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
	string mDebugName;
	Renderer* mRenderer;
	vector<Texture*> mTextureVec;

	// TODO: Hide API specific implementation in Renderer
	ID3D12Resource* mUniformBuffer; 
	DescriptorHeap::Handle mCbvSrvUavDescriptorHeapTableHandle;
};
