#pragma once

#include "GlobalInclude.h"
#include "Renderer.h"

class Texture : public ShaderResource
{
public:
	enum { ALL_SLICES = 0xffffffff };
	enum CubeFace { X_POS, X_NEG, Y_POS, Y_NEG, Z_POS, Z_NEG, COUNT };

	Texture(
		const string& fileName,
		const string& debugName,
		bool useMipmap,
		Format format = Format::R8G8B8A8_UNORM);
	virtual ~Texture();

	ID3D12Resource* GetTextureBuffer();
	D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc() const;
	Format GetTextureFormat();
	string GetName();
	u32 GetWidth(u32 mipLevel = 0);
	u32 GetHeight(u32 mipLevel = 0);
	u32 GetDepth(u32 mipLevel = 0);
	u32 GetMipLevelCount();

	virtual void InitTexture(Renderer* renderer);
	virtual void Release(bool checkOnly = false);

	bool TransitionTextureLayoutSingleTime(ResourceLayout newLayout, u32 depthSlice, u32 mipSlice);
	bool TransitionTextureLayout(CommandList commandList, ResourceLayout newLayout, u32 depthSlice, u32 mipSlice);
	
protected:
	virtual void CreateTextureBuffer();
	virtual void CreateView();

	Texture(
		TextureType type,
		const string& fileName,
		const string& debugName,
		bool useMipmap,
		Format format,
		u32 width,
		u32 height,
		u32 depth);

	TextureType mTextureType;
	string mFileName;
	bool mUseMipmap;
	u32 mWidth;
	u32 mHeight;
	u32 mDepth;
	u32 mMipLevelCount;
	Format mTextureFormat;
	View mSrv;

	Renderer* mRenderer;
	BYTE* mImageData;
	Resource* mTextureBuffer; // TODO: hide API specific implementation in Renderer
	vector<vector<ResourceLayout>> mTextureLayouts;

	void ResetLayouts(vector<vector<ResourceLayout>>& layouts, ResourceLayout newLayout, u32 newDepth, u32 newMipLevelCount);
	void SetLayout(vector<vector<ResourceLayout>>& layouts, ResourceLayout newLayout, u32 depthSlice, u32 mipSlice);
	bool TransitionLayoutInternal(CommandList* commandList, Resource* resource, vector<vector<ResourceLayout>>& layouts, ResourceLayout newLayout, u32 depthSlice, u32 mipSlice);
};

// RenderTexture only supports reading from a one buffer, either mRenderTargetBuffer or mDepthStencilBuffer.
// Because it is easier to manage texture slots and handle srv this way. If you want to read from both buffer, try to bind 2 render textures.
// TODO: add support to read from both color and depth stencil buffers (not necessarily read both in the same pass, but at least in different passes)
class RenderTexture : public Texture
{
public:
	// only enable color buffer
	RenderTexture(
		TextureType type,
		const string& debugName,
		u32 width,
		u32 height,
		u32 depth,
		u32 mipLevelCount,
		ReadFrom readFrom,
		Format renderTargetFormat,
		XMFLOAT4 colorClearValue,
		u8 multiSampleCount = 1);
	
	// only enable depthStencil buffer
	RenderTexture(
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
		u8 multiSampleCount = 1);

	// enable both color and depthStencil buffer
	RenderTexture(
		TextureType type,
		const string& debugName,
		u32 width,
		u32 height,
		u32 depth,
		u32 mipLevelCount,
		ReadFrom readFrom,
		Format renderTargetFormat,
		Format depthStencilFormat,
		XMFLOAT4 colorClearValue = XMFLOAT4(0.f, 0.f, 0.f, 0.f),
		float depthClearValue = DEPTH_FAR_REVERSED_Z_SWITCH,
		u8 stencilClearValue = 0,
		u8 multiSampleCount = 1);

	virtual ~RenderTexture();

	bool IsRenderTargetUsed();
	bool IsDepthStencilUsed();
	Resource* GetRenderTargetBuffer();
	Resource* GetDepthStencilBuffer();
	D3D12_RENDER_TARGET_VIEW_DESC GetRtvDesc(u32 depthSlice, u32 mipSlice);
	D3D12_DEPTH_STENCIL_VIEW_DESC GetDsvDesc(u32 depthSlice, u32 mipSlice);
	D3D12_UNORDERED_ACCESS_VIEW_DESC GetUavDesc(u32 mipSlice);
	Format GetRenderTargetFormat();
	Format GetDepthStencilFormat();
	int GetMultiSampleCount();

	void MakeReadyToRender(CommandList commandList, u32 depthSlice = Texture::ALL_SLICES, u32 mipSlice = Texture::ALL_SLICES);
	void MakeReadyToRead(CommandList commandList, u32 depthSlice = Texture::ALL_SLICES, u32 mipSlice = Texture::ALL_SLICES);
	void MakeReadyToWrite(CommandList commandList, u32 depthSlice = Texture::ALL_SLICES, u32 mipSlice = Texture::ALL_SLICES);
	void MakeReadyToWriteAgain(CommandList commandList, u32 depthSlice = Texture::ALL_SLICES, u32 mipSlice = Texture::ALL_SLICES);
	virtual void Release(bool checkOnly = false);

	XMFLOAT4 mColorClearValue;
	float mDepthClearValue;
	u8 mStencilClearValue;

protected:
	virtual void CreateTextureBuffer();
	virtual void CreateView();

	RenderTexture(
		TextureType type,
		const string& debugName,
		u32 width,
		u32 height,
		u32 depth,
		u32 mipmapCount,
		ReadFrom readFrom,
		bool useRenderTarget,
		bool useDepthStencil,
		Format renderTargetFormat,
		Format depthStencilFormat,
		XMFLOAT4 colorClearValue,
		float depthClearValue,
		u8 stencilClearValue,
		u8 multiSampleCount);
	
	// TODO: hide API specific implementation in Renderer
	bool TransitionDepthStencilLayout(CommandList commandList, ResourceLayout newLayout, u32 depthSlice, u32 mipSlice);
	bool TransitionRenderTargetLayout(CommandList commandList, ResourceLayout newLayout, u32 depthSlice, u32 mipSlice);
	void ResolveRenderTargetToTexture(CommandList commandList, u32 depthSlice, u32 mipSlice);
	void ResolveDepthStencilToTexture(CommandList commandList, u32 depthSlice, u32 mipSlice);

private:
	ReadFrom mReadFrom;
	bool mUseRenderTarget;
	bool mUseDepthStencil;
	u8 mMultiSampleCount;
	Resource* mRenderTargetBuffer; // TODO: hide API specific implementation in Renderer
	Resource* mDepthStencilBuffer;

	View mRtvMip0;
	View mDsvMip0;
	View mUavMip0;
	vector<vector<ResourceLayout>> mRenderTargetLayouts; // [depthSlice][mipSlice]
	vector<vector<ResourceLayout>> mDepthStencilLayouts; // TODO: make these layouts thread safe
	Format mDepthStencilFormat;
	Format mRenderTargetFormat;

	bool ReadFromMultiSampledBuffer();
	bool ReadFromColor();
	bool ReadFromStencil();
	bool ReadFromDepth();
	bool ReadFromDepthStencil();
	bool IsMultiSampleUsed();
	bool IsResolveNeeded();
	void CreateRtv();
	void CreateDsv();
	void CreateSrv();
	void CreateUav();
	void ResolveToTexture(CommandList commandList, Resource* resource, Format format, u32 depthSlice, u32 mipSlice);
};
