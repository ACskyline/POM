#include "Buffer.h"

Buffer::Buffer(
	const wstring& debugName,
	u32 elementSizeInByte,
	u32 elementCount) : 
	mElementSizeInByte(elementSizeInByte),
	mElementCount(elementCount),
	ShaderResource(debugName, ShaderResource::Type::BUFFER)
{
	mSrv.mType = View::Type::SRV;
}

Buffer::~Buffer()
{
	Buffer::Release(true);
}

ID3D12Resource* Buffer::GetBuffer()
{
	return mBuffer;
}

D3D12_SHADER_RESOURCE_VIEW_DESC Buffer::GetSrvDesc() const
{
	char output[256] = { 0 };
	sprintf_s(output, "buffer %ws is not initialized!", mDebugName.c_str());
	fatalAssertf(mInitialized, "%s", output);
	fatalAssertf(mSrv.mType == View::Type::SRV, "srv type wrong");
	return mSrv.mSrvDesc;
}

void Buffer::InitBuffer(Renderer* renderer)
{
	mRenderer = renderer;
	CreateBuffer();
	SetBufferData(nullptr, mElementCount * mElementSizeInByte);
	CreateView();
	mInitialized = true;
}

void Buffer::Release(bool checkOnly)
{
	SAFE_RELEASE(mBuffer, checkOnly);
}

void Buffer::CreateBuffer()
{
	HRESULT hr = mRenderer->mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(mElementSizeInByte * mElementCount),
		Renderer::TranslateResourceLayout(ResourceLayout::UPLOAD), // will be data that is read from so we keep it in the generic read state
		nullptr, // we do not use an optimized clear value for constant buffers
		IID_PPV_ARGS(&mBuffer));

	fatalAssertf(!CheckError(hr, mRenderer->mDevice), "create buffer failed");
	wstring name(L": buffer ");
	mBuffer->SetName((mDebugName + name).c_str());
}

void Buffer::CreateView()
{
	// D3D12 binds the view to a descriptor heap as soon as it is created, so this function is not actually creating the API view
	// what we are creating here is the view defined by ourselves
	mSrv.mSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	mSrv.mSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	mSrv.mSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	mSrv.mSrvDesc.Buffer.FirstElement = 0;
	mSrv.mSrvDesc.Buffer.NumElements = mElementCount;
	mSrv.mSrvDesc.Buffer.StructureByteStride = mElementSizeInByte;
	mSrv.mSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
}

// pass in nullptr to zero out the memory
void Buffer::SetBufferData(void* data, int sizeInByte)
{
	CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU. (so end is less than or equal to begin)
	void* cpuAddress;
	mBuffer->Map(0, &readRange, &cpuAddress);
	if (!data)
		memset(cpuAddress, 0, sizeInByte);
	else
		memcpy(cpuAddress, data, sizeInByte);
	mBuffer->Unmap(0, &readRange);
}

WriteBuffer::WriteBuffer(const wstring& debugName, u32 elementSizeInByte, u32 elementCount)
	: Buffer(debugName, elementSizeInByte, elementCount)
{
	mUav.mType = View::Type::UAV;
}

WriteBuffer::~WriteBuffer()
{
	
}

D3D12_UNORDERED_ACCESS_VIEW_DESC WriteBuffer::GetUavDesc() const
{
	char output[256] = { 0 };
	sprintf_s(output, "buffer %ws is not initialized!", mDebugName.c_str());
	fatalAssertf(mInitialized, "%s", output);
	fatalAssertf(mUav.mType == View::Type::UAV, "uav type wrong");
	return mUav.mUavDesc;
}

void WriteBuffer::SetBufferData(void* data, int sizeInByte)
{
	mRenderer->UploadDataToBuffer(data, sizeInByte, sizeInByte, sizeInByte, mBuffer);
}

void WriteBuffer::Release(bool checkOnly)
{
	Buffer::Release(checkOnly);
}

void WriteBuffer::CreateBuffer()
{
	HRESULT hr = mRenderer->mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(mElementSizeInByte * mElementCount, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		Renderer::TranslateResourceLayout(ResourceLayout::SHADER_WRITE), // will be data that is read from so we keep it in the generic read state
		nullptr, // we do not use an optimized clear value for constant buffers
		IID_PPV_ARGS(&mBuffer));

	fatalAssertf(!CheckError(hr, mRenderer->mDevice), "create buffer failed");
	wstring name(L": buffer ");
	mBuffer->SetName((mDebugName + name).c_str());
}

void WriteBuffer::CreateView()
{
	CreateSrv();
	CreateUav();
}

void WriteBuffer::CreateSrv()
{
	mSrv.mSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	mSrv.mSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	mSrv.mSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	mSrv.mSrvDesc.Buffer.FirstElement = 0;
	mSrv.mSrvDesc.Buffer.NumElements = mElementCount;
	mSrv.mSrvDesc.Buffer.StructureByteStride = mElementSizeInByte;
	mSrv.mSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
}

void WriteBuffer::CreateUav()
{
	mUav.mUavDesc.Format = DXGI_FORMAT_UNKNOWN;
	mUav.mUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	mUav.mUavDesc.Buffer.FirstElement = 0;
	mUav.mUavDesc.Buffer.NumElements = mElementCount;
	mUav.mUavDesc.Buffer.StructureByteStride = mElementSizeInByte;
	mUav.mUavDesc.Buffer.CounterOffsetInBytes = 0;
	mUav.mUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
}
