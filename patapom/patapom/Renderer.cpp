#include "Renderer.h"
#include "Level.h"

DescriptorHeap::DescriptorHeap() : 
	mHead(D3D12_DEFAULT), 
	mFree(D3D12_DEFAULT), 
	mHandleIncrementSize(0), 
	mHeap(nullptr),
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
	mFree = mHead;
	mCount = 0;
	mCountMax = countMax;
	mType = type;
	mHandleIncrementSize = mRenderer->mDevice->GetDescriptorHandleIncrementSize(Renderer::TranslateDescriptorHeapType(mType));
	mRenderer->CreateDescriptorHeap(*this, mCountMax);
	mHead = mHeap->GetCPUDescriptorHandleForHeapStart();
	mHeadGpu = mHeap->GetGPUDescriptorHandleForHeapStart();
}

void DescriptorHeap::Release()
{
	SAFE_RELEASE(mHeap);
	mRenderer = nullptr;
	mHead = D3D12_DEFAULT;
	mFree = D3D12_DEFAULT;
	mHandleIncrementSize = 0;
	mHeap = nullptr;
	mCount = 0;
	mCountMax = 0;
	mType = Type::INVALID;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::AllocateDsv(ID3D12Resource* resource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc, int count)
{
	mRenderer->mDevice->CreateDepthStencilView(resource, &desc, mFree);
	CD3DX12_CPU_DESCRIPTOR_HANDLE result = mFree;
	mFree.Offset(count, mHandleIncrementSize);
	return result;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::AllocateRtv(ID3D12Resource* resource, const D3D12_RENDER_TARGET_VIEW_DESC& desc, int count)
{
	mRenderer->mDevice->CreateRenderTargetView(resource, &desc, mFree);
	CD3DX12_CPU_DESCRIPTOR_HANDLE result = mFree;
	mFree.Offset(count, mHandleIncrementSize);
	return result;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::AllocateRtv(ID3D12Resource* resource, int count)
{
	mRenderer->mDevice->CreateRenderTargetView(resource, nullptr, mFree);
	CD3DX12_CPU_DESCRIPTOR_HANDLE result = mFree;
	mFree.Offset(count, mHandleIncrementSize);
	return result;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::AllocateSrv(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc, int count)
{
	mRenderer->mDevice->CreateShaderResourceView(resource, &desc, mFree);
	CD3DX12_CPU_DESCRIPTOR_HANDLE result = mFree;
	mFree.Offset(count, mHandleIncrementSize);
	return result;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::AllocateSampler(const D3D12_SAMPLER_DESC& desc, int count)
{
	mRenderer->mDevice->CreateSampler(&desc, mFree);
	CD3DX12_CPU_DESCRIPTOR_HANDLE result = mFree;
	mFree.Offset(count, mHandleIncrementSize);
	return result;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCurrentFreeGpuAddress()
{
	CD3DX12_GPU_DESCRIPTOR_HANDLE result = {};
	result.ptr = mHeadGpu.ptr + (mFree.ptr - mHead.ptr);
	return result;
}

inline DescriptorHeap::Type DescriptorHeap::GetType()
{
	return mType;
}

inline ID3D12DescriptorHeap*& DescriptorHeap::GetHeapRef()
{
	return mHeap;
}

inline ID3D12DescriptorHeap* DescriptorHeap::GetHeap()
{
	return mHeap;
}

Renderer::Renderer()
{

}

Renderer::~Renderer()
{
	Release();
}

D3D12_DEPTH_STENCIL_DESC Renderer::NoDepthStencilTest()
{
	D3D12_DEPTH_STENCIL_DESC result = {};
	result.DepthEnable = FALSE;
	result.StencilEnable = FALSE;
	return result;
} 	

D3D12_BLEND_DESC Renderer::AdditiveBlend()
{
	D3D12_BLEND_DESC result = {};
	result.AlphaToCoverageEnable = FALSE;
	result.IndependentBlendEnable = FALSE;
	result.RenderTarget[0].BlendEnable = TRUE;
	result.RenderTarget[0].LogicOpEnable = FALSE;
	result.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	result.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	result.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	result.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	result.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	result.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	result.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_CLEAR;
	result.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	return result;
}

D3D12_BLEND_DESC Renderer::NoBlend()
{
	D3D12_BLEND_DESC result = {};
	result.AlphaToCoverageEnable = FALSE;
	result.IndependentBlendEnable = FALSE;
	result.RenderTarget[0].BlendEnable = FALSE;
	result.RenderTarget[0].LogicOpEnable = FALSE;
	result.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	result.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	result.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	result.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	result.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	result.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	result.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_CLEAR;
	result.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	return result;
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
	if(sampler.mUseCompare)
	{
		if(sampler.mCompareOp == CompareOp::MINIMUM)
		{
			if (sampler.mUseAnisotropy)
			{
				return D3D12_FILTER_MINIMUM_ANISOTROPIC;
			}
			else if (sampler.mMinFilter == Sampler::Filter::POINT)
			{
				if (sampler.mMaxFilter == Sampler::Filter::POINT)
				{
					if (sampler.mUseMipmap)
						return D3D12_FILTER_MINIMUM_MIN_MAG_POINT_MIP_LINEAR;
					else
						return D3D12_FILTER_MINIMUM_MIN_MAG_MIP_POINT;
				}
				else if (sampler.mMaxFilter == Sampler::Filter::LINEAR)
				{
					if (sampler.mUseMipmap)
						return D3D12_FILTER_MINIMUM_MIN_POINT_MAG_MIP_LINEAR;
					else
						return D3D12_FILTER_MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT;
				}
				else
					fatalf("filter type wrong");
			}
			else if (sampler.mMinFilter == Sampler::Filter::LINEAR)
			{
				if (sampler.mMaxFilter == Sampler::Filter::POINT)
				{
					if (sampler.mUseMipmap)
						return D3D12_FILTER_MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
					else
						return D3D12_FILTER_MINIMUM_MIN_LINEAR_MAG_MIP_POINT;
				}
				else if (sampler.mMaxFilter == Sampler::Filter::LINEAR)
				{
					if (sampler.mUseMipmap)
						return D3D12_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR;
					else
						return D3D12_FILTER_MINIMUM_MIN_MAG_LINEAR_MIP_POINT;
				}
				else
					fatalf("filter type wrong");
			}
			else
				fatalf("filter type wrong");
		}
		else if(sampler.mCompareOp == CompareOp::MAXIMUM)
		{
			if (sampler.mUseAnisotropy)
			{
				return D3D12_FILTER_MAXIMUM_ANISOTROPIC;
			}
			else if (sampler.mMinFilter == Sampler::Filter::POINT)
			{
				if (sampler.mMaxFilter == Sampler::Filter::POINT)
				{
					if (sampler.mUseMipmap)
						return D3D12_FILTER_MAXIMUM_MIN_MAG_POINT_MIP_LINEAR;
					else
						return D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT;
				}
				else if (sampler.mMaxFilter == Sampler::Filter::LINEAR)
				{
					if (sampler.mUseMipmap)
						return D3D12_FILTER_MAXIMUM_MIN_POINT_MAG_MIP_LINEAR;
					else
						return D3D12_FILTER_MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT;
				}
				else
					fatalf("filter type wrong");
			}
			else if (sampler.mMinFilter == Sampler::Filter::LINEAR)
			{
				if (sampler.mMaxFilter == Sampler::Filter::POINT)
				{
					if (sampler.mUseMipmap)
						return D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
					else
						return D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_MIP_POINT;
				}
				else if (sampler.mMaxFilter == Sampler::Filter::LINEAR)
				{
					if (sampler.mUseMipmap)
						return D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR;
					else
						return D3D12_FILTER_MAXIMUM_MIN_MAG_LINEAR_MIP_POINT;
				}
				else
					fatalf("filter type wrong");
			}
			else
				fatalf("filter type wrong");
		}
		else
		{
			if (sampler.mUseAnisotropy)
			{
				return D3D12_FILTER_COMPARISON_ANISOTROPIC;
			}
			else if (sampler.mMinFilter == Sampler::Filter::POINT)
			{
				if (sampler.mMaxFilter == Sampler::Filter::POINT)
				{
					if (sampler.mUseMipmap)
						return D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
					else
						return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
				}
				else if (sampler.mMaxFilter == Sampler::Filter::LINEAR)
				{
					if (sampler.mUseMipmap)
						return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR;
					else
						return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT;
				}
				else
					fatalf("filter type wrong");
			}
			else if (sampler.mMinFilter == Sampler::Filter::LINEAR)
			{
				if (sampler.mMaxFilter == Sampler::Filter::POINT)
				{
					if (sampler.mUseMipmap)
						return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
					else
						return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;
				}
				else if (sampler.mMaxFilter == Sampler::Filter::LINEAR)
				{
					if (sampler.mUseMipmap)
						return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
					else
						return D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
				}
				else
					fatalf("filter type wrong");
			}
			else
				fatalf("filter type wrong");
		}
	}
	else
	{
		if (sampler.mUseAnisotropy)
		{
			return D3D12_FILTER_ANISOTROPIC;
		}
		else if (sampler.mMinFilter == Sampler::Filter::POINT)
		{
			if (sampler.mMaxFilter == Sampler::Filter::POINT)
			{
				if (sampler.mUseMipmap)
					return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
				else
					return D3D12_FILTER_MIN_MAG_MIP_POINT;
			}
			else if (sampler.mMaxFilter == Sampler::Filter::LINEAR)
			{
				if (sampler.mUseMipmap)
					return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
				else
					return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
			}
			else
				fatalf("filter type wrong");
		}
		else if (sampler.mMinFilter == Sampler::Filter::LINEAR)
		{
			if (sampler.mMaxFilter == Sampler::Filter::POINT)
			{
				if (sampler.mUseMipmap)
					return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
				else
					return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
			}
			else if (sampler.mMaxFilter == Sampler::Filter::LINEAR)
			{
				if (sampler.mUseMipmap)
					return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
				else
					return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			}
			else
				fatalf("filter type wrong");
		}
		else
			fatalf("filter type wrong");
	}
	return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
}

D3D12_TEXTURE_ADDRESS_MODE Renderer::TranslateAddressMode(Sampler::AddressMode addressMode)
{
	switch(addressMode)
	{
	case Sampler::AddressMode::WARP:
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

D3D12_COMPARISON_FUNC Renderer::TranslateCompareOp(CompareOp compareOp)
{
	switch (compareOp)
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
		return D3D12_COMPARISON_FUNC_NEVER;
	default:
		fatalf("compare op wrong");
	}
	return D3D12_COMPARISON_FUNC_NEVER;
}

D3D12_RESOURCE_STATES Renderer::TranslateTextureLayout(TextureLayout textureLayout)
{
	switch (textureLayout)
	{
	case TextureLayout::SHADER_READ:
		return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	case TextureLayout::RENDER_TARGET:
		return D3D12_RESOURCE_STATE_RENDER_TARGET;
	case TextureLayout::DEPTH_STENCIL_READ:
		return D3D12_RESOURCE_STATE_DEPTH_READ;
	case TextureLayout::DEPTH_STENCIL_WRITE:
		return D3D12_RESOURCE_STATE_DEPTH_WRITE;
	case TextureLayout::COPY_SRC:
		return D3D12_RESOURCE_STATE_COPY_SOURCE;
	case TextureLayout::COPY_DEST:
		return D3D12_RESOURCE_STATE_COPY_DEST;
	case TextureLayout::RESOLVE_SRC:
		return D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
	case TextureLayout::RESOLVE_DEST:
		return D3D12_RESOURCE_STATE_RESOLVE_DEST;
	case TextureLayout::PRESENT:
		return D3D12_RESOURCE_STATE_PRESENT;
	default:
		fatalf("texture layout wrong");
	}
	return D3D12_RESOURCE_STATE_COMMON;
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
	return mRenderTargetBuffers[frameIndex];
}

ID3D12Resource* Renderer::GetDepthStencilBuffer(int frameIndex)
{
	return mDepthStencilBuffers[frameIndex];
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
	for(auto frame : mFrames)
	{
		if (frame.GetTextureCount() > maxFrameTextureCount)
			maxFrameTextureCount = frame.GetTextureCount();
	}
	return maxFrameTextureCount;
}

bool Renderer::InitRenderer(HWND hwnd, int frameCount, int multiSampleCount, int width, int height)
{
	mFrameCount = frameCount;
	mMultiSampleCount = multiSampleCount;
	mWidth = width;
	mHeight = height;
	HRESULT hr;

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
		{
			// don't want a software device
			continue;
		}
		// want a device that is compatible with direct3d 12 (feature level 11 or higher)
		hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr);
		if (SUCCEEDED(hr))
		{
			adapterFound = true;
			break;
		}
		adapterIndex++;
	}
	if (!adapterFound)
		return false;

	// 2. create command queue
	// TODO: only one direct command queue is used currently
	D3D12_COMMAND_QUEUE_DESC cqDesc = {};
	cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // direct means the gpu can directly execute this command queue
	hr = mDevice->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&mGraphicsCommandQueue));
	if (CheckError(hr))
		return false;
	mGraphicsCommandQueue->SetName(L"Graphics Command Queue");

	// 3. create swapchain
	DXGI_MODE_DESC backBufferDesc = {};
	backBufferDesc.Width = mWidth;
	backBufferDesc.Height = mHeight;
	backBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	// describe our multi-sampling. We are not multi-sampling, so we set the count to 1 (we need at least one sample of course)
	DXGI_SAMPLE_DESC sampleDesc = {};
	sampleDesc.Count = mMultiSampleCount; // TODO: add support to MSAA
	sampleDesc.Quality = 0;
	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = mFrameCount;
	swapChainDesc.BufferDesc = backBufferDesc;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.OutputWindow = hwnd; // handle to our window
	swapChainDesc.SampleDesc = sampleDesc; // our multi-sampling description
	swapChainDesc.Windowed = false; // better leave it to false and use SetFullscreenState to toggle fullscreen https://docs.microsoft.com/en-us/windows/win32/api/dxgi/ns-dxgi-dxgi_swap_chain_desc
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
	mCbvSrvUavDescriptorHeap.InitDescriptorHeap(this, mLevel->EstimateTotalCbvSrvUavCount(), DescriptorHeap::SRV_CBV_UAV);
	mSamplerDescriptorHeap.InitDescriptorHeap(this, mLevel->EstimateTotalSamplerCount(), DescriptorHeap::SAMPLER);
	mRtvDescriptorHeap.InitDescriptorHeap(this, mLevel->EstimateTotalRtvCount(), DescriptorHeap::RTV);
	mDsvDescriptorHeap.InitDescriptorHeap(this, mLevel->EstimateTotalDsvCount(), DescriptorHeap::DSV);

	CreateRenderTargetBuffers();
	CreateDepthStencilBuffers();

	// 7. init level
	mLevel->InitLevel(this);
}

void Renderer::CreateDepthStencilBuffers()
{
	D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
	depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

	D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
	depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
	depthOptimizedClearValue.DepthStencil.Stencil = 0;

	int dsvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	for (int i = 0; i < mFrameCount; i++)
	{
		fatalAssert(SUCCEEDED(mDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, mWidth, mHeight, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(&mDepthStencilBuffers[i])
		)));

		mDsvHandles[i] = mDsvDescriptorHeap.AllocateDsv(mDepthStencilBuffers[i], depthStencilDesc, 1);
	}
}

void Renderer::CreateRenderTargetBuffers()
{
	for (int i = 0; i < mFrameCount; i++)
	{
		fatalAssert(SUCCEEDED(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mRenderTargetBuffers[i]))));
		mRtvHandles[i] = mRtvDescriptorHeap.AllocateRtv(mRenderTargetBuffers[i], 1);
	}
}

void Renderer::Release()
{
	// 1. release level
	mLevel->Release();

	// 2. release global GPU resources
	for (int i = 0; i < mFrameCount; i++)
	{
		mRenderTargetBuffers[i]->Release();
		mDepthStencilBuffers[i]->Release();
	}

	mCbvSrvUavDescriptorHeap.Release();
	mSamplerDescriptorHeap.Release();
	mDsvDescriptorHeap.Release();
	mRtvDescriptorHeap.Release();

	// 3. release fences
	for (int i = 0; i < mFrameCount; i++)
	{
		SAFE_RELEASE(mFences[i]);
	}

	// 4. release command allocators
	SAFE_RELEASE(mCopyCommandAllocator);
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
	SAFE_RELEASE(mDevice);
}

void Renderer::RecordBegin(int frameIndex, ID3D12GraphicsCommandList* commandList)
{
	// transition the "frameIndex" render target from the present state to the render target state so the command list draws to it starting from here
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetRenderTargetBuffer(frameIndex), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
}

void Renderer::RecordEnd(int frameIndex, ID3D12GraphicsCommandList* commandList)
{
	// transition the "frameIndex" render target from the render target state to the present state. If the debug layer is enabled, you will receive a
	// warning if present is called on the render target when it's not in the present state
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetRenderTargetBuffer(frameIndex), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
}

void Renderer::UploadTextureDataToBuffer(void* srcData, int srcBytePerRow, int srcBytePerSlice, D3D12_RESOURCE_DESC textureDesc, ID3D12Resource* dstBuffer)
{
	UINT64 textureUploadBufferSize = 0;
	// TODO: add support to subresources https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-getcopyablefootprints
	mDevice->GetCopyableFootprints(&textureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);
	UploadDataToBuffer(srcData, srcBytePerRow, srcBytePerSlice, textureUploadBufferSize, dstBuffer);
}

void Renderer::UploadDataToBuffer(void* srcData, int srcBytePerRow, int srcBytePerSlice, int uploadBufferSize, ID3D12Resource* dstBuffer)
{
	ID3D12GraphicsCommandList* commandList = BeginSingleTimeCommands(mCopyCommandAllocator);
	
	// creaete an upload buffer
	ID3D12Resource* uploadBuffer;
	mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize), // resource description for a buffer (storing the image data in this heap just to copy to the default heap)
		D3D12_RESOURCE_STATE_GENERIC_READ, // we will copy the contents from this heap to the default heap above
		nullptr,
		IID_PPV_ARGS(&uploadBuffer));

	D3D12_SUBRESOURCE_DATA data = {};
	data.pData = srcData; // pointer to our image data
	data.RowPitch = srcBytePerRow; // number of bytes per row
	data.SlicePitch = srcBytePerSlice; // number of bytes per slice

	// TODO: add support to subresources
	UpdateSubresources(commandList, dstBuffer, uploadBuffer, 0, 0, 1, &data);

	SAFE_RELEASE(uploadBuffer);
	EndSingleTimeCommands(mGraphicsCommandQueue, commandList);
}

ID3D12GraphicsCommandList* Renderer::BeginSingleTimeCommands(ID3D12CommandAllocator* commandAllocator)
{
	ID3D12GraphicsCommandList* commandList = nullptr;
	mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, NULL, IID_PPV_ARGS(&commandList));
	commandAllocator->Reset();
	return commandList;
}

void Renderer::EndSingleTimeCommands(ID3D12CommandQueue* commandQueue, ID3D12GraphicsCommandList* commandList)
{
	ID3D12Fence* fence;
	HANDLE fenceEvent;
	mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));//initialize 0
	CreateEvent(nullptr, FALSE, FALSE, nullptr);
	commandList->Close();
	ID3D12CommandList* commandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
	commandQueue->Signal(fence, 1); // singal 1
	if(!fence->GetCompletedValue()) { // if fence value is 0
		fence->SetEventOnCompletion(1, fenceEvent); // trigger event once fence value is 1
		WaitForSingleObject(fenceEvent, INFINITE); // wait for event
	}
	SAFE_RELEASE(fence);
	SAFE_RELEASE(commandList);
}

void Renderer::CreateDescriptorHeap(DescriptorHeap& descriptorHeap, int size)
{
	if (size > 0)
	{
		HRESULT hr;
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = size;
		switch(descriptorHeap.GetType())
		{
		case DescriptorHeap::SRV_CBV_UAV:
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			break;
		case DescriptorHeap::SAMPLER:
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			break;
		case DescriptorHeap::RTV:
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; 
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			break;
		case DescriptorHeap::DSV:
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			break;
		default:
			break;
		}
		fatalAssert(SUCCEEDED(mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap.GetHeapRef()))));
	}
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
	inputLayoutDesc.NumElements = sizeof(VertexInputLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC);
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
	sampleDesc.Count = mMultiSampleCount;

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
	psoDesc.SampleDesc = sampleDesc; // must be the same sample description as the swapchain and depth/stencil buffer
	psoDesc.SampleMask = 0xffffffff; // sample mask has to do with multi-sampling. 0xffffffff means point sampling is done
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
	psoDesc.BlendState = blendDesc;// CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
	psoDesc.NumRenderTargets = rtvFormatVec.size(); // we are only binding one render target
	psoDesc.DepthStencilState = dsDesc;// CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
	psoDesc.DSVFormat = dsvFormat;

	// create the pso
	fatalAssert(!CheckError(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pso))));

	(*pso)->SetName(name.c_str());
}

void Renderer::RecordPass(
	ID3D12GraphicsCommandList* commandList,
	Pass* pass,
	bool clearColor,
	bool clearDepth,
	XMFLOAT4 clearColorValue,
	float clearDepthValue)
{
	// set PSO
	commandList->SetPipelineState(pass->GetPso());

	// set the render target for the output merger stage (the output of the pipeline), viewports and scissor rects
	//!!!* only the first depth stencil buffer will be used *!!!//
	//the last parameter is the address of one dsv handle
	int renderTextureCount = pass->GetRenderTextureCount();
	vector<CD3DX12_CPU_DESCRIPTOR_HANDLE>& frameRtvHandles = pass->GetRtvHandles(mCurrentFrameIndex);
	vector<CD3DX12_CPU_DESCRIPTOR_HANDLE>& frameDsvHandles = pass->GetDsvHandles(mCurrentFrameIndex);//!!!* only the first depth stencil buffer will be used *!!!//
	vector<D3D12_VIEWPORT> frameViewPorts(renderTextureCount, pass->GetCamera()->GetViewport());
	vector<D3D12_RECT> frameScissorRects(renderTextureCount, pass->GetCamera()->GetScissorRect());
	commandList->OMSetRenderTargets(renderTextureCount, frameRtvHandles.data(), FALSE, frameDsvHandles.data());
	commandList->RSSetViewports(renderTextureCount, frameViewPorts.data()); // set the viewports
	commandList->RSSetScissorRects(renderTextureCount, frameScissorRects.data()); // set the scissor rects
	
	// Clear the render target by using the ClearRenderTargetView command
	if (clearColor || clearDepth)
	{
		const float clearColorValueV[] = { clearColorValue.x, clearColorValue.y, clearColorValue.z, clearColorValue.w };
		for (int i = 0; i < renderTextureCount; i++)
		{
			// clear the render target buffer
			if(clearColor)
				commandList->ClearRenderTargetView(frameRtvHandles[i], clearColorValueV, 0, nullptr);
			// clear the depth/stencil buffer
			if(clearDepth)
				commandList->ClearDepthStencilView(frameDsvHandles[i], D3D12_CLEAR_FLAG_DEPTH, clearDepthValue, 0, 0, nullptr);
		}
	}

	// set root signature
	commandList->SetGraphicsRootSignature(pass->GetRootSignature()); // set the root signature

	// set descriptor heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavDescriptorHeap.GetHeap(), mSamplerDescriptorHeap.GetHeap(), mRtvDescriptorHeap.GetHeap(), mDsvDescriptorHeap.GetHeap() };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// set descriptors
	commandList->SetGraphicsRootConstantBufferView(GetUniformSlot(UNIFORM_REGISTER_SPACE::SCENE, UNIFORM_TYPE::CONSTANT), pass->GetScene()->GetUniformBufferGpuAddress(mCurrentFrameIndex));
	commandList->SetGraphicsRootConstantBufferView(GetUniformSlot(UNIFORM_REGISTER_SPACE::FRAME, UNIFORM_TYPE::CONSTANT), mFrames[mCurrentFrameIndex].GetUniformBufferGpuAddress());
	commandList->SetGraphicsRootConstantBufferView(GetUniformSlot(UNIFORM_REGISTER_SPACE::PASS, UNIFORM_TYPE::CONSTANT), pass->GetUniformBufferGpuAddress(mCurrentFrameIndex));
	commandList->SetGraphicsRootDescriptorTable(GetUniformSlot(UNIFORM_REGISTER_SPACE::SCENE, UNIFORM_TYPE::TEXTURE_TABLE), pass->GetScene()->GetCbvSrvUavDescriptorHeapTableHandle(mCurrentFrameIndex));
	commandList->SetGraphicsRootDescriptorTable(GetUniformSlot(UNIFORM_REGISTER_SPACE::FRAME, UNIFORM_TYPE::TEXTURE_TABLE), mFrames[mCurrentFrameIndex].GetCbvSrvUavDescriptorHeapTableHandle());
	commandList->SetGraphicsRootDescriptorTable(GetUniformSlot(UNIFORM_REGISTER_SPACE::PASS, UNIFORM_TYPE::TEXTURE_TABLE), pass->GetCbvSrvUavDescriptorHeapTableHandle(mCurrentFrameIndex));
	commandList->SetGraphicsRootDescriptorTable(GetUniformSlot(UNIFORM_REGISTER_SPACE::SCENE, UNIFORM_TYPE::SAMPLER_TABLE), pass->GetScene()->GetSamplerDescriptorHeapTableHandle(mCurrentFrameIndex));
	commandList->SetGraphicsRootDescriptorTable(GetUniformSlot(UNIFORM_REGISTER_SPACE::FRAME, UNIFORM_TYPE::SAMPLER_TABLE), mFrames[mCurrentFrameIndex].GetSamplerDescriptorHeapTableHandle());
	commandList->SetGraphicsRootDescriptorTable(GetUniformSlot(UNIFORM_REGISTER_SPACE::PASS, UNIFORM_TYPE::SAMPLER_TABLE), pass->GetSamplerDescriptorHeapTableHandle(mCurrentFrameIndex));
	
	int meshCount = pass->GetMeshVec().size();
	for (int i = 0; i < meshCount; i++)
	{
		commandList->IASetPrimitiveTopology(pass->GetMeshVec()[i]->GetPrimitiveType());
		commandList->IASetVertexBuffers(0, 1, &pass->GetMeshVec()[i]->GetVertexBufferView());
		commandList->IASetIndexBuffer(&pass->GetMeshVec()[i]->GetIndexBufferView());
		commandList->SetGraphicsRootConstantBufferView(GetUniformSlot(UNIFORM_REGISTER_SPACE::OBJECT, UNIFORM_TYPE::CONSTANT), pass->GetMeshVec()[i]->GetUniformBufferGpuAddress(mCurrentFrameIndex));
		commandList->SetGraphicsRootDescriptorTable(GetUniformSlot(UNIFORM_REGISTER_SPACE::OBJECT, UNIFORM_TYPE::TEXTURE_TABLE), pass->GetMeshVec()[i]->GetCbvSrvUavDescriptorHeapTableHandle(mCurrentFrameIndex));
		commandList->SetGraphicsRootDescriptorTable(GetUniformSlot(UNIFORM_REGISTER_SPACE::OBJECT, UNIFORM_TYPE::SAMPLER_TABLE), pass->GetMeshVec()[i]->GetSamplerDescriptorHeapTableHandle(mCurrentFrameIndex));
		commandList->DrawIndexedInstanced(pass->GetMeshVec()[i]->GetIndexCount(), 1, 0, 0, 0);
	}
}

void Renderer::CreateGraphicsRootSignature(
	ID3D12RootSignature** rootSignature,
	int maxTextureCount[UNIFORM_REGISTER_SPACE::COUNT])
{
	D3D12_ROOT_PARAMETER  rootParameterArr[UNIFORM_TYPE::COUNT * UNIFORM_REGISTER_SPACE::COUNT]; // each register space has two types of uniforms, the constant buffer and descriptor table
	D3D12_DESCRIPTOR_RANGE  textureTableRanges[UNIFORM_REGISTER_SPACE::COUNT];
	D3D12_DESCRIPTOR_RANGE  samplerTableRanges[UNIFORM_REGISTER_SPACE::COUNT];
	for(int uType = 0;uType<UNIFORM_TYPE::COUNT;uType++)
	{
		for(int uSpace = 0;uSpace<UNIFORM_REGISTER_SPACE::COUNT;uSpace++)
		{
			const int slot = GetUniformSlot(uSpace, uType);
			if (uType == UNIFORM_TYPE::CONSTANT)
			{
				rootParameterArr[slot].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
				rootParameterArr[slot].Descriptor.RegisterSpace = uSpace;
				rootParameterArr[slot].Descriptor.ShaderRegister = 0;
				rootParameterArr[slot].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			}
			else if(uType == UNIFORM_TYPE::TEXTURE_TABLE)
			{
				textureTableRanges[uSpace].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				textureTableRanges[uSpace].NumDescriptors = maxTextureCount[uSpace];
				textureTableRanges[uSpace].BaseShaderRegister = 0; // start index of the shader registers in the range
				textureTableRanges[uSpace].RegisterSpace = uSpace;
				textureTableRanges[uSpace].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // this appends the range to the end of the root signature descriptor tables
				
				rootParameterArr[slot].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				rootParameterArr[slot].DescriptorTable.NumDescriptorRanges = 1;
				rootParameterArr[slot].DescriptorTable.pDescriptorRanges = &textureTableRanges[uSpace]; // the pointer to the beginning of our ranges array
				rootParameterArr[slot].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			}
			else if(uType == UNIFORM_TYPE::SAMPLER_TABLE)
			{
				samplerTableRanges[uSpace].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
				samplerTableRanges[uSpace].NumDescriptors = maxTextureCount[uSpace];
				samplerTableRanges[uSpace].BaseShaderRegister = 0; // start index of the shader registers in the range
				samplerTableRanges[uSpace].RegisterSpace = uSpace;
				samplerTableRanges[uSpace].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // this appends the range to the end of the root signature descriptor tables

				rootParameterArr[slot].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				rootParameterArr[slot].DescriptorTable.NumDescriptorRanges = 1;
				rootParameterArr[slot].DescriptorTable.pDescriptorRanges = &samplerTableRanges[uSpace]; // the pointer to the beginning of our ranges array
				rootParameterArr[slot].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			}
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
		CheckError(hr, errorBuff);

	hr = mDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(rootSignature));
	if (FAILED(hr))
		fatalf("create root signature failed");
}