#pragma once

#include "GlobalInclude.h"

class Renderer;

enum TextureLayout {
	INVALID,
	SHADER_READ,
	RENDER_TARGET,
	// D3D12 doesn't seem to support different access qualifiers for depth and stencil channel 
	// unlike Vulkan's VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL and etc.
	DEPTH_STENCIL_READ, 
	DEPTH_STENCIL_WRITE,
	COPY_SRC,
	COPY_DEST,
	RESOLVE_SRC,
	RESOLVE_DEST,
	PRESENT,
	COUNT
};

struct BlendState {
	enum BlendFactor {
		INVALID,
		ZERO, 
		ONE, 
		SRC_COLOR, 
		INV_SRC_COLOR, 
		SRC_ALPHA, 
		INV_SRC_ALPHA, 
		DEST_ALPHA, 
		INV_DEST_ALPHA, 
		DEST_COLOR, 
		INV_DEST_COLOR, 
		SRC_ALPHA_SAT, 
		CONSTANT, // D3D12 set this constant using OMSetBlendFactor, Vulkan put it in VkPipelineColorBlendStateCreateInfo 
		INV_CONSTANT, // same as above
		SRC1_COLOR,
		INV_SRC1_COLOR,
		SRC1_ALPHA,
		INV_SRC1_ALPHA,
		COUNT
		};
	enum BlendOp { INVALID, ADD, SUBTRACT, REV_SUBTRACT, MIN, MAX, COUNT	};
	enum LogicOp { INVALID, CLEAR, SET, COPY, COPY_INVERTED, NOOP, INVERT, AND, NAND, OR, NOR, XOR, EQUIV, AND_REVERSE, AND_INVERTED, OR_REVERSE, OR_INVERTED, COUNT };
	enum WriteMask : uint8_t { INVALID, RED = 0x01, GREEN = 0x02, BLUE = 0x04, ALPHA = 0x08 };

	bool mEnableBlend;
	bool mEnableLogicOp;
	BlendFactor mSrcBlend;
	BlendFactor mDestBlend;
	BlendOp mBlendOp;
	BlendFactor mSrcBlendAlpha;
	BlendFactor mDestBlendAlpha;
	BlendOp mBlendOpAlpha;
	LogicOp mLogicOp;
	WriteMask mWriteMask;
	float mBlendConstants[4];

	D3D12_RENDER_TARGET_BLEND_DESC mRenderTargetBlendDesc; // TODO: hide API specific implementation in Renderer
};

enum CompareOp { INVALID, NEVER, LESS, EQUAL, LESS_EQUAL, GREATER, NOT_EQUAL, GREATER_EQUAL, ALWAYS, MINIMUM, MAXIMUM, COUNT };

struct DepthStencilState {
	enum StencilOp { INVALID, KEEP, ZERO, REPLACE, INCR_SAT, DECR_SAT, INVERT, INCR, DECR, COUNT };
	struct StencilOpSet {
		StencilOp mFailOp;
		StencilOp mPassOp;
		StencilOp mDepthFailOp;
		CompareOp mStencilCompareOp;
	};
	bool mDepthTestEnable;
	bool mDepthWriteEnable;
	bool mStencilTestEnable; // stencil write depends on the stencil op
	bool mDepthBoundsTestEnable; // not sure what this is used for
	CompareOp mDepthCompareOp;
	uint8_t mStencilReadMask;
	uint8_t mStencilWriteMask;
	StencilOpSet mFront;
	StencilOpSet mBack;
	uint32_t mReference; // D3D12 set this reference using OMSetStencilRef, Vulkan put it in VkStencilOpState  
 
	D3D12_DEPTH_STENCIL_DESC mDepthStencilDesc; // TODO: hide API specific implementation in Renderer
};

struct View {
	enum Type { INVALID, SRV, RTV, DSV, COUNT };
	Type mType;

	union {
		D3D12_SHADER_RESOURCE_VIEW_DESC mSrvDesc; // TODO: hide API specific implementation in Renderer
		D3D12_RENDER_TARGET_VIEW_DESC mRtvDesc; // TODO: hide API specific implementation in Renderer
		D3D12_DEPTH_STENCIL_VIEW_DESC mDsvDesc; // TODO: hide API specific implementation in Renderer
	};
};

struct Sampler {
	enum Filter { INVALID, POINT, LINEAR, COUNT };
	enum AddressMode { INVALID, WARP, MIRROR, CLAMP, BORDER, MIRROR_ONCE, COUNT };

	Filter mMinFilter;
	Filter mMaxFilter;
	AddressMode mAddressModeU;
	AddressMode mAddressModeV;
	AddressMode mAddressModeW;
	float mMipLodBias;
	bool mUseAnisotropy;
	float mMaxAnisotropy;
	bool mUseCompare;
	CompareOp mCompareOp;
	float mMinLod;
	float mMaxLod;
	float mBorderColor[4];
	bool mUseMipmap;

	D3D12_SAMPLER_DESC mSamplerDesc; // TODO: hide API specific implementation in Renderer
};

enum class Format {
	INVALID,
	R8G8B8A8_UNORM,
	R16G16B16A16_UNORM,
	R16G16B16A16_FLOAT,
	D16_UNORM,
	D32_FLOAT,
	D24_UNORM_S8_UINT,
	COUNT
};

class Texture
{
public:
	Texture(
		const string& fileName,
		const wstring& debugName,
		Sampler sampler,
		int width = -1,
		int height = -1,
		Format format = Format::INVALID);
	virtual ~Texture();

	ID3D12Resource* GetTextureBuffer();
	D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc();
	D3D12_SAMPLER_DESC GetSamplerDesc();
	Format GetFormat();
	View GetSrv();
	Sampler GetSampler();
	string GetName();
	wstring GetDebugName();

	virtual void InitTexture(Renderer* renderer);
	virtual void CreateTextureBuffer();
	virtual void CreateView();
	virtual void CreateSampler();
	void Release();

protected:
	string mFileName;
	wstring mDebugName;
	int mWidth;
	int mHeight;
	int mMipLevelCount;
	int mSamplePerPixel;
	Format mFormat;
	View mSrv;
	Sampler mSampler; // TODO: separate sampler from texture so that we don't create duplicate textures when we just want to use a different sampler

	Renderer* mRenderer;
	BYTE* mImageData;
	ID3D12Resource* mTextureBuffer; // TODO: hide API specific implementation in Renderer
};

class RenderTexture : public Texture
{
public:
	enum ReadFrom { COLOR, DEPTH, STENCIL, COUNT };

	RenderTexture(
		const string& fileName,
		const wstring& debugName,
		int width,
		int height,
		ReadFrom readFrom,
		Sampler sampler,
		Format format,
		Format depthStencilFormat,
		BlendState blendState,
		DepthStencilState depthStencilState,
		bool supportDepthStencil = false);
	virtual ~RenderTexture();
	
	bool IsDepthStencilSupported();
	ID3D12Resource* GetDepthStencilBuffer();
	D3D12_RENDER_TARGET_VIEW_DESC GetRtvDesc();
	D3D12_DEPTH_STENCIL_VIEW_DESC GetDsvDesc();
	D3D12_RENDER_TARGET_BLEND_DESC GetRenderTargetBlendDesc();
	D3D12_DEPTH_STENCIL_DESC GetDepthStencilDesc();
	Format GetDepthStencilFormat();

	// TODO: hide API specific implementation in Renderer
	CD3DX12_RESOURCE_BARRIER TransitionLayout(TextureLayout newLayout);

	virtual void CreateTextureBuffer();
	virtual void CreateView();
	void Release();

private:
	ReadFrom mReadFrom;
	bool mSupportDepthStencil;
	ID3D12Resource* mDepthStencilBuffer; // TODO: hide API specific implementation in Renderer

	View mRtv;
	View mDsv;
	TextureLayout mLayout;
	Format mDepthStencilFormat;

	BlendState mBlendState;
	DepthStencilState mDepthStencilState;
};
