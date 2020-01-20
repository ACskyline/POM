#include "Renderer.h"
#include "Pass.h"

Pass::Pass(const wstring& debugName) : mRenderer(nullptr), mDebugName(debugName)
{
	for(int i = 0;i<Shader::ShaderType::COUNT;i++)
	{
		mShaders[i] = nullptr;
	}
}

Pass::~Pass()
{
	Release();
}

void Pass::SetCamera(Camera* camera)
{
	mCamera = camera;
}

void Pass::AddMesh(Mesh* mesh)
{
	mMeshes.push_back(mesh);
}

void Pass::AddTexture(Texture* texture)
{
	mTextures.push_back(texture);
}

void Pass::AddRenderTexture(RenderTexture* renderTexture)
{
	mRenderTextures.push_back(renderTexture);
}

void Pass::AddShader(Shader* shader)
{
	mShaders[shader->GetShaderType()] = shader;
}

void Pass::CreateUniformBuffer(int frameCount)
{
	mUniformBuffers.resize(frameCount);
	for (int i = 0; i < frameCount; i++)
	{
		HRESULT hr = mRenderer->mDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // this heap will be used to upload the constant buffer data
			D3D12_HEAP_FLAG_NONE, // no flags
			&CD3DX12_RESOURCE_DESC::Buffer(sizeof(PassUniform)), // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
			D3D12_RESOURCE_STATE_GENERIC_READ, // will be data that is read from so we keep it in the generic read state
			nullptr, // we do not have use an optimized clear value for constant buffers
			IID_PPV_ARGS(&mUniformBuffers[i]));

		fatalAssertf(SUCCEEDED(hr), "create pass uniform buffer failed");

		mUniformBuffers[i]->SetName(L"pass uniform buffer " + i);
	}
}

void Pass::UpdateUniformBuffer(int frameIndex, Camera* camera)
{
	if (camera == nullptr)
		camera = mCamera;
	camera->Update();
	mPassUniform.mPassIndex = 1;//TODO: create a class static counter so that we can save an unique id for each frame
	mPassUniform.mCameraPosX = camera->GetPosition().x;
	mPassUniform.mCameraPosY = camera->GetPosition().y;
	mPassUniform.mCameraPosZ = camera->GetPosition().z;
	mPassUniform.mViewProj = camera->GetViewProjMatrix();
	mPassUniform.mViewProjInv = camera->GetViewProjInvMatrix();

	CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU. (so end is less than or equal to begin)
	void* cpuAddress;
	mUniformBuffers[frameIndex]->Map(0, &readRange, &cpuAddress);
	memcpy(cpuAddress, &mPassUniform, sizeof(mPassUniform));
	mUniformBuffers[frameIndex]->Unmap(0, &readRange);
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

void Pass::InitPass(
	Renderer* renderer,
	int frameCount,
	DescriptorHeap& cbvSrvUavDescriptorHeap,
	DescriptorHeap& samplerDescriptorHeap,
	DescriptorHeap& rtvDescriptorHeap,
	DescriptorHeap& dsvDescriptorHeap)
{
	mRenderer = renderer;

	// 1. create uniform buffers 
	CreateUniformBuffer(frameCount);
	for (int i = 0; i < frameCount; i++)
		UpdateUniformBuffer(i);

	// 2. bind textures to heaps
	mCbvSrvUavDescriptorHeapTableHandles.resize(frameCount);
	mSamplerDescriptorHeapTableHandles.resize(frameCount);
	for (int i = 0; i < frameCount; i++)
	{
		// pass texture table
		mCbvSrvUavDescriptorHeapTableHandles[i] = cbvSrvUavDescriptorHeap.GetCurrentFreeGpuAddress();
		for (auto texture : mTextures)
			cbvSrvUavDescriptorHeap.AllocateSrv(texture->GetTextureBuffer(), texture->GetSrvDesc(), 1);

		// pass sampler table
		mSamplerDescriptorHeapTableHandles[i] = samplerDescriptorHeap.GetCurrentFreeGpuAddress();
		for (auto texture : mTextures)
			samplerDescriptorHeap.AllocateSampler(texture->GetSamplerDesc(), 1);
	}

	// 3. create root signature
	int maxTextureCount[UNIFORM_REGISTER_SPACE::COUNT] = { mScene->GetTextureCount(), mRenderer->GetMaxFrameTextureCount(), mTextures.size(), GetMaxMeshTextureCount() };
	mRenderer->CreateGraphicsRootSignature(&mRootSignature, maxTextureCount);

	// 4. bind render textures and create pso
	mRtvHandles.resize(frameCount);
	mDsvHandles.resize(frameCount);
	D3D12_BLEND_DESC blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // TODO: alpha to coverage is disabled by default, add it as a member variable to pass class if needed in the future
	if (mRenderTextures.size() > 1)
		blendDesc.IndependentBlendEnable = true; // independent blend is disabled by default, turn it on when we have more than 1 render targets, add it as a member variable to pass class
	vector<DXGI_FORMAT> rtvFormatVec(mRenderTextures.size());
	for(int i = 0; i < mRenderTextures.size(); i++)
	{
		blendDesc.RenderTarget[i] = mRenderTextures[i]->GetRenderTargetBlendDesc();
		rtvFormatVec[i] = Renderer::TranslateFormat(mRenderTextures[i]->GetFormat());
		for (int j = 0; j < frameCount; j++)
		{
			mRtvHandles[j].push_back(rtvDescriptorHeap.AllocateRtv(mRenderTextures[i]->GetTextureBuffer(), mRenderTextures[i]->GetRtvDesc(), 1));
			mDsvHandles[j].push_back(dsvDescriptorHeap.AllocateDsv(mRenderTextures[i]->GetDepthStencilBuffer(), mRenderTextures[i]->GetDsvDesc(), 1));
		}
	}

	mRenderer->CreatePSO(
		mRenderer->mDevice,
		&mPso,
		mRootSignature,
		mPrimitiveType,
		blendDesc,
		mRenderTextures[0]->GetDepthStencilDesc(),
		rtvFormatVec,
		Renderer::TranslateFormat(mRenderTextures[0]->GetDepthStencilFormat()),
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

int Pass::GetRenderTextureCount()
{
	return mRenderTextures.size();
}

vector<Texture*>& Pass::GetTextureVec()
{
	return mTextures;
}

vector<RenderTexture*>& Pass::GetRenderTextureVec()
{
	return mRenderTextures;
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