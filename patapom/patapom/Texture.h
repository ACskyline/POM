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

	ID3D12Resource* GetTextureBuffer();
	D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc();
	D3D12_SAMPLER_DESC GetSamplerDesc();
	Format GetTextureFormat();
	View GetSrv();
	Sampler GetSampler();
	string GetName();
	wstring GetDebugName();

	virtual void InitTexture(Renderer* renderer);
	virtual void CreateTextureBuffer();
	virtual void CreateView();
	virtual void CreateSampler();
	void Release();

	bool TransitionTextureLayoutSingleTime(ResourceLayout newLayout);
	bool TransitionTextureLayout(ID3D12GraphicsCommandList* commandList, ResourceLayout newLayout);

protected:
	Texture(
		const string& fileName,
		const wstring& debugName,
		Sampler sampler,
		bool useMipmap,
		Format format,
		int width,
		int height);

	string mFileName;
	wstring mDebugName;
	bool mUseMipmap;
	int mWidth;
	int mHeight;
	int mMipLevelCount;
	Format mTextureFormat;
	View mSrv;
	Sampler mSampler; // TODO: separate sampler from texture so that we don't create duplicate textures when we just want to use a different sampler

	Renderer* mRenderer;
	BYTE* mImageData;
	ID3D12Resource* mTextureBuffer; // TODO: hide API specific implementation in Renderer
	ResourceLayout mTextureBufferLayout;
};

class RenderTexture : public Texture
{
public:
	// only enable color buffer
	RenderTexture(
		const wstring& debugName,
		int width,
		int height,
		ReadFrom readFrom,
		Sampler sampler,
		Format renderTargetFormat,
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
		Format renderTargetFormat,
		Format depthStencilFormat,
		int multiSampleCount = 1,
		XMFLOAT4 colorClearValue = XMFLOAT4(0.f, 0.f, 0.f, 0.f),
		float depthClearValue = 1.f,
		uint8_t stencilClearValue = 0,
		BlendState blendState = BlendState::Default(),
		DepthStencilState depthStencilState = DepthStencilState::Default());

	virtual ~RenderTexture();

	bool ReadFromMultiSampledBuffer();
	bool ReadFromStencil();
	bool ReadFromDepth();
	bool ReadFromDepthStencilBuffer();
	bool ReadFromColor();
	bool IsMultiSampleUsed();
	bool IsResolveNeeded();
	bool IsDepthStencilUsed();
	ID3D12Resource* GetRenderTargetBuffer();
	ID3D12Resource* GetDepthStencilBuffer();
	D3D12_RENDER_TARGET_VIEW_DESC GetRtvDesc();
	D3D12_DEPTH_STENCIL_VIEW_DESC GetDsvDesc();
	D3D12_RENDER_TARGET_BLEND_DESC GetRenderTargetBlendDesc();
	D3D12_DEPTH_STENCIL_DESC GetDepthStencilDesc();
	Format GetRenderTargetFormat();
	Format GetDepthStencilFormat();
	BlendState GetBlendState();
	DepthStencilState GetDepthStencilState();
	int GetMultiSampleCount();

	void MakeReadyToWrite(ID3D12GraphicsCommandList* commandList);
	void MakeReadyToRead(ID3D12GraphicsCommandList* commandList);

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
		bool useDepthStencil,
		Format renderTargetFormat,
		Format depthStencilFormat,
		int multiSampleCount,
		XMFLOAT4 colorClearValue,
		float depthClearValue,
		uint8_t stencilClearValue,
		BlendState blendState,
		DepthStencilState depthStencilState);
	
	// TODO: hide API specific implementation in Renderer
	bool TransitionDepthStencilLayout(ID3D12GraphicsCommandList* commandList, ResourceLayout newLayout);
	bool TransitionRenderTargetLayout(ID3D12GraphicsCommandList* commandList, ResourceLayout newLayout);
	void ResolveRenderTargetToTexture(ID3D12GraphicsCommandList* commandList);
	void ResolveDepthStencilToTexture(ID3D12GraphicsCommandList* commandList);

private:
	ReadFrom mReadFrom;
	bool mUseDepthStencil;
	int mMultiSampleCount;
	ID3D12Resource* mRenderTargetBuffer; // TODO: hide API specific implementation in Renderer
	ID3D12Resource* mDepthStencilBuffer;

	View mRtv;
	View mDsv;
	ResourceLayout mRenderTargetBufferLayout;
	ResourceLayout mDepthStencilBufferLayout;
	Format mDepthStencilFormat;
	Format mRenderTargetFormat;

	BlendState mBlendState;
	DepthStencilState mDepthStencilState;
};
