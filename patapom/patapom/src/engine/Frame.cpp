#include "Frame.h"
#include "Texture.h"

Frame::Frame()
{
}

Frame::~Frame()
{
	Release(true);
}

void Frame::AddTexture(Texture* texture)
{
	mTextureVec.push_back(texture);
}

vector<Texture*>& Frame::GetTextureVec()
{
	return mTextureVec;
}

int Frame::GetTextureCount()
{
	return mTextureVec.size();
}

D3D12_GPU_VIRTUAL_ADDRESS Frame::GetUniformBufferGpuAddress()
{
	return mUniformBuffer->GetGPUVirtualAddress();
}

DescriptorHeap::Handle Frame::GetCbvSrvUavDescriptorHeapTableHandle()
{
	return mCbvSrvUavDescriptorHeapTableHandle;
}

void Frame::InitFrame(
	int index,
	Renderer* renderer,
	DescriptorHeap& cbvSrvUavDescriptorHeap,
	DescriptorHeap& samplerDescriptorHeap,
	DescriptorHeap& rtvDescriptorHeap,
	DescriptorHeap& dsvDescriptorHeap)
{
	mIndex = index;
	mRenderer = renderer;

	// constant buffer
	CreateUniformBuffer();
	UpdateUniformBuffer();
	
	// frame texture table
	mCbvSrvUavDescriptorHeapTableHandle = cbvSrvUavDescriptorHeap.GetCurrentFreeHandle();
	for (auto texture : mTextureVec)
		cbvSrvUavDescriptorHeap.AllocateSrv(texture->GetTextureBuffer(), texture->GetSrvDesc());
}

void Frame::CreateUniformBuffer()
{
	HRESULT hr = mRenderer->mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // this heap will be used to upload the constant buffer data
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(FrameUniform)),
		Renderer::TranslateResourceLayout(ResourceLayout::UPLOAD), // will be data that is read from so we keep it in the generic read state
		nullptr, // we do not use an optimized clear value for constant buffers
		IID_PPV_ARGS(&mUniformBuffer));
	fatalAssert(!CheckError(hr));
	mUniformBuffer->SetName(L"frame uniform buffer");
}

void Frame::UpdateUniformBuffer()
{
	CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU. (so end is less than or equal to begin)
	void* cpuAddress;
	mUniformBuffer->Map(0, &readRange, &cpuAddress);
	memcpy(cpuAddress, &mFrameUniform, sizeof(mFrameUniform));
	mUniformBuffer->Unmap(0, nullptr);
}

void Frame::Release(bool checkOnly)
{
	SAFE_RELEASE(mUniformBuffer, checkOnly);
}
