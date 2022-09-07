#include "Renderer.h"
#include "Store.h"
#include "Scene.h"
#include "Frame.h"
#include "Pass.h"
#include "Mesh.h"
#include "Camera.h"
#include "Shader.h"
#include "Texture.h"

int GetUniformSlot(RegisterSpace space, RegisterType type)
{
	// constant buffer is root parameter, SRV, sampler and UAV are stored in table
	//                 b0 CBV | t0 SRV | s0 sampler | u0 UAV |
	// space 0 scene   0      | 4      | 8          | 12     |
	// space 1 frame   1      | 5      | 9          | 13     |
	// space 2 pass    2      | 6      | 10         | 14     |
	// space 3 object  3      | 7      | 11         | 15     |

	return (int)space + (int)type * (int)RegisterSpace::COUNT;
}

Sampler gSamplerLinear = {
	Sampler::Filter::LINEAR,
	Sampler::Filter::LINEAR,
	Sampler::Filter::LINEAR,
	Sampler::AddressMode::WRAP,
	Sampler::AddressMode::WRAP,
	Sampler::AddressMode::WRAP,
	0.f,
	false,
	1.f,
	false,
	CompareOp::NEVER,
	0.f,
	1.f,
	{0.f, 0.f, 0.f, 0.f}
};

Sampler gSamplerPoint = {
	Sampler::Filter::POINT,
	Sampler::Filter::POINT,
	Sampler::Filter::POINT,
	Sampler::AddressMode::WRAP,
	Sampler::AddressMode::WRAP,
	Sampler::AddressMode::WRAP,
	0.f,
	false,
	1.f,
	false,
	CompareOp::NEVER,
	0.f,
	1.f,
	{0.f, 0.f, 0.f, 0.f}
};

Format gSwapchainColorBufferFormat = Format::R8G8B8A8_UNORM;
Format gSwapchainDepthStencilBufferFormat = Format::D24_UNORM_S8_UINT;
Shader gDeferredVS(Shader::ShaderType::VERTEX_SHADER, "vs_deferred");
Mesh gCube("cube", Mesh::MeshType::CUBE, XMFLOAT3(0, 0, 0), XMFLOAT3(0, 45, 0), XMFLOAT3(1, 1, 1));
Mesh gFullscreenTriangle("fullscreen_triangle", Mesh::MeshType::FULLSCREEN_TRIANGLE, XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1));
Camera gCameraDummy(XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), gWindowWidth, gWindowHeight, 45.0f, 1000.0f, 0.1f);
OrbitCamera gCameraMain(160.f, 90.f, 0.f, XMFLOAT3(80.0f, 80.0f, 80.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), gWidthDeferred, gHeightDeferred, 45.0f, 1000.0f, 0.1f);

DescriptorHeap::Handle DescriptorHeap::sHead[DescriptorHeap::Type::COUNT];
DescriptorHeap::Handle DescriptorHeap::sFree[DescriptorHeap::Type::COUNT];
int DescriptorHeap::sHandleIncrementSize[DescriptorHeap::Type::COUNT];
ID3D12DescriptorHeap* DescriptorHeap::sImpl[DescriptorHeap::Type::COUNT];
Renderer* DescriptorHeap::sRenderer[DescriptorHeap::Type::COUNT];

void DescriptorHeap::InitGlobalDescriptorHeap(Renderer* renderer, int countMax, Type type)
{
	fatalAssert(!sHead[type].IsValid() && !sFree[type].IsValid() && !sImpl[type]);
	sRenderer[type] = renderer;
	sHandleIncrementSize[type] = renderer->mDevice->GetDescriptorHandleIncrementSize(Renderer::TranslateDescriptorHeapType(type));
	renderer->CreateDescriptorHeap(&sImpl[type], type, countMax);
	sHead[type].ResetHandle(sImpl[type]->GetCPUDescriptorHandleForHeapStart(), sImpl[type]->GetGPUDescriptorHandleForHeapStart(), true);
	sFree[type] = sHead[type];
}

DescriptorHeap::Handle DescriptorHeap::AllocateDescriptorHeap(int countMax, Type type)
{
	fatalAssert(sHead[type].IsValid() && sFree[type].IsValid() && sImpl[type] && sHandleIncrementSize[type]);
	DescriptorHeap::Handle result = sFree[type];
	sFree[type].Offset(countMax, sHandleIncrementSize[type]);
	return result;
}

void DescriptorHeap::ReleaseGlobalDescriptorHeap(bool checkOnly)
{
	for (int type = 0; type < Type::COUNT; type++)
	{
		fatalAssert(checkOnly || (sHead[type].IsValid() && sFree[type].IsValid() && sImpl[type] && sHandleIncrementSize[type]));
		sHead[type].ResetHandle({ 0 }, { 0 }, false);
		sFree[type] = sHead[type];
		SAFE_RELEASE(sImpl[type], checkOnly);
		sHandleIncrementSize[type] = 0;
	}
}

DescriptorHeap::DescriptorHeap() : 
	mRenderer(nullptr),
	mHead(this, { 0 }, { 0 }),
	mFree(this, { 0 }, { 0 }),
	mHandleIncrementSize(0), 
	mImpl(nullptr),
	mCurrentCount(0),
	mMaxCount(0),
	mType(Type::INVALID)
{
}

DescriptorHeap::~DescriptorHeap()
{
	Release();
}

void DescriptorHeap::Reset()
{
	mCurrentCount = 0;
	mFree = mHead;
}

void DescriptorHeap::InitDescriptorHeap(int countMax, Type type)
{
	fatalAssert(sRenderer[type]);
	mRenderer = sRenderer[type];
	mMaxCount = countMax;
	mType = type;
	mHead = AllocateDescriptorHeap(mMaxCount, mType);
	mHead.SetHeap(this);
	mHandleIncrementSize = sHandleIncrementSize[type];
	mImpl = sImpl[type];
	Reset();
}

void DescriptorHeap::Release()
{
	mRenderer = nullptr;
	mHead.ResetHandle({ 0 }, { 0 }, false);
	mFree.ResetHandle({ 0 }, { 0 }, false);
	mHandleIncrementSize = 0;
	mCurrentCount = 0;
	mMaxCount = 0;
	mType = Type::INVALID;
}

DescriptorHeap::Handle DescriptorHeap::AllocateDsv(ID3D12Resource* resource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc)
{
	fatalAssert(mCurrentCount < mMaxCount);
	mCurrentCount++;
	return AllocateDsv(resource, desc, mFree);
}

DescriptorHeap::Handle DescriptorHeap::AllocateDsv(ID3D12Resource* resource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc, DescriptorHeap::Handle& offset)
{
	fatalAssert(mType == DescriptorHeap::Type::DSV);
	fatalAssert(offset.IsValid());
	mRenderer->mDevice->CreateDepthStencilView(resource, &desc, offset.GetCpuHandle());
	DescriptorHeap::Handle result = offset;
	offset.Offset(1, mHandleIncrementSize);
	return result;
}

DescriptorHeap::Handle DescriptorHeap::AllocateRtv(ID3D12Resource* resource)
{
	fatalAssert(mCurrentCount < mMaxCount);
	mCurrentCount++;
	return AllocateRtvInternal(resource, nullptr, mFree);
}

DescriptorHeap::Handle DescriptorHeap::AllocateRtv(ID3D12Resource* resource, const D3D12_RENDER_TARGET_VIEW_DESC& desc)
{
	fatalAssert(mCurrentCount < mMaxCount);
	mCurrentCount++;
	return AllocateRtvInternal(resource, &desc, mFree);
}

DescriptorHeap::Handle DescriptorHeap::AllocateRtv(ID3D12Resource* resource, const D3D12_RENDER_TARGET_VIEW_DESC& desc, DescriptorHeap::Handle& offset)
{
	return AllocateRtvInternal(resource, &desc, offset);
}

DescriptorHeap::Handle DescriptorHeap::AllocateRtvInternal(ID3D12Resource* resource, const D3D12_RENDER_TARGET_VIEW_DESC* desc, DescriptorHeap::Handle& offset)
{
	fatalAssert(mType == DescriptorHeap::Type::RTV);
	fatalAssert(offset.IsValid());
	mRenderer->mDevice->CreateRenderTargetView(resource, desc, offset.GetCpuHandle());
	DescriptorHeap::Handle result = offset;
	offset.Offset(1, mHandleIncrementSize);
	return result;
}

DescriptorHeap::Handle DescriptorHeap::AllocateSrv(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc)
{
	fatalAssert(mCurrentCount < mMaxCount);
	mCurrentCount++;
	return AllocateSrv(resource, desc, mFree);
}

DescriptorHeap::Handle DescriptorHeap::AllocateSrv(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc, DescriptorHeap::Handle& offset)
{
	fatalAssert(mType == DescriptorHeap::Type::CBV_SRV_UAV);
	fatalAssert(offset.IsValid());
	mRenderer->mDevice->CreateShaderResourceView(resource, &desc, offset.GetCpuHandle());
	DescriptorHeap::Handle result = offset;
	offset.Offset(1, mHandleIncrementSize);
	return result;
}

DescriptorHeap::Handle DescriptorHeap::AllocateUav(ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc)
{
	fatalAssert(mCurrentCount < mMaxCount);
	mCurrentCount++;
	return AllocateUav(resource, desc, mFree);
}

DescriptorHeap::Handle DescriptorHeap::AllocateUav(ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc, DescriptorHeap::Handle& offset)
{
	fatalAssert(mType == DescriptorHeap::Type::CBV_SRV_UAV);
	fatalAssert(offset.IsValid());
	mRenderer->mDevice->CreateUnorderedAccessView(resource, nullptr, &desc, offset.GetCpuHandle());
	DescriptorHeap::Handle result = offset;
	offset.Offset(1, mHandleIncrementSize);
	return result;
}

DescriptorHeap::Handle DescriptorHeap::AllocateSampler(const D3D12_SAMPLER_DESC& desc)
{
	fatalAssert(mCurrentCount < mMaxCount);
	mCurrentCount++;
	return AllocateSampler(desc, mFree);
}

DescriptorHeap::Handle DescriptorHeap::AllocateSampler(const D3D12_SAMPLER_DESC& desc, DescriptorHeap::Handle& offset)
{
	fatalAssert(mType == DescriptorHeap::Type::SAMPLER);
	fatalAssert(offset.IsValid());
	mRenderer->mDevice->CreateSampler(&desc, offset.GetCpuHandle());
	DescriptorHeap::Handle result = offset;
	offset.Offset(1, mHandleIncrementSize);
	return result;
}

DescriptorHeap::Type DescriptorHeap::GetType()
{
	return mType;
}

ID3D12DescriptorHeap* DescriptorHeap::GetImpl()
{
	return mImpl;
}

DynamicUniformBuffer::DynamicUniformBuffer() :
	mRenderer(nullptr),
	mImpl(nullptr),
	mHead(nullptr),
	mFree(nullptr),
	mSizeInByte(0)
{
}

DynamicUniformBuffer::~DynamicUniformBuffer()
{
	Release(true);
}

void DynamicUniformBuffer::Reset()
{
	mFree = mHead;
	memset(mHead, 0xcd, mSizeInByte); // for easier debugging
}

void DynamicUniformBuffer::InitDynamicUniformBuffer(Renderer* renderer, int sizeInByte)
{
	fatalAssertf(sizeInByte > 0, "size needs to be greater than 0");
	mSizeInByte = sizeInByte;
	HRESULT hr = renderer->mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // this heap will be used to upload the constant buffer data
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(mSizeInByte),
		Renderer::TranslateResourceLayout(ResourceLayout::UPLOAD), // will be data that is read from so we keep it in the generic read state
		nullptr, // we do not use an optimized clear value for constant buffers
		IID_PPV_ARGS(&mImpl));

	fatalAssertf(!CheckError(hr, renderer->mDevice), "create dynamic uniform buffer failed");
	mImpl->SetName(StringToWstring(": dynamic uniform buffer ").c_str());

	CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU. (so end is less than or equal to begin)
	mImpl->Map(0, &readRange, &mHead);
	memset(mHead, 0xcd, mSizeInByte); // for easier debugging
	Reset();

	mHeadGpu = mImpl->GetGPUVirtualAddress();
}

void DynamicUniformBuffer::Release(bool checkOnly)
{
	if (!checkOnly && mImpl)
		mImpl->Unmap(0, nullptr);
	SAFE_RELEASE(mImpl, checkOnly);
}

DynamicUniformBuffer::Address DynamicUniformBuffer::Allocate(int sizeInByte)
{
	fatalAssertf(sizeInByte > 0, "size needs to be greater than 0");
	fatalAssertf((size_t)mFree + (size_t)sizeInByte < (size_t)mHead + (size_t)mSizeInByte, "buffer is full, current size is %d bytes", mSizeInByte);
	void* resultCpu = mFree;
	D3D12_GPU_VIRTUAL_ADDRESS resultGpu = mHeadGpu + ((size_t)mFree - (size_t)mHead);
	size_t alignedSizeInByte = (sizeInByte + (256 - 1)) & -256;
	mFree = (void*)((size_t)mFree + alignedSizeInByte);
	DynamicUniformBuffer::Address result;
	result.mCpuAddress = resultCpu;
	result.mGpuAddress = resultGpu;
	return result;
}

unordered_map<ID3D12CommandAllocator*, u8> CommandList::sAllocatorRefMap;

bool CommandList::AllReleased()
{
	return sAllocatorRefMap.empty();
}

CommandList::CommandList()
{
}

CommandList::~CommandList()
{
}

bool CommandList::InitCommandList(Renderer* renderer)
{
	mRenderer = renderer;
	HRESULT hr = mRenderer->mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mAllocator));
	if (CheckError(hr))
		return false;
	hr = mRenderer->mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mAllocator, NULL, IID_PPV_ARGS(&mImpl));
	if (CheckError(hr))
		return false;

	// command lists are created in open state
	// https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-close
	mImpl->Close();

	fatalAssert(sAllocatorRefMap.find(mAllocator) == sAllocatorRefMap.end());
	sAllocatorRefMap[mAllocator] = 1;
	return true;
}

void CommandList::Release(bool checkOnly)
{
	if (!checkOnly)
	{
		fatalAssert(mAllocator && sAllocatorRefMap.find(mAllocator) != sAllocatorRefMap.end() && sAllocatorRefMap[mAllocator] == 1);
		sAllocatorRefMap.erase(mAllocator);
	}
	SAFE_RELEASE(mImpl, checkOnly);
	SAFE_RELEASE(mAllocator, checkOnly);
}

bool CommandList::Reset()
{
	// command lists are created in open state, we close them immediately after creation
	// create cl -> close cl
	//   reset allocator -> reset cl -> record -> close cl -> execute cl
	//   reset allocator -> reset cl -> record -> close cl -> execute cl
	//   reset allocator -> reset cl -> record -> close cl -> execute cl
	//   ...
	if (CheckError(mAllocator->Reset()))
		return false;

	// reset the command list. by resetting the command list we are putting it into
	// a recording state so we can start recording commands into the command allocator.
	// the command allocator that we reference here may have multiple command lists
	// associated with it, but only one can be recording at any time. Make sure
	// that any other command lists associated to this command allocator are in
	// the closed state (not recording).
	// Here you will pass an initial pipeline state object as the second parameter,
	// but in this tutorial we are only clearing the rtv, and do not actually need
	// anything but an initial default pipeline, which is what we get by setting
	// the second parameter to NULL
	if (CheckError(mImpl->Reset(mAllocator, nullptr)))
		return false;

	return true;
}

Renderer::Renderer()
{

}

Renderer::~Renderer()
{
	Release(true);
}

D3D12_DEPTH_STENCIL_DESC Renderer::TranslateDepthStencilState(DepthStencilState state)
{
	D3D12_DEPTH_STENCIL_DESC result = {};
	result.DepthEnable = state.mDepthTestEnable;
	result.DepthWriteMask = state.mDepthWriteEnable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	result.DepthFunc = Renderer::TranslateCompareOp(state.mDepthCompareOp);
	result.StencilEnable = state.mStencilTestEnable;
	result.StencilReadMask = state.mStencilReadMask;
	result.StencilWriteMask = state.mStencilWriteMask;
	result.FrontFace = Renderer::TranslateStencilOpSet(state.mFrontStencilOpSet);
	result.BackFace = Renderer::TranslateStencilOpSet(state.mBackStencilOpSet);
	return result;
}

D3D12_RENDER_TARGET_BLEND_DESC Renderer::TranslateBlendState(BlendState state)
{
	D3D12_RENDER_TARGET_BLEND_DESC result = {};
	result.BlendEnable = state.mBlendEnable;
	result.LogicOpEnable = state.mLogicOpEnable;
	result.SrcBlend = Renderer::TranslateBlendFactor(state.mSrcBlend);
	result.DestBlend = Renderer::TranslateBlendFactor(state.mDestBlend);
	result.BlendOp = Renderer::TranslateBlendOp(state.mBlendOp);
	result.SrcBlendAlpha = Renderer::TranslateBlendFactor(state.mSrcBlendAlpha);
	result.DestBlendAlpha = Renderer::TranslateBlendFactor(state.mDestBlendAlpha);
	result.BlendOpAlpha = Renderer::TranslateBlendOp(state.mBlendOpAlpha);
	result.LogicOp = Renderer::TranslateLogicOp(state.mLogicOp);
	result.RenderTargetWriteMask = state.mWriteMask;
	return result;
}

D3D12_BLEND Renderer::TranslateBlendFactor(BlendState::BlendFactor factor)
{
	switch (factor)
	{
	case BlendState::BlendFactor::ZERO:
		return D3D12_BLEND_ZERO;
	case BlendState::BlendFactor::ONE:
		return D3D12_BLEND_ONE;
	case BlendState::BlendFactor::SRC_COLOR:
		return D3D12_BLEND_SRC_COLOR;
	case BlendState::BlendFactor::INV_SRC_COLOR:
		return D3D12_BLEND_INV_SRC_COLOR;
	case BlendState::BlendFactor::SRC_ALPHA:
		return D3D12_BLEND_SRC_ALPHA;
	case BlendState::BlendFactor::INV_SRC_ALPHA:
		return D3D12_BLEND_INV_SRC_ALPHA;
	case BlendState::BlendFactor::DEST_ALPHA:
		return D3D12_BLEND_DEST_ALPHA;
	case BlendState::BlendFactor::INV_DEST_ALPHA:
		return D3D12_BLEND_INV_DEST_ALPHA;
	case BlendState::BlendFactor::DEST_COLOR:
		return D3D12_BLEND_DEST_COLOR;
	case BlendState::BlendFactor::INV_DEST_COLOR:
		return D3D12_BLEND_INV_DEST_COLOR;
	case BlendState::BlendFactor::SRC_ALPHA_SAT:
		return D3D12_BLEND_SRC_ALPHA_SAT;
	case BlendState::BlendFactor::CONSTANT:
		return D3D12_BLEND_BLEND_FACTOR;
	case BlendState::BlendFactor::INV_CONSTANT:
		return D3D12_BLEND_INV_BLEND_FACTOR;
	case BlendState::BlendFactor::SRC1_COLOR:
		return D3D12_BLEND_SRC1_COLOR;
	case BlendState::BlendFactor::INV_SRC1_COLOR:
		return D3D12_BLEND_INV_SRC1_COLOR;
	case BlendState::BlendFactor::SRC1_ALPHA:
		return D3D12_BLEND_SRC1_ALPHA;
	case BlendState::BlendFactor::INV_SRC1_ALPHA:
		return D3D12_BLEND_INV_SRC1_ALPHA;
	default:
		fatalf("TranslateBlendFactor wrong factor");
	}
	return D3D12_BLEND_ZERO;
}

D3D12_BLEND_OP Renderer::TranslateBlendOp(BlendState::BlendOp op)
{
	switch (op)
	{
	case BlendState::BlendOp::ADD:
		return D3D12_BLEND_OP_ADD;
	case BlendState::BlendOp::SUBTRACT:
		return D3D12_BLEND_OP_SUBTRACT;
	case BlendState::BlendOp::REV_SUBTRACT:
		return D3D12_BLEND_OP_REV_SUBTRACT;
	case BlendState::BlendOp::MIN:
		return D3D12_BLEND_OP_MIN;
	case BlendState::BlendOp::MAX:
		return D3D12_BLEND_OP_MAX;
	default:
		fatalf("blend op wrong");
	}
	return D3D12_BLEND_OP_ADD;
}

D3D12_LOGIC_OP Renderer::TranslateLogicOp(BlendState::LogicOp op)
{
	switch (op)
	{
	case BlendState::LogicOp::CLEAR:
		return D3D12_LOGIC_OP_CLEAR;
	case BlendState::LogicOp::SET:
		return D3D12_LOGIC_OP_SET;
	case BlendState::LogicOp::COPY:
		return D3D12_LOGIC_OP_COPY;
	case BlendState::LogicOp::COPY_INVERTED:
		return D3D12_LOGIC_OP_COPY_INVERTED;
	case BlendState::LogicOp::NOOP:
		return D3D12_LOGIC_OP_NOOP;
	case BlendState::LogicOp::INVERT:
		return D3D12_LOGIC_OP_INVERT;
	case BlendState::LogicOp::AND:
		return D3D12_LOGIC_OP_AND;
	case BlendState::LogicOp::NAND:
		return D3D12_LOGIC_OP_NAND;
	case BlendState::LogicOp::OR:
		return D3D12_LOGIC_OP_OR;
	case BlendState::LogicOp::NOR:
		return D3D12_LOGIC_OP_NOR;
	case BlendState::LogicOp::XOR:
		return D3D12_LOGIC_OP_XOR;
	case BlendState::LogicOp::EQUIV:
		return D3D12_LOGIC_OP_EQUIV;
	case BlendState::LogicOp::AND_REVERSE:
		return D3D12_LOGIC_OP_AND_REVERSE;
	case BlendState::LogicOp::AND_INVERTED:
		return D3D12_LOGIC_OP_AND_INVERTED;
	case BlendState::LogicOp::OR_REVERSE:
		return D3D12_LOGIC_OP_OR_REVERSE;
	case BlendState::LogicOp::OR_INVERTED:
		return D3D12_LOGIC_OP_OR_INVERTED;
	default:
		fatalf("logic op wrong");
	}
	return D3D12_LOGIC_OP_CLEAR;
}

DXGI_FORMAT Renderer::TranslateFormat(Format format)
{
	switch(format)
	{
	case Format::R8G8B8A8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case Format::R16G16B16A16_UNORM:
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	case Format::R16G16B16A16_FLOAT:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case Format::R16_FLOAT:
		return DXGI_FORMAT_R16_FLOAT;
	case Format::R32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case Format::R32_UINT:
		return DXGI_FORMAT_R32_UINT;
	case Format::D16_UNORM:
		return DXGI_FORMAT_D16_UNORM;
	case Format::D32_FLOAT:
		return DXGI_FORMAT_D32_FLOAT;
	case Format::D24_UNORM_S8_UINT:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	default:
		fatalf("TranslateFormat wrong format");
	}
	return DXGI_FORMAT_UNKNOWN;
}

DXGI_FORMAT Renderer::TranslateToTypelessFormat(Format format)
{
	switch (format)
	{
	case Format::R8G8B8A8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_TYPELESS;
	case Format::R16G16B16A16_UNORM:
	case Format::R16G16B16A16_FLOAT:
		return DXGI_FORMAT_R16G16B16A16_TYPELESS;
	case Format::R16_FLOAT:
	case Format::D16_UNORM:
		return DXGI_FORMAT_R16_TYPELESS;
	case Format::R32_FLOAT:
	case Format::D32_FLOAT:
		return DXGI_FORMAT_R32_TYPELESS;
	case Format::D24_UNORM_S8_UINT:
		return DXGI_FORMAT_R24G8_TYPELESS;
	default:
		fatalf("TranslateFormat wrong format");
	}
	return DXGI_FORMAT_UNKNOWN;
}

DXGI_FORMAT Renderer::TranslateFormatToRead(Format format, ReadFrom access)
{
	if (access & ReadFrom::COLOR)
	{
		return TranslateFormat(format);
	}
	else if (access & ReadFrom::DEPTH)
	{
		switch (format)
		{
		case Format::D16_UNORM:
			return DXGI_FORMAT_R16_UNORM;
		case Format::D32_FLOAT:
			return DXGI_FORMAT_R32_FLOAT;
		case Format::D24_UNORM_S8_UINT:
			return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		default:
			fatalf("accessing wrong format");
		}
	}
	else if (access & ReadFrom::STENCIL)
	{
		switch (format)
		{
		case Format::D24_UNORM_S8_UINT:
			return DXGI_FORMAT_X24_TYPELESS_G8_UINT;
		default:
			fatalf("accessing wrong format");
		}
	}
	else
		fatalf("accessing wrong format");
	
	return DXGI_FORMAT_UNKNOWN;
}

D3D12_DESCRIPTOR_HEAP_TYPE Renderer::TranslateDescriptorHeapType(DescriptorHeap::Type type)
{
	switch (type)
	{
	case DescriptorHeap::Type::CBV_SRV_UAV:
		return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	case DescriptorHeap::Type::SAMPLER:
		return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	case DescriptorHeap::Type::RTV:
		return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	case DescriptorHeap::Type::DSV:
		return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	default:
		fatalf("wrong descriptor heap type");
	}
	return D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE Renderer::TranslatePrimitiveTopologyType(PrimitiveType primitiveType)
{
	switch (primitiveType)
	{
	case PrimitiveType::POINT:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	case PrimitiveType::LINE:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	case PrimitiveType::TRIANGLE:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	default:
		fatalf("wring primitive type");
	}
	return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
}

D3D12_PRIMITIVE_TOPOLOGY Renderer::TranslatePrimitiveTopology(PrimitiveType primitiveType)
{
	switch (primitiveType)
	{
	case PrimitiveType::POINT:
		return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	case PrimitiveType::LINE:
		return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	case PrimitiveType::TRIANGLE:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	default:
		fatalf("wring primitive type");
	}
	return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

D3D12_FILTER Renderer::ExtractFilter(Sampler sampler)
{
	//https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_filter
	uint16_t reductionType = 0;
	uint16_t anistropy = 0;
	uint16_t min = 0;
	uint16_t mag = 0;
	uint16_t mip = 0;
	
	if(sampler.mUseCompare)
	{
		switch (sampler.mCompareOp)
		{
		case CompareOp::INVALID:
			fatalf("wrong filter");
			break;
		case CompareOp::MAXIMUM:
			reductionType = 3;
			break;
		case CompareOp::MINIMUM:
			reductionType = 2;
			break;
		default:
			reductionType = 1;
			break;
		}
	}
	
	if (sampler.mUseAnisotropy)
		anistropy = 1;

	if (sampler.mMinFilter == Sampler::Filter::LINEAR)
		min = 1;

	if (sampler.mMagFilter == Sampler::Filter::LINEAR)
		mag = 1;

	if (sampler.mMipFilter == Sampler::Filter::LINEAR)
		mip = 1;

	return (D3D12_FILTER)(reductionType << 7 | anistropy << 6 | min << 4 | mag << 2 | mip);
}

D3D12_TEXTURE_ADDRESS_MODE Renderer::TranslateAddressMode(Sampler::AddressMode addressMode)
{
	switch(addressMode)
	{
	case Sampler::AddressMode::WRAP:
		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	case Sampler::AddressMode::MIRROR:
		return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
	case Sampler::AddressMode::CLAMP:
		return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	case Sampler::AddressMode::BORDER:
		return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	case Sampler::AddressMode::MIRROR_ONCE:
		return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
	default:
		fatalf("address mode wrong");
	}
	return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
}

D3D12_COMPARISON_FUNC Renderer::TranslateCompareOp(CompareOp op)
{
	switch (op)
	{
	case CompareOp::NEVER:
		return D3D12_COMPARISON_FUNC_NEVER;
	case CompareOp::LESS:
		return D3D12_COMPARISON_FUNC_LESS;
	case CompareOp::EQUAL:
		return D3D12_COMPARISON_FUNC_EQUAL;
	case CompareOp::LESS_EQUAL:
		return D3D12_COMPARISON_FUNC_LESS_EQUAL;
	case CompareOp::GREATER:
		return D3D12_COMPARISON_FUNC_GREATER;
	case CompareOp::NOT_EQUAL:
		return D3D12_COMPARISON_FUNC_NOT_EQUAL;
	case CompareOp::GREATER_EQUAL:
		return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	case CompareOp::ALWAYS:
		return D3D12_COMPARISON_FUNC_ALWAYS;
	case CompareOp::MINIMUM:
	case CompareOp::MAXIMUM:
		// not sure about this, what comparison function should it be 
		// if minimum or maximum reduction is used in the filter
		return D3D12_COMPARISON_FUNC_NEVER; 
	default:
		fatalf("compare op wrong");
	}
	return D3D12_COMPARISON_FUNC_NEVER;
}

D3D12_DEPTH_STENCILOP_DESC Renderer::TranslateStencilOpSet(DepthStencilState::StencilOpSet opSet)
{
	D3D12_DEPTH_STENCILOP_DESC result = {};
	result.StencilFailOp = Renderer::TranslateStencilOp(opSet.mFailOp);
	result.StencilDepthFailOp = Renderer::TranslateStencilOp(opSet.mDepthFailOp);
	result.StencilPassOp = Renderer::TranslateStencilOp(opSet.mPassOp);
	result.StencilFunc = Renderer::TranslateCompareOp(opSet.mStencilCompareOp);
	return result;
}

D3D12_STENCIL_OP Renderer::TranslateStencilOp(DepthStencilState::StencilOp op)
{
	switch (op)
	{
	case DepthStencilState::StencilOp::KEEP:
		return D3D12_STENCIL_OP_KEEP;
	case DepthStencilState::StencilOp::ZERO:
		return D3D12_STENCIL_OP_ZERO;
	case DepthStencilState::StencilOp::REPLACE:
		return D3D12_STENCIL_OP_REPLACE;
	case DepthStencilState::StencilOp::INCR_SAT:
		return D3D12_STENCIL_OP_INCR_SAT;
	case DepthStencilState::StencilOp::DECR_SAT:
		return D3D12_STENCIL_OP_DECR_SAT;
	case DepthStencilState::StencilOp::INVERT:
		return D3D12_STENCIL_OP_INVERT;
	case DepthStencilState::StencilOp::INCR:
		return D3D12_STENCIL_OP_INCR;
	case DepthStencilState::StencilOp::DECR:
		return D3D12_STENCIL_OP_DECR;
	default:
		fatalf("stencil op wrong");
	}
	return D3D12_STENCIL_OP_KEEP;
}

D3D12_RESOURCE_STATES Renderer::TranslateResourceLayout(ResourceLayout resourceLayout)
{
	switch (resourceLayout)
	{
	case ResourceLayout::SHADER_READ:
		return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; // TODO: examine the necessity to split these two
	case ResourceLayout::SHADER_WRITE:
		return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	case ResourceLayout::RENDER_TARGET:
		return D3D12_RESOURCE_STATE_RENDER_TARGET;
	case ResourceLayout::DEPTH_STENCIL_READ:
		return D3D12_RESOURCE_STATE_DEPTH_READ;
	case ResourceLayout::DEPTH_STENCIL_WRITE:
		return D3D12_RESOURCE_STATE_DEPTH_WRITE;
	case ResourceLayout::COPY_SRC:
		return D3D12_RESOURCE_STATE_COPY_SOURCE;
	case ResourceLayout::COPY_DST:
		return D3D12_RESOURCE_STATE_COPY_DEST;
	case ResourceLayout::RESOLVE_SRC:
		return D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
	case ResourceLayout::RESOLVE_DST:
		return D3D12_RESOURCE_STATE_RESOLVE_DEST;
	case ResourceLayout::PRESENT:
		return D3D12_RESOURCE_STATE_PRESENT;
	case ResourceLayout::UPLOAD:
		return D3D12_RESOURCE_STATE_GENERIC_READ; // D3D12_RESOURCE_STATE_GENERIC_READ is a logically OR'd combination of other read-state bits. This is the required starting state for an upload heap.
	default:
		fatalf("texture layout wrong");
	}
	return D3D12_RESOURCE_STATE_COMMON;
}

D3D12_VIEWPORT Renderer::TranslateViewport(Viewport viewport)
{
	D3D12_VIEWPORT vp = {};
	vp.TopLeftX = viewport.mTopLeftX;
	vp.TopLeftY = viewport.mTopLeftY;
	vp.Width = viewport.mWidth;
	vp.Height = viewport.mHeight;
	vp.MinDepth = viewport.mMinDepth;
	vp.MaxDepth = viewport.mMaxDepth;
	return vp;
}

D3D12_RECT Renderer::TranslateScissorRect(ScissorRect rect)
{
	D3D12_RECT rc = {};
	rc.left = rect.mLeft;
	rc.top = rect.mTop;
	rc.right = rect.mRight;
	rc.bottom = rect.mBottom;
	return rc;
}

D3D12_RESOURCE_DIMENSION Renderer::GetResourceDimensionFromTextureType(TextureType type)
{
	switch (type)
	{
	case TextureType::TEX_2D:
	case TextureType::TEX_CUBE:
		return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	case TextureType::TEX_VOLUME:
		return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	default:
		fatalf("wrong texture type");
	}
	return D3D12_RESOURCE_DIMENSION_UNKNOWN;
}

D3D12_SAMPLER_DESC Renderer::TranslateSamplerDesc(Sampler sampler)
{
	D3D12_SAMPLER_DESC impl = { 0 };
	impl.Filter = ExtractFilter(sampler);
	impl.AddressU = TranslateAddressMode(sampler.mAddressModeU);
	impl.AddressV = TranslateAddressMode(sampler.mAddressModeV);
	impl.AddressW = TranslateAddressMode(sampler.mAddressModeW);
	impl.MipLODBias = sampler.mMipLodBias;
	impl.MaxAnisotropy = 0; // TODO: add support for anisotropic filtering
	impl.ComparisonFunc = TranslateCompareOp(sampler.mCompareOp);
	impl.BorderColor[0] = sampler.mBorderColor[0];
	impl.BorderColor[1] = sampler.mBorderColor[1];
	impl.BorderColor[2] = sampler.mBorderColor[2];
	impl.BorderColor[3] = sampler.mBorderColor[3];
	impl.MinLOD = sampler.mMinLod;
	impl.MaxLOD = sampler.mMaxLod;
	return impl;
}

DescriptorHeap::Handle Renderer::GetRtvHandle(int frameIndex)
{
	return mRtvHandles[frameIndex];
}

DescriptorHeap::Handle Renderer::GetDsvHandle(int frameIndex)
{
	return mDsvHandles[frameIndex];
}

DescriptorHeap& Renderer::GetDynamicCbvSrvUavDescriptorHeap(int frameIndex)
{
	return mDynamicCbvSrvUavDescriptorHeap[frameIndex];
}

DescriptorHeap& Renderer::GetDynamicRtvDescriptorHeap(int frameIndex)
{
	return mDynamicRtvDescriptorHeap[frameIndex];
}

DescriptorHeap& Renderer::GetDynamicDsvDescriptorHeap(int frameIndex)
{
	return mDynamicDsvDescriptorHeap[frameIndex];
}

DynamicUniformBuffer::Address Renderer::AllocateDynamicUniformBuffer(int frameIndex, int sizeInByte)
{
	return mDynamicUniformBuffers[frameIndex].Allocate(sizeInByte);
}

ID3D12Resource* Renderer::GetRenderTargetBuffer(int frameIndex)
{
	return mColorBuffers[frameIndex];
}

ID3D12Resource* Renderer::GetDepthStencilBuffer(int frameIndex)
{
	return mDepthStencilBuffers[frameIndex];
}

ID3D12Resource* Renderer::GetPreResolveBuffer(int frameIndex)
{
	return mPreResolveColorBuffers[frameIndex];
}

IDXGISwapChain3* Renderer::GetSwapChain()
{
	return mSwapChain;
}

vector<Frame>& Renderer::GetFrames()
{
	return mFrames;
}

Frame& Renderer::GetFrame(int frameIndex)
{
	return mFrames[frameIndex];
}

int Renderer::GetMaxFrameTextureCount()
{
	int maxFrameTextureCount = 0;
	for(auto& frame : mFrames)
	{
		if (frame.GetTextureCount() > maxFrameTextureCount)
			maxFrameTextureCount = frame.GetTextureCount();
	}
	return maxFrameTextureCount;
}

bool Renderer::IsMultiSampleUsed()
{
	return mMultiSampleCount > 1;
}

bool Renderer::IsResolveNeeded()
{
	return IsMultiSampleUsed();
}

void Renderer::SetStore(Store* store)
{
	mStore = store;
}

bool Renderer::InitRenderer(
	HWND hwnd,
	int frameCount,
	int multiSampleCount,
	int width,
	int height,
	Format colorBufferFormat,
	Format depthStencilBufferFormat,
	DebugMode debugMode,
	BlendState blendState,
	DepthStencilState depthStencilState)
{
	mInitialized = false;
	mFrameCount = frameCount;
	mFrameCountSinceGameStart = 1;
	mLastFrameTimeInSecond = 1.0f;
	mMultiSampleCount = multiSampleCount;
	mWidth = width;
	mHeight = height;
	mBlendState = blendState;
	mDepthStencilState = depthStencilState;
	mColorBufferFormat = colorBufferFormat;
	mDepthStencilBufferFormat = depthStencilBufferFormat;
	mDebugMode = debugMode;
	HRESULT hr;

	//debug mode
	fatalAssert(mDebugMode >= DebugMode::OFF && mDebugMode < DebugMode::COUNT);
	if (mDebugMode >= DebugMode::ON)
		EnableDebugLayer();
	if (mDebugMode >= DebugMode::GBV)
		EnableGBV();

	// 1. create device
	hr = CreateDXGIFactory1(IID_PPV_ARGS(&mDxgiFactory));
	if(CheckError(hr))
		return false;
	IDXGIAdapter1* adapter = nullptr;
	int adapterIndex = 0;
	bool adapterFound = false;
	// find first hardware gpu that supports d3d 12
	while (mDxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue; // don't want a software device
		// want a device that is compatible with direct3d 12 (feature level 11 or higher)
		hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr);
		if (!CheckError(hr)) 
		{
			adapterFound = true;
			break;
		}
		adapterIndex++;
	}
	if (!adapterFound)
		return false;
	
	hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&mDevice));
	if (CheckError(hr))
		return false;

	// 2. create command queue
	// TODO: only one direct command queue is used currently
	D3D12_COMMAND_QUEUE_DESC cqDesc = {};
	cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // direct means the gpu can directly execute this command queue
	hr = mDevice->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&mGraphicsCommandQueue));
	if (CheckError(hr, mDevice))
		return false;
	mGraphicsCommandQueue->SetName(L"Graphics Command Queue");

	// 3. create swapchain
	DXGI_MODE_DESC backBufferDesc = {};
	backBufferDesc.Width = mWidth;
	backBufferDesc.Height = mHeight;
	backBufferDesc.Format = TranslateFormat(mColorBufferFormat);
	// describe our multi-sampling. We are not multi-sampling, so we set the count to 1 (we need at least one sample of course)
	DXGI_SAMPLE_DESC sampleDesc = {};
	sampleDesc.Count = 1; // this is the final render targets, MSAA is done in PreResolve buffers
	sampleDesc.Quality = 0;
	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = mFrameCount;
	swapChainDesc.BufferDesc = backBufferDesc;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.OutputWindow = hwnd; // handle to our window
	swapChainDesc.SampleDesc = sampleDesc; // our multi-sampling description
	swapChainDesc.Windowed = true; // better leave it to true and use SetFullscreenState to toggle fullscreen https://docs.microsoft.com/en-us/windows/win32/api/dxgi/ns-dxgi-dxgi_swap_chain_desc
	IDXGISwapChain* swapChain = nullptr;
	hr = mDxgiFactory->CreateSwapChain(
		mGraphicsCommandQueue, // the queue will be flushed once the swap chain is created
		&swapChainDesc, // give it the swap chain description we created above
		&swapChain // store the created swap chain in a temp IDXGISwapChain interface
	);
	if (CheckError(hr))
		return false;
	mSwapChain = static_cast<IDXGISwapChain3*>(swapChain);
	mCurrentFramebufferIndex = mSwapChain->GetCurrentBackBufferIndex();

	// 4. create command allocators and command lists
	mCommandLists.resize(mFrameCount);
	for (int i = 0; i < mFrameCount; i++)
	{
		mCommandLists[i].InitCommandList(this);
	}

	// 5. create fences
	mFences.resize(mFrameCount);
	mFenceValues.resize(mFrameCount);
	for (int i = 0; i < mFrameCount; i++)
	{
		hr = mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFences[i]));
		if (CheckError(hr))
			return false;
		mFenceValues[i] = 0;
	}
	// create a handle to a fence event
	mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (mFenceEvent == nullptr)
		return false;
	
	// 6. create global GPU resources
	int staticCbvSrvUavHandleCount = EstimateTotalCbvSrvUavCount();
	int staticSamplerHandleCount = EstimateTotalSamplerCount();
	int staticRtvHandleCount = EstimateTotalRtvCount();
	int staticDsvHandleCount = EstimateTotalDsvCount();
	
	DescriptorHeap::InitGlobalDescriptorHeap(this, staticCbvSrvUavHandleCount + mFrameCount * gDynamicCbvSrvUavDescriptorCountPerFrame, DescriptorHeap::Type::CBV_SRV_UAV);
	DescriptorHeap::InitGlobalDescriptorHeap(this, staticSamplerHandleCount, DescriptorHeap::Type::SAMPLER); // no dynamic sampler
	DescriptorHeap::InitGlobalDescriptorHeap(this, staticRtvHandleCount + mFrameCount * gDynamicRtvDescriptorCountPerFrame, DescriptorHeap::Type::RTV);
	DescriptorHeap::InitGlobalDescriptorHeap(this, staticDsvHandleCount + mFrameCount * gDynamicDsvDescriptorCountPerFrame, DescriptorHeap::Type::DSV);

	mCbvSrvUavDescriptorHeap.InitDescriptorHeap(staticCbvSrvUavHandleCount, DescriptorHeap::Type::CBV_SRV_UAV);
	mSamplerDescriptorHeap.InitDescriptorHeap(staticSamplerHandleCount, DescriptorHeap::Type::SAMPLER);
	mRtvDescriptorHeap.InitDescriptorHeap(staticRtvHandleCount, DescriptorHeap::Type::RTV);
	mDsvDescriptorHeap.InitDescriptorHeap(staticDsvHandleCount, DescriptorHeap::Type::DSV);

	mDynamicCbvSrvUavDescriptorHeap.resize(mFrameCount);
	mDynamicRtvDescriptorHeap.resize(mFrameCount);
	mDynamicDsvDescriptorHeap.resize(mFrameCount);
	mDynamicUniformBuffers.resize(mFrameCount);

	for (int i = 0; i < mFrameCount; i++)
	{
		mDynamicCbvSrvUavDescriptorHeap[i].InitDescriptorHeap(gDynamicCbvSrvUavDescriptorCountPerFrame, DescriptorHeap::Type::CBV_SRV_UAV);
		mDynamicRtvDescriptorHeap[i].InitDescriptorHeap(gDynamicRtvDescriptorCountPerFrame, DescriptorHeap::Type::RTV);
		mDynamicDsvDescriptorHeap[i].InitDescriptorHeap(gDynamicDsvDescriptorCountPerFrame, DescriptorHeap::Type::DSV);
		mDynamicUniformBuffers[i].InitDynamicUniformBuffer(this, gDynamicUniformBufferSizePerFrameInByte);
	}

	CreateColorBuffers();
	CreateDepthStencilBuffers(mDepthStencilBufferFormat);//hard coded to DXGI_FORMAT_D24_UNORM_S8_UINT
	CreatePreResolveBuffers(mColorBufferFormat);

	for(int i = 0;i<mFrames.size();i++)
	{
		mFrames[i].InitFrame(i, this, mCbvSrvUavDescriptorHeap, mSamplerDescriptorHeap, mRtvDescriptorHeap, mDsvDescriptorHeap);
	}

	// 7. init level
	mStore->InitStore(this, mFrameCount, mCbvSrvUavDescriptorHeap, mSamplerDescriptorHeap, mRtvDescriptorHeap, mDsvDescriptorHeap);

	return mInitialized = true;
}

// unlike renter textuers, there is no need for pre resolve depth stencil buffers
// because we will not read or present swap chain depth stencil buffers
void Renderer::CreateDepthStencilBuffers(Format format)
{
	D3D12_RESOURCE_DESC dsBufferDesc = {};
	dsBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	dsBufferDesc.Alignment = 0;
	dsBufferDesc.Width = mWidth;
	dsBufferDesc.Height = mHeight;
	dsBufferDesc.DepthOrArraySize = 1;
	dsBufferDesc.MipLevels = 1;
	dsBufferDesc.Format = Renderer::TranslateFormat(format);
	dsBufferDesc.SampleDesc.Count = mMultiSampleCount;
	dsBufferDesc.SampleDesc.Quality = 0;
	dsBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	dsBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
	depthOptimizedClearValue.Format = dsBufferDesc.Format;
	depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
	depthOptimizedClearValue.DepthStencil.Stencil = 0;

	D3D12_DEPTH_STENCIL_VIEW_DESC dsViewDesc = {};
	dsViewDesc.Format = dsBufferDesc.Format;
	dsViewDesc.Flags = D3D12_DSV_FLAG_NONE;
	if (IsMultiSampleUsed())
	{
		dsViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
		dsViewDesc.Texture2DMS.UnusedField_NothingToDefine = 0;
	}
	else
	{
		dsViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsViewDesc.Texture2D.MipSlice = 0;
	}

	mDepthStencilBuffers.resize(mFrameCount);
	mDsvHandles.resize(mFrameCount);

	for (int i = 0; i < mFrameCount; i++)
	{
		HRESULT hr = mDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&dsBufferDesc,
			Renderer::TranslateResourceLayout(ResourceLayout::DEPTH_STENCIL_WRITE),
			&depthOptimizedClearValue,
			IID_PPV_ARGS(&mDepthStencilBuffers[i])
		);

		assertf(SUCCEEDED(hr), "create swap chain depth stencil buffer %d failed", i);
		mDepthStencilBuffers[i]->SetName(StringToWstring("swap chain depth stencil buffer : " + to_string(i)).c_str());
		mDsvHandles[i] = mDsvDescriptorHeap.AllocateDsv(mDepthStencilBuffers[i], dsViewDesc);
	}
}

void Renderer::CreatePreResolveBuffers(Format format)
{
	if (!IsMultiSampleUsed())
		return;

	D3D12_RESOURCE_DESC rtBufferDesc = {};
	rtBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rtBufferDesc.Alignment = 0;
	rtBufferDesc.Width = mWidth;
	rtBufferDesc.Height = mHeight;
	rtBufferDesc.DepthOrArraySize = 1;
	rtBufferDesc.MipLevels = 1;
	rtBufferDesc.Format = Renderer::TranslateFormat(format);
	rtBufferDesc.SampleDesc.Count = mMultiSampleCount;
	rtBufferDesc.SampleDesc.Quality = 0;
	rtBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rtBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; // | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;

	D3D12_CLEAR_VALUE colorOptimizedClearValue = {};
	colorOptimizedClearValue.Format = rtBufferDesc.Format;
	colorOptimizedClearValue.Color[0] = 0.f;
	colorOptimizedClearValue.Color[1] = 0.f;
	colorOptimizedClearValue.Color[2] = 0.f;
	colorOptimizedClearValue.Color[3] = 0.f;

	D3D12_RENDER_TARGET_VIEW_DESC rtViewDesc = {};
	rtViewDesc.Format = rtBufferDesc.Format;
	if (IsMultiSampleUsed())
	{
		rtViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
		rtViewDesc.Texture2DMS.UnusedField_NothingToDefine = 0;
	}
	else
	{
		rtViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtViewDesc.Texture2D.MipSlice = 0;
		rtViewDesc.Texture2D.PlaneSlice = 0;
	}

	mPreResolveColorBuffers.resize(mFrameCount);
	mPreResolvedRtvHandles.resize(mFrameCount);

	for (int i = 0; i < mFrameCount; i++)
	{
		HRESULT hr = mDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rtBufferDesc,
			Renderer::TranslateResourceLayout(ResourceLayout::RESOLVE_SRC),
			&colorOptimizedClearValue,
			IID_PPV_ARGS(&mPreResolveColorBuffers[i]));

		assertf(SUCCEEDED(hr), "create swap chain pre resolve buffer %d failed", i);
		mPreResolveColorBuffers[i]->SetName(StringToWstring("swap chain pre resolve buffer : " + to_string(i)).c_str());
		mPreResolvedRtvHandles[i] = mRtvDescriptorHeap.AllocateRtv(mPreResolveColorBuffers[i]);
	}
}

void Renderer::CreateColorBuffers()
{
	mColorBuffers.resize(mFrameCount);
	mRtvHandles.resize(mFrameCount);
	for (int i = 0; i < mFrameCount; i++)
	{
		fatalAssert(SUCCEEDED(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mColorBuffers[i]))));
		mRtvHandles[i] = mRtvDescriptorHeap.AllocateRtv(mColorBuffers[i]);
	}
}

void Renderer::Release(bool checkOnly)
{
	// 1. release level
	mStore->Release(checkOnly);

	// 2. release global GPU resources
	for (int i = 0; i < mFrameCount; i++)
	{
		SAFE_RELEASE(mColorBuffers[i], checkOnly);
		SAFE_RELEASE(mDepthStencilBuffers[i], checkOnly);
		mFrames[i].Release(checkOnly);
		mDynamicUniformBuffers[i].Release(checkOnly);
	}

	DescriptorHeap::ReleaseGlobalDescriptorHeap(checkOnly);

	// 3. release fences
	if (mFenceEvent != NULL) {
		CloseHandle(mFenceEvent);
		mFenceEvent = NULL;
	}

	for (int i = 0; i < mFrameCount; i++)
	{
		SAFE_RELEASE(mFences[i], checkOnly);
	}

	// 4. release command allocators
	fatalAssert(mSingleTimeCommandLists.empty());
	for (int i = 0; i < mCommandLists.size(); i++)
	{
		mCommandLists[i].Release(checkOnly);
	}
	fatalAssert(CommandList::AllReleased());

	// 5. release swap chain
	SAFE_RELEASE(mSwapChain, checkOnly);

	// 6. release command queue
	SAFE_RELEASE(mGraphicsCommandQueue, checkOnly);

	// 7. release device
	if (mDevice)
	{
		ID3D12DebugDevice* debugDev;
		bool hasDebugDev = false;
		if (SUCCEEDED(mDevice->QueryInterface(IID_PPV_ARGS(&debugDev))))
			hasDebugDev = true;
		SAFE_RELEASE(mDebugController, checkOnly);
		SAFE_RELEASE(mDevice, checkOnly);
		if (hasDebugDev)
		{
			OutputDebugStringW(L">>>>>>>>>>>>>>>>>>>>>>>>>> Debug Report Live Device Objects >>>>>>>>>>>>>>>>>>>>>>>>>>\n");
			debugDev->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY | D3D12_RLDO_IGNORE_INTERNAL);
			OutputDebugStringW(L"<<<<<<<<<<<<<<<<<<<<<<<<<< Debug Report Live Device Objects <<<<<<<<<<<<<<<<<<<<<<<<<<\n");
		}
		SAFE_RELEASE(debugDev, checkOnly);
	}
}

void Renderer::WaitAllFrames()
{
	if (mInitialized)
	{
		// wait for the gpu to finish all frames
		for (int i = 0; i < mFrameCount; ++i)
		{
			mFenceValues[i]++;
			mGraphicsCommandQueue->Signal(mFences[i], mFenceValues[i]);
			WaitForFrame(i);
		}
	}
}

bool Renderer::RecordBegin(int frameIndex, CommandList commandList)
{
	if (!mCommandLists[frameIndex].Reset())
		return false;

	if (IsResolveNeeded())
	{
		RecordTransition(commandList, GetPreResolveBuffer(frameIndex), 0, ResourceLayout::RESOLVE_SRC, ResourceLayout::RENDER_TARGET);
	}
	else
	{
		// transition the "frameIndex" render target from the present state to the render target state so the command list draws to it starting from here
		// "Swap chain back buffers automatically start out in the D3D12_RESOURCE_STATE_COMMON state."
		// https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#initial-states-for-resources
		// "D3D12_RESOURCE_STATE_PRESENT	Synonymous with D3D12_RESOURCE_STATE_COMMON."
		// https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_states#constants
		RecordTransition(commandList, GetRenderTargetBuffer(frameIndex), 0, ResourceLayout::PRESENT, ResourceLayout::RENDER_TARGET);
	}
	return true;
}

bool Renderer::ResolveFrame(int frameIndex, CommandList commandList)
{
	if (!IsResolveNeeded())
		return false;

	RecordTransition(commandList, GetPreResolveBuffer(frameIndex), 0, ResourceLayout::RENDER_TARGET, ResourceLayout::RESOLVE_SRC);
	RecordTransition(commandList, GetRenderTargetBuffer(frameIndex), 0, ResourceLayout::PRESENT, ResourceLayout::RESOLVE_DST);
	RecordResolve(commandList, GetPreResolveBuffer(frameIndex), 0, GetRenderTargetBuffer(frameIndex), 0, mColorBufferFormat);
	RecordTransition(commandList, GetRenderTargetBuffer(frameIndex), 0, ResourceLayout::RESOLVE_DST, ResourceLayout::RENDER_TARGET);
	
	return true;
}

bool Renderer::RecordEnd(int frameIndex, CommandList commandList)
{
	// transition the "frameIndex" render target from the render target state to the present state. 
	// If the debug layer is enabled, you will receive a warning if present is called on the render target when it's not in the present state
	RecordTransition(commandList, GetRenderTargetBuffer(frameIndex), 0, ResourceLayout::RENDER_TARGET, ResourceLayout::PRESENT);

	if (CheckError(commandList.Close()))
		return false;

	return true;
}

bool Renderer::Submit(int frameIndex, ID3D12CommandQueue* commandQueue, vector<CommandList> commandLists)
{
	vector<ID3D12CommandList*> commandListsImpl(commandLists.size());
	// execute the array of command lists
	for (int i = 0; i < commandLists.size(); i++)
		commandListsImpl[i] = commandLists[i].GetImpl();

	commandQueue->ExecuteCommandLists(commandLists.size(), (ID3D12CommandList**)commandListsImpl.data());
		
	// this command goes in at the end of our command queue. we will know when our command queue 
	// has finished because the fence value will be set to "fenceValue" from the GPU since the command
	// queue is being executed on the GPU
	if (FAILED(commandQueue->Signal(mFences[frameIndex], mFenceValues[frameIndex])))
		return false;

	return true;
}

bool Renderer::Present()
{
	// present the current backbuffer
	if (CheckError(mSwapChain->Present(0, 0)))
		return false;

	mCurrentFramebufferIndex = (mCurrentFramebufferIndex + 1) % mFrameCount;
	mFrameCountSinceGameStart++;
	return true;
}

void Renderer::WriteTextureDataToBufferSync(ID3D12Resource* dstBuffer, D3D12_RESOURCE_DESC textureDesc, vector<void*>& srcData, vector<int>& srcBytePerRow, vector<int>& srcBytePerSlice)
{
	int numSubresources = srcData.size();
	fatalAssert(numSubresources > 0 && numSubresources == srcBytePerRow.size() && numSubresources == srcBytePerSlice.size());

	CommandList commandList = BeginSingleTimeCommandsInternal();

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layouts = new D3D12_PLACED_SUBRESOURCE_FOOTPRINT[numSubresources];
	UINT* numRows = new UINT[numSubresources];
	UINT64* rowSizesInBytes = new UINT64[numSubresources];
	UINT64 totalBytes = 0;
	mDevice->GetCopyableFootprints(&textureDesc, 0, numSubresources, 0, layouts, numRows, rowSizesInBytes, &totalBytes);

	// creaete an upload buffer
	ID3D12Resource* uploadBuffer;
	fatalAssert(!CheckError(mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(totalBytes), // resource description for a buffer (storing the image data in this heap just to copy to the default heap)
		Renderer::TranslateResourceLayout(ResourceLayout::UPLOAD), // we will copy the contents from this heap to the default heap above
		nullptr,
		IID_PPV_ARGS(&uploadBuffer)),
		mDevice)
	);

	D3D12_SUBRESOURCE_DATA* data = new D3D12_SUBRESOURCE_DATA[numSubresources];
	for (int i = 0; i < numSubresources; i++)
	{
		data[i].pData = srcData[i]; // pointer to our image data
		data[i].RowPitch = srcBytePerRow[i]; // number of bytes per row
		data[i].SlicePitch = srcBytePerSlice[i]; // number of bytes per slice
	}

	UINT64 r = UpdateSubresources(commandList.GetImpl(), dstBuffer, uploadBuffer, 0, numSubresources, totalBytes, layouts, numRows, rowSizesInBytes, data);

	EndSingleTimeCommandsInternal(mGraphicsCommandQueue, commandList);
	SAFE_RELEASE_NO_CHECK(uploadBuffer);
	delete[] layouts;
	delete[] numRows;
	delete[] rowSizesInBytes;
	delete[] data;
}

void Renderer::WriteDataToBufferAsync(CommandList commandList, ID3D12Resource* dstBuffer, ID3D12Resource* uploadBuffer, void* srcData, int srcSizeInByte)
{
	fatalAssert(srcSizeInByte > 0);
	CD3DX12_RANGE readRange(0, 0); // "It is valid to specify the CPU won't read any data by passing a range where End is less than or equal to Begin."
	void* cpuAddress;
	uploadBuffer->Map(0, &readRange, &cpuAddress);
	if (!srcData)
		memset(cpuAddress, 0, srcSizeInByte);
	else
		memcpy(cpuAddress, srcData, srcSizeInByte);
	uploadBuffer->Unmap(0, nullptr); // "A null pointer indicates the entire subresource might have been modified by the CPU."
	commandList.GetImpl()->CopyBufferRegion(dstBuffer, 0, uploadBuffer, 0, srcSizeInByte);
}

void Renderer::WriteDataToBufferSync(ID3D12Resource* dstBuffer, void* srcData, int srcSizeInByte)
{
	CommandList commandList = BeginSingleTimeCommandsInternal();
	// creaete an upload buffer
	ID3D12Resource* uploadBuffer;
	UINT64 uploadBufferSize = GetRequiredIntermediateSize(dstBuffer, 0, 1);
	fatalAssert(!CheckError(mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize), // resource description for a buffer (storing the image data in this heap just to copy to the default heap)
		Renderer::TranslateResourceLayout(ResourceLayout::UPLOAD), // we will copy the contents from this heap to the default heap above
		nullptr,
		IID_PPV_ARGS(&uploadBuffer)),
		mDevice)
	);
	WriteDataToBufferAsync(commandList, dstBuffer, uploadBuffer, srcData, srcSizeInByte);
	EndSingleTimeCommandsInternal(mGraphicsCommandQueue, commandList);
	SAFE_RELEASE_NO_CHECK(uploadBuffer);
}

void Renderer::PrepareToReadDataFromBuffer(CommandList commandList, ID3D12Resource* srcBuffer, ID3D12Resource* readbackBuffer, int dstSizeInByte)
{
	fatalAssert(dstSizeInByte > 0);
	commandList.GetImpl()->CopyBufferRegion(readbackBuffer, 0, srcBuffer, 0, dstSizeInByte);
}

void Renderer::ReadDataFromReadbackBuffer(ID3D12Resource* readbackBuffer, void* dstData, int dstSizeInByte)
{
	fatalAssert(dstSizeInByte > 0);
	void* cpuAddress;
	readbackBuffer->Map(0, nullptr, &cpuAddress); // "A null pointer indicates the entire subresource might be read by the CPU."
	memcpy(dstData, cpuAddress, dstSizeInByte);
	CD3DX12_RANGE readRange(0, 0);
	readbackBuffer->Unmap(0, &readRange); // "It is valid to specify the CPU didn't write any data by passing a range where End is less than or equal to Begin."
}

void Renderer::ReadDataFromBufferSync(ID3D12Resource* srcBuffer, void* dstData, int dstSizeInByte)
{
	fatalAssert(dstSizeInByte > 0);
	CommandList commandList = BeginSingleTimeCommandsInternal();
	// creaete an stage buffer
	ID3D12Resource* readbackBuffer;
	UINT64 readbackBufferSize = GetRequiredIntermediateSize(srcBuffer, 0, 1);
	fatalAssert(!CheckError(mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), // upload heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(readbackBufferSize), // resource description for a buffer (storing the image data in this heap just to copy to the default heap)
		Renderer::TranslateResourceLayout(ResourceLayout::COPY_DST), // we will copy the contents from this heap to the default heap above
		nullptr,
		IID_PPV_ARGS(&readbackBuffer)),
		mDevice)
	);
	PrepareToReadDataFromBuffer(commandList, srcBuffer, readbackBuffer, dstSizeInByte);
	EndSingleTimeCommandsInternal(mGraphicsCommandQueue, commandList);
	ReadDataFromReadbackBuffer(readbackBuffer, dstData, dstSizeInByte);
	SAFE_RELEASE_NO_CHECK(readbackBuffer);
}

bool Renderer::TransitionLayout(ID3D12Resource* resource, u32 subresource, ResourceLayout oldLayout, ResourceLayout newLayout, CD3DX12_RESOURCE_BARRIER& barrier)
{
	// TODO: this is currently not thread safe, improve it
	assertf2(oldLayout != newLayout, "new layout is the same as the current layout");
	if (oldLayout == newLayout)
		return false;
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, Renderer::TranslateResourceLayout(oldLayout), Renderer::TranslateResourceLayout(newLayout), subresource);
	return true;
}

bool Renderer::BarrierWithoutTransition(ID3D12Resource* resource, CD3DX12_RESOURCE_BARRIER& barrier)
{
	// TODO: this is currently not thread safe, improve it
	barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource);
	return true;
}

u32 Renderer::CalculateSubresource(u32 depthSlice, u32 mipSlice, u32 depth, u32 mipLevelCount)
{
	return D3D12CalcSubresource(mipSlice, depthSlice, 0, mipLevelCount, depth);
}

bool Renderer::TransitionSingleTime(ID3D12Resource* resource, u32 subresource, ResourceLayout oldLayout, ResourceLayout newLayout)
{
	CD3DX12_RESOURCE_BARRIER barrier = {};
	if (TransitionLayout(resource, subresource, oldLayout, newLayout, barrier))
	{
		CommandList commandList = BeginSingleTimeCommandsInternal();
		commandList.GetImpl()->ResourceBarrier(1, &barrier);
		EndSingleTimeCommandsInternal(mGraphicsCommandQueue, commandList);
		return true;
	}
	return false;
}

bool Renderer::RecordTransition(CommandList commandList, ID3D12Resource* resource, u32 subresource, ResourceLayout oldLayout, ResourceLayout newLayout)
{
	CD3DX12_RESOURCE_BARRIER barrier = {};
	if (TransitionLayout(resource, subresource, oldLayout, newLayout, barrier))
	{
		commandList.GetImpl()->ResourceBarrier(1, &barrier);
		return true;
	}
	return false;
}

bool Renderer::RecordBarrierWithoutTransition(CommandList commandList, ID3D12Resource* resource)
{
	// make sure this resource is accessed through uav when calling this
	CD3DX12_RESOURCE_BARRIER barrier = {};
	if (BarrierWithoutTransition(resource, barrier))
	{
		commandList.GetImpl()->ResourceBarrier(1, &barrier);
		return true;
	}
	return false;
}

void Renderer::RecordResolve(CommandList commandList, ID3D12Resource* src, u32 srcSubresource, ID3D12Resource* dst, u32 dstSubresource, Format format)
{
	commandList.GetImpl()->ResolveSubresource(dst, dstSubresource, src, srcSubresource, Renderer::TranslateFormat(format));
}

CommandList Renderer::BeginSingleTimeCommandsInternal()
{
	CommandList newCommandList;
	newCommandList.InitCommandList(this);
	mSingleTimeCommandLists.push(newCommandList);
	newCommandList.Reset();
	return newCommandList;
}

void Renderer::EndSingleTimeCommandsInternal(ID3D12CommandQueue* commandQueue, CommandList commandList)
{
	CommandList currentSingleCommandList = mSingleTimeCommandLists.top();
	fatalAssert(currentSingleCommandList == commandList, "single time command list begin/end pair mismatch!");
	mSingleTimeCommandLists.pop();
	ID3D12Fence* fence;
	HANDLE fenceEvent;
	fatalAssert(!CheckError(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))); //initialize 0
	fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	commandList.Close();
	ID3D12CommandList* commandLists[] = { commandList.GetImpl() };
	commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
	commandQueue->Signal(fence, 1); // singal 1
	UINT64 complete = fence->GetCompletedValue();
	if(complete < 1) { // if fence value is 0
		fence->SetEventOnCompletion(1, fenceEvent); // trigger event once fence value is 1
		WaitForSingleObject(fenceEvent, INFINITE); // wait for event
	}
	commandList.Reset();
	commandList.Release();
	SAFE_RELEASE_NO_CHECK(fence);
}

void Renderer::EnableDebugLayer()
{
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&mDebugController))))
		mDebugController->EnableDebugLayer();
	else
		displayfln("can't enable debug layer");
}

void Renderer::EnableGBV()
{
	ID3D12Debug* spDebugController0;
	ID3D12Debug1* spDebugController1;
	HRESULT hr;

	hr = D3D12GetDebugInterface(IID_PPV_ARGS(&spDebugController0));
	if (CheckError(hr))
		displayfln("can't enable GBV");

	hr = spDebugController0->QueryInterface(IID_PPV_ARGS(&spDebugController1));
	if (CheckError(hr))
		displayfln("can't enable GBV");

	spDebugController1->SetEnableGPUBasedValidation(true);
}

bool Renderer::WaitForFrame(int frameIndex)
{
	HRESULT hr;

	// if the current fence value is still less than "fenceValue", then we know the GPU has not finished executing
	// the command queue since it has not reached the "commandQueue->Signal(fence, fenceValue)" command
	if (mFences[frameIndex]->GetCompletedValue() < mFenceValues[frameIndex])
	{
		// we have the fence create an event which is signaled once the fence's current value is "fenceValue"
		hr = mFences[frameIndex]->SetEventOnCompletion(mFenceValues[frameIndex], mFenceEvent);
		if (FAILED(hr))
			return false;

		// We will wait until the fence has triggered the event that it's current value has reached "fenceValue". once it's value
		// has reached "fenceValue", we know the command queue has finished executing
		WaitForSingleObject(mFenceEvent, INFINITE);
	}

	// increment fenceValue for next frame
	mFenceValues[frameIndex]++;

	return true;
}

void Renderer::BeginFrame(int frameIndex)
{
	// copy allocated dynamic descriptors during last frame to static descriptors for the current frame
	mStore->RefreshStaticDescriptors(frameIndex);
	// reset the dynamic descriptor heaps for the current frame
	mDynamicCbvSrvUavDescriptorHeap[frameIndex].Reset();
	mDynamicRtvDescriptorHeap[frameIndex].Reset();
	mDynamicDsvDescriptorHeap[frameIndex].Reset();
	mDynamicUniformBuffers[frameIndex].Reset();
}

void Renderer::EndFrame(int frameIndex)
{
}

void Renderer::CreateDescriptorHeap(ID3D12DescriptorHeap** impl, DescriptorHeap::Type type, int size)
{
	assertf(size > 0, "can't create 0 size descriptor heap");
	if (size <= 0)
		size = 1;
	HRESULT hr;
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = size;
	switch (type)
	{
	case DescriptorHeap::Type::CBV_SRV_UAV:
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		break;
	case DescriptorHeap::Type::SAMPLER:
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		break;
	case DescriptorHeap::Type::RTV:
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		break;
	case DescriptorHeap::Type::DSV:
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		break;
	default:
		break;
	}
	fatalAssert(SUCCEEDED(mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(impl))));
}

void Renderer::CreateGraphicsPSO(
	Pass& pass,
	ID3D12PipelineState** pso,
	ID3D12RootSignature* rootSignature,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveType,
	Shader* vertexShader,
	Shader* hullShader,
	Shader* domainShader,
	Shader* geometryShader,
	Shader* pixelShader,
	const string& name)
{
	// initialize
	vector<DXGI_FORMAT> rtvFormatVec;
	DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN;
	D3D12_BLEND_DESC blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // TODO: alpha to coverage is disabled by default, add it as a member variable to pass class if needed in the future
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = Renderer::TranslateDepthStencilState(DepthStencilState::None());
	int multiSampleCount = -1;
	vector<ShaderTarget>& shaderTargets = pass.GetShaderTargets();
	
	int renderTargetCount = 0;
	int depthStencilCount = 0;
	int depthStencilIndex = -1;
	if (shaderTargets.size() > 0) 
	{ 
		// if render textures are used
		multiSampleCount = shaderTargets[0].mRenderTexture->GetMultiSampleCount();
		if (shaderTargets.size() > 1)
			blendDesc.IndependentBlendEnable = true; // independent blend is disabled by default, turn it on when we have more than 1 render targets, TODO: add it as a member variable to pass class

		// assign to render textures parameter
		for (int i = 0; i < shaderTargets.size(); i++) // only the first 8 targets will be used
		{
			if (shaderTargets[i].mRenderTexture->IsRenderTargetUsed())
			{
				if (renderTargetCount < 8)
				{
					// render targets recording
					blendDesc.RenderTarget[renderTargetCount] = Renderer::TranslateBlendState(shaderTargets[i].mBlendState);
					rtvFormatVec.push_back(Renderer::TranslateFormat(shaderTargets[i].mRenderTexture->GetRenderTargetFormat()));
				}
				renderTargetCount++;
			}
		}

		for (int i = 0; i < shaderTargets.size(); i++) // only the first 8 targets will be used
		{
			if (shaderTargets[i].mRenderTexture->IsDepthStencilUsed())
			{
				//!!!* only the first usable depth stencil buffer will be used *!!!//
				if (depthStencilCount == 0)
				{
					dsvFormat = Renderer::TranslateFormat(shaderTargets[i].mRenderTexture->GetDepthStencilFormat());
					depthStencilIndex = i;
				}
				depthStencilCount++;
			}
		}

		fatalAssertf(renderTargetCount == pass.GetRenderTargetCount(), "render target count mismatch!");
		fatalAssertf(depthStencilCount == pass.GetDepthStencilCount(), "depth stencil count mismatch!");
		fatalAssertf(depthStencilCount <= 0 || depthStencilIndex == pass.GetDepthStencilIndex(), "depth stencil index mismatch!");

		if (depthStencilCount > 0)
			depthStencilDesc = Renderer::TranslateDepthStencilState(shaderTargets[depthStencilIndex].mDepthStencilState);
	}
	
	if (renderTargetCount == 0 && pass.UseRenderTarget())
	{
		// use backbuffer parameters
		rtvFormatVec.clear();
		rtvFormatVec.push_back(Renderer::TranslateFormat(mColorBufferFormat));
		blendDesc.RenderTarget[0] = Renderer::TranslateBlendState(mBlendState);
	}

	if (depthStencilCount == 0 && pass.UseDepthStencil())
	{
		// use backbuffer parameters
		dsvFormat = Renderer::TranslateFormat(mDepthStencilBufferFormat);
		depthStencilDesc = Renderer::TranslateDepthStencilState(mDepthStencilState);
	}

	if (multiSampleCount < 0)
		multiSampleCount = mMultiSampleCount;

	// create PSO
	CreateGraphicsPSOInternal(
		mDevice,
		pso,
		rootSignature,
		primitiveType,
		blendDesc,
		depthStencilDesc,
		rtvFormatVec,
		dsvFormat,
		multiSampleCount,
		vertexShader,
		hullShader,
		domainShader,
		geometryShader,
		pixelShader,
		name);
}

void Renderer::CreateComputePSO(
	Pass& pass,
	ID3D12PipelineState** pso,
	ID3D12RootSignature* rootSignature,
	Shader* computeShader,
	const string& name)
{
	// create PSO
	CreateComputePSOInternal(
		mDevice,
		pso,
		rootSignature,
		computeShader,
		name);
}

void Renderer::CreateGraphicsPSOInternal(
	ID3D12Device* device,
	ID3D12PipelineState** pso,
	ID3D12RootSignature* rootSignature,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveType,
	D3D12_BLEND_DESC blendDesc,
	D3D12_DEPTH_STENCIL_DESC dsDesc,
	const vector<DXGI_FORMAT>& rtvFormatVec,
	DXGI_FORMAT dsvFormat,
	int multiSampleCount,
	Shader* vertexShader,
	Shader* hullShader,
	Shader* domainShader,
	Shader* geometryShader,
	Shader* pixelShader,
	const string& name)
{
	// create input layout
	// fill out an input layout description structure
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};

	// we can get the number of elements in an array by "sizeof(array) / sizeof(arrayElementType)"
	inputLayoutDesc.NumElements = _countof(VertexInputLayout);
	inputLayoutDesc.pInputElementDescs = VertexInputLayout;

	// create a pipeline state object (PSO)

	// In a real application, you will have many pso's. for each different shader
	// or different combinations of shaders, different blend states or different rasterizer states,
	// different topology types (point, line, triangle, patch), or a different number
	// of render targets you will need a pso

	// VS is the only required shader for a pso. You might be wondering when a case would be where
	// you only set the VS. It's possible that you have a pso that only outputs data with the stream
	// output, and not on a render target, which means you would not need anything after the stream
	// output.

	DXGI_SAMPLE_DESC sampleDesc = {};
	sampleDesc.Count = multiSampleCount;
	sampleDesc.Quality = 0;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {}; // a structure to define a pso
	psoDesc.InputLayout = inputLayoutDesc; // the structure describing our input layout
	psoDesc.pRootSignature = rootSignature; // the root signature that describes the input data this pso needs
	if (vertexShader != nullptr) psoDesc.VS = vertexShader->GetShaderByteCode(); // structure describing where to find the vertex shader bytecode and how large it is
	if (hullShader != nullptr) psoDesc.HS = hullShader->GetShaderByteCode();
	if (domainShader != nullptr) psoDesc.DS = domainShader->GetShaderByteCode();
	if (geometryShader != nullptr) psoDesc.GS = geometryShader->GetShaderByteCode();
	if (pixelShader != nullptr) psoDesc.PS = pixelShader->GetShaderByteCode(); // same as VS but for pixel shader
	psoDesc.PrimitiveTopologyType = primitiveType;// D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // type of topology we are drawing
	for (int i = 0; i < rtvFormatVec.size(); i++)
	{
		psoDesc.RTVFormats[i] = rtvFormatVec[i]; // format of the render target
	}
	psoDesc.SampleDesc = sampleDesc; // must be the same sample description as the render target
	psoDesc.SampleMask = (UINT)-1; // sample mask has to do with multi-sampling. 0xffffffff means point sampling is done
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
	// For feature levels 9.1, 9.2, 9.3, and 10.0, if you set MultisampleEnable to FALSE, 
	// the runtime renders all points, lines, and triangles without anti - aliasing even for render targets with a sample count greater than 1. 
	// For feature levels 10.1 and higher, the setting of MultisampleEnable has no effect on points and triangles with regard to MSAA and impacts only the selection of the line - rendering algorithms.
	//psoDesc.RasterizerState.MultisampleEnable = multiSampleCount > 1;
	psoDesc.BlendState = blendDesc;
	psoDesc.NumRenderTargets = rtvFormatVec.size();
	psoDesc.DepthStencilState = dsDesc;// CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
	psoDesc.DSVFormat = dsvFormat;

	// create the pso
	HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pso));
	fatalAssert(!CheckError(hr));
	(*pso)->SetName(StringToWstring(name).c_str());
}

void Renderer::CreateComputePSOInternal(
	ID3D12Device* device,
	ID3D12PipelineState** pso,
	ID3D12RootSignature* rootSignature,
	Shader* computeShader,
	const string& name)
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSignature;
	psoDesc.CS = computeShader->GetShaderByteCode();
	psoDesc.NodeMask = 0; // For single GPU operation, set this to zero.
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE; // TODO: investigate D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG
	// create the pso
	HRESULT hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(pso));
	fatalAssert(!CheckError(hr));
	(*pso)->SetName(StringToWstring(name).c_str());
}

void Renderer::RecordGraphicsPassInstanced(
	Pass& pass,
	CommandList commandList,
	u32 instanceCount,
	bool clearColor,
	bool clearDepth,
	bool clearStencil,
	XMFLOAT4 clearColorValue,
	float clearDepthValue,
	u8 clearStencilValue)
{
	// flush dirty uniforms
	if (pass.GetScene()->IsUniformDirty(mCurrentFramebufferIndex))
		pass.GetScene()->UpdateUniformBuffer(mCurrentFramebufferIndex);

	pass.FlushDynamicDescriptors(mCurrentFramebufferIndex);

	// set PSO
	commandList.GetImpl()->SetPipelineState(pass.GetPso());

	// set the render target for the output merger stage (the output of the pipeline), viewports and scissor rects
	const vector<DescriptorHeap::Handle>& passRtvHandles = pass.GetRtvHandles(mCurrentFramebufferIndex);
	const vector<DescriptorHeap::Handle>& passDsvHandles = pass.GetDsvHandles(mCurrentFramebufferIndex);//!!!* only the first depth stencil buffer will be used *!!!//
	vector<D3D12_CPU_DESCRIPTOR_HANDLE> frameRtvHandles;
	D3D12_CPU_DESCRIPTOR_HANDLE frameDsvHandle;
	fatalAssert(pass.GetShaderTargetCount() == passRtvHandles.size());
	fatalAssert(pass.GetShaderTargetCount() == passDsvHandles.size());
	
	int renderTargetCount = 0;
	for (int i = 0; i < pass.GetShaderTargetCount(); i++)
	{
		if (pass.GetShaderTarget(i).IsRenderTargetUsed())
		{
			fatalAssertf(pass.GetShaderTarget(i).GetHeight() == pass.GetCamera()->GetHeight() &&
				pass.GetShaderTarget(i).GetWidth() == pass.GetCamera()->GetWidth(),
				"render texture's dimension doesn't match the viewport's dimension!");
			if(renderTargetCount < 8)
				frameRtvHandles.push_back(passRtvHandles[i].GetCpuHandle());
			renderTargetCount++;
		}
	}

	//!!!* only the first usable depth stencil buffer will be used *!!!//
	int depthStencilCount = 0;
	int depthStencilIndex = -1;
	for(int i = 0;i< pass.GetShaderTargetCount();i++)
	{
		if(pass.GetShaderTarget(i).IsDepthStencilUsed())
		{
			fatalAssertf(pass.GetShaderTarget(i).GetHeight() == pass.GetCamera()->GetHeight() &&
				pass.GetShaderTarget(i).GetWidth() == pass.GetCamera()->GetWidth(),
				"render texture's dimension doesn't match the viewport's dimension!");
			if (depthStencilCount == 0)
			{
				frameDsvHandle = passDsvHandles[i].GetCpuHandle();
				depthStencilIndex = i;
			}
			depthStencilCount++;
		}
	}

	fatalAssert(renderTargetCount == pass.GetRenderTargetCount());
	fatalAssert(depthStencilCount == pass.GetDepthStencilCount());
	fatalAssert(depthStencilIndex == pass.GetDepthStencilIndex());

	// if the pass has outputs but none are bound, use swap chain back buffer instead
	if (pass.GetRenderTargetCount() == 0 && pass.UseRenderTarget())
	{
		fatalAssertf(mHeight == pass.GetCamera()->GetHeight() &&
			mWidth == pass.GetCamera()->GetWidth(),
			"render texture's dimension doesn't match the viewport's dimension!");
		frameRtvHandles.clear();
		frameRtvHandles.push_back(IsResolveNeeded() ? mPreResolvedRtvHandles[mCurrentFramebufferIndex].GetCpuHandle() : mRtvHandles[mCurrentFramebufferIndex].GetCpuHandle());
	}

	if (pass.GetDepthStencilCount() == 0 && pass.UseDepthStencil())
	{
		fatalAssertf(mHeight == pass.GetCamera()->GetHeight() &&
			mWidth == pass.GetCamera()->GetWidth(),
			"render texture's dimension doesn't match the viewport's dimension!");
		frameDsvHandle = mDsvHandles[mCurrentFramebufferIndex].GetCpuHandle(); 
		// assuming backbuffer always supports depth stencil buffer
		fatalAssert(mDepthStencilBuffers[mCurrentFramebufferIndex]);
		depthStencilIndex = 0;
	}

	int numViewPorts = frameRtvHandles.size() > 0 ? frameRtvHandles.size() : 1;
	// duplicate the same viewport and scissor rect for all render targets, we don't support different viewports at the moment
	vector<D3D12_VIEWPORT> frameViewPorts(numViewPorts, TranslateViewport(pass.GetCamera()->GetViewport()));
	vector<D3D12_RECT> frameScissorRects(numViewPorts, TranslateScissorRect(pass.GetCamera()->GetScissorRect()));
	commandList.GetImpl()->OMSetRenderTargets(frameRtvHandles.size(), frameRtvHandles.data(), FALSE, depthStencilIndex >= 0 ? &frameDsvHandle : nullptr);
	commandList.GetImpl()->RSSetViewports(frameViewPorts.size(), frameViewPorts.data()); // set the viewports
	commandList.GetImpl()->RSSetScissorRects(frameScissorRects.size(), frameScissorRects.data()); // set the scissor rects
	if (pass.IsConstantBlendFactorsUsed(mCurrentFramebufferIndex))
		commandList.GetImpl()->OMSetBlendFactor(pass.GetConstantBlendFactors(mCurrentFramebufferIndex));
	if (pass.IsStencilReferenceUsed(mCurrentFramebufferIndex))
		commandList.GetImpl()->OMSetStencilRef(pass.GetStencilReference(mCurrentFramebufferIndex));

	// Clear the render targets
	if (clearColor)
	{
		fatalAssert(pass.UseRenderTarget());
		const float clearColorValueV[] = { clearColorValue.x, clearColorValue.y, clearColorValue.z, clearColorValue.w };
		for (int i = 0; i < frameRtvHandles.size(); i++)
		{
			// clear the render target buffer
			commandList.GetImpl()->ClearRenderTargetView(frameRtvHandles[i], clearColorValueV, 0, nullptr);
		}
	}

	if (clearDepth || clearStencil)
	{
		fatalAssert(pass.UseDepthStencil());
		// clear the depth/stencil buffer
		if (clearDepth || clearStencil)
		{
			D3D12_CLEAR_FLAGS flag = clearDepth ? (clearStencil ? D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL : D3D12_CLEAR_FLAG_DEPTH) : D3D12_CLEAR_FLAG_STENCIL;
			commandList.GetImpl()->ClearDepthStencilView(frameDsvHandle, flag, clearDepthValue, clearStencilValue, 0, nullptr);
		}
	}

	// set root signature
	commandList.GetImpl()->SetGraphicsRootSignature(pass.GetRootSignature()); // set the root signature

	// set descriptor heap
	ID3D12DescriptorHeap* dynamicHeaps[] = { mDynamicCbvSrvUavDescriptorHeap[mCurrentFramebufferIndex].GetImpl(), mSamplerDescriptorHeap.GetImpl() }; // sampler is always static
	ID3D12DescriptorHeap* staticHeaps[] = { mCbvSrvUavDescriptorHeap.GetImpl(), mSamplerDescriptorHeap.GetImpl() };
	if (pass.mResourceDirtyFlag[mCurrentFramebufferIndex] & ResourceDirtyFlag::SHADER_RESOURCE_DIRTY_FRAME ||
		pass.mResourceDirtyFlag[mCurrentFramebufferIndex] & ResourceDirtyFlag::WRITE_TARGET_DIRTY_FRAME)
		commandList.GetImpl()->SetDescriptorHeaps(_countof(dynamicHeaps), dynamicHeaps);
	else
		commandList.GetImpl()->SetDescriptorHeaps(_countof(staticHeaps), staticHeaps);

	// set descriptors
	commandList.GetImpl()->SetGraphicsRootConstantBufferView(GetUniformSlot(RegisterSpace::SCENE, RegisterType::CBV), pass.GetScene()->GetUniformBufferGpuAddress(mCurrentFramebufferIndex));
	commandList.GetImpl()->SetGraphicsRootConstantBufferView(GetUniformSlot(RegisterSpace::FRAME, RegisterType::CBV), mFrames[mCurrentFramebufferIndex].GetUniformBufferGpuAddress());
	commandList.GetImpl()->SetGraphicsRootConstantBufferView(GetUniformSlot(RegisterSpace::PASS, RegisterType::CBV), pass.GetUniformBufferGpuAddress(mCurrentFramebufferIndex));
	
	if (pass.GetScene()->GetTextureCount()) {
		commandList.GetImpl()->SetGraphicsRootDescriptorTable(GetUniformSlot(RegisterSpace::SCENE, RegisterType::SRV), pass.GetScene()->GetCbvSrvUavDescriptorHeapTableHandle(mCurrentFramebufferIndex).GetGpuHandle());
		commandList.GetImpl()->SetGraphicsRootDescriptorTable(GetUniformSlot(RegisterSpace::SCENE, RegisterType::SAMPLER), pass.GetScene()->GetSamplerDescriptorHeapTableHandle(mCurrentFramebufferIndex).GetGpuHandle());
	}
	
	if (mFrames[mCurrentFramebufferIndex].GetTextureCount()) {
		commandList.GetImpl()->SetGraphicsRootDescriptorTable(GetUniformSlot(RegisterSpace::FRAME, RegisterType::SRV), mFrames[mCurrentFramebufferIndex].GetCbvSrvUavDescriptorHeapTableHandle().GetGpuHandle());
	}
	
	if (pass.GetShaderResourceCount()) {
		commandList.GetImpl()->SetGraphicsRootDescriptorTable(GetUniformSlot(RegisterSpace::PASS, RegisterType::SRV), pass.GetSrvDescriptorHeapHandle(mCurrentFramebufferIndex).GetGpuHandle());
	}

	if (pass.GetWriteTargetCount()) {
		commandList.GetImpl()->SetGraphicsRootDescriptorTable(GetUniformSlot(RegisterSpace::PASS, RegisterType::UAV), pass.GetUavDescriptorHeapHandle(mCurrentFramebufferIndex).GetGpuHandle());
	}

	int meshCount = pass.GetMeshVec().size();
	for (int i = 0; i < meshCount; i++)
	{
		const Mesh* mesh = pass.GetMeshVec()[i];
		PrimitiveType meshPrimitiveType = mesh->GetPrimitiveType();
		if (verifyf(meshPrimitiveType == pass.GetPrimitiveType(), "mesh has a different primitive type from pass"))
		{
			commandList.GetImpl()->IASetPrimitiveTopology(Renderer::TranslatePrimitiveTopology(meshPrimitiveType));
			commandList.GetImpl()->IASetVertexBuffers(0, 1, &mesh->GetVertexBufferView());
			commandList.GetImpl()->IASetIndexBuffer(&mesh->GetIndexBufferView());
			commandList.GetImpl()->SetGraphicsRootConstantBufferView(GetUniformSlot(RegisterSpace::OBJECT, RegisterType::CBV), mesh->GetUniformBufferGpuAddress(mCurrentFramebufferIndex));
			commandList.GetImpl()->SetGraphicsRootDescriptorTable(GetUniformSlot(RegisterSpace::OBJECT, RegisterType::SRV), mesh->GetCbvSrvUavDescriptorHeapTableHandle(mCurrentFramebufferIndex).GetGpuHandle());
			string passFullname = pass.GetDebugName() + ":" + pass.GetGraphicShaderNames();
			GPU_MARKER(commandList, "%s", passFullname.c_str());
			commandList.GetImpl()->DrawIndexedInstanced(mesh->GetIndexCount(), instanceCount, 0, 0, 0);
		}
	}
}

void Renderer::RecordGraphicsPass(
	Pass& pass,
	CommandList commandList,
	bool clearColor,
	bool clearDepth,
	bool clearStencil,
	XMFLOAT4 clearColorValue,
	float clearDepthValue,
	u8 clearStencilValue)
{
	RecordGraphicsPassInstanced(pass, commandList, 1, clearColor, clearDepth, clearStencil, clearColorValue, clearDepthValue, clearStencilValue);
}

void Renderer::RecordComputePass(
	Pass& pass,
	CommandList commandList,
	int threadGroupCountX,
	int threadGroupCountY,
	int threadGroupCountZ)
{
	// D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION
	fatalAssert(threadGroupCountX < 65535 && threadGroupCountY < 65535 && threadGroupCountZ < 65535);

	// flush dirty uniforms
	if (pass.GetScene()->IsUniformDirty(mCurrentFramebufferIndex))
		pass.GetScene()->UpdateUniformBuffer(mCurrentFramebufferIndex);

	pass.FlushDynamicDescriptors(mCurrentFramebufferIndex);

	// set PSO
	commandList.GetImpl()->SetPipelineState(pass.GetPso());

	// set root signature
	commandList.GetImpl()->SetComputeRootSignature(pass.GetRootSignature()); // set the root signature

	// set descriptor heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavDescriptorHeap.GetImpl(), mSamplerDescriptorHeap.GetImpl() };
	commandList.GetImpl()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// set descriptors
	commandList.GetImpl()->SetComputeRootConstantBufferView(GetUniformSlot(RegisterSpace::SCENE, RegisterType::CBV), pass.GetScene()->GetUniformBufferGpuAddress(mCurrentFramebufferIndex));
	commandList.GetImpl()->SetComputeRootConstantBufferView(GetUniformSlot(RegisterSpace::FRAME, RegisterType::CBV), mFrames[mCurrentFramebufferIndex].GetUniformBufferGpuAddress());
	commandList.GetImpl()->SetComputeRootConstantBufferView(GetUniformSlot(RegisterSpace::PASS, RegisterType::CBV), pass.GetUniformBufferGpuAddress(mCurrentFramebufferIndex));

	if (pass.GetScene()->GetTextureCount()) {
		commandList.GetImpl()->SetComputeRootDescriptorTable(GetUniformSlot(RegisterSpace::SCENE, RegisterType::SRV), pass.GetScene()->GetCbvSrvUavDescriptorHeapTableHandle(mCurrentFramebufferIndex).GetGpuHandle());
		commandList.GetImpl()->SetComputeRootDescriptorTable(GetUniformSlot(RegisterSpace::SCENE, RegisterType::SAMPLER), pass.GetScene()->GetSamplerDescriptorHeapTableHandle(mCurrentFramebufferIndex).GetGpuHandle());
	}

	if (mFrames[mCurrentFramebufferIndex].GetTextureCount()) {
		commandList.GetImpl()->SetComputeRootDescriptorTable(GetUniformSlot(RegisterSpace::FRAME, RegisterType::SRV), mFrames[mCurrentFramebufferIndex].GetCbvSrvUavDescriptorHeapTableHandle().GetGpuHandle());
	}

	if (pass.GetShaderResourceCount()) {
		commandList.GetImpl()->SetComputeRootDescriptorTable(GetUniformSlot(RegisterSpace::PASS, RegisterType::SRV), pass.GetSrvDescriptorHeapHandle(mCurrentFramebufferIndex).GetGpuHandle());
	}

	if (pass.GetWriteTargetCount()) {
		commandList.GetImpl()->SetComputeRootDescriptorTable(GetUniformSlot(RegisterSpace::PASS, RegisterType::UAV), pass.GetUavDescriptorHeapHandle(mCurrentFramebufferIndex).GetGpuHandle());
	}

	string passFullname = pass.GetDebugName() + ":" + pass.GetComputeShaderName();
	GPU_MARKER(commandList, "%s", passFullname.c_str());
	commandList.GetImpl()->Dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
}

void Renderer::CreateRootSignature(
	ID3D12RootSignature** rootSignature,
	int maxSrvCount[(int)RegisterSpace::COUNT],
	int maxUavCount[(int)RegisterSpace::COUNT])
{
	D3D12_ROOT_PARAMETER  rootParameterArr[(int)RegisterType::COUNT * (int)RegisterSpace::COUNT]; // each register space has two types of uniforms, the constant buffer and descriptor table
	D3D12_DESCRIPTOR_RANGE  srvTableRanges[(int)RegisterSpace::COUNT];
	D3D12_DESCRIPTOR_RANGE  samplerTableRanges[(int)RegisterSpace::COUNT];
	D3D12_DESCRIPTOR_RANGE  uavTableRanges[(int)RegisterSpace::COUNT];
	for(int uType = 0;uType< (int)RegisterType::COUNT;uType++)
	{
		for(int uSpace = 0;uSpace< (int)RegisterSpace::COUNT;uSpace++)
		{
			const int slot = GetUniformSlot((RegisterSpace)uSpace, (RegisterType)uType);
			if (uType == (int)RegisterType::CBV)
			{
				rootParameterArr[slot].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
				rootParameterArr[slot].Descriptor.RegisterSpace = uSpace;
				rootParameterArr[slot].Descriptor.ShaderRegister = 0;
				rootParameterArr[slot].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			}
			else if (uType == (int)RegisterType::SRV)
			{
				srvTableRanges[uSpace].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				srvTableRanges[uSpace].NumDescriptors = maxSrvCount[uSpace] + 1; // plus 1 to account for unbounded resources and 0 resource situation
				srvTableRanges[uSpace].BaseShaderRegister = 0; // start index of the shader registers in the range
				srvTableRanges[uSpace].RegisterSpace = uSpace;
				srvTableRanges[uSpace].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // this appends the range to the end of the root signature descriptor tables
				
				rootParameterArr[slot].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				rootParameterArr[slot].DescriptorTable.NumDescriptorRanges = 1;
				rootParameterArr[slot].DescriptorTable.pDescriptorRanges = &srvTableRanges[uSpace]; // the pointer to the beginning of our ranges array
				rootParameterArr[slot].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			}
			else if (uType == (int)RegisterType::SAMPLER)
			{
				samplerTableRanges[uSpace].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
				samplerTableRanges[uSpace].NumDescriptors = maxSrvCount[uSpace] + 1; // plus 1 to account for unbounded resources and 0 resource situation
				samplerTableRanges[uSpace].BaseShaderRegister = 0; // start index of the shader registers in the range
				samplerTableRanges[uSpace].RegisterSpace = uSpace;
				samplerTableRanges[uSpace].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // this appends the range to the end of the root signature descriptor tables

				rootParameterArr[slot].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				rootParameterArr[slot].DescriptorTable.NumDescriptorRanges = 1;
				rootParameterArr[slot].DescriptorTable.pDescriptorRanges = &samplerTableRanges[uSpace]; // the pointer to the beginning of our ranges array
				rootParameterArr[slot].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			}
			else if (uType == (int)RegisterType::UAV)
			{
				uavTableRanges[uSpace].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
				uavTableRanges[uSpace].NumDescriptors = maxUavCount[uSpace] + 1; // plus 1 to account for unbounded resources and 0 resource situation
				uavTableRanges[uSpace].BaseShaderRegister = 0; // start index of the shader registers in the range
				uavTableRanges[uSpace].RegisterSpace = uSpace;
				uavTableRanges[uSpace].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // this appends the range to the end of the root signature descriptor tables

				rootParameterArr[slot].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				rootParameterArr[slot].DescriptorTable.NumDescriptorRanges = 1;
				rootParameterArr[slot].DescriptorTable.pDescriptorRanges = &uavTableRanges[uSpace]; // the pointer to the beginning of our ranges array
				rootParameterArr[slot].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			}
		}
	}

	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(_countof(rootParameterArr), rootParameterArr, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT // we can deny shader stages here for better performance
		// | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
		// | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
		// | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
	);

	ID3DBlob* errorBuff = nullptr; // a buffer holding the error data if any
	ID3DBlob* signature = nullptr;
	HRESULT hr;
	hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errorBuff);
	if (FAILED(hr))
		CheckError(hr, mDevice, errorBuff);

	hr = mDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(rootSignature));
	if (FAILED(hr))
		fatalf("create root signature failed");
}

int Renderer::EstimateTotalCbvSrvUavCount()
{
	int totalCbvSrvUavCount = mStore->EstimateTotalCbvSrvUavCount(mFrameCount);
	for (auto& frame : mFrames)
		totalCbvSrvUavCount += frame.GetTextureCount();
	return totalCbvSrvUavCount;
}

int Renderer::EstimateTotalSamplerCount()
{
	// only scene has samplers and they are all static
	return mStore->EstimateTotalSamplerCount(mFrameCount);
}

int Renderer::EstimateTotalRtvCount()
{
	int totalRtvCount = mStore->EstimateTotalRtvCount(mFrameCount);
	totalRtvCount += IsResolveNeeded() ? mFrameCount * 2 : mFrameCount; // swap chain frame buffer
	return totalRtvCount;
}

int Renderer::EstimateTotalDsvCount()
{
	// assume one rtv correspond to one dsv
	return EstimateTotalRtvCount();
}

CommandList Renderer::BeginSingleTimeCommands()
{
	return BeginSingleTimeCommandsInternal();
}

void Renderer::EndSingleTimeCommands()
{
	EndSingleTimeCommandsInternal(mGraphicsCommandQueue, mSingleTimeCommandLists.top());
}

D3D12_SAMPLER_DESC Sampler::GetDesc()
{ 
	return Renderer::TranslateSamplerDesc(*this); 
}
