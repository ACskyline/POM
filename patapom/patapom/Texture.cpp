#include "Texture.h"

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
		TextureType::INVALID,
		fileName,
		debugName,
		sampler,
		useMipmap,
		format,
		0,
		0,
		0)
{
}

Texture::Texture(
	TextureType type,
	const string& fileName, 
	const wstring& debugName, 
	Sampler sampler,
	bool useMipmap,
	Format format,
	u32 width,
	u32 height,
	u32 depth) :
	mType(type),
	mFileName(fileName), 
	mDebugName(debugName),
	mUseMipmap(useMipmap),
	mInitialized(false),
	mWidth(width), 
	mHeight(height), 
	mDepth(depth),
	mMipLevelCount(1),
	mRenderer(nullptr), 
	mImageData(nullptr), 
	mTextureBuffer(nullptr),
	mTextureLayouts(vector<vector<ResourceLayout>>(depth, vector<ResourceLayout>(1, ResourceLayout::SHADER_READ))),
	mSampler(sampler),
	mTextureFormat(format)
{
	mSrv.mType = View::Type::SRV;
	fatalAssertf(mType != TextureType::DEFAULT || mDepth == 1, "2d render texture can only have 1 depth slice at the moment");
	fatalAssertf(mType != TextureType::CUBE || mDepth == 6, "cube map must have 6 slices");
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
	int width = 0;
	int height = 0;
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
		data = stbi_load_16(mFileName.c_str(), &width, &height, &channelCountOriginal, channelCountRequested);
		break;
	case Format::R8G8B8A8_UNORM:
		bytePerChannel = 1;
		channelCountRequested = STBI_rgb_alpha;
		data = stbi_load(mFileName.c_str(), &width, &height, &channelCountOriginal, channelCountRequested);//default is 8 bit version
		break;
	default:
		fatalf("Texture format is wrong");
		break;
	}
	
	fatalAssertf(data, "Texture load failed for %s", mFileName.c_str());
	mWidth = width;
	mHeight = height;
	int bytePerRow = width * channelCountRequested * bytePerChannel;
	int bytePerSlice = bytePerRow * height;

	if(!mUseMipmap)
		mMipLevelCount = 1;
	else
	{
		mMipLevelCount = floor(log2(max(mWidth, mHeight))) + 1;
	}

	// TODO: add support for reading .dds file so that we can load cube maps or volume textures directly
	mType = TextureType::DEFAULT;
	mDepth = 1;

	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Dimension = Renderer::GetResourceDimensionFromTextureType(mType);//TODO: add support for cube map and texture array
	textureDesc.Alignment = 0; // automatically seleted by API
	textureDesc.Width = mWidth;
	textureDesc.Height = mHeight;
	textureDesc.DepthOrArraySize = mDepth; // Width, Height, and DepthOrArraySize must be between 1 and the maximum dimension supported for the particular feature level and texture dimension https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_resource_desc TODO: add support for cube map and texture array
	textureDesc.MipLevels = mMipLevelCount;
	textureDesc.Format = Renderer::TranslateFormat(mTextureFormat);
	textureDesc.SampleDesc.Count = 1; // same as 0
	textureDesc.SampleDesc.Quality = 0; // TODO: investigate how this affect the visual quality
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; // The arrangement of the pixels. Setting to unknown lets the driver choose the most efficient one, this is not the resource state
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE; // TODO: add uav, cross queue and mgpu support

	// create a default heap where the upload heap will copy its contents into (contents being the texture)
	ResetLayouts(mTextureLayouts, ResourceLayout::COPY_DST, mDepth, mMipLevelCount);
	mRenderer->mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&textureDesc, // the description of our texture
		Renderer::TranslateResourceLayout(ResourceLayout::COPY_DST), // We will copy the texture from the upload heap to here, so we start it out in a copy dest state
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

	TransitionTextureLayoutSingleTime(ResourceLayout::SHADER_READ, ALL_SLICES, ALL_SLICES);

	stbi_image_free(data);
	for(int i = 1;i<mMipLevelCount;i++)
		delete[] dataVec[i];

	mTextureBuffer->SetName(mDebugName.c_str());
}

// TODO: Add support for cube maps and volume maps
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

u32 Texture::GetWidth(u32 mipLevel)
{
	return mWidth / (1 << mipLevel);
}

u32 Texture::GetHeight(u32 mipLevel)
{
	return mHeight / (1 << mipLevel);
}

u32 Texture::GetDepth(u32 mipLevel)
{
	return mDepth / (1 << mipLevel);
}

u32 Texture::GetMipLevelCount()
{
	return mMipLevelCount;
}

void Texture::InitTexture(Renderer* renderer)
{
	mRenderer = renderer;
	CreateTextureBuffer();
	CreateView();
	CreateSampler();
	mInitialized = true;
}

bool Texture::TransitionTextureLayoutSingleTime(ResourceLayout newLayout, u32 depthSlice, u32 mipSlice)
{
	return TransitionLayoutInternal(nullptr, mTextureBuffer, mTextureLayouts, newLayout, depthSlice, mipSlice);
}

bool Texture::TransitionTextureLayout(ID3D12GraphicsCommandList* commandList, ResourceLayout newLayout, u32 depthSlice, u32 mipSlice)
{
	return TransitionLayoutInternal(commandList, mTextureBuffer, mTextureLayouts, newLayout, depthSlice, mipSlice);
}

bool Texture::TransitionLayoutInternal(ID3D12GraphicsCommandList* commandList, Resource* resource, vector<vector<ResourceLayout>>& layouts, ResourceLayout newLayout, u32 depthSlice, u32 mipSlice)
{
	bool result = true;
	int depthBegin = depthSlice == ALL_SLICES ? 0 : depthSlice;
	int depthEnd = depthBegin + (depthSlice == ALL_SLICES ? mDepth : 1);
	fatalAssert(depthBegin >= 0 && depthBegin < mDepth);
	fatalAssert(depthEnd >= depthBegin && depthEnd <= mDepth);
	for (int i = depthBegin; i < depthEnd; i++) {
		int mipBegin = mipSlice == ALL_SLICES ? 0 : mipSlice;
		int mipEnd = mipBegin + (mipSlice == ALL_SLICES ? mMipLevelCount : 1);
		fatalAssert(mipBegin >= 0 && mipBegin < mMipLevelCount);
		fatalAssert(mipEnd >= mipBegin && mipEnd <= mMipLevelCount);
		for (int j = mipBegin; j < mipEnd; j++) {
			u32 subresource = mRenderer->CalculateSubresource(i, j, mDepth, mMipLevelCount);
			bool tempResult = commandList ? mRenderer->RecordTransition(commandList, resource, subresource, layouts[i][j], newLayout) 
				: mRenderer->TransitionSingleTime(resource, subresource, layouts[i][j], newLayout);
			if (tempResult)
				SetLayout(layouts, newLayout, i, j);
			result = result && tempResult;
		}
	}
	return result;
}

void Texture::ResetLayouts(vector<vector<ResourceLayout>>& layouts, ResourceLayout newLayout, u32 depth, u32 mipLevelCount)
{
	layouts.resize(depth);
	for (auto& it : layouts)
		it.resize(mMipLevelCount);
	SetLayout(layouts, newLayout, ALL_SLICES, ALL_SLICES);
}

void Texture::SetLayout(vector<vector<ResourceLayout>>& layouts, ResourceLayout newLayout, u32 depthSlice, u32 mipSlice)
{
	int depthBegin = depthSlice == ALL_SLICES ? 0 : depthSlice;
	int depthEnd = depthBegin + (depthSlice == ALL_SLICES ? layouts.size() : 1);
	fatalAssert(depthBegin >= 0 && depthBegin < layouts.size());
	fatalAssert(depthEnd >= depthBegin && depthEnd <= layouts.size());
	for (int i = depthBegin; i < depthEnd; i++) {
		int mipBegin = mipSlice == ALL_SLICES ? 0 : mipSlice;
		int mipEnd = mipBegin + (mipSlice == ALL_SLICES ? layouts[i].size() : 1);
		fatalAssert(mipBegin >= 0 && mipBegin < layouts[i].size());
		fatalAssert(mipEnd >= mipBegin && mipEnd <= layouts[i].size());
		for (int j = mipBegin; j < mipEnd; j++) {
			layouts[i][j] = newLayout;
		}
	}
}

///////////////////////////////////////////////////////////////
/////////////// RENDER TEXTURE STARTS FROM HERE ///////////////
///////////////////////////////////////////////////////////////

// only enable color buffer
RenderTexture::RenderTexture(
	TextureType type,
	const wstring& debugName,
	u32 width,
	u32 height,
	u32 depth,
	u32 mipLevelCount,
	ReadFrom readFrom,
	Sampler sampler,
	Format renderTargetFormat,
	XMFLOAT4 colorClearValue,
	u8 multiSampleCount) :
	RenderTexture(
		type,
		debugName,
		width,
		height,
		depth,
		mipLevelCount,
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

// only enable depthStencil buffer
RenderTexture::RenderTexture(
	TextureType type,
	const wstring& debugName,
	u32 width,
	u32 height,
	u32 depth,
	u32 mipLevelCount,
	ReadFrom readFrom,
	Sampler sampler,
	Format depthStencilFormat,
	float depthClearValue,
	u8 stencilClearValue,
	u8 multiSampleCount) :
	RenderTexture(
		type,
		debugName,
		width,
		height,
		depth,
		mipLevelCount,
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

// enable both color and depthStencil buffer
RenderTexture::RenderTexture(
	TextureType type,
	const wstring& debugName,
	u32 width,
	u32 height,
	u32 depth,
	u32 mipLevelCount,
	ReadFrom readFrom,
	Sampler sampler,
	Format renderTargetFormat,
	Format depthStencilFormat,
	XMFLOAT4 colorClearValue,
	float depthClearValue,
	u8 stencilClearValue,
	u8 multiSampleCount) :
	RenderTexture(
		type,
		debugName,
		width,
		height,
		depth,
		mipLevelCount,
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
	TextureType type,
	const wstring& debugName,
	u32 width,
	u32 height,
	u32 depth,
	u32 mipLevelCount,
	ReadFrom readFrom,
	Sampler sampler,
	bool useRenderTarget,
	bool useDepthStencil,
	Format renderTargetFormat,
	Format depthStencilFormat,
	XMFLOAT4 colorClearValue,
	float depthClearValue,
	u8 stencilClearValue,
	u8 multiSampleCount) :
	mReadFrom(readFrom),
	mUseRenderTarget(useRenderTarget),
	mUseDepthStencil(useDepthStencil),
	mRenderTargetFormat(renderTargetFormat),
	mDepthStencilFormat(depthStencilFormat),
	mColorClearValue(colorClearValue),
	mDepthClearValue(depthClearValue),
	mStencilClearValue(stencilClearValue),
	mDepthStencilBuffer(nullptr),
	mDepthStencilLayouts(vector<vector<ResourceLayout>>(depth, vector<ResourceLayout>(mipLevelCount, ResourceLayout::DEPTH_STENCIL_WRITE))),
	mRenderTargetLayouts(vector<vector<ResourceLayout>>(depth, vector<ResourceLayout>(mipLevelCount, ResourceLayout::RENDER_TARGET))),
	mMultiSampleCount(multiSampleCount),
	Texture(type, "no file", debugName, sampler, mipLevelCount > 1, Format::INVALID, width, height, depth)
{
	u32 maxMipLevelCount = floor(log2(max(mWidth, mHeight))) + 1;
	fatalAssertf(maxMipLevelCount > 0 && mipLevelCount <= maxMipLevelCount, "mip level count out of range");
	fatalAssertf(mipLevelCount == 1 || !IsMultiSampleUsed(), "multi sampled render textures don't have mip maps");
	mMipLevelCount = mipLevelCount;
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
	fatalAssertf((!mUseMipmap && mMipLevelCount == 1) || (mUseMipmap && mMipLevelCount > 1), "mip level count error!");

	// 1. Render Target Buffer
	D3D12_RESOURCE_DESC renderTargetDesc = {};
	if (mUseRenderTarget)
	{
		renderTargetDesc.Dimension = Renderer::GetResourceDimensionFromTextureType(mType);
		renderTargetDesc.Alignment = 0;
		renderTargetDesc.Width = mWidth;
		renderTargetDesc.Height = mHeight;
		renderTargetDesc.DepthOrArraySize = mDepth;
		renderTargetDesc.MipLevels = mMipLevelCount;
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

		ResetLayouts(mRenderTargetLayouts, ResourceLayout::RENDER_TARGET, mDepth, mMipLevelCount);
		HRESULT hr = mRenderer->mDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&renderTargetDesc,
			Renderer::TranslateResourceLayout(ResourceLayout::RENDER_TARGET),
			&colorClearValue,
			IID_PPV_ARGS(&mRenderTargetBuffer));

		fatalAssertf(SUCCEEDED(hr), "create render target buffer failed");
		mRenderTargetBuffer->SetName(mDebugName.c_str());
	}

	// 2. Depth Stencil Buffer
	D3D12_RESOURCE_DESC depthStencilDesc = {};
	if (mUseDepthStencil)
	{
		depthStencilDesc.Dimension = Renderer::GetResourceDimensionFromTextureType(mType);
		depthStencilDesc.Alignment = 0;
		depthStencilDesc.Width = mWidth;
		depthStencilDesc.Height = mHeight;
		depthStencilDesc.DepthOrArraySize = mDepth;
		depthStencilDesc.MipLevels = mMipLevelCount;
		depthStencilDesc.Format = (ReadFromDepth() || ReadFromStencil()) ? Renderer::TranslateToTypelessFormat(mDepthStencilFormat) : Renderer::TranslateFormat(mDepthStencilFormat);
		depthStencilDesc.SampleDesc.Count = mMultiSampleCount;
		depthStencilDesc.SampleDesc.Quality = 0;
		depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE depthStencilClearValue = {};
		depthStencilClearValue.Format = Renderer::TranslateFormat(mDepthStencilFormat);
		depthStencilClearValue.DepthStencil.Depth = mDepthClearValue;
		depthStencilClearValue.DepthStencil.Stencil = mStencilClearValue;

		ResetLayouts(mDepthStencilLayouts, ResourceLayout::DEPTH_STENCIL_WRITE, mDepth, mMipLevelCount);
		HRESULT hr = mRenderer->mDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			Renderer::TranslateResourceLayout(ResourceLayout::DEPTH_STENCIL_WRITE),
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

		ResetLayouts(mTextureLayouts, ResourceLayout::RESOLVE_DST, mDepth, mMipLevelCount);
		HRESULT hr = mRenderer->mDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			Renderer::TranslateResourceLayout(ResourceLayout::RESOLVE_DST),
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

		ResetLayouts(mTextureLayouts, ResourceLayout::SHADER_READ, mDepth, mMipLevelCount);
	}
}

// TODO: Add support for cube maps and volume maps
void RenderTexture::CreateView()
{
	// 1.Render Target View
	SetUpRTV();
	
	// 2.Depth Stencil View Desc
	// if you want to bind null dsv desciptor, you need to create the (null) dsv
	// if you want to create the dsv, you need an initialized dsvDesc to avoid the debug layer throwing out error
	SetUpDSV();
	
	// 3.Shader Resource View
	SetUpSRV();
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

D3D12_RENDER_TARGET_VIEW_DESC RenderTexture::GetRtvDesc(u32 depthSlice, u32 mipSlice)
{
	fatalAssertf(mRtvMip0.mType == View::Type::RTV, "rtv type wrong");
	fatalAssertf(depthSlice < mDepth, "depthSlice out of range");
	fatalAssertf(mipSlice < mMipLevelCount, "mipSlice out of range");
	fatalAssertf(mipSlice == 0 || !IsMultiSampleUsed(), "multi sampled rtv doesn't have mip map");
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = mRtvMip0.mRtvDesc;
	switch (mType)
	{
	case TextureType::DEFAULT:
		if (!IsMultiSampleUsed()) // multi sampled rtv doesn't have mip map
			rtvDesc.Texture2D.MipSlice = mipSlice;
		break;
	case TextureType::CUBE:
		if (IsMultiSampleUsed())
		{
			rtvDesc.Texture2DArray.ArraySize = 1; // TODO: only render to 1 depth slice, add support to render to multiple array slices
			rtvDesc.Texture2DMSArray.FirstArraySlice = depthSlice;
		}
		else
		{
			rtvDesc.Texture2DArray.MipSlice = mipSlice;
			rtvDesc.Texture2DArray.ArraySize = 1; // TODO: only render to 1 depth slice, add support to render to multiple array slices
			rtvDesc.Texture2DArray.FirstArraySlice = depthSlice;
		}
		break;
	case TextureType::VOLUME:
		if (IsMultiSampleUsed())
		{
			fatalf("volume rtv can't be multisampled");
		}
		else
		{
			rtvDesc.Texture3D.MipSlice = mipSlice;
			rtvDesc.Texture3D.FirstWSlice = depthSlice;
			rtvDesc.Texture3D.WSize = 1; // TODO: only render to 1 depth slice, add support to render to multiple array slices
		}
		break;
	default:
		fatalf("wrong texture type");
	}
	return rtvDesc;
}

D3D12_DEPTH_STENCIL_VIEW_DESC RenderTexture::GetDsvDesc(u32 depthSlice, u32 mipSlice)
{
	fatalAssertf(mDsvMip0.mType == View::Type::DSV, "dsv type wrong");
	fatalAssertf(depthSlice < mDepth, "depthSlice out of range");
	fatalAssertf(mipSlice < mMipLevelCount, "mipSlice out of range");
	fatalAssertf(mipSlice == 0 || !IsMultiSampleUsed(), "multi sampled dsv doesn't have mip map");
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = mDsvMip0.mDsvDesc;
	switch (mType)
	{
	case TextureType::DEFAULT:
		if (!IsMultiSampleUsed()) // multi sampled dsv doesn't have mip map
			dsvDesc.Texture2D.MipSlice = mipSlice;
		break;
	case TextureType::CUBE:
		if (IsMultiSampleUsed())
		{
			dsvDesc.Texture2DMSArray.ArraySize = 1; // TODO: only render to 1 depth slice, add support to render to multiple array slices
			dsvDesc.Texture2DMSArray.FirstArraySlice = depthSlice;
		}
		else
		{
			dsvDesc.Texture2DArray.ArraySize = 1; // TODO: only render to 1 depth slice, add support to render to multiple array slices
			dsvDesc.Texture2DArray.MipSlice = mipSlice;
			dsvDesc.Texture2DArray.FirstArraySlice = depthSlice;
		}
		break;
	case TextureType::VOLUME:
		fatalf("volume dsv is not allowed");
		break;
	default:
		fatalf("wrong texture type");
	}
	return dsvDesc;
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

bool RenderTexture::TransitionDepthStencilLayout(ID3D12GraphicsCommandList* commandList, ResourceLayout newLayout, u32 depthSlice, u32 mipSlice)
{
	return TransitionLayoutInternal(commandList, mDepthStencilBuffer, mDepthStencilLayouts, newLayout, depthSlice, mipSlice);
}

bool RenderTexture::TransitionRenderTargetLayout(ID3D12GraphicsCommandList* commandList, ResourceLayout newLayout, u32 depthSlice, u32 mipSlice)
{
	return TransitionLayoutInternal(commandList, mRenderTargetBuffer, mRenderTargetLayouts, newLayout, depthSlice, mipSlice);
}

void RenderTexture::ResolveRenderTargetToTexture(ID3D12GraphicsCommandList* commandList, u32 depthSlice, u32 mipSlice)
{
	ResolveToTexture(commandList, mRenderTargetBuffer, mRenderTargetFormat, depthSlice, mipSlice);
}

void RenderTexture::ResolveDepthStencilToTexture(ID3D12GraphicsCommandList* commandList, u32 depthSlice, u32 mipSlice)
{
	ResolveToTexture(commandList, mDepthStencilBuffer, mDepthStencilFormat, depthSlice, mipSlice);
}

void RenderTexture::MakeReadyToWrite(ID3D12GraphicsCommandList* commandList, u32 depthSlice, u32 mipSlice)
{
	if (ReadFromColor())
		TransitionRenderTargetLayout(commandList, ResourceLayout::RENDER_TARGET, depthSlice, mipSlice);
	else if (ReadFromDepth() || ReadFromStencil())
		TransitionDepthStencilLayout(commandList, ResourceLayout::DEPTH_STENCIL_WRITE, depthSlice, mipSlice);
	else
		fatalf("read from wrong source");
}

void RenderTexture::MakeReadyToRead(ID3D12GraphicsCommandList* commandList, u32 depthSlice, u32 mipSlice)
{
	if (IsResolveNeeded())
	{
		if (ReadFromColor())
		{
			TransitionRenderTargetLayout(commandList, ResourceLayout::RESOLVE_SRC, depthSlice, mipSlice);
			TransitionTextureLayout(commandList, ResourceLayout::RESOLVE_DST, depthSlice, mipSlice);
			ResolveRenderTargetToTexture(commandList, depthSlice, mipSlice);
			TransitionTextureLayout(commandList, ResourceLayout::SHADER_READ, depthSlice, mipSlice);
		}
		else if (ReadFromDepth() || ReadFromStencil())
		{
			TransitionDepthStencilLayout(commandList, ResourceLayout::RESOLVE_SRC, depthSlice, mipSlice);
			TransitionTextureLayout(commandList, ResourceLayout::RESOLVE_DST, depthSlice, mipSlice);
			ResolveDepthStencilToTexture(commandList, depthSlice, mipSlice);
			TransitionTextureLayout(commandList, ResourceLayout::SHADER_READ, depthSlice, mipSlice);
		}
		else
			fatalf("read from wrong source");
	}
	else
	{
		if (ReadFromColor())
			TransitionRenderTargetLayout(commandList, ResourceLayout::SHADER_READ, depthSlice, mipSlice);
		else if (ReadFromDepth() || ReadFromStencil())
			TransitionDepthStencilLayout(commandList, ResourceLayout::SHADER_READ, depthSlice, mipSlice);
		else
			fatalf("read from wrong source");
	}
}

void RenderTexture::SetUpRTV()
{
	mRtvMip0.mType = View::Type::RTV;
	mRtvMip0.mRtvDesc.Format = Renderer::TranslateFormat(mRenderTargetFormat);
	switch (mType)
	{
	case TextureType::DEFAULT:
		if (IsMultiSampleUsed())
		{
			mRtvMip0.mRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
			mRtvMip0.mRtvDesc.Texture2DMS.UnusedField_NothingToDefine = 0;
		}
		else
		{
			mRtvMip0.mRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			mRtvMip0.mRtvDesc.Texture2D.MipSlice = 0;
			mRtvMip0.mRtvDesc.Texture2D.PlaneSlice = 0;
		}
		break;
	case TextureType::CUBE:
		if (IsMultiSampleUsed())
		{
			mRtvMip0.mRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
			mRtvMip0.mRtvDesc.Texture2DMSArray.FirstArraySlice = 0;
			mRtvMip0.mRtvDesc.Texture2DMSArray.ArraySize = 1;
		}
		else
		{
			mRtvMip0.mRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			mRtvMip0.mRtvDesc.Texture2DArray.MipSlice = 0;
			mRtvMip0.mRtvDesc.Texture2DArray.FirstArraySlice = 0;
			mRtvMip0.mRtvDesc.Texture2DArray.ArraySize = 1;
			mRtvMip0.mRtvDesc.Texture2DArray.PlaneSlice = 0;
		}
		break;
	case TextureType::VOLUME:
		if (IsMultiSampleUsed())
		{
			fatalf("volume rtv can't be multisampled");
		}
		else
		{
			mRtvMip0.mRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			mRtvMip0.mRtvDesc.Texture3D.MipSlice = 0;
			mRtvMip0.mRtvDesc.Texture3D.FirstWSlice = 0;
			mRtvMip0.mRtvDesc.Texture3D.WSize = 1;
		}
		break;
	default:
		fatalf("wrong texture type");
	}
}

void RenderTexture::SetUpDSV()
{
	mDsvMip0.mType = View::Type::DSV;
	mDsvMip0.mDsvDesc.Format = Renderer::TranslateFormat(mDepthStencilFormat);
	mDsvMip0.mDsvDesc.Flags = D3D12_DSV_FLAG_NONE; // TODO: this flag contains samilar feature as Vulkan layouts like VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
	switch (mType)
	{
	case TextureType::DEFAULT:
		if (IsMultiSampleUsed())
		{
			mDsvMip0.mDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
			mDsvMip0.mDsvDesc.Texture2DMS.UnusedField_NothingToDefine = 0;
		}
		else
		{
			mDsvMip0.mDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			mDsvMip0.mDsvDesc.Texture2D.MipSlice = 0;
		}
		break;
	case TextureType::CUBE:
		if (IsMultiSampleUsed())
		{
			mDsvMip0.mDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
			mDsvMip0.mDsvDesc.Texture2DMSArray.FirstArraySlice = 0;
			mDsvMip0.mDsvDesc.Texture2DMSArray.ArraySize = 1;
		}
		else
		{
			mDsvMip0.mDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			mDsvMip0.mDsvDesc.Texture2DArray.MipSlice = 0;
			mDsvMip0.mDsvDesc.Texture2DArray.FirstArraySlice = 0;
			mDsvMip0.mDsvDesc.Texture2DArray.ArraySize = 1;
		}
		break;
	case TextureType::VOLUME:
		fatalf("volume dsv is not allowed");
		break;
	default:
		fatalf("wrong texture type");
	}
}

void RenderTexture::SetUpSRV()
{
	mSrv.mType = View::Type::SRV;
	mSrv.mSrvDesc.Format = Renderer::TranslateToSrvFormat(mTextureFormat, mReadFrom);
	mSrv.mSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	switch (mType)
	{
	case TextureType::DEFAULT:
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
		break;
	case TextureType::CUBE:
		if (ReadFromMultiSampledBuffer())
		{
			fatalf("cube srv can't be multisampled");
		}
		else
		{
			mSrv.mSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			mSrv.mSrvDesc.TextureCube.MostDetailedMip = 0;
			mSrv.mSrvDesc.TextureCube.MipLevels = mMipLevelCount;
			mSrv.mSrvDesc.TextureCube.ResourceMinLODClamp = 0;
		}
		break;
	case TextureType::VOLUME:
		if (ReadFromMultiSampledBuffer())
		{
			fatalf("volume srv can't be multisampled");
		}
		else
		{
			mSrv.mSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			mSrv.mSrvDesc.Texture3D.MostDetailedMip = 0;
			mSrv.mSrvDesc.Texture3D.MipLevels = mMipLevelCount;
			mSrv.mSrvDesc.Texture3D.ResourceMinLODClamp = 0;
		}
		break;
	default:
		fatalf("wrong texture type");
	}
}

void RenderTexture::ResolveToTexture(ID3D12GraphicsCommandList* commandList, Resource* resource, Format format, u32 depthSlice, u32 mipSlice)
{
	int depthBegin = depthSlice == ALL_SLICES ? 0 : depthSlice;
	int depthEnd = depthBegin + (depthSlice == ALL_SLICES ? mDepth : 1);
	fatalAssert(depthBegin >= 0 && depthBegin < mDepth);
	fatalAssert(depthEnd >= depthBegin && depthEnd <= mDepth);
	for (int i = depthBegin; i < depthEnd; i++) {
		int mipBegin = mipSlice == ALL_SLICES ? 0 : mipSlice;
		int mipEnd = mipBegin + (mipSlice == ALL_SLICES ? mMipLevelCount : 1);
		fatalAssert(mipBegin >= 0 && mipBegin < mMipLevelCount);
		fatalAssert(mipEnd >= mipBegin && mipEnd <= mMipLevelCount);
		for (int j = mipBegin; j < mipEnd; j++) {
			u32 subresource = mRenderer->CalculateSubresource(i, j, mDepth, mMipLevelCount);
			mRenderer->RecordResolve(commandList, resource, subresource, mTextureBuffer, subresource, format);
		}
	}
}
