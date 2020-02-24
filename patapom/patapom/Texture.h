#pragma once

#include "GlobalInclude.h"

class Texture
{
public:
	Texture(
	const string& fileName,
	const wstring& debugName,
	Sampler sampler,
	bool useMipmap,
	Format format = Format::R8G8B8A8_UNORM);
	virtual ~Texture();

	ID3D12Resource* GetColorBuffer();
	D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc();
	D3D12_SAMPLER_DESC GetSamplerDesc();
	Format GetFormat();
	View GetSrv();
	Sampler GetSampler();
	string GetName();
	wstring GetDebugName();
	int GetMultiSampleCount();

	virtual void InitTexture(Renderer* renderer);
	virtual void CreateTextureBuffer();
	virtual void CreateView();
	virtual void CreateSampler();
	void Release();

protected:
	Texture(
		const string& fileName,
		const wstring& debugName,
		Sampler sampler,
		bool useMipmap,
		Format format,
		int multiSampleCount,
		int width,
		int height);

	string mFileName;
	wstring mDebugName;
	bool mUseMipmap;
	int mWidth;
	int mHeight;
	int mMipLevelCount;
	int mMultiSampleCount;
	Format mFormat;
	View mSrv;
	Sampler mSampler; // TODO: separate sampler from texture so that we don't create duplicate textures when we just want to use a different sampler

	Renderer* mRenderer;
	BYTE* mImageData;
	ID3D12Resource* mColorBuffer; // TODO: hide API specific implementation in Renderer
};

class RenderTexture : public Texture
{
public:
	enum class ReadFrom { COLOR, DEPTH, STENCIL, COUNT };

	// only enable color buffer
	RenderTexture(
		const wstring& debugName,
		int width,
		int height,
		ReadFrom readFrom,
		Sampler sampler,
		Format colorFormat,
		int multiSampleCount = 1,
		XMFLOAT4 colorClearValue = XMFLOAT4(0.f, 0.f, 0.f, 0.f),
		BlendState blendState = BlendState::Default());

	// enable both color and depthStencil buffer
	RenderTexture(
		const wstring& debugName,
		int width,
		int height,
		ReadFrom readFrom,
		Sampler sampler,
		Format colorFormat,
		Format depthStencilFormat,
		int multiSampleCount = 1,
		XMFLOAT4 colorClearValue = XMFLOAT4(0.f, 0.f, 0.f, 0.f),
		float depthClearValue = 1.f,
		uint8_t stencilClearValue = 0,
		BlendState blendState = BlendState::Default(),
		DepthStencilState depthStencilState = DepthStencilState::Default());

	virtual ~RenderTexture();
	
	bool IsDepthStencilSupported();
	ID3D12Resource* GetDepthStencilBuffer();
	D3D12_RENDER_TARGET_VIEW_DESC GetRtvDesc();
	D3D12_DEPTH_STENCIL_VIEW_DESC GetDsvDesc();
	D3D12_RENDER_TARGET_BLEND_DESC GetRenderTargetBlendDesc();
	D3D12_DEPTH_STENCIL_DESC GetDepthStencilDesc();
	Format GetDepthStencilFormat();
	BlendState GetBlendState();
	DepthStencilState GetDepthStencilState();

	// TODO: hide API specific implementation in Renderer
	bool TransitionColorBufferLayout(ID3D12GraphicsCommandList* commandList, TextureLayout newLayout);
	bool TransitionDepthStencilBufferLayout(ID3D12GraphicsCommandList* commandList, TextureLayout newLayout);

	virtual void CreateTextureBuffer();
	virtual void CreateView();
	void Release();

	XMFLOAT4 mColorClearValue;
	float mDepthClearValue;
	uint8_t mStencilClearValue;

protected:
	RenderTexture(
		const wstring& debugName,
		int width,
		int height,
		ReadFrom readFrom,
		Sampler sampler,
		bool supportDepthStencil,
		Format colorFormat,
		Format depthStencilFormat,
		int multiSampleCount,
		XMFLOAT4 colorClearValue,
		float depthClearValue,
		uint8_t stencilClearValue,
		BlendState blendState,
		DepthStencilState depthStencilState);

private:
	ReadFrom mReadFrom;
	bool mSupportDepthStencil;
	ID3D12Resource* mDepthStencilBuffer; // TODO: hide API specific implementation in Renderer

	View mRtv;
	View mDsv;
	TextureLayout mColorBufferLayout;
	TextureLayout mDepthStencilBufferLayout;
	Format mDepthStencilFormat;

	BlendState mBlendState;
	DepthStencilState mDepthStencilState;
};
