#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../dependency/stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../dependency/stb_image_resize.h"

Texture::Texture(
	const string& fileName,
	const string& debugName,
	bool useMipmap,
	Format format) :
	Texture(
		TextureType::INVALID,
		fileName,
		debugName,
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
	const string& debugName,
	bool useMipmap,
	Format format,
	u32 width,
	u32 height,
	u32 depth) :
	mTextureType(type),
	mFileName(fileName),
	mUseMipmap(useMipmap),
	mWidth(width), 
	mHeight(height), 
	mDepth(depth),
	mMipLevelCount(1),
	mRenderer(nullptr), 
	mImageData(nullptr), 
	mTextureBuffer(nullptr),
	mTextureLayouts(vector<vector<ResourceLayout>>(depth, vector<ResourceLayout>(1, ResourceLayout::SHADER_READ))),
	mTextureFormat(format),
	ShaderResource(debugName, ShaderResource::Type::TEXTURE)
{
	mSrv.mType = View::Type::SRV;
	fatalAssertf(mTextureType != TextureType::TEX_2D || mDepth == 1, "2d render texture can only have 1 depth slice at the moment");
	fatalAssertf(mTextureType != TextureType::TEX_CUBE || mDepth == 6, "cube map must have 6 slices");
}

Texture::~Texture()
{
	Texture::Release(true);
}

void Texture::Release(bool checkOnly)
{
	SAFE_RELEASE(mTextureBuffer, checkOnly);
}

void Texture::CreateTextureBuffer()
{
	int width = 0;
	int height = 0;
	int channelCountOriginal = 0;
	int channelCountRequested = 0;
	int bytePerChannel = 0;
	void* data = nullptr;
	string fileName = AssetPath + mFileName;
	switch (mTextureFormat)
	{
	case Format::R16G16B16A16_UNORM:
	case Format::R16G16B16A16_FLOAT:
		bytePerChannel = 2;
		channelCountRequested = STBI_rgb_alpha;
		data = stbi_load_16(fileName.c_str(), &width, &height, &channelCountOriginal, channelCountRequested);
		break;
	case Format::R8G8B8A8_UNORM:
		bytePerChannel = 1;
		channelCountRequested = STBI_rgb_alpha;
		data = stbi_load(fileName.c_str(), &width, &height, &channelCountOriginal, channelCountRequested);//default is 8 bit version
		break;
	default:
		fatalf("Texture format is wrong");
		break;
	}
	
	fatalAssertf(data, "Texture load failed for %s", fileName.c_str());
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
	mTextureType = TextureType::TEX_2D;
	mDepth = 1;

	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Dimension = Renderer::GetResourceDimensionFromTextureType(mTextureType);//TODO: add support for cube map and texture array
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
	mRenderer->WriteTextureDataToBufferSync(mTextureBuffer, textureDesc, dataVec, bytePerRowVec, bytePerSliceVec);

	TransitionTextureLayoutSingleTime(ResourceLayout::SHADER_READ, ALL_SLICES, ALL_SLICES);

	stbi_image_free(data);
	for(int i = 1;i<mMipLevelCount;i++)
		delete[] dataVec[i];

	mTextureBuffer->SetName(StringToWstring(mDebugName).c_str());
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

ID3D12Resource* Texture::GetTextureBuffer()
{
	fatalAssertf(mInitialized, "texture %s is not initialized!", mFileName.c_str());
	return mTextureBuffer;
}

D3D12_SHADER_RESOURCE_VIEW_DESC Texture::GetSrvDesc() const
{
	fatalAssertf(mInitialized, "texture %s is not initialized!", mFileName.c_str());
	fatalAssertf(mSrv.mType == View::Type::SRV, "srv type wrong");
	return mSrv.mSrvDesc;
}

Format Texture::GetTextureFormat()
{
	return mTextureFormat;
}

string Texture::GetName()
{
	return mFileName;
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
	mInitialized = true;
}

bool Texture::TransitionTextureLayoutSingleTime(ResourceLayout newLayout, u32 depthSlice, u32 mipSlice)
{
	return TransitionLayoutInternal(nullptr, mTextureBuffer, mTextureLayouts, newLayout, depthSlice, mipSlice);
}

bool Texture::TransitionTextureLayout(CommandList commandList, ResourceLayout newLayout, u32 depthSlice, u32 mipSlice)
{
	return TransitionLayoutInternal(&commandList, mTextureBuffer, mTextureLayouts, newLayout, depthSlice, mipSlice);
}

bool Texture::TransitionLayoutInternal(CommandList* commandList, Resource* resource, vector<vector<ResourceLayout>>& layouts, ResourceLayout newLayout, u32 depthSlice, u32 mipSlice)
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
			bool tempResult = commandList ? mRenderer->RecordTransition(*commandList, resource, subresource, layouts[i][j], newLayout) 
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
	const string& debugName,
	u32 width,
	u32 height,
	u32 depth,
	u32 mipLevelCount,
	ReadFrom readFrom,
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
	const string& debugName,
	u32 width,
	u32 height,
	u32 depth,
	u32 mipLevelCount,
	ReadFrom readFrom,
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
	const string& debugName,
	u32 width,
	u32 height,
	u32 depth,
	u32 mipLevelCount,
	ReadFrom readFrom,
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
	const string& debugName,
	u32 width,
	u32 height,
	u32 depth,
	u32 mipLevelCount,
	ReadFrom readFrom,
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
	mRenderTargetBuffer(nullptr),
	mDepthStencilBuffer(nullptr),
	mDepthStencilLayouts(vector<vector<ResourceLayout>>(depth, vector<ResourceLayout>(mipLevelCount, ResourceLayout::DEPTH_STENCIL_WRITE))),
	mRenderTargetLayouts(vector<vector<ResourceLayout>>(depth, vector<ResourceLayout>(mipLevelCount, ResourceLayout::RENDER_TARGET))),
	mMultiSampleCount(multiSampleCount),
	Texture(type, "no file", debugName, mipLevelCount > 1, Format::INVALID, width, height, depth)
{
	u32 maxMipLevelCount = floor(log2(max(mWidth, mHeight))) + 1;
	fatalAssertf(maxMipLevelCount > 0 && mipLevelCount <= maxMipLevelCount, "mip level count out of range");
	fatalAssertf(mipLevelCount == 1 || !IsMultiSampleUsed(), "multi sampled render textures don't have mip maps");
	mMipLevelCount = mipLevelCount;
}

RenderTexture::~RenderTexture()
{
	RenderTexture::Release(true);
}

void RenderTexture::Release(bool checkOnly)
{
	bool textureBufferIsRenderTargetBuffer = mTextureBuffer == mRenderTargetBuffer;
	bool textureBufferIsDepthStencilBuffer = mTextureBuffer == mDepthStencilBuffer;
	Texture::Release(checkOnly);
	if (textureBufferIsRenderTargetBuffer)
		mRenderTargetBuffer = nullptr;
	else
		SAFE_RELEASE(mRenderTargetBuffer, checkOnly);
	if (textureBufferIsDepthStencilBuffer)
		mDepthStencilBuffer = nullptr;
	else
		SAFE_RELEASE(mDepthStencilBuffer, checkOnly);
}

void RenderTexture::CreateTextureBuffer()
{
	fatalAssertf((!mUseMipmap && mMipLevelCount == 1) || (mUseMipmap && mMipLevelCount > 1), "mip level count error!");

	// 1. Render Target Buffer
	D3D12_RESOURCE_DESC renderTargetDesc = {};
	if (mUseRenderTarget)
	{
		renderTargetDesc.Dimension = Renderer::GetResourceDimensionFromTextureType(mTextureType);
		renderTargetDesc.Alignment = 0;
		renderTargetDesc.Width = mWidth;
		renderTargetDesc.Height = mHeight;
		renderTargetDesc.DepthOrArraySize = mDepth;
		renderTargetDesc.MipLevels = mMipLevelCount;
		renderTargetDesc.Format = Renderer::TranslateFormat(mRenderTargetFormat);
		renderTargetDesc.SampleDesc.Count = mMultiSampleCount;
		renderTargetDesc.SampleDesc.Quality = 0;
		renderTargetDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		renderTargetDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // always allow UAV no matter want it or not

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
		mRenderTargetBuffer->SetName(StringToWstring(mDebugName).c_str());
	}

	// 2. Depth Stencil Buffer
	D3D12_RESOURCE_DESC depthStencilDesc = {};
	if (mUseDepthStencil)
	{
		depthStencilDesc.Dimension = Renderer::GetResourceDimensionFromTextureType(mTextureType);
		depthStencilDesc.Alignment = 0;
		depthStencilDesc.Width = mWidth;
		depthStencilDesc.Height = mHeight;
		depthStencilDesc.DepthOrArraySize = mDepth;
		depthStencilDesc.MipLevels = mMipLevelCount;
		depthStencilDesc.Format = ReadFromDepthStencil() ? Renderer::TranslateToTypelessFormat(mDepthStencilFormat) : Renderer::TranslateFormat(mDepthStencilFormat);
		depthStencilDesc.SampleDesc.Count = mMultiSampleCount;
		depthStencilDesc.SampleDesc.Quality = 0;
		depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		// D3D12 ERROR: ID3D12Device::CreateCommittedResource: D3D12_RESOURCE_DESC::Flags cannot have D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL set 
		// with D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, nor D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS. 
		// [ STATE_CREATION ERROR #599: CREATERESOURCE_INVALIDMISCFLAGS]
		depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // can't set this due to the error  above

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
		mDepthStencilBuffer->SetName(StringToWstring(mDebugName).c_str());
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
		else if (ReadFromDepthStencil())
		{
			mTextureFormat = mDepthStencilFormat;
			textureDesc = depthStencilDesc;
			textureDesc.Format = Renderer::TranslateFormatToRead(mTextureFormat, mReadFrom);
			textureDesc.SampleDesc.Count = 1;
		}
		else
			fatalf("mReadFrom is invalid");

		// always allow UAV no matter want it or not
		textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
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
		mTextureBuffer->SetName(StringToWstring(mDebugName).c_str());
	}
	else // don't need to resolve
	{
		if (ReadFromColor())
		{
			mTextureFormat = mRenderTargetFormat;
			mTextureBuffer = mRenderTargetBuffer;
		}
		else if (ReadFromDepthStencil())
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
	CreateRtv();
	
	// 2.Depth Stencil View Desc
	// if you want to bind null dsv desciptor, you need to create the (null) dsv
	// if you want to create the dsv, you need an initialized dsvDesc to avoid the debug layer throwing out error
	CreateDsv();
	
	// 3.Shader Resource View
	CreateSrv();

	// 4.Unordered Access View
	CreateUav();
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

bool RenderTexture::ReadFromDepthStencil()
{
	return ReadFromDepth() || ReadFromStencil();
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
	fatalAssertf(mInitialized, "texture %s is not initialized!", mFileName.c_str());
	return  mRenderTargetBuffer;
}

ID3D12Resource* RenderTexture::GetDepthStencilBuffer()
{
	// make sure to create null descriptor for render textures which does not support depth
	fatalAssertf(mInitialized, "texture %s is not initialized!", mFileName.c_str());
	return  mDepthStencilBuffer;
}

D3D12_RENDER_TARGET_VIEW_DESC RenderTexture::GetRtvDesc(u32 depthSlice, u32 mipSlice)
{
	fatalAssertf(mRtvMip0.mType == View::Type::RTV, "rtv type wrong");
	fatalAssertf(depthSlice < mDepth, "depthSlice out of range");
	fatalAssertf(mipSlice < mMipLevelCount, "mipSlice out of range");
	fatalAssertf(mipSlice == 0 || !IsMultiSampleUsed(), "multi sampled rtv doesn't have mip map");
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = mRtvMip0.mRtvDesc;
	switch (mTextureType)
	{
	case TextureType::TEX_2D:
		if (!IsMultiSampleUsed()) // multi sampled rtv doesn't have mip map
			rtvDesc.Texture2D.MipSlice = mipSlice;
		break;
	case TextureType::TEX_CUBE:
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
	case TextureType::TEX_VOLUME:
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
	switch (mTextureType)
	{
	case TextureType::TEX_2D:
		if (!IsMultiSampleUsed()) // multi sampled dsv doesn't have mip map
			dsvDesc.Texture2D.MipSlice = mipSlice;
		break;
	case TextureType::TEX_CUBE:
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
	case TextureType::TEX_VOLUME:
		fatalf("volume dsv is not allowed");
		break;
	default:
		fatalf("wrong texture type");
	}
	return dsvDesc;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC RenderTexture::GetUavDesc(u32 mipSlice)
{
	fatalAssertf(mipSlice < mMipLevelCount, "mipSlice out of range");
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = mUavMip0.mUavDesc;
	switch (mTextureType)
	{
	case TextureType::TEX_2D:
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = mipSlice;
		uavDesc.Texture2D.PlaneSlice = 0;
		break;
	case TextureType::TEX_CUBE:
		// when write to cube map, access all depth slices
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.ArraySize = CubeFace::COUNT;
		uavDesc.Texture2DArray.MipSlice = mipSlice;
		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.PlaneSlice = 0;
		break;
	case TextureType::TEX_VOLUME:
		// when write to volume texture, access all depth slices
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		uavDesc.Texture3D.MipSlice = mipSlice;
		uavDesc.Texture3D.FirstWSlice = 0;
		uavDesc.Texture3D.WSize = mDepth;
		break;
	default:
		fatalf("wrong texture type");
	}
	return uavDesc;
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

bool RenderTexture::TransitionDepthStencilLayout(CommandList commandList, ResourceLayout newLayout, u32 depthSlice, u32 mipSlice)
{
	return TransitionLayoutInternal(&commandList, mDepthStencilBuffer, mDepthStencilLayouts, newLayout, depthSlice, mipSlice);
}

bool RenderTexture::TransitionRenderTargetLayout(CommandList commandList, ResourceLayout newLayout, u32 depthSlice, u32 mipSlice)
{
	return TransitionLayoutInternal(&commandList, mRenderTargetBuffer, mRenderTargetLayouts, newLayout, depthSlice, mipSlice);
}

void RenderTexture::ResolveRenderTargetToTexture(CommandList commandList, u32 depthSlice, u32 mipSlice)
{
	ResolveToTexture(commandList, mRenderTargetBuffer, mRenderTargetFormat, depthSlice, mipSlice);
}

void RenderTexture::ResolveDepthStencilToTexture(CommandList commandList, u32 depthSlice, u32 mipSlice)
{
	ResolveToTexture(commandList, mDepthStencilBuffer, mDepthStencilFormat, depthSlice, mipSlice);
}

void RenderTexture::MakeReadyToRender(CommandList commandList, u32 depthSlice, u32 mipSlice)
{
	if (ReadFromColor())
		TransitionRenderTargetLayout(commandList, ResourceLayout::RENDER_TARGET, depthSlice, mipSlice);
	else if (ReadFromDepthStencil())
		TransitionDepthStencilLayout(commandList, ResourceLayout::DEPTH_STENCIL_WRITE, depthSlice, mipSlice);
	else
		fatalf("read from wrong source");
}

void RenderTexture::MakeReadyToRead(CommandList commandList, u32 depthSlice, u32 mipSlice)
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
		else if (ReadFromDepthStencil())
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
		else if (ReadFromDepthStencil())
			TransitionDepthStencilLayout(commandList, ResourceLayout::SHADER_READ, depthSlice, mipSlice);
		else
			fatalf("read from wrong source");
	}
}

void RenderTexture::MakeReadyToWrite(CommandList commandList, u32 depthSlice, u32 mipSlice)
{
	if (ReadFromColor())
		TransitionRenderTargetLayout(commandList, ResourceLayout::SHADER_WRITE, depthSlice, mipSlice);
	else if (ReadFromDepthStencil())
		TransitionDepthStencilLayout(commandList, ResourceLayout::SHADER_WRITE, depthSlice, mipSlice);
	else
		fatalf("read from wrong source");
}

void RenderTexture::MakeReadyToWriteAgain(CommandList commandList, u32 depthSlice, u32 mipSlice)
{
	if (ReadFromColor())
	{
		bool hasUav = false;
		for (const auto& layoutVec : mRenderTargetLayouts)
		{
			for (const auto& layout : layoutVec)
			{
				hasUav = hasUav || layout == ResourceLayout::SHADER_WRITE;
				if (hasUav)
					break;
			}
			if (hasUav)
				break;
		}
		fatalAssertf(hasUav, "resource is not in shader write state, no need to use uav barrier");
		mRenderer->RecordBarrierWithoutTransition(commandList, mTextureBuffer);
	}
	else if (ReadFromDepthStencil())
	{
		bool hasUav = false;
		for (const auto& layoutVec : mDepthStencilLayouts)
		{
			for (const auto& layout : layoutVec)
			{
				hasUav = hasUav || layout == ResourceLayout::SHADER_WRITE;
				if (hasUav)
					break;
			}
			if (hasUav)
				break;
		}
		fatalAssertf(hasUav, "resource is not in shader write state, no need to use uav barrier");
		mRenderer->RecordBarrierWithoutTransition(commandList, mDepthStencilBuffer);
	}
	else
		fatalf("read from wrong source");
}

void RenderTexture::CreateRtv()
{
	mRtvMip0.mType = View::Type::RTV;
	mRtvMip0.mRtvDesc.Format = Renderer::TranslateFormat(mRenderTargetFormat);
	switch (mTextureType)
	{
	case TextureType::TEX_2D:
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
	case TextureType::TEX_CUBE:
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
	case TextureType::TEX_VOLUME:
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

void RenderTexture::CreateDsv()
{
	mDsvMip0.mType = View::Type::DSV;
	mDsvMip0.mDsvDesc.Format = Renderer::TranslateFormat(mDepthStencilFormat);
	mDsvMip0.mDsvDesc.Flags = D3D12_DSV_FLAG_NONE; // TODO: this flag contains samilar feature as Vulkan layouts like VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
	switch (mTextureType)
	{
	case TextureType::TEX_2D:
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
	case TextureType::TEX_CUBE:
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
	case TextureType::TEX_VOLUME:
		fatalf("volume dsv is not allowed");
		break;
	default:
		fatalf("wrong texture type");
	}
}

void RenderTexture::CreateSrv()
{
	mSrv.mType = View::Type::SRV;
	mSrv.mSrvDesc.Format = Renderer::TranslateFormatToRead(mTextureFormat, mReadFrom);
	mSrv.mSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	switch (mTextureType)
	{
	case TextureType::TEX_2D:
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
	case TextureType::TEX_CUBE:
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
	case TextureType::TEX_VOLUME:
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

void RenderTexture::CreateUav()
{
	mUavMip0.mType = View::Type::UAV;
	mUavMip0.mUavDesc.Format = Renderer::TranslateFormatToRead(mTextureFormat, mReadFrom);
	switch (mTextureType)
	{
	case TextureType::TEX_2D:
		mUavMip0.mUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		mUavMip0.mUavDesc.Texture2D.MipSlice = 0;
		mUavMip0.mUavDesc.Texture2D.PlaneSlice = 0;
		break;
	case TextureType::TEX_CUBE:
		mUavMip0.mUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		mUavMip0.mUavDesc.Texture2DArray.ArraySize = CubeFace::COUNT;
		mUavMip0.mUavDesc.Texture2DArray.MipSlice = 0;
		mUavMip0.mUavDesc.Texture2DArray.FirstArraySlice = 0;
		mUavMip0.mUavDesc.Texture2DArray.PlaneSlice = 0;
		break;
	case TextureType::TEX_VOLUME:
		mUavMip0.mUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		mUavMip0.mUavDesc.Texture3D.MipSlice = 0;
		mUavMip0.mUavDesc.Texture3D.FirstWSlice = 0;
		mUavMip0.mUavDesc.Texture3D.WSize = mDepth;
		break;
	default:
		fatalf("wrong texture type");
	}
}

void RenderTexture::ResolveToTexture(CommandList commandList, Resource* resource, Format format, u32 depthSlice, u32 mipSlice)
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
