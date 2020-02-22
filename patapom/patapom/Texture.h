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
		Format format = Format::INVALID,
		int width = -1,
		int height = -1);
	virtual ~Texture();

	ID3D12Resource* GetColorBuffer();
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
	bool mUseMipmap;
	int mWidth;
	int mHeight;
	int mMipLevelCount;
	int mSamplePerPixel;
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

	RenderTexture(
		const string& fileName,
		const wstring& debugName,
		int width,
		int height,
		ReadFrom readFrom,
		Sampler sampler,
		Format colorFormat,
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
	BlendState GetBlendState();
	DepthStencilState GetDepthStencilState();

	// TODO: hide API specific implementation in Renderer
	bool TransitionColorBufferLayout(ID3D12GraphicsCommandList* commandList, TextureLayout newLayout);
	bool TransitionDepthStencilBufferLayout(ID3D12GraphicsCommandList* commandList, TextureLayout newLayout);

	virtual void CreateTextureBuffer();
	virtual void CreateView();
	void Release();

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
