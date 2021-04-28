#include "Pass.h"
#include "Camera.h"
#include "Texture.h"
#include "Buffer.h"
#include "Scene.h"
#include "Mesh.h"

void PassUniformDefault::Update(Camera* camera) 
{
	mViewProj = camera->GetViewProjMatrix();
	mViewProjInv = camera->GetViewProjInvMatrix();
	mView = camera->GetViewMatrix();
	mViewInv = camera->GetViewInvMatrix();
	mProj = camera->GetProjMatrix();
	mProjInv = camera->GetProjInvMatrix();
	mPassIndex = 1; // TODO: create a class static counter so that we can save an unique id for each frame
	mEyePos = camera->GetPosition();
	mNearClipPlane = camera->GetNearClipPlane();
	mFarClipPlane = camera->GetFarClipPlane();
	mWidth = camera->GetWidth();
	mHeight = camera->GetHeight();
	mFov = camera->GetFov();
}

Pass::Pass()
{

}

Pass::Pass(const wstring& debugName,
	bool outputRenderTarget,
	bool outputDepthStencil,
	bool shareMeshesWithPathTracer,
	PrimitiveType primitiveType) :
	mRenderer(nullptr), mDebugName(debugName),
	mConstantBlendFactorsUsed(false), mStencilReferenceUsed(false),
	mRenderTargetCount(0), mDepthStencilCount(0), mDepthStencilIndex(-1),
	mUseRenderTarget(outputRenderTarget), mUseDepthStencil(outputDepthStencil),
	mShareMeshesWithPathTracer(shareMeshesWithPathTracer),
	mPrimitiveType(primitiveType),
	mUniformDirtyFlag(0)
{
	memset(mShaders, 0, sizeof(mShaders));
}

Pass::~Pass()
{
	Release(true);
}

void Pass::SetScene(Scene* scene)
{
	mScene = scene;
}

void Pass::SetCamera(Camera* camera)
{
	mCamera = camera;
	mCamera->AddPass(this);
}

void Pass::AddMesh(Mesh* mesh)
{
	mMeshes.push_back(mesh);
}

void Pass::AddShaderResource(ShaderResource* sr)
{
	mShaderResources.push_back(sr);
}

void Pass::AddRenderTexture(RenderTexture* renderTexture, u32 depthSlice, u32 mipSlice, const BlendState& blendState, const DepthStencilState& depthStencilState)
{
	fatalAssertf(!(!renderTexture->IsRenderTargetUsed() && blendState.mBlendEnable), "render texture does not have render target buffer but trying to enable blend!");
	fatalAssertf(!(!renderTexture->IsRenderTargetUsed() && blendState.mLogicOpEnable), "render texture does not have render target buffer but trying to enable logic op!");
	fatalAssertf(!(!renderTexture->IsDepthStencilUsed() && depthStencilState.mDepthWriteEnable), "render texture does not have depth stencil buffer but trying to enable depth write!");
	fatalAssertf(!(!renderTexture->IsDepthStencilUsed() && depthStencilState.mDepthTestEnable), "render texture does not have depth stencil buffer but trying to enable depth test!");
	fatalAssertf(depthSlice < renderTexture->GetDepth() && mipSlice < renderTexture->GetMipLevelCount(), "render texture depth/mip out of range");
	ShaderTarget st = { renderTexture, blendState, depthStencilState, depthSlice, mipSlice };
	mShaderTargets.push_back(st);
	if (renderTexture->IsRenderTargetUsed())
		mRenderTargetCount++;
	if (renderTexture->IsDepthStencilUsed())
	{
		if (mDepthStencilCount == 0)
			mDepthStencilIndex = mShaderTargets.size() - 1;
		mDepthStencilCount++;
	}
}

void Pass::AddRenderTexture(RenderTexture* renderTexture, u32 depthSlice, u32 mipSlice, const BlendState& blendState)
{
	AddRenderTexture(renderTexture, depthSlice, mipSlice, blendState, DepthStencilState::None());
}

void Pass::AddRenderTexture(RenderTexture* renderTexture, u32 depthSlice, u32 mipSlice, const DepthStencilState& depthStencilState)
{
	AddRenderTexture(renderTexture, depthSlice, mipSlice, BlendState::NoBlend(), depthStencilState);
}

void Pass::AddShader(Shader* shader)
{
	mShaders[shader->GetShaderType()] = shader;
}

void Pass::AddTexture(Texture* texture)
{
	AddShaderResource(texture);
}

void Pass::AddBuffer(Buffer* buffer)
{
	AddShaderResource(buffer);
}

void Pass::AddWriteBuffer(WriteBuffer* writeBuffer)
{
	WriteTarget wt = { WriteTarget::BUFFER, 0 };
	wt.mWriteBuffer = writeBuffer;
	mWriteTargets.push_back(wt);
}

void Pass::AddWriteTexture(RenderTexture* writeTexture, u32 mipSlice)
{
	WriteTarget wt = { WriteTarget::TEXTURE, mipSlice };
	wt.mRenderTexture = writeTexture;
	mWriteTargets.push_back(wt);
}

void Pass::SetUniformDirty(int frameIndex)
{
	if (frameIndex < 0)
		mUniformDirtyFlag = ~0;
	else
		mUniformDirtyFlag |= 1 << frameIndex;
}

void Pass::ResetUniformDirty(int frameIndex)
{
	if (frameIndex < 0)
		mUniformDirtyFlag = 0;
	else
		mUniformDirtyFlag &= ~(1 << frameIndex);
}

void Pass::Release(bool checkOnly)
{
	for (auto& uniformBuffer : mUniformBuffers)
		SAFE_RELEASE(uniformBuffer, checkOnly);
}

D3D12_GPU_VIRTUAL_ADDRESS Pass::GetUniformBufferGpuAddress(int frameIndex)
{
	return mUniformBuffers[frameIndex]->GetGPUVirtualAddress();
}

vector<CD3DX12_CPU_DESCRIPTOR_HANDLE>& Pass::GetRtvHandles(int frameIndex)
{
	return mRtvHandles[frameIndex];
}

vector<CD3DX12_CPU_DESCRIPTOR_HANDLE>& Pass::GetDsvHandles(int frameIndex)
{
	return mDsvHandles[frameIndex];
}

D3D12_GPU_DESCRIPTOR_HANDLE Pass::GetSrvDescriptorHeapTableHandle(int frameIndex)
{
	return mSrvDescriptorHeapTableHandles[frameIndex];
}

D3D12_GPU_DESCRIPTOR_HANDLE Pass::GetUavDescriptorHeapTableHandle(int frameIndex)
{
	return mUavDescriptorHeapTableHandles[frameIndex];
}

D3D12_GPU_DESCRIPTOR_HANDLE Pass::GetSamplerDescriptorHeapTableHandle(int frameIndex)
{
	return mSamplerDescriptorHeapTableHandles[frameIndex];
}

bool Pass::IsConstantBlendFactorsUsed()
{
	return mConstantBlendFactorsUsed;
}

bool Pass::IsStencilReferenceUsed()
{
	return mStencilReferenceUsed;
}

float* Pass::GetConstantBlendFactors()
{
	return mBlendConstants;
}

uint32_t Pass::GetStencilReference()
{
	return mStencilReference;
}

const wstring& Pass::GetDebugName()
{
	return mDebugName;
}

PrimitiveType Pass::GetPrimitiveType()
{
	return mPrimitiveType;
}

void Pass::InitPass(
	Renderer* renderer,
	int frameCount,
	DescriptorHeap& cbvSrvUavDescriptorHeap,
	DescriptorHeap& samplerDescriptorHeap,
	DescriptorHeap& rtvDescriptorHeap,
	DescriptorHeap& dsvDescriptorHeap)
{
	fatalAssertf(mCamera, "pass must have a camera");
	fatalAssertf(mMeshes.size() || mShaders[Shader::ShaderType::COMPUTE_SHADER], "no mesh in this pass!");
	// revisit the two asserts below after adding support for compute shaders
	assertf(mShaders[Shader::VERTEX_SHADER] != nullptr, "no vertex shader in this pass!");
	assertf(mShaders[Shader::PIXEL_SHADER] != nullptr, "no pixel shader in this pass!");

	mRenderer = renderer;

	// 1. create uniform buffers 
	CreateUniformBuffer(frameCount);

	// 2. bind textures to heaps
	mSrvDescriptorHeapTableHandles.resize(frameCount);
	mUavDescriptorHeapTableHandles.resize(frameCount);
	mSamplerDescriptorHeapTableHandles.resize(frameCount);
	for (int i = 0; i < frameCount; i++)
	{
		// pass texture table and pass sampler table
		mSrvDescriptorHeapTableHandles[i] = cbvSrvUavDescriptorHeap.GetCurrentFreeGpuAddress();
		mSamplerDescriptorHeapTableHandles[i] = samplerDescriptorHeap.GetCurrentFreeGpuAddress();
		for (auto sr : mShaderResources)
		{
			if (sr->IsTexture())
			{
				Texture* texture = static_cast<Texture*>(sr);
				cbvSrvUavDescriptorHeap.AllocateSrv(texture->GetTextureBuffer(), texture->GetSrvDesc(), 1);
				samplerDescriptorHeap.AllocateSampler(texture->GetSamplerDesc(), 1);
			}
			else if (sr->IsBuffer())
			{
				Buffer* buffer = static_cast<Buffer*>(sr);
				cbvSrvUavDescriptorHeap.AllocateSrv(buffer->GetBuffer(), buffer->GetSrvDesc(), 1);
				// this is a dummy sampler just to keep the register number match, so that it's easier to maintain in shaders
				// it will waste some sampler registers but it's a trade off for easier maintenance
				// TODO: find a better way to improve this
				samplerDescriptorHeap.AllocateSampler(gSamplerLinear.GetDesc(), 1);
			}
		}
		mUavDescriptorHeapTableHandles[i] = cbvSrvUavDescriptorHeap.GetCurrentFreeGpuAddress();
		for (auto wt : mWriteTargets)
		{
			if(wt.mType == WriteTarget::BUFFER)
				cbvSrvUavDescriptorHeap.AllocateUav(wt.mWriteBuffer->GetBuffer(), wt.mWriteBuffer->GetUavDesc(), 1);
			else if (wt.mType == WriteTarget::TEXTURE)
				cbvSrvUavDescriptorHeap.AllocateUav(wt.mRenderTexture->GetTextureBuffer(), wt.mRenderTexture->GetUavDesc(wt.mMipSlice), 1);
		}
	}

	// 3. create root signature
	// ordered in scene/frame/pass/object
	// TODO: add support for buffer, write buffer, write texture to all spaces
	int srvCounts[(int)RegisterSpace::COUNT] = { mScene->GetTextureCount(), mRenderer->GetMaxFrameTextureCount(), mShaderResources.size(), GetMaxMeshTextureCount() };
	int uavCounts[(int)RegisterSpace::COUNT] = { 0, 0, mWriteTargets.size(), 0 };
	mRenderer->CreateRootSignature(&mRootSignature, srvCounts, uavCounts);

	// 4. bind render textures and create pso
	mRtvHandles.resize(frameCount);
	mDsvHandles.resize(frameCount);

	// if render textures are used
	for (int i = 0; i < frameCount; i++)
	{
		mRtvHandles[i].resize(mShaderTargets.size());
		mDsvHandles[i].resize(mShaderTargets.size());
	}

	// populate output merger stage parameter with render texture attributes
	mConstantBlendFactorsUsed = false;
	mStencilReferenceUsed = false;
	for (int i = 0; i < mShaderTargets.size(); i++) // only the first 8 targets will be used
	{
		// these values are set using OMSet... functions in D3D12, so we cache them here
		const BlendState& blendState = mShaderTargets[i].mBlendState;
		if (!mConstantBlendFactorsUsed && (
			blendState.mSrcBlend == BlendState::BlendFactor::CONSTANT ||
			blendState.mSrcBlend == BlendState::BlendFactor::INV_CONSTANT ||
			blendState.mDestBlend == BlendState::BlendFactor::CONSTANT ||
			blendState.mDestBlend == BlendState::BlendFactor::INV_CONSTANT ||
			blendState.mSrcBlendAlpha == BlendState::BlendFactor::CONSTANT ||
			blendState.mSrcBlendAlpha == BlendState::BlendFactor::INV_CONSTANT ||
			blendState.mDestBlendAlpha == BlendState::BlendFactor::CONSTANT ||
			blendState.mDestBlendAlpha == BlendState::BlendFactor::INV_CONSTANT))
		{
			mConstantBlendFactorsUsed = true; // only the first one will be used
			memcpy(mBlendConstants, blendState.mBlendConstants, sizeof(blendState.mBlendConstants));
		}

		const DepthStencilState& depthStencilState = mShaderTargets[i].mDepthStencilState;
		if (!mStencilReferenceUsed && (
			depthStencilState.mFrontStencilOpSet.mDepthFailOp == DepthStencilState::StencilOp::REPLACE ||
			depthStencilState.mFrontStencilOpSet.mFailOp == DepthStencilState::StencilOp::REPLACE ||
			depthStencilState.mFrontStencilOpSet.mPassOp == DepthStencilState::StencilOp::REPLACE ||
			depthStencilState.mBackStencilOpSet.mDepthFailOp == DepthStencilState::StencilOp::REPLACE ||
			depthStencilState.mBackStencilOpSet.mFailOp == DepthStencilState::StencilOp::REPLACE ||
			depthStencilState.mBackStencilOpSet.mPassOp == DepthStencilState::StencilOp::REPLACE
			))
		{
			mStencilReferenceUsed = true; // only the first one will be used
			mStencilReference = depthStencilState.mStencilReference;
		}

		for (int j = 0; j < frameCount; j++)
		{
			mRtvHandles[j][i] = rtvDescriptorHeap.AllocateRtv(mShaderTargets[i].mRenderTexture->GetRenderTargetBuffer(), mShaderTargets[i].mRenderTexture->GetRtvDesc(mShaderTargets[i].mDepthSlice, mShaderTargets[i].mMipSlice), 1);
			mDsvHandles[j][i] = dsvDescriptorHeap.AllocateDsv(mShaderTargets[i].mRenderTexture->GetDepthStencilBuffer(), mShaderTargets[i].mRenderTexture->GetDsvDesc(mShaderTargets[i].mDepthSlice, mShaderTargets[i].mMipSlice), 1);
		}
	}

	if (mShaders[Shader::COMPUTE_SHADER])
		mRenderer->CreateComputePSO(*this, &mPso, mRootSignature, mShaders[Shader::COMPUTE_SHADER], mDebugName);
	else
		mRenderer->CreateGraphicsPSO(
			*this,
			&mPso,
			mRootSignature,
			Renderer::TranslatePrimitiveTopologyType(mPrimitiveType),
			mShaders[Shader::VERTEX_SHADER],
			mShaders[Shader::HULL_SHADER],
			mShaders[Shader::DOMAIN_SHADER],
			mShaders[Shader::GEOMETRY_SHADER],
			mShaders[Shader::PIXEL_SHADER],
			mDebugName);
}

int Pass::GetCbvSrvUavCount()
{
	return mShaderResources.size() + mWriteTargets.size();
}

int Pass::GetShaderResourceCount()
{
	return mShaderResources.size();
}

int Pass::GetShaderTargetCount()
{
	return mShaderTargets.size();
}

int Pass::GetWriteTargetCount()
{
	return mWriteTargets.size();
}

vector<ShaderResource*>& Pass::GetShaderResources()
{
	return mShaderResources;
}

vector<ShaderTarget>& Pass::GetShaderTargets()
{
	return mShaderTargets;
}

ShaderTarget Pass::GetShaderTarget(int i)
{
	return mShaderTargets[i];
}

int Pass::GetRenderTargetCount()
{
	return mRenderTargetCount;
}

int Pass::GetDepthStencilCount()
{
	return mDepthStencilCount;
}

int Pass::GetDepthStencilIndex()
{
	return mDepthStencilIndex;
}

bool Pass::UseRenderTarget()
{
	return mUseRenderTarget;
}

bool Pass::UseDepthStencil()
{
	return mUseDepthStencil;
}

bool Pass::ShareMeshesWithPathTracer()
{
	return mShareMeshesWithPathTracer;
}

bool Pass::IsUniformDirty(int frameIndex)
{
	if (frameIndex < 0)
		return mUniformDirtyFlag;
	else
		return mUniformDirtyFlag & (1 << frameIndex);
}

Camera* Pass::GetCamera()
{
	return mCamera;
}

Scene* Pass::GetScene()
{
	return mScene;
}

vector<Mesh*>& Pass::GetMeshVec()
{
	return mMeshes;
}

ID3D12PipelineState* Pass::GetPso()
{
	return mPso;
}

ID3D12RootSignature* Pass::GetRootSignature()
{
	return mRootSignature;
}

int Pass::GetMaxMeshTextureCount()
{
	int maxTextureCount = 0;
	for(auto obj : mMeshes)
	{
		if (obj->GetTextureCount() > maxTextureCount)
			maxTextureCount = obj->GetTextureCount();
	}
	return maxTextureCount;
}

bool ShaderTarget::IsRenderTargetUsed()
{
	return mRenderTexture->IsRenderTargetUsed();
}

bool ShaderTarget::IsDepthStencilUsed()
{
	return mRenderTexture->IsDepthStencilUsed();
}

u32 ShaderTarget::GetHeight()
{
	return mRenderTexture->GetHeight(mMipSlice);
}

u32 ShaderTarget::GetWidth()
{
	return mRenderTexture->GetWidth(mMipSlice);
}