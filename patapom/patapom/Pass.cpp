#include "Pass.h"
#include "Camera.h"
#include "Texture.h"
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
	bool outputDepthStencil) : 
	mRenderer(nullptr), mDebugName(debugName), 
	mConstantBlendFactorsUsed(false), mStencilReferenceUsed(false),
	mRenderTargetCount(0), mDepthStencilCount(0), mDepthStencilIndex(-1),
	mUseRenderTarget(outputRenderTarget), mUseDepthStencil(outputDepthStencil)
{
	memset(mShaders, 0, sizeof(mShaders));
}

Pass::~Pass()
{
	Release();
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

void Pass::AddTexture(Texture* texture)
{
	mTextures.push_back(texture);
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

void Pass::Release()
{
	for (int i = 0; i < mUniformBuffers.size(); i++)
	{
		SAFE_RELEASE(mUniformBuffers[i]);
	}
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

D3D12_GPU_DESCRIPTOR_HANDLE Pass::GetCbvSrvUavDescriptorHeapTableHandle(int frameIndex)
{
	return mCbvSrvUavDescriptorHeapTableHandles[frameIndex];
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

void Pass::InitPass(
	Renderer* renderer,
	int frameCount,
	DescriptorHeap& cbvSrvUavDescriptorHeap,
	DescriptorHeap& samplerDescriptorHeap,
	DescriptorHeap& rtvDescriptorHeap,
	DescriptorHeap& dsvDescriptorHeap)
{
	fatalAssertf(mMeshes.size() != 0, "no mesh in this pass!");
	// revisit the two asserts below after adding support for compute shaders
	assertf(mShaders[Shader::VERTEX_SHADER] != nullptr, "no vertex shader in this pass!");
	assertf(mShaders[Shader::PIXEL_SHADER] != nullptr, "no pixel shader in this pass!");

	mRenderer = renderer;

	// 1. create uniform buffers 
	CreateUniformBuffer(frameCount);

	// 2. bind textures to heaps
	mCbvSrvUavDescriptorHeapTableHandles.resize(frameCount);
	mSamplerDescriptorHeapTableHandles.resize(frameCount);
	for (int i = 0; i < frameCount; i++)
	{
		// pass texture table and pass sampler table
		mCbvSrvUavDescriptorHeapTableHandles[i] = cbvSrvUavDescriptorHeap.GetCurrentFreeGpuAddress();
		mSamplerDescriptorHeapTableHandles[i] = samplerDescriptorHeap.GetCurrentFreeGpuAddress();
		for (auto texture : mTextures)
		{
			cbvSrvUavDescriptorHeap.AllocateSrv(texture->GetTextureBuffer(), texture->GetSrvDesc(), 1);
			samplerDescriptorHeap.AllocateSampler(texture->GetSamplerDesc(), 1);
		}
	}

	// 3. create root signature
	// ordered in scene/frame/pass/object
	int maxTextureCount[(int)RegisterSpace::COUNT] = { mScene->GetTextureCount(), mRenderer->GetMaxFrameTextureCount(), mTextures.size(), GetMaxMeshTextureCount() };
	mRenderer->CreateGraphicsRootSignature(&mRootSignature, maxTextureCount);

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
	for(int i = 0; i < mShaderTargets.size(); i++) // only the first 8 targets will be used
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
	
	// create PSO
	mPrimitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	mRenderer->CreatePSO(
		*this,
		&mPso,
		mRootSignature,
		mPrimitiveType,
		mShaders[Shader::VERTEX_SHADER],
		mShaders[Shader::HULL_SHADER],
		mShaders[Shader::DOMAIN_SHADER],
		mShaders[Shader::GEOMETRY_SHADER],
		mShaders[Shader::PIXEL_SHADER],
		mDebugName);
}

int Pass::GetTextureCount()
{
	return mTextures.size();
}

int Pass::GetShaderTargetCount()
{
	return mShaderTargets.size();
}

vector<Texture*>& Pass::GetTextures()
{
	return mTextures;
}

vector<ShaderTarget>& Pass::GetShaderTargets()
{
	return mShaderTargets;
}

RenderTexture* Pass::GetRenderTexture(int i)
{
	return mShaderTargets[i].mRenderTexture;
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