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
		1,
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
	int multiSampleCount,
	int width,
	int height) :
	mFileName(fileName), 
	mDebugName(debugName),
	mUseMipmap(useMipmap),
	mWidth(width), 
	mHeight(height), 
	mMipLevelCount(1), 
	mMultiSampleCount(multiSampleCount), 
	mRenderer(nullptr), 
	mImageData(nullptr), 
	mColorBuffer(nullptr),
	mSampler(sampler),
	mFormat(format)
{
	mSrv.mType = View::Type::SRV;
}

Texture::~Texture()
{
	Release();
}

void Texture::Release()
{
	SAFE_RELEASE(mColorBuffer);
}

void Texture::CreateTextureBuffer()
{
	int channelCountOriginal = 0;
	int channelCountRequested = 0;
	int bytePerChannel = 0;
	void* data = nullptr;
	switch (mFormat)
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
	textureDesc.Format = Renderer::TranslateFormat(mFormat);
	textureDesc.SampleDesc.Count = mMultiSampleCount;
	textureDesc.SampleDesc.Quality = 0;//TODO: investigate how this affect the visual quality
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; //The arrangement of the pixels. Setting to unknown lets the driver choose the most efficient one
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;//TODO: add uav, cross queue and mgpu support

	// create a default heap where the upload heap will copy its contents into (contents being the texture)
	mRenderer->mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&textureDesc, // the description of our texture
		Renderer::TranslateTextureLayout(TextureLayout::COPY_DEST), // We will copy the texture from the upload heap to here, so we start it out in a copy dest state
		nullptr, // used for render targets and depth/stencil buffers
		IID_PPV_ARGS(&mColorBuffer));

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
	mRenderer->UploadTextureDataToBuffer(dataVec, bytePerRowVec, bytePerSliceVec, textureDesc, mColorBuffer);

	mRenderer->Transition(mColorBuffer, TextureLayout::COPY_DEST, TextureLayout::SHADER_READ);

	stbi_image_free(data);
	for(int i = 1;i<mMipLevelCount;i++)
		delete[] dataVec[i];

	mColorBuffer->SetName(mDebugName.c_str());
}

void Texture::CreateView()
{
	// D3D12 binds the view to a descriptor heap as soon as it is created, so this function is not actually creating the API view
	// what we are creating here is the view defined by ourselves
	mSrv.mSrvDesc.Format = Renderer::TranslateFormat(mFormat);
	mSrv.mSrvDesc.ViewDimension = mMultiSampleCount > 1 ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D; // TODO: add support for cube map and texture array
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

ID3D12Resource* Texture::GetColorBuffer()
{
	return mColorBuffer;
}

D3D12_SHADER_RESOURCE_VIEW_DESC Texture::GetSrvDesc()
{
	fatalAssertf(mSrv.mType == View::Type::SRV, "srv type wrong");
	return mSrv.mSrvDesc;
}

D3D12_SAMPLER_DESC Texture::GetSamplerDesc()
{
	return mSampler.mImpl;
}

Format Texture::GetFormat()
{
	return mFormat;
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

int Texture::GetMultiSampleCount()
{
	return mMultiSampleCount;
}

void Texture::InitTexture(Renderer* renderer)
{
	mRenderer = renderer;
	CreateTextureBuffer();
	CreateView();
	CreateSampler();
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
	Format colorFormat,
	int samplePerPixel,
	XMFLOAT4 colorClearValue,
	BlendState blendState) :
	RenderTexture(
		debugName,
		width,
		height,
		readFrom,
		sampler,
		false,
		colorFormat,
		Format::D24_UNORM_S8_UINT,
		samplePerPixel,
		colorClearValue,
		1.f,
		0,
		blendState,
		DepthStencilState::Default())
{
}

RenderTexture::RenderTexture(
	const wstring& debugName,
	int width,
	int height,
	ReadFrom readFrom,
	Sampler sampler,
	Format colorFormat,
	Format depthStencilFormat,
	int samplePerPixel,
	XMFLOAT4 colorClearValue,
	float depthClearValue,
	uint8_t stencilClearValue,
	BlendState blendState,
	DepthStencilState depthStencilState) :
	RenderTexture(
		debugName,
		width,
		height,
		readFrom,
		sampler,
		true,
		colorFormat,
		depthStencilFormat,
		samplePerPixel,
		colorClearValue,
		depthClearValue,
		stencilClearValue,
		blendState,
		depthStencilState)
{
}

RenderTexture::RenderTexture(
	const wstring& debugName,
	int width,
	int height,
	ReadFrom readFrom,
	Sampler sampler,
	bool supportDepthStencil,
	Format colorFormat,
	Format depthStencilFormat,
	int samplePerPixel,
	XMFLOAT4 colorClearValue,
	float depthClearValue,
	uint8_t stencilClearValue,
	BlendState blendState,
	DepthStencilState depthStencilState) :
	mReadFrom(readFrom),
	mSupportDepthStencil(supportDepthStencil),
	mDepthStencilFormat(depthStencilFormat),
	mColorClearValue(colorClearValue),
	mDepthClearValue(depthClearValue),
	mStencilClearValue(stencilClearValue),
	mBlendState(blendState),
	mDepthStencilState(depthStencilState),
	mDepthStencilBuffer(nullptr),
	mColorBufferLayout(TextureLayout::RENDER_TARGET),
	mDepthStencilBufferLayout(TextureLayout::DEPTH_STENCIL_WRITE),
	Texture("no file", debugName,  sampler, false, colorFormat, samplePerPixel, width, height)
{
}

RenderTexture::~RenderTexture()
{
	Release();
}

void RenderTexture::Release()
{
	SAFE_RELEASE(mDepthStencilBuffer);
}

void RenderTexture::CreateTextureBuffer()
{
	//if(!mUseMipmap) // TODO: add support for render texture
		mMipLevelCount = 1;

	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Alignment = 0;
	textureDesc.Width = mWidth;
	textureDesc.Height = mHeight;
	textureDesc.DepthOrArraySize = 1; 
	textureDesc.MipLevels = mMipLevelCount; // TODO: add support for mip map tool chain
	textureDesc.Format = Renderer::TranslateFormat(mFormat);
	textureDesc.SampleDesc.Count = mMultiSampleCount;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; // | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;

	D3D12_CLEAR_VALUE colorOptimizedClearValue = {};
	colorOptimizedClearValue.Format = textureDesc.Format;
	colorOptimizedClearValue.Color[0] = mColorClearValue.x;
	colorOptimizedClearValue.Color[1] = mColorClearValue.y;
	colorOptimizedClearValue.Color[2] = mColorClearValue.z;
	colorOptimizedClearValue.Color[3] = mColorClearValue.w;

	HRESULT hr = mRenderer->mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&colorOptimizedClearValue,
		IID_PPV_ARGS(&mColorBuffer));
	
	assertf(SUCCEEDED(hr), "create render texture color buffer failed");
	mColorBuffer->SetName(mDebugName.c_str());

	////////////////////////////////////////////////////////////////////
	if (mSupportDepthStencil)
	{
		D3D12_RESOURCE_DESC depthStencilBufferDesc = {};
		depthStencilBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthStencilBufferDesc.Alignment = 0;
		depthStencilBufferDesc.Width = mWidth;
		depthStencilBufferDesc.Height = mHeight;
		depthStencilBufferDesc.DepthOrArraySize = 1; // TODO: add support for cube map and texture array
		depthStencilBufferDesc.MipLevels = mMipLevelCount; // TODO: add support for mip map tool chain
		depthStencilBufferDesc.Format = Renderer::TranslateFormat(mDepthStencilFormat);
		depthStencilBufferDesc.SampleDesc.Count = mMultiSampleCount; // TODO: add support for multi sampling
		depthStencilBufferDesc.SampleDesc.Quality = 0;
		depthStencilBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthStencilBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
		depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		depthOptimizedClearValue.DepthStencil.Depth = mDepthClearValue;
		depthOptimizedClearValue.DepthStencil.Stencil = mStencilClearValue;

		hr = mRenderer->mDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&depthStencilBufferDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(&mDepthStencilBuffer)
		);

		assertf(SUCCEEDED(hr), "create render texture depth stencil buffer failed");
		mDepthStencilBuffer->SetName(mDebugName.c_str());
	}
}

void RenderTexture::CreateView()
{
	// D3D12 binds the view to a descriptor heap as soon as it is created, so this function is not actually creating the API view
	// what we are creating here is the view defined by ourselves
	mSrv.mType = View::Type::SRV;
	mSrv.mSrvDesc.Format = mReadFrom == ReadFrom::COLOR ? Renderer::TranslateFormat(mFormat) : Renderer::TranslateFormat(mDepthStencilFormat);
	mSrv.mSrvDesc.ViewDimension = mMultiSampleCount > 1 ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D;
	mSrv.mSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	if(mMultiSampleCount > 1)
	{
		// do nothing
		// mSrv.mSrvDesc.Texture2DMS.UnusedField_NothingToDefine = 0;
	}
	else
	{
		mSrv.mSrvDesc.Texture2D.MostDetailedMip = 0;
		mSrv.mSrvDesc.Texture2D.MipLevels = mMipLevelCount;
		mSrv.mSrvDesc.Texture2D.PlaneSlice = 0;
		mSrv.mSrvDesc.Texture2D.ResourceMinLODClamp = 0;
	}
	
	//Render Target View Desc
	mRtv.mType = View::Type::RTV;
	mRtv.mRtvDesc.Format = Renderer::TranslateFormat(mFormat);
	mRtv.mRtvDesc.ViewDimension = mMultiSampleCount > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;
	mRtv.mRtvDesc.Texture2D.MipSlice = 0;
	mRtv.mRtvDesc.Texture2D.PlaneSlice = 0;
	
	mBlendState.mImpl = Renderer::TranslateBlendState(mBlendState);

	//Stencil Depth View Desc
	//if you want to bind null dsv desciptor, you need to create the dsv
	//if you want to create the dsv, you need an initialized dsvDesc to avoid the debug layer throwing out error
	mDsv.mType = View::Type::DSV;
	mDsv.mDsvDesc.Format = Renderer::TranslateFormat(mDepthStencilFormat);;
	mDsv.mDsvDesc.ViewDimension = mMultiSampleCount > 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
	mDsv.mDsvDesc.Flags = D3D12_DSV_FLAG_NONE; // TODO: this flag contains samilar feature as Vulkan layouts like VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
	mDsv.mDsvDesc.Texture2D.MipSlice = 0;

	mDepthStencilState.mImpl = Renderer::TranslateDepthStencilState(mDepthStencilState);
}

bool RenderTexture::IsDepthStencilSupported()
{
	return mSupportDepthStencil;
}

ID3D12Resource* RenderTexture::GetDepthStencilBuffer()
{
	// make sure to create null descriptor for render textures which does not support depth
	return mSupportDepthStencil ? mDepthStencilBuffer : nullptr;
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

D3D12_RENDER_TARGET_BLEND_DESC RenderTexture::GetRenderTargetBlendDesc()
{
	return mBlendState.mImpl;
}

D3D12_DEPTH_STENCIL_DESC RenderTexture::GetDepthStencilDesc()
{
	return mDepthStencilState.mImpl;
}

Format RenderTexture::GetDepthStencilFormat()
{
	return mDepthStencilFormat;
}

BlendState RenderTexture::GetBlendState()
{
	return mBlendState;
}

DepthStencilState RenderTexture::GetDepthStencilState()
{
	return mDepthStencilState;
}

bool RenderTexture::TransitionColorBufferLayout(ID3D12GraphicsCommandList* commandList, TextureLayout newLayout)
{
	return mRenderer->RecordTransition(commandList, mColorBuffer, mColorBufferLayout, newLayout);
}

bool RenderTexture::TransitionDepthStencilBufferLayout(ID3D12GraphicsCommandList* commandList, TextureLayout newLayout)
{
	return mRenderer->RecordTransition(commandList, mDepthStencilBuffer, mDepthStencilBufferLayout, newLayout);
}
