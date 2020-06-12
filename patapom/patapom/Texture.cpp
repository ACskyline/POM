#include "Texture.h"
#include "Renderer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "Dependencies/stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "Dependencies/stb_image_resize.h"

Texture::Texture(
	const string& fileName,
	const wstring& debugName,
	Sampler sampler,
	bool useMipmap,
	Format format) :
	Texture(
		fileName,
		debugName,
		sampler,
		useMipmap,
		format,
		-1,
		-1)
{
}

Texture::Texture(
	const string& fileName, 
	const wstring& debugName, 
	Sampler sampler,
	bool useMipmap,
	Format format,
	int width,
	int height) :
	mFileName(fileName), 
	mDebugName(debugName),
	mUseMipmap(useMipmap),
	mInitialized(false),
	mWidth(width), 
	mHeight(height), 
	mMipLevelCount(1),
	mRenderer(nullptr), 
	mImageData(nullptr), 
	mTextureBuffer(nullptr),
	mTextureBufferLayout(ResourceLayout::SHADER_READ),
	mSampler(sampler),
	mTextureFormat(format)
{
	mSrv.mType = View::Type::SRV;
}

Texture::~Texture()
{
	Release();
}

void Texture::Release()
{
	SAFE_RELEASE(mTextureBuffer);
}

void Texture::CreateTextureBuffer()
{
	int channelCountOriginal = 0;
	int channelCountRequested = 0;
	int bytePerChannel = 0;
	void* data = nullptr;
	switch (mTextureFormat)
	{
	case Format::R16G16B16A16_UNORM:
	case Format::R16G16B16A16_FLOAT:
		bytePerChannel = 2;
		channelCountRequested = STBI_rgb_alpha;
		data = stbi_load_16(mFileName.c_str(), &mWidth, &mHeight, &channelCountOriginal, channelCountRequested);
		break;
	case Format::R8G8B8A8_UNORM:
		bytePerChannel = 1;
		channelCountRequested = STBI_rgb_alpha;
		data = stbi_load(mFileName.c_str(), &mWidth, &mHeight, &channelCountOriginal, channelCountRequested);//default is 8 bit version
		break;
	default:
		fatalf("Texture format is wrong");
		break;
	}
		
	int bytePerRow = mWidth * channelCountRequested * bytePerChannel;
	int bytePerSlice = bytePerRow * mHeight;

	if(!mUseMipmap) // TODO: add support to mip map tool chain
		mMipLevelCount = 1;
	else
	{
		//fatalAssertf((mWidth > 0) && ((mWidth - 1) & mWidth == 0) && (mHeight > 0) && ((mHeight - 1) & mHeight == 0), "texture size is not power of 2");
		mMipLevelCount = static_cast<uint32_t>(floor(log2(max(mWidth, mHeight)))) + 1;
	}

	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;//TODO: add support for cube map and texture array
	textureDesc.Alignment = 0;//automatically seleted by API
	textureDesc.Width = mWidth;
	textureDesc.Height = mHeight;
	textureDesc.DepthOrArraySize = 1;//Width, Height, and DepthOrArraySize must be between 1 and the maximum dimension supported for the particular feature level and texture dimension https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_resource_desc TODO: add support for cube map and texture array
	textureDesc.MipLevels = mMipLevelCount;
	textureDesc.Format = Renderer::TranslateFormat(mTextureFormat);
	textureDesc.SampleDesc.Count = 1; // same as 0
	textureDesc.SampleDesc.Quality = 0;//TODO: investigate how this affect the visual quality
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; //The arrangement of the pixels. Setting to unknown lets the driver choose the most efficient one, this is not the resource state
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;//TODO: add uav, cross queue and mgpu support

	// create a default heap where the upload heap will copy its contents into (contents being the texture)
	mTextureBufferLayout = ResourceLayout::COPY_DST;
	mRenderer->mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&textureDesc, // the description of our texture
		Renderer::TranslateResourceLayout(mTextureBufferLayout), // We will copy the texture from the upload heap to here, so we start it out in a copy dest state
		nullptr, // only used for render targets and depth/stencil buffers
		IID_PPV_ARGS(&mTextureBuffer));

	vector<void*> dataVec(mMipLevelCount);
	vector<int> bytePerRowVec(mMipLevelCount);
	vector<int> bytePerSliceVec(mMipLevelCount);
	dataVec[0] = data;
	bytePerRowVec[0] = bytePerRow;
	bytePerSliceVec[0] = bytePerSlice;
	int w = mWidth;
	int h = mHeight;
	for (int i = 1; i < mMipLevelCount; i++)
	{
		int wNew = w / 2;
		int hNew = h / 2;
		switch (bytePerChannel) {
		case 1:
			dataVec[i] = new uint8_t[wNew * hNew * channelCountRequested];
			stbir_resize_uint8_generic((uint8_t*)dataVec[i - 1], w, h, 0, (uint8_t*)dataVec[i], wNew, hNew, 0, channelCountRequested, -1, 0, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR, nullptr);
			break;
		case 2:
			dataVec[i] = new uint16_t[wNew * hNew * channelCountRequested];
			stbir_resize_uint16_generic((uint16_t*)dataVec[i - 1], w, h, 0, (uint16_t*)dataVec[i], wNew, hNew, 0, channelCountRequested, -1, 0, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR, nullptr);
			break;
		default:
			fatalf("texture format wrong");
			break;
		}
		
		bytePerRowVec[i] = bytePerRowVec[i - 1] / 2;
		bytePerSliceVec[i] = bytePerRowVec[i - 1] / 4;
		w = wNew;
		h = hNew;
	}
	mRenderer->UploadTextureDataToBuffer(dataVec, bytePerRowVec, bytePerSliceVec, textureDesc, mTextureBuffer);

	TransitionTextureLayoutSingleTime(ResourceLayout::SHADER_READ);

	stbi_image_free(data);
	for(int i = 1;i<mMipLevelCount;i++)
		delete[] dataVec[i];

	mTextureBuffer->SetName(mDebugName.c_str());
}

void Texture::CreateView()
{
	// D3D12 binds the view to a descriptor heap as soon as it is created, so this function is not actually creating the API view
	// what we are creating here is the view defined by ourselves
	mSrv.mSrvDesc.Format = Renderer::TranslateFormat(mTextureFormat);
	mSrv.mSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // plain texture does not support MSAA
	mSrv.mSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	mSrv.mSrvDesc.Texture2D.MostDetailedMip = 0; // this is an API level integer to restrict smallest mip level
	mSrv.mSrvDesc.Texture2D.MipLevels = mMipLevelCount; // TODO: add support for mip map tool chain
	mSrv.mSrvDesc.Texture2D.PlaneSlice = 0; // most formats only have 1 plane slice and normally we just need to use the first plane
	mSrv.mSrvDesc.Texture2D.ResourceMinLODClamp = 0; // this is an GPU level float to restrict the lerp involving the smallest mip level https://github.com/Microsoft/DirectX-Graphics-Samples/issues/215
}

void Texture::CreateSampler()
{
	mSampler.mImpl.Filter = Renderer::ExtractFilter(mSampler);
	mSampler.mImpl.AddressU = Renderer::TranslateAddressMode(mSampler.mAddressModeU);
	mSampler.mImpl.AddressV = Renderer::TranslateAddressMode(mSampler.mAddressModeV);
	mSampler.mImpl.AddressW = Renderer::TranslateAddressMode(mSampler.mAddressModeW);
	mSampler.mImpl.MipLODBias = mSampler.mMipLodBias;
	mSampler.mImpl.MaxAnisotropy = 0; // TODO: add support for anisotropic filtering
	mSampler.mImpl.ComparisonFunc = Renderer::TranslateCompareOp(mSampler.mCompareOp);
	mSampler.mImpl.BorderColor[0] = mSampler.mBorderColor[0];
	mSampler.mImpl.BorderColor[1] = mSampler.mBorderColor[1];
	mSampler.mImpl.BorderColor[2] = mSampler.mBorderColor[2];
	mSampler.mImpl.BorderColor[3] = mSampler.mBorderColor[3];
	mSampler.mImpl.MinLOD = mSampler.mMinLod;
	mSampler.mImpl.MaxLOD = mSampler.mMaxLod;
}

ID3D12Resource* Texture::GetTextureBuffer()
{
	fatalAssertf(mInitialized, "texture %s is not initialized!", mFileName.c_str());
	return mTextureBuffer;
}

D3D12_SHADER_RESOURCE_VIEW_DESC Texture::GetSrvDesc()
{
	fatalAssertf(mInitialized, "texture %s is not initialized!", mFileName.c_str());
	fatalAssertf(mSrv.mType == View::Type::SRV, "srv type wrong");
	return mSrv.mSrvDesc;
}

D3D12_SAMPLER_DESC Texture::GetSamplerDesc()
{
	fatalAssertf(mInitialized, "texture %s is not initialized!", mFileName.c_str());
	return mSampler.mImpl;
}

Format Texture::GetTextureFormat()
{
	return mTextureFormat;
}

View Texture::GetSrv()
{
	return mSrv;
}

Sampler Texture::GetSampler()
{
	return mSampler;
}

string Texture::GetName()
{
	return mFileName;
}

wstring Texture::GetDebugName()
{
	return mDebugName;
}

void Texture::InitTexture(Renderer* renderer)
{
	mRenderer = renderer;
	CreateTextureBuffer();
	CreateView();
	CreateSampler();
	mInitialized = true;
}

bool Texture::TransitionTextureLayoutSingleTime(ResourceLayout newLayout)
{
	bool result = mRenderer->TransitionSingleTime(mTextureBuffer, mTextureBufferLayout, newLayout);
	if (result)
		mTextureBufferLayout = newLayout;
	return result;
}

bool Texture::TransitionTextureLayout(ID3D12GraphicsCommandList* commandList, ResourceLayout newLayout)
{
	bool result = mRenderer->RecordTransition(commandList, mTextureBuffer, mTextureBufferLayout, newLayout);
	if(result)
		mTextureBufferLayout = newLayout;
	return result;
}

///////////////////////////////////////////////////////////////
/////////////// RENDER TEXTURE STARTS FROM HERE ///////////////
///////////////////////////////////////////////////////////////

RenderTexture::RenderTexture(
	const wstring& debugName,
	int width,
	int height,
	ReadFrom readFrom,
	Sampler sampler,
	Format renderTargetFormat,
	XMFLOAT4 colorClearValue,
	int multiSampleCount) :
	RenderTexture(
		debugName,
		width,
		height,
		readFrom,
		sampler,
		true,
		false,
		renderTargetFormat,
		Format::D24_UNORM_S8_UINT, // place holder format so that we can create an empty dsv if we don't need depth stencil buffer
		colorClearValue,
		1.f,
		0,
		multiSampleCount)
{
}

RenderTexture::RenderTexture(
	const wstring& debugName,
	int width,
	int height,
	ReadFrom readFrom,
	Sampler sampler,
	Format depthStencilFormat,
	float depthClearValue,
	uint8_t stencilClearValue,
	int multiSampleCount) :
	RenderTexture(
		debugName,
		width,
		height,
		readFrom,
		sampler,
		false,
		true,
		Format::R8G8B8A8_UNORM, // place holder format so that we can create an empty dsv if we don't need render target buffer
		depthStencilFormat,
		XMFLOAT4(0.f, 0.f, 0.f, 0.f),
		depthClearValue,
		stencilClearValue,
		multiSampleCount)
{
}

RenderTexture::RenderTexture(
	const wstring& debugName,
	int width,
	int height,
	ReadFrom readFrom,
	Sampler sampler,
	Format renderTargetFormat,
	Format depthStencilFormat,
	XMFLOAT4 colorClearValue,
	float depthClearValue,
	uint8_t stencilClearValue,
	int multiSampleCount) :
	RenderTexture(
		debugName,
		width,
		height,
		readFrom,
		sampler,
		true,
		true,
		renderTargetFormat,
		depthStencilFormat,
		colorClearValue,
		depthClearValue,
		stencilClearValue,
		multiSampleCount)
{
}

RenderTexture::RenderTexture(
	const wstring& debugName,
	int width,
	int height,
	ReadFrom readFrom,
	Sampler sampler,
	bool useRenderTarget,
	bool useDepthStencil,
	Format renderTargetFormat,
	Format depthStencilFormat,
	XMFLOAT4 colorClearValue,
	float depthClearValue,
	uint8_t stencilClearValue,
	int multiSampleCount) :
	mReadFrom(readFrom),
	mUseRenderTarget(useRenderTarget),
	mUseDepthStencil(useDepthStencil),
	mRenderTargetFormat(renderTargetFormat),
	mDepthStencilFormat(depthStencilFormat),
	mColorClearValue(colorClearValue),
	mDepthClearValue(depthClearValue),
	mStencilClearValue(stencilClearValue),
	mDepthStencilBuffer(nullptr),
	mDepthStencilBufferLayout(ResourceLayout::DEPTH_STENCIL_WRITE),
	mRenderTargetBufferLayout(ResourceLayout::RENDER_TARGET),
	mMultiSampleCount(multiSampleCount),
	Texture("no file", debugName, sampler, false, Format::INVALID, width, height)
{
}

RenderTexture::~RenderTexture()
{
	Release();
}

void RenderTexture::Release()
{
	SAFE_RELEASE(mRenderTargetBuffer);
	SAFE_RELEASE(mDepthStencilBuffer);
}

void RenderTexture::CreateTextureBuffer()
{
	//if(!mUseMipmap) // TODO: add mip map support for render texture
		mMipLevelCount = 1;
	
	// 1. Render Target Buffer
	D3D12_RESOURCE_DESC renderTargetDesc = {};
	if (mUseRenderTarget)
	{
		renderTargetDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		renderTargetDesc.Alignment = 0;
		renderTargetDesc.Width = mWidth;
		renderTargetDesc.Height = mHeight;
		renderTargetDesc.DepthOrArraySize = 1; // TODO: add support for cube map and texture array
		renderTargetDesc.MipLevels = mMipLevelCount; // TODO: add support for mip map tool chain
		renderTargetDesc.Format = Renderer::TranslateFormat(mRenderTargetFormat);
		renderTargetDesc.SampleDesc.Count = mMultiSampleCount;
		renderTargetDesc.SampleDesc.Quality = 0;
		renderTargetDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		renderTargetDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE colorClearValue = {};
		colorClearValue.Format = renderTargetDesc.Format;
		colorClearValue.Color[0] = mColorClearValue.x;
		colorClearValue.Color[1] = mColorClearValue.y;
		colorClearValue.Color[2] = mColorClearValue.z;
		colorClearValue.Color[3] = mColorClearValue.w;

		mRenderTargetBufferLayout = ResourceLayout::RENDER_TARGET;
		HRESULT hr = mRenderer->mDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&renderTargetDesc,
			Renderer::TranslateResourceLayout(mRenderTargetBufferLayout),
			&colorClearValue,
			IID_PPV_ARGS(&mRenderTargetBuffer));

		fatalAssertf(SUCCEEDED(hr), "create render target buffer failed");
		mRenderTargetBuffer->SetName(mDebugName.c_str());
	}

	// 2. Depth Stencil Buffer
	D3D12_RESOURCE_DESC depthStencilDesc = {};
	if (mUseDepthStencil)
	{
		depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthStencilDesc.Alignment = 0;
		depthStencilDesc.Width = mWidth;
		depthStencilDesc.Height = mHeight;
		depthStencilDesc.DepthOrArraySize = 1; // TODO: add support for cube map and texture array
		depthStencilDesc.MipLevels = mMipLevelCount; // TODO: add support for mip map tool chain
		depthStencilDesc.Format = (ReadFromDepth() || ReadFromStencil()) ? Renderer::TranslateToTypelessFormat(mDepthStencilFormat) : Renderer::TranslateFormat(mDepthStencilFormat);
		depthStencilDesc.SampleDesc.Count = mMultiSampleCount;
		depthStencilDesc.SampleDesc.Quality = 0;
		depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE depthStencilClearValue = {};
		depthStencilClearValue.Format = Renderer::TranslateFormat(mDepthStencilFormat);
		depthStencilClearValue.DepthStencil.Depth = mDepthClearValue;
		depthStencilClearValue.DepthStencil.Stencil = mStencilClearValue;

		mDepthStencilBufferLayout = ResourceLayout::DEPTH_STENCIL_WRITE;
		HRESULT hr = mRenderer->mDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			Renderer::TranslateResourceLayout(mDepthStencilBufferLayout),
			&depthStencilClearValue,
			IID_PPV_ARGS(&mDepthStencilBuffer)
		);

		fatalAssertf(SUCCEEDED(hr), "create depth stencil buffer failed");
		mDepthStencilBuffer->SetName(mDebugName.c_str());
	}

	// 3. Texture Buffer
	if (IsResolveNeeded()) // need to resolve
	{
		D3D12_RESOURCE_DESC textureDesc = {};
		if(ReadFromColor())
		{
			mTextureFormat = mRenderTargetFormat;
			textureDesc = renderTargetDesc;
			textureDesc.Format = Renderer::TranslateFormat(mTextureFormat);
			textureDesc.SampleDesc.Count = 1;
		}
		else if (ReadFromDepth() || ReadFromStencil())
		{
			mTextureFormat = mDepthStencilFormat;
			textureDesc = depthStencilDesc;
			textureDesc.Format = Renderer::TranslateToSrvFormat(mTextureFormat, mReadFrom);
			textureDesc.SampleDesc.Count = 1;
		}
		else
			fatalf("mReadFrom is invalid");

		mTextureBufferLayout = ResourceLayout::RESOLVE_DST;
		HRESULT hr = mRenderer->mDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			Renderer::TranslateResourceLayout(mTextureBufferLayout),
			nullptr,
			IID_PPV_ARGS(&mTextureBuffer)
		);
		fatalAssertf(SUCCEEDED(hr), "create texture buffer failed");
		mTextureBuffer->SetName(mDebugName.c_str());
	}
	else // don't need to resolve
	{
		if (ReadFromColor())
		{
			mTextureFormat = mRenderTargetFormat;
			mTextureBuffer = mRenderTargetBuffer;
		}
		else if (ReadFromDepth() || ReadFromStencil())
		{
			mTextureFormat = mDepthStencilFormat;
			mTextureBuffer = mDepthStencilBuffer;
		}
		else
			fatalf("mReadFrom is invalid");

		mTextureBufferLayout = ResourceLayout::SHADER_READ;
	}
}

void RenderTexture::CreateView()
{
	// 1.Render Target View
	mRtv.mType = View::Type::RTV;
	mRtv.mRtvDesc.Format = Renderer::TranslateFormat(mRenderTargetFormat);
	if (IsMultiSampleUsed())
	{
		mRtv.mRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
		mRtv.mRtvDesc.Texture2DMS.UnusedField_NothingToDefine = 0;
	}
	else
	{
		mRtv.mRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		mRtv.mRtvDesc.Texture2D.MipSlice = 0;
		mRtv.mRtvDesc.Texture2D.PlaneSlice = 0;
	}
	
	// 2.Depth Stencil View Desc
	// if you want to bind null dsv desciptor, you need to create the (null) dsv
	// if you want to create the dsv, you need an initialized dsvDesc to avoid the debug layer throwing out error
	mDsv.mType = View::Type::DSV;
	mDsv.mDsvDesc.Format = Renderer::TranslateFormat(mDepthStencilFormat);
	mDsv.mDsvDesc.Flags = D3D12_DSV_FLAG_NONE; // TODO: this flag contains samilar feature as Vulkan layouts like VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
	if (IsMultiSampleUsed())
	{
		mDsv.mDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
		mDsv.mDsvDesc.Texture2DMS.UnusedField_NothingToDefine = 0;
	}
	else
	{
		mDsv.mDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		mDsv.mDsvDesc.Texture2D.MipSlice = 0;
	}
	
	// 3.Shader Resource View
	mSrv.mType = View::Type::SRV;
	mSrv.mSrvDesc.Format = Renderer::TranslateToSrvFormat(mTextureFormat, mReadFrom);
	mSrv.mSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	if (ReadFromMultiSampledBuffer())
	{
		mSrv.mSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
		mSrv.mSrvDesc.Texture2DMS.UnusedField_NothingToDefine = 0;
	}
	else
	{
		mSrv.mSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		mSrv.mSrvDesc.Texture2D.MostDetailedMip = 0;
		mSrv.mSrvDesc.Texture2D.MipLevels = mMipLevelCount;
		mSrv.mSrvDesc.Texture2D.PlaneSlice = 0;
		mSrv.mSrvDesc.Texture2D.ResourceMinLODClamp = 0;
	}
}

bool RenderTexture::ReadFromMultiSampledBuffer()
{
	if (mReadFrom & ReadFrom::MS)
	{
		fatalAssert(IsMultiSampleUsed());
		return true;
	}
	return false;
}

bool RenderTexture::ReadFromStencil()
{
	if (mReadFrom & ReadFrom::STENCIL)
	{
		fatalAssert(IsDepthStencilUsed());
		return true;
	}
	return false;
}

bool RenderTexture::ReadFromDepth()
{
	if (mReadFrom & ReadFrom::DEPTH)
	{
		fatalAssert(IsDepthStencilUsed());
		return true;
	}
	return false;
}

bool RenderTexture::ReadFromDepthStencilBuffer()
{
	return !IsResolveNeeded() && (ReadFromDepth() || ReadFromStencil());
}

bool RenderTexture::ReadFromColor()
{
	if (mReadFrom & ReadFrom::COLOR)
	{
		fatalAssert(IsRenderTargetUsed());
		return true;
	}
	return false;
}

bool RenderTexture::IsMultiSampleUsed()
{
	return GetMultiSampleCount() > 1;
}

bool RenderTexture::IsResolveNeeded()
{
	return !ReadFromMultiSampledBuffer() && IsMultiSampleUsed();
}

bool RenderTexture::IsDepthStencilUsed()
{
	return mUseDepthStencil;
}

bool RenderTexture::IsRenderTargetUsed()
{
	return mUseRenderTarget;
}

ID3D12Resource* RenderTexture::GetRenderTargetBuffer()
{
	return  mRenderTargetBuffer;
}

ID3D12Resource* RenderTexture::GetDepthStencilBuffer()
{
	// make sure to create null descriptor for render textures which does not support depth
	return  mDepthStencilBuffer;
}

D3D12_RENDER_TARGET_VIEW_DESC RenderTexture::GetRtvDesc()
{
	fatalAssertf(mRtv.mType == View::Type::RTV, "rtv type wrong");
	return mRtv.mRtvDesc;
}

D3D12_DEPTH_STENCIL_VIEW_DESC RenderTexture::GetDsvDesc()
{
	fatalAssertf(mDsv.mType == View::Type::DSV, "dsv type wrong");
	return mDsv.mDsvDesc;
}

Format RenderTexture::GetRenderTargetFormat()
{
	return mRenderTargetFormat;
}

Format RenderTexture::GetDepthStencilFormat()
{
	return mDepthStencilFormat;
}

int RenderTexture::GetMultiSampleCount()
{
	return mMultiSampleCount;
}

bool RenderTexture::TransitionDepthStencilLayout(ID3D12GraphicsCommandList* commandList, ResourceLayout newLayout)
{
	bool result = mRenderer->RecordTransition(commandList, mDepthStencilBuffer, mDepthStencilBufferLayout, newLayout);
	if (result)
		mDepthStencilBufferLayout = newLayout;
	return result;
}

bool RenderTexture::TransitionRenderTargetLayout(ID3D12GraphicsCommandList* commandList, ResourceLayout newLayout)
{
	bool result = mRenderer->RecordTransition(commandList, mRenderTargetBuffer, mRenderTargetBufferLayout, newLayout);
	if (result)
		mRenderTargetBufferLayout = newLayout;
	return result;
}

void RenderTexture::ResolveRenderTargetToTexture(ID3D12GraphicsCommandList* commandList)
{
	fatalAssert(mRenderTargetFormat == mTextureFormat && mRenderTargetBuffer != mTextureBuffer);
	mRenderer->RecordResolve(commandList, mRenderTargetBuffer, 0, mTextureBuffer, 0, mRenderTargetFormat);
}

void RenderTexture::ResolveDepthStencilToTexture(ID3D12GraphicsCommandList* commandList)
{
	fatalAssert(mDepthStencilFormat == mTextureFormat && mDepthStencilBuffer != mTextureBuffer);
	mRenderer->RecordResolve(commandList, mDepthStencilBuffer, 0, mTextureBuffer, 0, mDepthStencilFormat);
}

void RenderTexture::MakeReadyToWrite(ID3D12GraphicsCommandList* commandList)
{
	if (ReadFromColor())
		TransitionRenderTargetLayout(commandList, ResourceLayout::RENDER_TARGET);
	else if (ReadFromDepth() || ReadFromStencil())
		TransitionDepthStencilLayout(commandList, ResourceLayout::DEPTH_STENCIL_WRITE);
	else
		fatalf("read from wrong source");
}

void RenderTexture::MakeReadyToRead(ID3D12GraphicsCommandList* commandList)
{
	if(IsResolveNeeded())
	{
		if (ReadFromColor())
		{
			TransitionRenderTargetLayout(commandList, ResourceLayout::RESOLVE_SRC);
			TransitionTextureLayout(commandList, ResourceLayout::RESOLVE_DST);
			ResolveRenderTargetToTexture(commandList);
			TransitionTextureLayout(commandList, ResourceLayout::SHADER_READ);
		}
		else if (ReadFromDepth() || ReadFromStencil())
		{
			TransitionDepthStencilLayout(commandList, ResourceLayout::RESOLVE_SRC);
			TransitionTextureLayout(commandList, ResourceLayout::RESOLVE_DST);
			ResolveDepthStencilToTexture(commandList);
			TransitionTextureLayout(commandList, ResourceLayout::SHADER_READ);
		}
		else
			fatalf("read from wrong source");
	}
	else
	{
		if (ReadFromColor())
			TransitionRenderTargetLayout(commandList, ResourceLayout::SHADER_READ);
		else if (ReadFromDepth() || ReadFromStencil())
			TransitionDepthStencilLayout(commandList, ResourceLayout::SHADER_READ);
		else
			fatalf("read from wrong source");
	}
}
