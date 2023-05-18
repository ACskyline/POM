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
	mResolution.x = camera->GetWidth();
	mResolution.y = camera->GetHeight();
	mFov = camera->GetFov();
}

Pass::Pass() : 
	Pass("uinitialized pass",
		false,
		false,
		false,
		PrimitiveType::INVALID,
		-1)
{
}

Pass::Pass(const string& debugName,
	bool outputRenderTarget,
	bool outputDepthStencil,
	bool shareMeshesWithPathTracer,
	PrimitiveType primitiveType,
	int uniformSizeInByte) :
	mRenderer(nullptr),
	mShaderTargetState({ false, false, 0, {0} }),
	mDynamicShaderTargetState({ false, false, 0, {0} }),
	mRenderTargetCount(0), 
	mDepthStencilCount(0), 
	mDepthStencilIndex(-1),
	mResourceDirtyFlag(ResourceDirtyFlag::ALL_CLEAN),
	mPso(nullptr),
	mRootSignature(nullptr)
{
	CreatePass(
		debugName,
		outputRenderTarget,
		outputDepthStencil,
		shareMeshesWithPathTracer,
		primitiveType,
		uniformSizeInByte);
	memset(mShaders, 0, sizeof(mShaders));
}

Pass::~Pass()
{
	Release(true);
}

void Pass::CreatePass(const string& debugName, bool useRenderTarget, bool useDepthStencil, bool shareMeshesWithPathTracer, PrimitiveType primitiveType, int uniformSizeInByte)
{
	mDebugName = debugName;
	mUseRenderTarget = useRenderTarget;
	mUseDepthStencil = useDepthStencil;
	mShareMeshesWithPathTracer = shareMeshesWithPathTracer;
	mPrimitiveType = primitiveType;
	mUniformSizeInByte = uniformSizeInByte;
}

void Pass::SetDirty(ResourceDirtyFlag dirtyFlag)
{
	for(auto& flag : mResourceDirtyFlag)
		flag = flag | dirtyFlag;
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
	if (renderTexture)
	{
		fatalAssertf(!(!renderTexture->IsRenderTargetUsed() && blendState.mBlendEnable), "render texture does not have render target buffer but trying to enable blend!");
		fatalAssertf(!(!renderTexture->IsRenderTargetUsed() && blendState.mLogicOpEnable), "render texture does not have render target buffer but trying to enable logic op!");
		fatalAssertf(!(!renderTexture->IsDepthStencilUsed() && depthStencilState.mDepthWriteEnable), "render texture does not have depth stencil buffer but trying to enable depth write!");
		fatalAssertf(!(!renderTexture->IsDepthStencilUsed() && depthStencilState.mDepthTestEnable), "render texture does not have depth stencil buffer but trying to enable depth test!");
		fatalAssertf(depthSlice < renderTexture->GetDepth() && mipSlice < renderTexture->GetMipLevelCount(), "render texture depth/mip out of range");
		fatalAssertf(!(renderTexture->IsRenderTargetUsed() && !mUseRenderTarget), "pass doesn't support render target buffer");
		fatalAssertf(!(renderTexture->IsDepthStencilUsed() && !mUseDepthStencil), "pass doesn't support depth stencil buffer");
	}
	ShaderTarget st = { renderTexture, blendState, depthStencilState, depthSlice, mipSlice };
	mShaderTargets.push_back(st);
	if (!renderTexture || renderTexture->IsRenderTargetUsed())
		mRenderTargetCount++;
	if (!renderTexture || renderTexture->IsDepthStencilUsed())
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

void Pass::SetBuffer(u8 slot, Buffer* buffer)
{
	fatalAssertf(slot < mShaderResources.size(), "slot out of range");
	fatalAssertf(mShaderResources[slot]->IsBuffer(), "shader resource type mismatch at slot %d", (int)slot);
	fatalAssertf(buffer->IsInitialized(), "can't set to an uninitialized resource");
	mShaderResources[slot] = buffer;
	SetDirty(ResourceDirtyFlag::SHADER_RESOURCE_DIRTY_FRAME);
}

void Pass::AddWriteBuffer(WriteBuffer* writeBuffer)
{
	WriteTarget wt = { WriteTarget::Type::BUFFER, 0 };
	wt.mWriteBuffer = writeBuffer;
	mWriteTargets.push_back(wt);
}

void Pass::SetWriteBuffer(u8 slot, WriteBuffer* writeBuffer)
{
	fatalAssertf(slot < mWriteTargets.size(), "slot out of range");
	fatalAssertf(mWriteTargets[slot].mType == WriteTarget::Type::BUFFER, "write target type mismatch at slot %d", (int)slot);
	fatalAssertf(writeBuffer->IsInitialized(), "can't set to an uninitialized resource");
	mWriteTargets[slot].mWriteBuffer = writeBuffer;
	SetDirty(ResourceDirtyFlag::WRITE_TARGET_DIRTY_FRAME);
}

void Pass::AddWriteTexture(RenderTexture* writeTexture, u32 mipSlice)
{
	WriteTarget wt = { WriteTarget::Type::TEXTURE, mipSlice };
	wt.mRenderTexture = writeTexture;
	mWriteTargets.push_back(wt);
}

void Pass::ReallocateDescriptorsForShaderResources(DescriptorHeap::Handle srvDescriptorHeapHandle)
{
	assertf(mRenderer, "this pass is not initialized yet");
	fatalAssertf(srvDescriptorHeapHandle.IsValid(), "reusing old handle which should be valid");
	for (auto sr : mShaderResources)
	{
		if (sr->IsTexture())
		{
			Texture* texture = static_cast<Texture*>(sr);
			srvDescriptorHeapHandle.GetDescriptorHeap()->AllocateSrv(texture->GetTextureBuffer(), texture->GetSrvDesc(), srvDescriptorHeapHandle);
		}
		else if (sr->IsBuffer())
		{
			Buffer* buffer = static_cast<Buffer*>(sr);
			srvDescriptorHeapHandle.GetDescriptorHeap()->AllocateSrv(buffer->GetBuffer(), buffer->GetSrvDesc(), srvDescriptorHeapHandle);
		}
	}
}

void Pass::AllocateDescriptorsForShaderResources(DescriptorHeap& cbvSrvUavDescriptorHeap, DescriptorHeap::Handle& srvDescriptorHeapHandle)
{
	assertf(mRenderer, "this pass is not initialized yet");
	fatalAssertf(!srvDescriptorHeapHandle.IsValid() || srvDescriptorHeapHandle.GetDescriptorHeap() == &cbvSrvUavDescriptorHeap, 
		"dynamic handle is either invalid or from this frame's heap");
	srvDescriptorHeapHandle = cbvSrvUavDescriptorHeap.GetCurrentFreeHandle();
	for (auto sr : mShaderResources)
	{
		if (sr->IsTexture())
		{
			Texture* texture = static_cast<Texture*>(sr);
			cbvSrvUavDescriptorHeap.AllocateSrv(texture->GetTextureBuffer(), texture->GetSrvDesc());
		}
		else if (sr->IsBuffer())
		{
			Buffer* buffer = static_cast<Buffer*>(sr);
			cbvSrvUavDescriptorHeap.AllocateSrv(buffer->GetBuffer(), buffer->GetSrvDesc());
		}
	}
}

void Pass::ReallocateDescriptorsForWriteTargets(
	DescriptorHeap::Handle uavDescriptorHeapHandle)
{
	assertf(mRenderer, "this pass is not initialized yet");
	fatalAssertf(uavDescriptorHeapHandle.IsValid(), "reusing old handle which should be valid");
	for (auto wt : mWriteTargets)
	{
		if (wt.mType == WriteTarget::Type::BUFFER)
			uavDescriptorHeapHandle.GetDescriptorHeap()->AllocateUav(wt.mWriteBuffer->GetBuffer(), wt.mWriteBuffer->GetUavDesc(), uavDescriptorHeapHandle);
		else if (wt.mType == WriteTarget::Type::TEXTURE)
			uavDescriptorHeapHandle.GetDescriptorHeap()->AllocateUav(wt.mRenderTexture->GetTextureBuffer(), wt.mRenderTexture->GetUavDesc(wt.mMipSlice), uavDescriptorHeapHandle);
	}
}

void Pass::AllocateDescriptorsForWriteTargets(
	DescriptorHeap& cbvSrvUavDescriptorHeap,
	DescriptorHeap::Handle& uavDescriptorHeapHandle)
{
	assertf(mRenderer, "this pass is not initialized yet");
	fatalAssertf(!uavDescriptorHeapHandle.IsValid() || uavDescriptorHeapHandle.GetDescriptorHeap() == &cbvSrvUavDescriptorHeap,
		"dynamic handle is either invalid or from this frame's heap");
	uavDescriptorHeapHandle = cbvSrvUavDescriptorHeap.GetCurrentFreeHandle();
	for (auto wt : mWriteTargets)
	{
		if (wt.mType == WriteTarget::Type::BUFFER)
			cbvSrvUavDescriptorHeap.AllocateUav(wt.mWriteBuffer->GetBuffer(), wt.mWriteBuffer->GetUavDesc());
		else if (wt.mType == WriteTarget::Type::TEXTURE)
			cbvSrvUavDescriptorHeap.AllocateUav(wt.mRenderTexture->GetTextureBuffer(), wt.mRenderTexture->GetUavDesc(wt.mMipSlice));
	}
}

void Pass::ReallocateDescriptorsForShaderTargets(
	ShaderTargetState& shaderTargetState,
	vector<DescriptorHeap::Handle>& rtvHandles,
	vector<DescriptorHeap::Handle>& dsvHandles)
{
	assertf(mRenderer, "this pass is not initialized yet");
	fatalAssert(rtvHandles.size() > 0 && rtvHandles.size() == dsvHandles.size());
	shaderTargetState = FillShaderTargetState();
	for (int i = 0; i < mShaderTargets.size(); i++) // only the first 8 targets will be used
	{
		// displayfln("allocated an rtv for pass %s, render texture %d: %s, frame %d", WstringToString(mDebugName).c_str(), i, WstringToString(mShaderTargets[i].mRenderTexture->GetDebugName()).c_str(), j);
		fatalAssertf(rtvHandles[i].IsValid(), "static handles must be valid");
		rtvHandles[i] = rtvHandles[i].GetDescriptorHeap()->AllocateRtv(mShaderTargets[i].mRenderTexture->GetRenderTargetBuffer(), mShaderTargets[i].mRenderTexture->GetRtvDesc(mShaderTargets[i].mDepthSlice, mShaderTargets[i].mMipSlice), rtvHandles[i]);
		fatalAssertf(dsvHandles[i].IsValid(), "static handles must be valid");
		dsvHandles[i] = dsvHandles[i].GetDescriptorHeap()->AllocateDsv(mShaderTargets[i].mRenderTexture->GetDepthStencilBuffer(), mShaderTargets[i].mRenderTexture->GetDsvDesc(mShaderTargets[i].mDepthSlice, mShaderTargets[i].mMipSlice), dsvHandles[i]);
	}
}

void Pass::AllocateDescriptorsForShaderTargets(
	DescriptorHeap& rtvDescriptorHeap,
	DescriptorHeap& dsvDescriptorHeap,
	ShaderTargetState& shaderTargetState,
	vector<DescriptorHeap::Handle>& rtvHandles,
	vector<DescriptorHeap::Handle>& dsvHandles)
{
	assertf(mRenderer, "this pass is not initialized yet");
	rtvHandles.resize(mShaderTargets.size());
	dsvHandles.resize(mShaderTargets.size());
	shaderTargetState = FillShaderTargetState();
	for (int i = 0; i < mShaderTargets.size(); i++) // only the first 8 targets will be used
	{
		// displayfln("allocated an rtv for pass %s, render texture %d: %s, frame %d", WstringToString(mDebugName).c_str(), i, WstringToString(mShaderTargets[i].mRenderTexture->GetDebugName()).c_str(), j);
		fatalAssertf(!rtvHandles[i].IsValid() || rtvHandles[i].GetDescriptorHeap() == &rtvDescriptorHeap, 
			"dynamic handle is either invalid or from this frame's heap");
		fatalAssertf(!dsvHandles[i].IsValid() || dsvHandles[i].GetDescriptorHeap() == &dsvDescriptorHeap,
			"dynamic handle is either invalid or from this frame's heap");
		if (mShaderTargets[i].mRenderTexture)
		{
			rtvHandles[i] = rtvDescriptorHeap.AllocateRtv(mShaderTargets[i].mRenderTexture->GetRenderTargetBuffer(), mShaderTargets[i].mRenderTexture->GetRtvDesc(mShaderTargets[i].mDepthSlice, mShaderTargets[i].mMipSlice));
			dsvHandles[i] = dsvDescriptorHeap.AllocateDsv(mShaderTargets[i].mRenderTexture->GetDepthStencilBuffer(), mShaderTargets[i].mRenderTexture->GetDsvDesc(mShaderTargets[i].mDepthSlice, mShaderTargets[i].mMipSlice));
		}
	}
}

Pass::ShaderTargetState Pass::FillShaderTargetState()
{
	ShaderTargetState shaderTargetState = { 0 };
	for (int i = 0; i < mShaderTargets.size(); i++) // only the first 8 targets will be used
	{
		// these values are set using OMSet... functions in D3D12, so we cache them here
		const BlendState& blendState = mShaderTargets[i].mBlendState;
		if (!shaderTargetState.mConstantBlendFactorsUsed && (
			blendState.mSrcBlend == BlendState::BlendFactor::CONSTANT ||
			blendState.mSrcBlend == BlendState::BlendFactor::INV_CONSTANT ||
			blendState.mDestBlend == BlendState::BlendFactor::CONSTANT ||
			blendState.mDestBlend == BlendState::BlendFactor::INV_CONSTANT ||
			blendState.mSrcBlendAlpha == BlendState::BlendFactor::CONSTANT ||
			blendState.mSrcBlendAlpha == BlendState::BlendFactor::INV_CONSTANT ||
			blendState.mDestBlendAlpha == BlendState::BlendFactor::CONSTANT ||
			blendState.mDestBlendAlpha == BlendState::BlendFactor::INV_CONSTANT))
		{
			shaderTargetState.mConstantBlendFactorsUsed = true; // only the first one will be used
			memcpy(shaderTargetState.mBlendConstants, blendState.mBlendConstants, sizeof(blendState.mBlendConstants));
		}

		const DepthStencilState& depthStencilState = mShaderTargets[i].mDepthStencilState;
		if (!shaderTargetState.mStencilReferenceUsed && (
			depthStencilState.mFrontStencilOpSet.mDepthFailOp == DepthStencilState::StencilOp::REPLACE ||
			depthStencilState.mFrontStencilOpSet.mFailOp == DepthStencilState::StencilOp::REPLACE ||
			depthStencilState.mFrontStencilOpSet.mPassOp == DepthStencilState::StencilOp::REPLACE ||
			depthStencilState.mBackStencilOpSet.mDepthFailOp == DepthStencilState::StencilOp::REPLACE ||
			depthStencilState.mBackStencilOpSet.mFailOp == DepthStencilState::StencilOp::REPLACE ||
			depthStencilState.mBackStencilOpSet.mPassOp == DepthStencilState::StencilOp::REPLACE
			))
		{
			shaderTargetState.mStencilReferenceUsed = true; // only the first one will be used
			shaderTargetState.mStencilReference = depthStencilState.mStencilReference;
		}
	}
	return shaderTargetState;
}

void Pass::FlushDynamicDescriptors(int frameIndex)
{
	// 1. shader resources
	if (mResourceDirtyFlag[frameIndex] & ResourceDirtyFlag::SHADER_RESOURCE_DIRTY)
	{
		AllocateDescriptorsForShaderResources(mRenderer->GetDynamicCbvSrvUavDescriptorHeap(frameIndex), mDynamicSrvDescriptorHeapHandle);
		mResourceDirtyFlag[frameIndex] &= ~ResourceDirtyFlag::SHADER_RESOURCE_DIRTY;
	}

	// 2. write targets
	// TODO: can't share the flag with SRV but modify the same dirty flags separately
	// this will cause dirty UAV ignored. Use a separate flag
	if (mResourceDirtyFlag[frameIndex] & ResourceDirtyFlag::WRITE_TARGET_DIRTY)
	{
		AllocateDescriptorsForWriteTargets(mRenderer->GetDynamicCbvSrvUavDescriptorHeap(frameIndex), mDynamicUavDescriptorHeapHandle);
		mResourceDirtyFlag[frameIndex] &= ~ResourceDirtyFlag::WRITE_TARGET_DIRTY;
	}

	// 3. shader targets
	if (mResourceDirtyFlag[frameIndex] & ResourceDirtyFlag::SHADER_TARGET_DIRTY)
	{
		AllocateDescriptorsForShaderTargets(
			mRenderer->GetDynamicRtvDescriptorHeap(frameIndex),
			mRenderer->GetDynamicDsvDescriptorHeap(frameIndex),
			mDynamicShaderTargetState,
			mDynamicRtvHandles,
			mDynamicDsvHandles);
		mResourceDirtyFlag[frameIndex] &= ~ResourceDirtyFlag::SHADER_TARGET_DIRTY;
	}

	// 4. uniform buffer
	if (mResourceDirtyFlag[frameIndex] & ResourceDirtyFlag::UNIFORM_DIRTY)
	{
		mDynamicUniformBuffer = mRenderer->AllocateDynamicUniformBuffer(frameIndex, mUniformSizeInByte);
		UpdateDynamicUniformBuffer();
		mResourceDirtyFlag[frameIndex] &= ~ResourceDirtyFlag::UNIFORM_DIRTY;
	}
}

void Pass::RefreshStaticDescriptors(int frameIndex)
{
	// 1. shader resources
	if (mResourceDirtyFlag[frameIndex] & ResourceDirtyFlag::SHADER_RESOURCE_DIRTY_FRAME)
	{
		ReallocateDescriptorsForShaderResources(mSrvDescriptorHeapHandles[frameIndex]);
		mDynamicSrvDescriptorHeapHandle.SetInvalid();
		mResourceDirtyFlag[frameIndex] &= ~ResourceDirtyFlag::SHADER_RESOURCE_DIRTY_FRAME;
	}

	// 2. write targets
	if (mResourceDirtyFlag[frameIndex] & ResourceDirtyFlag::WRITE_TARGET_DIRTY_FRAME)
	{
		ReallocateDescriptorsForWriteTargets(mUavDescriptorHeapHandles[frameIndex]);
		mDynamicUavDescriptorHeapHandle.SetInvalid();
		mResourceDirtyFlag[frameIndex] &= ~ResourceDirtyFlag::WRITE_TARGET_DIRTY_FRAME;
	}

	// 3. shader targets
	auto SetDynamicShaderTargetsInvalid = [](vector<DescriptorHeap::Handle>& handles) {
		for (auto& d : handles)
			d.SetInvalid();
	};

	if (mResourceDirtyFlag[frameIndex] & ResourceDirtyFlag::SHADER_TARGET_DIRTY_FRAME)
	{
		ReallocateDescriptorsForShaderTargets(
			mShaderTargetState,
			mRtvHandles[frameIndex],
			mDsvHandles[frameIndex]);
		SetDynamicShaderTargetsInvalid(mDynamicRtvHandles);
		SetDynamicShaderTargetsInvalid(mDynamicDsvHandles);
		mResourceDirtyFlag[frameIndex] &= ~ResourceDirtyFlag::SHADER_TARGET_DIRTY_FRAME;
	}

	// 4. uniform buffer
	if (mResourceDirtyFlag[frameIndex] & ResourceDirtyFlag::UNIFORM_DIRTY_FRAME)
	{
		UpdateUniformBuffer(frameIndex);
		mDynamicUniformBuffer.SetInvalid();
		mResourceDirtyFlag[frameIndex] &= ~ResourceDirtyFlag::UNIFORM_DIRTY_FRAME;
	}
}

void Pass::Release(bool checkOnly)
{
	for (auto& uniformBuffer : mUniformBuffers)
		SAFE_RELEASE(uniformBuffer, checkOnly);
	SAFE_RELEASE(mPso, checkOnly);
	SAFE_RELEASE(mRootSignature, checkOnly);
}

D3D12_GPU_VIRTUAL_ADDRESS Pass::GetUniformBufferGpuAddress(int frameIndex)
{
	if (mDynamicUniformBuffer.IsValid())
	{
		return mDynamicUniformBuffer.mGpuAddress;
	}
	else
	{
		return mUniformBuffers[frameIndex]->GetGPUVirtualAddress();
	}
}

vector<DescriptorHeap::Handle>& Pass::GetRtvHandles(int frameIndex)
{
	if (mResourceDirtyFlag[frameIndex] & ResourceDirtyFlag::SHADER_TARGET_DIRTY_FRAME)
	{
		return mDynamicRtvHandles;
	}
	else
	{
		return mRtvHandles[frameIndex];
	}
}

vector<DescriptorHeap::Handle>& Pass::GetDsvHandles(int frameIndex)
{
	if (mResourceDirtyFlag[frameIndex] & ResourceDirtyFlag::SHADER_TARGET_DIRTY_FRAME)
	{
		return mDynamicDsvHandles;
	}
	else
	{
		return mDsvHandles[frameIndex];
	}
}

DescriptorHeap::Handle Pass::GetSrvDescriptorHeapHandle(int frameIndex)
{
	if (mDynamicSrvDescriptorHeapHandle.IsValid())
	{
		return mDynamicSrvDescriptorHeapHandle;
	}
	else
	{
		return mSrvDescriptorHeapHandles[frameIndex];
	}
}

DescriptorHeap::Handle Pass::GetUavDescriptorHeapHandle(int frameIndex)
{
	if (mDynamicUavDescriptorHeapHandle.IsValid())
	{
		return mDynamicUavDescriptorHeapHandle;
	}
	else
	{
		return mUavDescriptorHeapHandles[frameIndex];
	}
}

bool Pass::IsConstantBlendFactorsUsed(int frameIndex)
{
	if (mResourceDirtyFlag[frameIndex] & ResourceDirtyFlag::SHADER_TARGET_DIRTY_FRAME)
		return mDynamicShaderTargetState.mConstantBlendFactorsUsed;
	else
		return mShaderTargetState.mConstantBlendFactorsUsed;
}

bool Pass::IsStencilReferenceUsed(int frameIndex)
{
	if (mResourceDirtyFlag[frameIndex] & ResourceDirtyFlag::SHADER_TARGET_DIRTY_FRAME)
		return mDynamicShaderTargetState.mStencilReferenceUsed;
	else
		return mShaderTargetState.mStencilReferenceUsed;
}

float* Pass::GetConstantBlendFactors(int frameIndex)
{
	if (mResourceDirtyFlag[frameIndex] & ResourceDirtyFlag::SHADER_TARGET_DIRTY_FRAME)
		return mDynamicShaderTargetState.mBlendConstants;
	else
		return mShaderTargetState.mBlendConstants;
}

uint32_t Pass::GetStencilReference(int frameIndex)
{
	if (mResourceDirtyFlag[frameIndex] & ResourceDirtyFlag::SHADER_TARGET_DIRTY_FRAME)
		return mDynamicShaderTargetState.mStencilReference;
	else
		return mShaderTargetState.mStencilReference;
}

const string& Pass::GetDebugName()
{
	return mDebugName;
}

string Pass::GetGraphicShaderNames()
{
	string result;
	for (int i = 0; i < Shader::ShaderType::COMPUTE_SHADER; i++)
	{
		if (mShaders[i])
		{
			if (i != 0)
				result += "->";
			result += mShaders[i]->GetFileName();
		}
	}
	return result;
}

string Pass::GetComputeShaderName()
{
	return mShaders[Shader::ShaderType::COMPUTE_SHADER]->GetFileName();
}

PrimitiveType Pass::GetPrimitiveType()
{
	return mPrimitiveType;
}

void Pass::InitPass(
	Renderer* renderer,
	int frameCount,
	DescriptorHeap& cbvSrvUavDescriptorHeap,
	DescriptorHeap& rtvDescriptorHeap,
	DescriptorHeap& dsvDescriptorHeap)
{
	fatalAssertf(mScene, "pass '%s' not assigned to any scene", mDebugName.c_str());
	fatalAssertf(mCamera || mShaders[Shader::ShaderType::COMPUTE_SHADER], "non-compute pass must have a camera");
	fatalAssertf(mMeshes.size() || mShaders[Shader::ShaderType::COMPUTE_SHADER], "no mesh in this non-compute pass!");
	// revisit the two asserts below after adding support for compute shaders
	assertf(mShaders[Shader::VERTEX_SHADER] != nullptr || mShaders[Shader::PIXEL_SHADER] != nullptr || mShaders[Shader::ShaderType::COMPUTE_SHADER], "no vertex/pixel shader in this non-compute pass!");

	mRenderer = renderer;

	// 0. set dirty flags
	mResourceDirtyFlag.resize(frameCount);
	for (int i = 0; i < frameCount; i++)
	{
		mResourceDirtyFlag[i] = ResourceDirtyFlag::ALL_CLEAN;
	}

	// 1. create uniform buffers 
	CreateUniformBuffer(frameCount);

	// 2. bind resources to heaps
	mSrvDescriptorHeapHandles.resize(frameCount);
	mUavDescriptorHeapHandles.resize(frameCount);
	for (int i = 0; i < frameCount; i++)
	{
		AllocateDescriptorsForShaderResources(
			cbvSrvUavDescriptorHeap,
			mSrvDescriptorHeapHandles[i]);

		AllocateDescriptorsForWriteTargets(
			cbvSrvUavDescriptorHeap,
			mUavDescriptorHeapHandles[i]);
	}

	// 3. create root signature
	// ordered in scene/frame/pass/object
	// TODO: add support for buffer, write buffer, write texture to all spaces
	int srvCounts[(int)RegisterSpace::COUNT] = { mScene->GetTextureCount(), mRenderer->GetMaxFrameTextureCount(), mShaderResources.size(), GetMaxMeshTextureCount() };
	int uavCounts[(int)RegisterSpace::COUNT] = { 0, 0, mWriteTargets.size(), 0 };
	mRenderer->CreateRootSignature(&mRootSignature, srvCounts, uavCounts);

	// 4. bind render targets
	mRtvHandles.resize(frameCount);
	mDsvHandles.resize(frameCount);
	for (int i = 0; i < frameCount; i++)
	{
		AllocateDescriptorsForShaderTargets(
			rtvDescriptorHeap,
			dsvDescriptorHeap,
			mShaderTargetState,
			mRtvHandles[i],
			mDsvHandles[i]);
	}

	// 5. create pso
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
	// null means using swap chain
	return !mRenderTexture || mRenderTexture->IsRenderTargetUsed();
}

bool ShaderTarget::IsDepthStencilUsed()
{
	// null means using swap chain
	return !mRenderTexture || mRenderTexture->IsDepthStencilUsed();
}

u32 ShaderTarget::GetHeight()
{
	return mRenderTexture ? mRenderTexture->GetHeight(mMipSlice) : gRenderer.mHeight;
}

u32 ShaderTarget::GetWidth()
{
	return mRenderTexture ? mRenderTexture->GetWidth(mMipSlice) : gRenderer.mWidth;
}
