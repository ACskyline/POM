#pragma once

#include "GlobalInclude.h"
#include "Shader.h"
#include "Camera.h"

class Scene;
class Mesh;
class Texture;
class RenderTexture;
class Buffer;
class WriteBuffer;

struct ShaderTarget 
{
	RenderTexture* mRenderTexture;
	BlendState mBlendState;
	DepthStencilState mDepthStencilState;
	u32 mDepthSlice;
	u32 mMipSlice;
};

struct WriteTarget
{
	enum Type { INVALID, TEXTURE, BUFFER, COUNT };
	Type mType;
	u32 mMipSlice;
	union {
		RenderTexture* mRenderTexture;
		WriteBuffer* mWriteBuffer;
	};
};

// we will pass these uniforms directly to be consumed by GPU so no virtual pointer is allowed, 
// which means no virtual function in this class and all its child classes
struct PassUniformBase
{
protected:
	PassUniformBase() {}; // can't instantiate this
};

struct PassUniformDefault : PassUniformBase
{
	XMFLOAT4X4 mViewProj;
	XMFLOAT4X4 mViewProjInv;
	XMFLOAT4X4 mView;
	XMFLOAT4X4 mViewInv;
	XMFLOAT4X4 mProj;
	XMFLOAT4X4 mProjInv;
	u32 mPassIndex;
	XMFLOAT3 mEyePos;
	float mNearClipPlane;
	float mFarClipPlane;
	float mWidth;
	float mHeight;
	float mFov;
	u32 PADDING_0;
	u32 PADDING_1;
	u32 PADDING_2;
	void Update(Camera* camera);
};

class Pass
{
public:
	virtual ~Pass();

	void SetScene(Scene* scene);
	void SetCamera(Camera* camera);
	void AddMesh(Mesh* mesh);
	void AddRenderTexture(RenderTexture* renderTexture, u32 depthSlice, u32 mipSlice, const BlendState& blendState, const DepthStencilState& depthStencilState);
	void AddRenderTexture(RenderTexture* renderTexture, u32 depthSlice, u32 mipSlice, const BlendState& blendState);
	void AddRenderTexture(RenderTexture* renderTexture, u32 depthSlice, u32 mipSlice, const DepthStencilState& depthStencilState);
	void AddShader(Shader* shader);
	void AddTexture(Texture* texture);
	void AddBuffer(Buffer* buffer);
	void AddWriteBuffer(WriteBuffer* writeBuffer);
	void AddWriteTexture(RenderTexture* writeTexture, u32 mipSlice);

	int GetCbvSrvUavCount();
	int GetShaderResourceCount();
	int GetShaderTargetCount();
	int GetWriteTargetCount();
	vector<ShaderResource*>& GetShaderResources();
	vector<ShaderTarget>& GetShaderTargets();
	RenderTexture* GetRenderTexture(int i);
	int GetRenderTargetCount();
	int GetDepthStencilCount();
	int GetDepthStencilIndex();
	bool UseRenderTarget();
	bool UseDepthStencil();
	bool ShareMeshesWithPathTracer();
	Camera* GetCamera();
	Scene* GetScene();
	vector<Mesh*>& GetMeshVec();
	ID3D12PipelineState* GetPso();
	ID3D12RootSignature* GetRootSignature();
	D3D12_GPU_VIRTUAL_ADDRESS GetUniformBufferGpuAddress(int frameIndex);
	vector<CD3DX12_CPU_DESCRIPTOR_HANDLE>& GetRtvHandles(int frameIndex);
	vector<CD3DX12_CPU_DESCRIPTOR_HANDLE>& GetDsvHandles(int frameIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvDescriptorHeapTableHandle(int frameIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE GetUavDescriptorHeapTableHandle(int frameIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerDescriptorHeapTableHandle(int frameIndex);
	bool IsConstantBlendFactorsUsed();
	bool IsStencilReferenceUsed();
	float* GetConstantBlendFactors();
	uint32_t GetStencilReference();
	const wstring& GetDebugName();

	virtual void InitPass(
		Renderer* renderer,
		int frameCount,
		DescriptorHeap& cbvSrvUavDescriptorHeap,
		DescriptorHeap& samplerDescriptorHeap,
		DescriptorHeap& rtvDescriptorHeap,
		DescriptorHeap& dsvDescriptorHeap);

	// abstract class
	virtual void CreateUniformBuffer(int frameCount) = 0;
	virtual void UpdateUniformBuffer(int frameIndex) = 0;
	virtual void UpdateUniformBuffer(int frameIndex, void* src) = 0;
	void Release(bool checkOnly = false);

protected:
	// one for each frame
	// TODO: hide API specific implementation in Renderer
	Renderer* mRenderer;
	wstring mDebugName;
	Camera* mCamera;
	vector<ID3D12Resource*> mUniformBuffers;

	// default constructor for using in a container
	Pass();
	// common class can't be instantiated
	Pass(const wstring& debugName, bool useRenderTarget, bool useDepthStencil, bool shareMeshesWithPathTracer);

private:
	Scene* mScene;
	vector<Mesh*> mMeshes;
	vector<ShaderResource*> mShaderResources;
	vector<WriteTarget> mWriteTargets;
	vector<ShaderTarget> mShaderTargets;
	Shader* mShaders[Shader::ShaderType::COUNT];
	int mRenderTargetCount;
	int mDepthStencilCount;
	int mDepthStencilIndex;
	bool mUseRenderTarget; // if use render target but has 0 render target, then output to back buffer
	bool mUseDepthStencil; // if use depth stencil but has 0 depth stencil, then output to back buffer
	bool mShareMeshesWithPathTracer;

	// one for each frame
	// TODO: hide API specific implementation in Renderer
	vector<vector<CD3DX12_CPU_DESCRIPTOR_HANDLE>> mRtvHandles; // [frameIndex][renderTextureIndex]
	vector<vector<CD3DX12_CPU_DESCRIPTOR_HANDLE>> mDsvHandles; // [frameIndex][renderTextureIndex]
	vector<D3D12_GPU_DESCRIPTOR_HANDLE> mSrvDescriptorHeapTableHandles; // constant buffer is stored as root parameter, so this is really only for srv and uav
	vector<D3D12_GPU_DESCRIPTOR_HANDLE> mUavDescriptorHeapTableHandles;
	vector<D3D12_GPU_DESCRIPTOR_HANDLE> mSamplerDescriptorHeapTableHandles;
	ID3D12PipelineState* mPso;
	ID3D12RootSignature* mRootSignature;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE mPrimitiveType;

	// these values are set using OMSet... functions in D3D12, so we cache them here
	bool mConstantBlendFactorsUsed;
	bool mStencilReferenceUsed;

	float mBlendConstants[4];
	uint32_t mStencilReference;

	void AddShaderResource(ShaderResource* sr);
	int GetMaxMeshTextureCount();
};

template<class UniformType>
class PassCommon : public Pass
{
public:
	// default constructor for using in a container
	PassCommon() :
		Pass(L"unnamed pass", false, false, false)
	{

	}

	PassCommon(const wstring& debugName, bool useRenderTarget = true, bool useDepthStencil = true, bool shareMeshesWithPathTracer = false) :
		Pass(debugName, useRenderTarget, useDepthStencil, shareMeshesWithPathTracer)
	{

	}

	virtual ~PassCommon() override
	{

	}

	virtual void UpdateUniformBuffer(int frameIndex) override
	{
		CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU. (so end is less than or equal to begin)
		void* cpuAddress;
		mUniformBuffers[frameIndex]->Map(0, &readRange, &cpuAddress);
		memcpy(cpuAddress, &mPassUniform, sizeof(UniformType));
		mUniformBuffers[frameIndex]->Unmap(0, &readRange);
	}

	// caller must guarantee src is of the same size as the dest
	virtual void UpdateUniformBuffer(int frameIndex, void* src) override
	{
		fatalAssertf(src, "source is nullptr");
		memcpy(&mPassUniform, src, sizeof(UniformType));
		UpdateUniformBuffer(frameIndex);
	}

	virtual void CreateUniformBuffer(int frameCount) override
	{
		UpdateUniform();
		mUniformBuffers.resize(frameCount);
		for (int i = 0; i < frameCount; i++)
		{
			HRESULT hr = mRenderer->mDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // this heap will be used to upload the constant buffer data
				D3D12_HEAP_FLAG_NONE, // no flags
				&CD3DX12_RESOURCE_DESC::Buffer(sizeof(UniformType)),
				Renderer::TranslateResourceLayout(ResourceLayout::UPLOAD), // will be data that is read from so we keep it in the generic read state
				nullptr, // we do not use an optimized clear value for constant buffers
				IID_PPV_ARGS(&mUniformBuffers[i]));

			fatalAssertf(!CheckError(hr, mRenderer->mDevice), "create pass uniform buffer failed");
			wstring name(L": pass uniform buffer ");
			mUniformBuffers[i]->SetName((mDebugName + name + to_wstring(i)).c_str());
			UpdateUniformBuffer(i);
		}
	}

	virtual void UpdateUniform()
	{
		if (mCamera)
		{
			mPassUniform.Update(mCamera);
		}
	}

	UniformType mPassUniform;
};

typedef PassCommon<PassUniformDefault> PassDefault;
