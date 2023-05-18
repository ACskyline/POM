#pragma once

#include "GlobalInclude.h"
#include "Shader.h"
#include "Camera.h"
#include <type_traits>

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
	bool IsRenderTargetUsed();
	bool IsDepthStencilUsed();
	u32 GetHeight();
	u32 GetWidth();
};

struct WriteTarget
{
	enum class Type { INVALID, TEXTURE, BUFFER, COUNT };
	Type mType;
	u32 mMipSlice;
	union {
		RenderTexture* mRenderTexture;
		WriteBuffer* mWriteBuffer;
	};
};

class Pass
{
public:
	virtual ~Pass();

	void SetDirty(ResourceDirtyFlag dirtyFlag);
	void SetScene(Scene* scene);
	void SetCamera(Camera* camera);
	void AddMesh(Mesh* mesh);
	void AddRenderTexture(RenderTexture* renderTexture, u32 depthSlice, u32 mipSlice, const BlendState& blendState, const DepthStencilState& depthStencilState);
	void AddRenderTexture(RenderTexture* renderTexture, u32 depthSlice, u32 mipSlice, const BlendState& blendState);
	void AddRenderTexture(RenderTexture* renderTexture, u32 depthSlice, u32 mipSlice, const DepthStencilState& depthStencilState);
	void AddShader(Shader* shader);
	void AddTexture(Texture* texture);
	void AddBuffer(Buffer* buffer);
	void SetBuffer(u8 slot, Buffer* buffer);
	void AddWriteBuffer(WriteBuffer* writeBuffer);
	void SetWriteBuffer(u8 slot, WriteBuffer* writeBuffer);
	void AddWriteTexture(RenderTexture* writeTexture, u32 mipSlice);
	void FlushDynamicDescriptors(int frameIndex);
	void RefreshStaticDescriptors(int frameIndex);

	int GetCbvSrvUavCount();
	int GetShaderResourceCount();
	int GetShaderTargetCount();
	int GetWriteTargetCount();
	vector<ShaderResource*>& GetShaderResources();
	vector<ShaderTarget>& GetShaderTargets();
	ShaderTarget GetShaderTarget(int i);
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
	vector<DescriptorHeap::Handle>& GetRtvHandles(int frameIndex);
	vector<DescriptorHeap::Handle>& GetDsvHandles(int frameIndex);
	DescriptorHeap::Handle GetSrvDescriptorHeapHandle(int frameIndex);
	DescriptorHeap::Handle GetUavDescriptorHeapHandle(int frameIndex);
	bool IsConstantBlendFactorsUsed(int frameIndex);
	bool IsStencilReferenceUsed(int frameIndex);
	float* GetConstantBlendFactors(int frameIndex);
	uint32_t GetStencilReference(int frameIndex);
	const string& GetDebugName();
	string GetGraphicShaderNames();
	string GetComputeShaderName();
	PrimitiveType GetPrimitiveType();

	virtual void InitPass(
		Renderer* renderer,
		int frameCount,
		DescriptorHeap& cbvSrvUavDescriptorHeap,
		DescriptorHeap& rtvDescriptorHeap,
		DescriptorHeap& dsvDescriptorHeap);

	// abstract class
	virtual void UpdateUniform() = 0;
	virtual void CreateUniformBuffer(int frameCount) = 0;
	virtual void UpdateUniformBuffer(int frameIndex) = 0;
	virtual void UpdateDynamicUniformBuffer() = 0;
	//virtual void UpdateUniformBuffer(int frameIndex, void* src, size_t sizeInByte) = 0;
	void Release(bool checkOnly = false);

	vector<ResourceDirtyFlag> mResourceDirtyFlag;

protected:
	struct ShaderTargetState 
	{
		bool mConstantBlendFactorsUsed;
		bool mStencilReferenceUsed;
		u32 mStencilReference;
		float mBlendConstants[4];
	};

	// one for each frame
	// TODO: hide API specific implementation in Renderer
	Renderer* mRenderer;
	string mDebugName;
	Camera* mCamera;
	vector<ID3D12Resource*> mUniformBuffers;
	DynamicUniformBuffer::Address mDynamicUniformBuffer;
	int mUniformSizeInByte;

	// default constructor for using in a container
	Pass();
	// base class can't be instantiated
	Pass(const string& debugName, bool useRenderTarget, bool useDepthStencil, bool shareMeshesWithPathTracer, PrimitiveType primitiveType, int uniformSizeInByte);
	void CreatePass(const string& debugName, bool useRenderTarget, bool useDepthStencil, bool shareMeshesWithPathTracer, PrimitiveType primitiveType, int uniformSizeInByte);
	void ReallocateDescriptorsForShaderResources(DescriptorHeap::Handle srvDescriptorHeapHandle);
	void AllocateDescriptorsForShaderResources(DescriptorHeap& cbvSrvUavDescriptorHeap, DescriptorHeap::Handle& srvDescriptorHeapHandle);
	void ReallocateDescriptorsForWriteTargets(DescriptorHeap::Handle uavDescriptorHeapHandle);
	void AllocateDescriptorsForWriteTargets(DescriptorHeap& cbvSrvUavDescriptorHeap, DescriptorHeap::Handle& uavDescriptorHeapHandle);
	void ReallocateDescriptorsForShaderTargets(
		ShaderTargetState& shaderTargetState,
		vector<DescriptorHeap::Handle>& rtvHandles,
		vector<DescriptorHeap::Handle>& dsvHandles);
	void AllocateDescriptorsForShaderTargets(
		DescriptorHeap& rtvDescriptorHeap,
		DescriptorHeap& dsvDescriptorHeap,
		ShaderTargetState& shaderTargetState,
		vector<DescriptorHeap::Handle>& rtvHandles,
		vector<DescriptorHeap::Handle>& dsvHandles);
	ShaderTargetState FillShaderTargetState();

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
	vector<vector<DescriptorHeap::Handle>> mRtvHandles; // [frameIndex][renderTextureIndex]
	vector<vector<DescriptorHeap::Handle>> mDsvHandles; // [frameIndex][renderTextureIndex]
	vector<DescriptorHeap::Handle> mSrvDescriptorHeapHandles; // constant buffer is stored as root parameter, so this is really only for srv and uav
	vector<DescriptorHeap::Handle> mUavDescriptorHeapHandles;
	vector<DescriptorHeap::Handle> mDynamicRtvHandles;
	vector<DescriptorHeap::Handle> mDynamicDsvHandles;
	DescriptorHeap::Handle mDynamicSrvDescriptorHeapHandle;
	DescriptorHeap::Handle mDynamicUavDescriptorHeapHandle;

	ID3D12PipelineState* mPso;
	ID3D12RootSignature* mRootSignature;
	PrimitiveType mPrimitiveType;

	// these values are set using OMSet... functions in D3D12, so we cache them here
	ShaderTargetState mShaderTargetState;
	ShaderTargetState mDynamicShaderTargetState;

	void AddShaderResource(ShaderResource* sr);
	int GetMaxMeshTextureCount();
};

// we will pass these uniforms directly to be consumed by GPU so no virtual pointer is allowed, 
// which means no virtual function in this class and all its child classes

template<class UniformType>
class PassCommon : public Pass
{
public:
	// default constructor for using in a container
	PassCommon() :
		PassCommon("unnamed pass")
	{
	}

	PassCommon(const string& debugName, bool useRenderTarget = true, bool useDepthStencil = true, bool shareMeshesWithPathTracer = false, PrimitiveType primitiveType = PrimitiveType::TRIANGLE) :
		mPassUniform(), Pass(debugName, useRenderTarget, useDepthStencil, shareMeshesWithPathTracer, primitiveType, sizeof(UniformType))
	{
	}

	virtual ~PassCommon() override
	{
	}

	void CreatePass(const string& debugName, bool useRenderTarget = true, bool useDepthStencil = true, bool shareMeshesWithPathTracer = false, PrimitiveType primitiveType = PrimitiveType::TRIANGLE)
	{
		Pass::CreatePass(debugName, useRenderTarget, useDepthStencil, shareMeshesWithPathTracer, primitiveType, sizeof(UniformType));
	}

	virtual void UpdateDynamicUniformBuffer() override
	{
		fatalAssertf(mDynamicUniformBuffer.IsValid(), "can't update null dynamic uniform buffer");
		memcpy((void*)mDynamicUniformBuffer.mCpuAddress, (void*)&mPassUniform, sizeof(UniformType));
	}

	virtual void UpdateUniformBuffer(int frameIndex) override
	{
		CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU. (so end is less than or equal to begin)
		void* cpuAddress;
		mUniformBuffers[frameIndex]->Map(0, &readRange, &cpuAddress);
		memcpy(cpuAddress, &mPassUniform, sizeof(UniformType));
		mUniformBuffers[frameIndex]->Unmap(0, nullptr);
	}

	void UpdateAllUniformBuffers()
	{
		for (int i = 0; i < mUniformBuffers.size(); i++)
		{
			UpdateUniformBuffer(i);
		}
	}

	virtual void CreateUniformBuffer(int frameCount) override
	{
		fatalAssertf(!std::is_polymorphic<UniformType>::value, "UniformType must be a POD type."); // is_pod is deprecated in c++20
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
			mUniformBuffers[i]->SetName(StringToWstring(mDebugName + ": pass uniform buffer ").c_str());
			UpdateUniformBuffer(i);
		}
	}

	virtual void UpdateUniform() override
	{
		if (mCamera)
		{
			mPassUniform.Update(mCamera);
		}
	}

	UniformType mPassUniform;
};

typedef PassCommon<PassUniformDefault> PassCommonDefault;

// typedef can't be forward declared, so we create a class for it
class PassDefault : public PassCommonDefault
{
	
public:
	PassDefault(const string& debugName, bool useRenderTarget = true, bool useDepthStencil = true, bool shareMeshesWithPathTracer = false, PrimitiveType primitiveType = PrimitiveType::TRIANGLE) :
		PassCommonDefault(debugName, useRenderTarget, useDepthStencil, shareMeshesWithPathTracer, primitiveType)
	{
	}

	virtual ~PassDefault() override
	{
	}
};
