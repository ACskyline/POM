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
	CD3DX12_RANGE readRange(0, 0); // "It is valid to specify the CPU didn't write any data by passing a range where End is less than or equal to Begin."
	void* cpuAddress;
	mBuffer->Map(0, &readRange, &cpuAddress); // "A null pointer indicates the entire subresource might be read by the CPU."
	if (!data)
		memset(cpuAddress, 0, sizeInByte);
	else
		memcpy(cpuAddress, data, sizeInByte);
	mBuffer->Unmap(0, nullptr); // "A null pointer indicates the entire subresource might have been modified by the CPU."
}

WriteBuffer::WriteBuffer(const wstring& debugName, u32 elementSizeInByte, u32 elementCount)
	: Buffer(debugName, elementSizeInByte, elementCount), mLayout(ResourceLayout::SHADER_WRITE)
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

void WriteBuffer::RecordSetBufferData(ID3D12GraphicsCommandList* commandList, void* data, int sizeInByte)
{
	SetBufferDataInternal(commandList, data, sizeInByte);
}

void WriteBuffer::SetBufferData(void* data, int sizeInByte)
{
	ID3D12GraphicsCommandList* commandList = mRenderer->BeginSingleTimeCommands();
	SetBufferDataInternal(commandList, data, sizeInByte);
	mRenderer->EndSingleTimeCommands(commandList);
}

void WriteBuffer::Release(bool checkOnly)
{
	Buffer::Release(checkOnly);
	SAFE_RELEASE(mUploadBuffer, checkOnly);
}

bool WriteBuffer::TransitionLayout(ID3D12GraphicsCommandList* commandList, ResourceLayout newLayout)
{
	bool result = mRenderer->RecordTransition(commandList, mBuffer, 0, mLayout, newLayout);
	if (result)	
		mLayout = newLayout;
	return result;
}

void WriteBuffer::MakeReadyToRead(ID3D12GraphicsCommandList* commandList)
{
	TransitionLayout(commandList, ResourceLayout::SHADER_READ);
}

void WriteBuffer::MakeReadyToWrite(ID3D12GraphicsCommandList* commandList)
{
	TransitionLayout(commandList, ResourceLayout::SHADER_WRITE);
}

void WriteBuffer::CreateBuffer()
{
	HRESULT hr = mRenderer->mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(mElementSizeInByte * mElementCount, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		Renderer::TranslateResourceLayout(mLayout), // will be data that is read from so we keep it in the generic read state
		nullptr, // we do not use an optimized clear value for constant buffers
		IID_PPV_ARGS(&mBuffer));

	fatalAssertf(!CheckError(hr, mRenderer->mDevice), "create buffer failed");
	wstring name(L": buffer ");
	mBuffer->SetName((mDebugName + name).c_str());

	hr = mRenderer->mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(mElementSizeInByte * mElementCount), // resource description for a buffer (storing the image data in this heap just to copy to the default heap)
		Renderer::TranslateResourceLayout(ResourceLayout::UPLOAD), // we will copy the contents from this heap to the default heap above
		nullptr,
		IID_PPV_ARGS(&mUploadBuffer));

	fatalAssert(!CheckError(hr, mRenderer->mDevice));
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
	mSrv.mSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
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

void WriteBuffer::SetBufferDataInternal(ID3D12GraphicsCommandList* commandList, void* data, int sizeInByte)
{
	// "Map and Unmap can not be called on a resource associated with 
	// a heap that has the CPU page properties of D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE.
	// Heaps of the type D3D12_HEAP_TYPE_DEFAULT should be assumed to have these properties."

	// "A buffer cannot be created on a D3D12_HEAP_TYPE_UPLOAD or D3D12_HEAP_TYPE_READBACK heap 
	// when either D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET or D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS is used."

	// UAV can't use UPLOAD HEAP, so we can't simply map it like a read only buffer
	TransitionLayout(commandList, ResourceLayout::COPY_DST);
	char* zero = data ? nullptr : new char[sizeInByte];
	mRenderer->RecordUploadDataToBuffer(commandList, data ? data : zero, sizeInByte, sizeInByte, mBuffer, mUploadBuffer);
	if (zero)
		delete[] zero;
	TransitionLayout(commandList, ResourceLayout::SHADER_WRITE);
}
