#include "Renderer.h"
#include "Level.h"
#include "Scene.h"
#include "Frame.h"
#include "Pass.h"
#include "Mesh.h"
#include "Camera.h"
#include "Shader.h"
#include "Texture.h"

Sampler gSampler = {
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

Shader gDeferredVS(Shader::ShaderType::VERTEX_SHADER, L"vs_deferred.hlsl");
Mesh gCube(L"cube", Mesh::MeshType::CUBE, XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1));
Mesh gFullscreenTriangle(L"fullscreen_triangle", Mesh::MeshType::FULLSCREEN_TRIANGLE, XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1));
Camera gCameraDummy(XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), gWidth, gHeight, 45.0f, 100.0f, 0.1f);

DescriptorHeap::DescriptorHeap() : 
	mHead(D3D12_DEFAULT), 
	mFree(D3D12_DEFAULT), 
	mHandleIncrementSize(0), 
	mImpl(nullptr),
	mCount(0),
	mCountMax(0),
	mType(Type::INVALID)
{
}

DescriptorHeap::~DescriptorHeap()
{
	Release();
}

void DescriptorHeap::InitDescriptorHeap(Renderer* renderer, int countMax, Type type)
{
	mRenderer = renderer;
	mCount = 0;
	mCountMax = countMax;
	mType = type;
	mHandleIncrementSize = mRenderer->mDevice->GetDescriptorHandleIncrementSize(Renderer::TranslateDescriptorHeapType(mType));
	mRenderer->CreateDescriptorHeap(*this, mCountMax);
	mHead = mImpl->GetCPUDescriptorHandleForHeapStart();
	mHeadGpu = mImpl->GetGPUDescriptorHandleForHeapStart();
	mFree = mHead;
}

void DescriptorHeap::Release()
{
	SAFE_RELEASE(mImpl);
	mRenderer = nullptr;
	mHead = D3D12_DEFAULT;
	mFree = D3D12_DEFAULT;
	mHandleIncrementSize = 0;
	mImpl = nullptr;
	mCount = 0;
	mCountMax = 0;
	mType = Type::INVALID;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::AllocateDsv(ID3D12Resource* resource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc, int count)
{
	fatalAssert(mCount < mCountMax);
	mRenderer->mDevice->CreateDepthStencilView(resource, &desc, mFree);
	CD3DX12_CPU_DESCRIPTOR_HANDLE result = mFree;
	mFree.Offset(count, mHandleIncrementSize);
	mCount++;
	return result;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::AllocateRtv(ID3D12Resource* resource, const D3D12_RENDER_TARGET_VIEW_DESC& desc, int count)
{
	fatalAssert(mCount < mCountMax);
	mRenderer->mDevice->CreateRenderTargetView(resource, &desc, mFree);
	CD3DX12_CPU_DESCRIPTOR_HANDLE result = mFree;
	mFree.Offset(count, mHandleIncrementSize);
	mCount++;
	return result;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::AllocateRtv(ID3D12Resource* resource, int count)
{
	fatalAssert(mCount < mCountMax);
	mRenderer->mDevice->CreateRenderTargetView(resource, nullptr, mFree);
	CD3DX12_CPU_DESCRIPTOR_HANDLE result = mFree;
	mFree.Offset(count, mHandleIncrementSize);
	mCount++;
	return result;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::AllocateSrv(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc, int count)
{
	fatalAssert(mCount < mCountMax);
	mRenderer->mDevice->CreateShaderResourceView(resource, &desc, mFree);
	CD3DX12_CPU_DESCRIPTOR_HANDLE result = mFree;
	mFree.Offset(count, mHandleIncrementSize);
	mCount++;
	return result;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::AllocateSampler(const D3D12_SAMPLER_DESC& desc, int count)
{
	fatalAssert(mCount < mCountMax);
	mRenderer->mDevice->CreateSampler(&desc, mFree);
	CD3DX12_CPU_DESCRIPTOR_HANDLE result = mFree;
	mFree.Offset(count, mHandleIncrementSize);
	mCount++;
	return result;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCurrentFreeGpuAddress()
{
	CD3DX12_GPU_DESCRIPTOR_HANDLE result = {};
	result.ptr = mHeadGpu.ptr + (mFree.ptr - mHead.ptr);
	return result;
}

DescriptorHeap::Type DescriptorHeap::GetType()
{
	return mType;
}

ID3D12DescriptorHeap*& DescriptorHeap::GetHeapRef()
{
	return mImpl;
}

ID3D12DescriptorHeap* DescriptorHeap::GetHeap()
{
	return mImpl;
}

Renderer::Renderer()
{

}

Renderer::~Renderer()
{
	Release();
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
	case Format::D16_UNORM:
		return DXGI_FORMAT_R16_TYPELESS;
	case Format::D32_FLOAT:
		return DXGI_FORMAT_R32_TYPELESS;
	case Format::D24_UNORM_S8_UINT:
		return DXGI_FORMAT_R24G8_TYPELESS;
	default:
		fatalf("TranslateFormat wrong format");
	}
	return DXGI_FORMAT_UNKNOWN;
}

DXGI_FORMAT Renderer::TranslateToSrvFormat(Format format, ReadFrom readFrom)
{
	if (readFrom & ReadFrom::COLOR)
	{
		switch (format)
		{
		case Format::R8G8B8A8_UNORM:
		case Format::R16G16B16A16_UNORM:
		case Format::R16G16B16A16_FLOAT:
			return TranslateFormat(format);
		default:
			fatalf("read from wrong format");
		}
	}
	else if (readFrom & ReadFrom::DEPTH)
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
			fatalf("read from wrong format");
		}
	}
	else if (readFrom & ReadFrom::STENCIL)
	{
		switch (format)
		{
		case Format::D24_UNORM_S8_UINT:
			return DXGI_FORMAT_X24_TYPELESS_G8_UINT;
		default:
			fatalf("read from wrong format");
		}
	}
	else
		fatalf("read from wrong format");
	
	return DXGI_FORMAT_UNKNOWN;
}

D3D12_DESCRIPTOR_HEAP_TYPE Renderer::TranslateDescriptorHeapType(DescriptorHeap::Type type)
{
	switch (type)
	{
	case DescriptorHeap::Type::SRV_CBV_UAV:
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
	case TextureType::DEFAULT:
	case TextureType::CUBE:
		return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	case TextureType::VOLUME:
		return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	default:
		fatalf("wrong texture type");
	}
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Renderer::GetRtvHandle(int frameIndex)
{
	return mRtvHandles[frameIndex];
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Renderer::GetDsvHandle(int frameIndex)
{
	return mDsvHandles[frameIndex];
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

void Renderer::SetLevel(Level* level)
{
	mLevel = level;
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
	mFrameCountTotal = 0;
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
	mCurrentFrameIndex = mSwapChain->GetCurrentBackBufferIndex();

	// 4. create command allocators and command lists
	mGraphicsCommandAllocators.resize(mFrameCount);
	mCommandLists.resize(mFrameCount);
	for (int i = 0; i < mFrameCount; i++)
	{
		hr = mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mGraphicsCommandAllocators[i]));
		if (CheckError(hr))
			return false;
		hr = mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mGraphicsCommandAllocators[i], NULL, IID_PPV_ARGS(&mCommandLists[i]));
		if (CheckError(hr))
			return false;

		// command lists are created in open state
		// https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-close
		mCommandLists[i]->Close();
	}
	hr = mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mSingleTimeCommandAllocator)); // TODO: change to D3D12_COMMAND_LIST_TYPE_COPY only
	if (CheckError(hr))
		return false;

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
	mCbvSrvUavDescriptorHeap.InitDescriptorHeap(this, EstimateTotalCbvSrvUavCount(), DescriptorHeap::Type::SRV_CBV_UAV);
	mSamplerDescriptorHeap.InitDescriptorHeap(this, EstimateTotalSamplerCount(), DescriptorHeap::Type::SAMPLER);
	mRtvDescriptorHeap.InitDescriptorHeap(this, EstimateTotalRtvCount(), DescriptorHeap::Type::RTV);
	mDsvDescriptorHeap.InitDescriptorHeap(this, EstimateTotalDsvCount(), DescriptorHeap::Type::DSV);

	CreateColorBuffers();
	CreateDepthStencilBuffers(mDepthStencilBufferFormat);//hard coded to DXGI_FORMAT_D24_UNORM_S8_UINT
	CreatePreResolveBuffers(mColorBufferFormat);

	for(int i = 0;i<mFrames.size();i++)
	{
		mFrames[i].InitFrame(i, this, mCbvSrvUavDescriptorHeap, mSamplerDescriptorHeap, mRtvDescriptorHeap, mDsvDescriptorHeap);
	}

	// 7. init level
	mLevel->InitLevel(this, mFrameCount, mCbvSrvUavDescriptorHeap, mSamplerDescriptorHeap, mRtvDescriptorHeap, mDsvDescriptorHeap);

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
		wstring name(L"swap chain depth stencil buffer : ");
		mDepthStencilBuffers[i]->SetName((name + to_wstring(i)).c_str());
		mDsvHandles[i] = mDsvDescriptorHeap.AllocateDsv(mDepthStencilBuffers[i], dsViewDesc, 1);
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
		wstring name(L"swap chain pre resolve buffer : ");
		mPreResolveColorBuffers[i]->SetName((name + to_wstring(i)).c_str());
		mPreResolvedRtvHandles[i] = mRtvDescriptorHeap.AllocateRtv(mPreResolveColorBuffers[i], 1);
	}
}

void Renderer::CreateColorBuffers()
{
	mColorBuffers.resize(mFrameCount);
	mRtvHandles.resize(mFrameCount);
	for (int i = 0; i < mFrameCount; i++)
	{
		fatalAssert(SUCCEEDED(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mColorBuffers[i]))));
		mRtvHandles[i] = mRtvDescriptorHeap.AllocateRtv(mColorBuffers[i], 1);
	}
}

void Renderer::Release()
{
	// 1. release level
	SAFE_RELEASE(mLevel);

	// 2. release global GPU resources
	for (int i = 0; i < mFrameCount; i++)
	{
		SAFE_RELEASE(mColorBuffers[i]);
		SAFE_RELEASE(mDepthStencilBuffers[i]);
		mFrames[i].Release();
	}

	mCbvSrvUavDescriptorHeap.Release();
	mSamplerDescriptorHeap.Release();
	mDsvDescriptorHeap.Release();
	mRtvDescriptorHeap.Release();

	// 3. release fences
	if (mFenceEvent != NULL) {
		CloseHandle(mFenceEvent);
		mFenceEvent = NULL;
	}

	for (int i = 0; i < mFrameCount; i++)
	{
		SAFE_RELEASE(mFences[i]);
	}

	// 4. release command allocators
	SAFE_RELEASE(mSingleTimeCommandAllocator);
	for (int i = 0; i < mGraphicsCommandAllocators.size(); i++)
	{
		SAFE_RELEASE(mGraphicsCommandAllocators[i]);
	}

	// 5. release swap chain
	SAFE_RELEASE(mSwapChain);

	// 6. release command queue
	SAFE_RELEASE(mGraphicsCommandQueue);
	for (int i = 0; i < mCommandLists.size(); i++)
	{
		SAFE_RELEASE(mCommandLists[i]);
	}

	// 7. release device
	if (mDevice)
	{
		ID3D12DebugDevice* debugDev;
		bool hasDebugDev = false;
		if (SUCCEEDED(mDevice->QueryInterface(IID_PPV_ARGS(&debugDev))))
			hasDebugDev = true;
		SAFE_RELEASE(mDevice);
		if (hasDebugDev)
		{
			OutputDebugStringW(L"Debug Report Live Device Objects >>>>>>>>>>>>>>>>>>>>>>>>>>\n");
			debugDev->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
			OutputDebugStringW(L"Debug Report Live Device Objects >>>>>>>>>>>>>>>>>>>>>>>>>>\n");
		}
		SAFE_RELEASE(debugDev);
		SAFE_RELEASE(mDebugController);
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

bool Renderer::RecordBegin(int frameIndex, ID3D12GraphicsCommandList* commandList)
{
	// command lists are created in open state, we close them immediately after creation
	// create cl -> close cl
	//   reset allocator -> reset cl -> record -> close cl -> execute cl
	//   reset allocator -> reset cl -> record -> close cl -> execute cl
	//   reset allocator -> reset cl -> record -> close cl -> execute cl
	//   ...
	if (CheckError(mGraphicsCommandAllocators[frameIndex]->Reset()))
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
	if (CheckError(commandList->Reset(mGraphicsCommandAllocators[frameIndex], nullptr)))
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

bool Renderer::ResolveFrame(int frameIndex, ID3D12GraphicsCommandList* commandList)
{
	if (!IsResolveNeeded())
		return false;

	RecordTransition(commandList, GetPreResolveBuffer(frameIndex), 0, ResourceLayout::RENDER_TARGET, ResourceLayout::RESOLVE_SRC);
	RecordTransition(commandList, GetRenderTargetBuffer(frameIndex), 0, ResourceLayout::PRESENT, ResourceLayout::RESOLVE_DST);
	RecordResolve(commandList, GetPreResolveBuffer(frameIndex), 0, GetRenderTargetBuffer(frameIndex), 0, mColorBufferFormat);
	RecordTransition(commandList, GetRenderTargetBuffer(frameIndex), 0, ResourceLayout::RESOLVE_DST, ResourceLayout::RENDER_TARGET);
	
	return true;
}

bool Renderer::RecordEnd(int frameIndex, ID3D12GraphicsCommandList* commandList)
{
	// transition the "frameIndex" render target from the render target state to the present state. 
	// If the debug layer is enabled, you will receive a warning if present is called on the render target when it's not in the present state
	RecordTransition(commandList, GetRenderTargetBuffer(frameIndex), 0, ResourceLayout::RENDER_TARGET, ResourceLayout::PRESENT);

	if (CheckError(commandList->Close()))
		return false;

	return true;
}

bool Renderer::Submit(int frameIndex, ID3D12CommandQueue* commandQueue, vector<ID3D12GraphicsCommandList*> commandLists)
{
	// execute the array of command lists
	commandQueue->ExecuteCommandLists(commandLists.size(), (ID3D12CommandList**)commandLists.data());
		
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

	mCurrentFrameIndex = (mCurrentFrameIndex + 1) % mFrameCount;
	return true;
}

void Renderer::UploadTextureDataToBuffer(vector<void*>& srcData, vector<int>& srcBytePerRow, vector<int>& srcBytePerSlice, D3D12_RESOURCE_DESC textureDesc, ID3D12Resource* dstBuffer)
{
	int numSubresources = srcData.size();
	fatalAssert(numSubresources > 0 && numSubresources == srcBytePerRow.size() && numSubresources == srcBytePerSlice.size());

	ID3D12GraphicsCommandList* commandList = BeginSingleTimeCommands(mSingleTimeCommandAllocator);

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

	// TODO: add support to subresources
	UINT64 r = UpdateSubresources(commandList, dstBuffer, uploadBuffer, 0, numSubresources, totalBytes, layouts, numRows, rowSizesInBytes, data);

	EndSingleTimeCommands(mGraphicsCommandQueue, mSingleTimeCommandAllocator, commandList);
	SAFE_RELEASE(uploadBuffer);
	delete[] layouts;
	delete[] numRows;
	delete[] rowSizesInBytes;
	delete[] data;
}

//mainly for vertex buffer and index buffer for now
void Renderer::UploadDataToBuffer(void* srcData, int srcBytePerRow, int srcBytePerSlice, int uploadBufferSize, ID3D12Resource* dstBuffer)
{
	ID3D12GraphicsCommandList* commandList = BeginSingleTimeCommands(mSingleTimeCommandAllocator);
	
	// creaete an upload buffer
	ID3D12Resource* uploadBuffer;
	fatalAssert(!CheckError(mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize), // resource description for a buffer (storing the image data in this heap just to copy to the default heap)
		Renderer::TranslateResourceLayout(ResourceLayout::UPLOAD), // we will copy the contents from this heap to the default heap above
		nullptr,
		IID_PPV_ARGS(&uploadBuffer)),
		mDevice)
	);

	D3D12_SUBRESOURCE_DATA data = {};
	data.pData = srcData; // pointer to our image data
	data.RowPitch = srcBytePerRow; // number of bytes per row
	data.SlicePitch = srcBytePerSlice; // number of bytes per slice

	// TODO: add support to subresources
	UINT64 r = UpdateSubresources(commandList, dstBuffer, uploadBuffer, 0, 0, 1, &data);

	EndSingleTimeCommands(mGraphicsCommandQueue, mSingleTimeCommandAllocator, commandList);
	SAFE_RELEASE(uploadBuffer);
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

u32 Renderer::CalculateSubresource(u32 depthSlice, u32 mipSlice, u32 depth, u32 mipLevelCount)
{
	return D3D12CalcSubresource(mipSlice, depthSlice, 0, mipLevelCount, depth);
}

bool Renderer::TransitionSingleTime(ID3D12Resource* resource, u32 subresource, ResourceLayout oldLayout, ResourceLayout newLayout)
{
	CD3DX12_RESOURCE_BARRIER barrier = {};
	if (TransitionLayout(resource, subresource, oldLayout, newLayout, barrier))
	{
		ID3D12GraphicsCommandList* commandList = BeginSingleTimeCommands(mSingleTimeCommandAllocator);
		commandList->ResourceBarrier(1, &barrier);
		EndSingleTimeCommands(mGraphicsCommandQueue, mSingleTimeCommandAllocator, commandList);
		return true;
	}
	return false;
}

bool Renderer::RecordTransition(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource, u32 subresource, ResourceLayout oldLayout, ResourceLayout newLayout)
{
	CD3DX12_RESOURCE_BARRIER barrier = {};
	if(TransitionLayout(resource, subresource, oldLayout, newLayout, barrier))
	{
		commandList->ResourceBarrier(1, &barrier);
		return true;
	}
	return false;
}

void Renderer::RecordResolve(ID3D12GraphicsCommandList* commandList, ID3D12Resource* src, u32 srcSubresource, ID3D12Resource* dst, u32 dstSubresource, Format format)
{
	commandList->ResolveSubresource(dst, dstSubresource, src, srcSubresource, Renderer::TranslateFormat(format));
}

ID3D12GraphicsCommandList* Renderer::BeginSingleTimeCommands(ID3D12CommandAllocator* commandAllocator)
{
	ID3D12GraphicsCommandList* commandList = nullptr;
	fatalAssert(!CheckError(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, NULL, IID_PPV_ARGS(&commandList)), mDevice));
	return commandList;
}

void Renderer::EndSingleTimeCommands(ID3D12CommandQueue* commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12GraphicsCommandList* commandList)
{
	ID3D12Fence* fence;
	HANDLE fenceEvent;
	fatalAssert(!CheckError(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))); //initialize 0
	fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	commandList->Close();
	ID3D12CommandList* commandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
	commandQueue->Signal(fence, 1); // singal 1
	UINT64 complete = fence->GetCompletedValue();
	if(complete < 1) { // if fence value is 0
		fence->SetEventOnCompletion(1, fenceEvent); // trigger event once fence value is 1
		WaitForSingleObject(fenceEvent, INFINITE); // wait for event
	}
	commandAllocator->Reset();
	commandList->Reset(commandAllocator, nullptr);
	SAFE_RELEASE(fence);
	SAFE_RELEASE(commandList);
}

void Renderer::EnableDebugLayer()
{
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&mDebugController))))
		mDebugController->EnableDebugLayer();
	else
		displayf("can't enable debug layer");
}

void Renderer::EnableGBV()
{
	ID3D12Debug* spDebugController0;
	ID3D12Debug1* spDebugController1;
	HRESULT hr;

	hr = D3D12GetDebugInterface(IID_PPV_ARGS(&spDebugController0));
	if (CheckError(hr))
		displayf("can't enable GBV");

	hr = spDebugController0->QueryInterface(IID_PPV_ARGS(&spDebugController1));
	if (CheckError(hr))
		displayf("can't enable GBV");

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

void Renderer::CreateDescriptorHeap(DescriptorHeap& descriptorHeap, int size)
{
	assertf(size > 0, "can't create 0 size descriptor heap");
	if (size <= 0)
		size = 1;
	HRESULT hr;
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = size;
	switch (descriptorHeap.GetType())
	{
	case DescriptorHeap::Type::SRV_CBV_UAV:
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
	fatalAssert(SUCCEEDED(mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap.GetHeapRef()))));
}

void Renderer::CreatePSO(
	Pass& pass,
	ID3D12PipelineState** pso,
	ID3D12RootSignature* rootSignature,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveType,
	Shader* vertexShader,
	Shader* hullShader,
	Shader* domainShader,
	Shader* geometryShader,
	Shader* pixelShader,
	const wstring& name)
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
		depthStencilDesc = Renderer::TranslateDepthStencilState(shaderTargets[0].mDepthStencilState);
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
	}

	fatalAssertf(renderTargetCount == pass.GetRenderTargetCount(), "render target count mismatch!");
	fatalAssertf(depthStencilCount == pass.GetDepthStencilCount(), "depth stencil count mismatch!");
	fatalAssertf(depthStencilIndex == pass.GetDepthStencilIndex(), "depth stencil index mismatch!");

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
	CreatePSO(
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

void Renderer::CreatePSO(
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
	const wstring& name)
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

	(*pso)->SetName(name.c_str());
}

void Renderer::RecordPass(
	Pass& pass,
	ID3D12GraphicsCommandList* commandList,
	bool clearColor,
	bool clearDepth,
	bool clearStencil,
	XMFLOAT4 clearColorValue,
	float clearDepthValue,
	uint8_t clearStencilValue)
{
	// set PSO
	commandList->SetPipelineState(pass.GetPso());

	// set the render target for the output merger stage (the output of the pipeline), viewports and scissor rects
	const vector<CD3DX12_CPU_DESCRIPTOR_HANDLE>& passRtvHandles = pass.GetRtvHandles(mCurrentFrameIndex);
	const vector<CD3DX12_CPU_DESCRIPTOR_HANDLE>& passDsvHandles = pass.GetDsvHandles(mCurrentFrameIndex);//!!!* only the first depth stencil buffer will be used *!!!//
	vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> frameRtvHandles;
	CD3DX12_CPU_DESCRIPTOR_HANDLE frameDsvHandle;
	fatalAssert(pass.GetShaderTargetCount() == passRtvHandles.size());
	fatalAssert(pass.GetShaderTargetCount() == passDsvHandles.size());
	
	int renderTargetCount = 0;
	for (int i = 0; i < pass.GetShaderTargetCount(); i++)
	{
		if (pass.GetRenderTexture(i)->IsRenderTargetUsed())
		{
			if(renderTargetCount < 8)
				frameRtvHandles.push_back(passRtvHandles[i]);
			renderTargetCount++;
		}
	}

	//!!!* only the first usable depth stencil buffer will be used *!!!//
	int depthStencilCount = 0;
	int depthStencilIndex = -1;
	for(int i = 0;i< pass.GetShaderTargetCount();i++)
	{
		if(pass.GetRenderTexture(i)->IsDepthStencilUsed())
		{
			if (depthStencilCount == 0)
			{
				frameDsvHandle = passDsvHandles[i];
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
		frameRtvHandles.clear();
		frameRtvHandles.push_back(IsResolveNeeded() ? mPreResolvedRtvHandles[mCurrentFrameIndex] : mRtvHandles[mCurrentFrameIndex]);
	}

	if (pass.GetDepthStencilCount() == 0 && pass.UseDepthStencil())
	{
		frameDsvHandle = mDsvHandles[mCurrentFrameIndex]; 
		// assuming backbuffer always supports depth stencil buffer
		fatalAssert(mDepthStencilBuffers[mCurrentFrameIndex]);
		depthStencilIndex = 0;
	}

	int numViewPorts = frameRtvHandles.size() > 0 ? frameRtvHandles.size() : 1;
	vector<D3D12_VIEWPORT> frameViewPorts(numViewPorts, TranslateViewport(pass.GetCamera()->GetViewport()));
	vector<D3D12_RECT> frameScissorRects(numViewPorts, TranslateScissorRect(pass.GetCamera()->GetScissorRect()));
	commandList->OMSetRenderTargets(frameRtvHandles.size(), frameRtvHandles.data(), FALSE, depthStencilIndex >= 0 ? &frameDsvHandle : nullptr);
	commandList->RSSetViewports(frameViewPorts.size(), frameViewPorts.data()); // set the viewports
	commandList->RSSetScissorRects(frameScissorRects.size(), frameScissorRects.data()); // set the scissor rects
	if (pass.IsConstantBlendFactorsUsed())
		commandList->OMSetBlendFactor(pass.GetConstantBlendFactors());
	if (pass.IsStencilReferenceUsed())
		commandList->OMSetStencilRef(pass.GetStencilReference());

	// Clear the render targets
	if (clearColor)
	{
		fatalAssert(pass.UseRenderTarget());
		const float clearColorValueV[] = { clearColorValue.x, clearColorValue.y, clearColorValue.z, clearColorValue.w };
		for (int i = 0; i < frameRtvHandles.size(); i++)
		{
			// clear the render target buffer
			commandList->ClearRenderTargetView(frameRtvHandles[i], clearColorValueV, 0, nullptr);
		}
	}

	if (clearDepth || clearStencil)
	{
		fatalAssert(pass.UseDepthStencil());
		// clear the depth/stencil buffer
		if (clearDepth || clearStencil)
		{
			D3D12_CLEAR_FLAGS flag = clearDepth ? (clearStencil ? D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL : D3D12_CLEAR_FLAG_DEPTH) : D3D12_CLEAR_FLAG_STENCIL;
			commandList->ClearDepthStencilView(frameDsvHandle, flag, clearDepthValue, clearStencilValue, 0, nullptr);
		}
	}

	// set root signature
	commandList->SetGraphicsRootSignature(pass.GetRootSignature()); // set the root signature

	// set descriptor heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavDescriptorHeap.GetHeap(), mSamplerDescriptorHeap.GetHeap() };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// set descriptors
	commandList->SetGraphicsRootConstantBufferView(GetUniformSlot(RegisterSpace::SCENE, RegisterType::CONSTANT), pass.GetScene()->GetUniformBufferGpuAddress(mCurrentFrameIndex));
	commandList->SetGraphicsRootConstantBufferView(GetUniformSlot(RegisterSpace::FRAME, RegisterType::CONSTANT), mFrames[mCurrentFrameIndex].GetUniformBufferGpuAddress());
	commandList->SetGraphicsRootConstantBufferView(GetUniformSlot(RegisterSpace::PASS, RegisterType::CONSTANT), pass.GetUniformBufferGpuAddress(mCurrentFrameIndex));
	
	if (pass.GetScene()->GetTextureCount()) {
		commandList->SetGraphicsRootDescriptorTable(GetUniformSlot(RegisterSpace::SCENE, RegisterType::TEXTURE_TABLE), pass.GetScene()->GetCbvSrvUavDescriptorHeapTableHandle(mCurrentFrameIndex));
		commandList->SetGraphicsRootDescriptorTable(GetUniformSlot(RegisterSpace::SCENE, RegisterType::SAMPLER_TABLE), pass.GetScene()->GetSamplerDescriptorHeapTableHandle(mCurrentFrameIndex));
	}
	
	if (mFrames[mCurrentFrameIndex].GetTextureCount()) {
		commandList->SetGraphicsRootDescriptorTable(GetUniformSlot(RegisterSpace::FRAME, RegisterType::TEXTURE_TABLE), mFrames[mCurrentFrameIndex].GetCbvSrvUavDescriptorHeapTableHandle());
		commandList->SetGraphicsRootDescriptorTable(GetUniformSlot(RegisterSpace::FRAME, RegisterType::SAMPLER_TABLE), mFrames[mCurrentFrameIndex].GetSamplerDescriptorHeapTableHandle());
	}
	
	if (pass.GetTextureCount()) {
		commandList->SetGraphicsRootDescriptorTable(GetUniformSlot(RegisterSpace::PASS, RegisterType::TEXTURE_TABLE), pass.GetCbvSrvUavDescriptorHeapTableHandle(mCurrentFrameIndex));
		commandList->SetGraphicsRootDescriptorTable(GetUniformSlot(RegisterSpace::PASS, RegisterType::SAMPLER_TABLE), pass.GetSamplerDescriptorHeapTableHandle(mCurrentFrameIndex));
	}

	int meshCount = pass.GetMeshVec().size();
	for (int i = 0; i < meshCount; i++)
	{
		commandList->IASetPrimitiveTopology(pass.GetMeshVec()[i]->GetPrimitiveType());
		commandList->IASetVertexBuffers(0, 1, &pass.GetMeshVec()[i]->GetVertexBufferView());
		commandList->IASetIndexBuffer(&pass.GetMeshVec()[i]->GetIndexBufferView());
		commandList->SetGraphicsRootConstantBufferView(GetUniformSlot(RegisterSpace::OBJECT, RegisterType::CONSTANT), pass.GetMeshVec()[i]->GetUniformBufferGpuAddress(mCurrentFrameIndex));
		commandList->SetGraphicsRootDescriptorTable(GetUniformSlot(RegisterSpace::OBJECT, RegisterType::TEXTURE_TABLE), pass.GetMeshVec()[i]->GetCbvSrvUavDescriptorHeapTableHandle(mCurrentFrameIndex));
		commandList->SetGraphicsRootDescriptorTable(GetUniformSlot(RegisterSpace::OBJECT, RegisterType::SAMPLER_TABLE), pass.GetMeshVec()[i]->GetSamplerDescriptorHeapTableHandle(mCurrentFrameIndex));
		commandList->DrawIndexedInstanced(pass.GetMeshVec()[i]->GetIndexCount(), 1, 0, 0, 0);
	}
}

void Renderer::CreateGraphicsRootSignature(
	ID3D12RootSignature** rootSignature,
	int maxTextureCount[(int)RegisterSpace::COUNT])
{
	D3D12_ROOT_PARAMETER  rootParameterArr[(int)RegisterType::COUNT * (int)RegisterSpace::COUNT]; // each register space has two types of uniforms, the constant buffer and descriptor table
	D3D12_DESCRIPTOR_RANGE  textureTableRanges[(int)RegisterSpace::COUNT];
	D3D12_DESCRIPTOR_RANGE  samplerTableRanges[(int)RegisterSpace::COUNT];
	for(int uType = 0;uType< (int)RegisterType::COUNT;uType++)
	{
		for(int uSpace = 0;uSpace< (int)RegisterSpace::COUNT;uSpace++)
		{
			const int slot = GetUniformSlot((RegisterSpace)uSpace, (RegisterType)uType);
			if (uType == (int)RegisterType::CONSTANT)
			{
				rootParameterArr[slot].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
				rootParameterArr[slot].Descriptor.RegisterSpace = uSpace;
				rootParameterArr[slot].Descriptor.ShaderRegister = 0;
				rootParameterArr[slot].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			}
			else if(uType == (int)RegisterType::TEXTURE_TABLE)
			{
				textureTableRanges[uSpace].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				textureTableRanges[uSpace].NumDescriptors = maxTextureCount[uSpace] ? maxTextureCount[uSpace] : 1;
				textureTableRanges[uSpace].BaseShaderRegister = 0; // start index of the shader registers in the range
				textureTableRanges[uSpace].RegisterSpace = uSpace;
				textureTableRanges[uSpace].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // this appends the range to the end of the root signature descriptor tables
				
				rootParameterArr[slot].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				rootParameterArr[slot].DescriptorTable.NumDescriptorRanges = 1;
				rootParameterArr[slot].DescriptorTable.pDescriptorRanges = &textureTableRanges[uSpace]; // the pointer to the beginning of our ranges array
				rootParameterArr[slot].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			}
			else if(uType == (int)RegisterType::SAMPLER_TABLE)
			{
				samplerTableRanges[uSpace].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
				samplerTableRanges[uSpace].NumDescriptors = maxTextureCount[uSpace] ? maxTextureCount[uSpace] : 1;
				samplerTableRanges[uSpace].BaseShaderRegister = 0; // start index of the shader registers in the range
				samplerTableRanges[uSpace].RegisterSpace = uSpace;
				samplerTableRanges[uSpace].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // this appends the range to the end of the root signature descriptor tables

				rootParameterArr[slot].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				rootParameterArr[slot].DescriptorTable.NumDescriptorRanges = 1;
				rootParameterArr[slot].DescriptorTable.pDescriptorRanges = &samplerTableRanges[uSpace]; // the pointer to the beginning of our ranges array
				rootParameterArr[slot].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			}
			// TODO: add buffer support in the future
			else
			{
				fatalf("root parameter is wrong");
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
	int totalCbvSrvUavCount = mLevel->EstimateTotalCbvSrvUavCount(mFrameCount);
	for (auto& frame : mFrames)
		totalCbvSrvUavCount += frame.GetTextureCount();
	return totalCbvSrvUavCount;
}

int Renderer::EstimateTotalSamplerCount()
{
	// assume one srv correspond to one sampler
	return EstimateTotalCbvSrvUavCount();
}

int Renderer::EstimateTotalRtvCount()
{
	int totalRtvCount = mLevel->EstimateTotalRtvCount(mFrameCount);
	totalRtvCount += IsResolveNeeded() ? mFrameCount * 2 : mFrameCount; // swap chain frame buffer
	return totalRtvCount;
}

int Renderer::EstimateTotalDsvCount()
{
	// assume one rtv correspond to one dsv
	return EstimateTotalRtvCount();
}

ID3D12GraphicsCommandList* Renderer::BeginSingleTimeCommands()
{
	return BeginSingleTimeCommands(mSingleTimeCommandAllocator);
}

void Renderer::EndSingleTimeCommands(ID3D12GraphicsCommandList* commandList)
{
	EndSingleTimeCommands(mGraphicsCommandQueue, mSingleTimeCommandAllocator, commandList);
}