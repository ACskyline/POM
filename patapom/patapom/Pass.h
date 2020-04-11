#pragma once

#include "GlobalInclude.h"
#include "Shader.h"

class Scene;
class Camera;
class Mesh;
class Texture;
class RenderTexture;

class Pass
{
public:
	struct PassUniform
	{	
		uint32_t mPassIndex;
		XMFLOAT3 mCameraPos;
		XMFLOAT4X4 mViewProj;
		XMFLOAT4X4 mViewProjInv;
	};

	Pass(const wstring& debugName = L"unnamed pass");
	~Pass();

	void SetScene(Scene* scene);
	void SetCamera(Camera* camera);
	void AddMesh(Mesh* mesh);
	void AddTexture(Texture* texture);
	void AddRenderTexture(RenderTexture* renderTexture);
	void AddShader(Shader* shader);

	int GetTextureCount();
	int GetRenderTextureCount();
	vector<Texture*>& GetTextureVec();
	vector<RenderTexture*>& GetRenderTextureVec();
	RenderTexture* GetRenderTexture(int i);
	Camera* GetCamera();
	Scene* GetScene();
	vector<Mesh*>& GetMeshVec();
	ID3D12PipelineState* GetPso();
	ID3D12RootSignature* GetRootSignature();
	D3D12_GPU_VIRTUAL_ADDRESS GetUniformBufferGpuAddress(int frameIndex);
	vector<CD3DX12_CPU_DESCRIPTOR_HANDLE>& GetRtvHandles(int frameIndex);
	vector<CD3DX12_CPU_DESCRIPTOR_HANDLE>& GetDsvHandles(int frameIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE GetCbvSrvUavDescriptorHeapTableHandle(int frameIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerDescriptorHeapTableHandle(int frameIndex);
	bool IsConstantBlendFactorsUsed();
	bool IsStencilReferenceUsed();
	float* GetConstantBlendFactors();
	uint32_t GetStencilReference();

	void InitPass(
		Renderer* renderer,
		int frameCount, 
		DescriptorHeap& cbvSrvUavDescriptorHeap,
		DescriptorHeap& samplerDescriptorHeap,
		DescriptorHeap& rtvDescriptorHeap,
		DescriptorHeap& dsvDescriptorHeap);
	void CreateUniformBuffer(int frameCount);
	void UpdateUniformBuffer(int frameIndex, Camera* camera = nullptr);
	void Release();

private:
	PassUniform mPassUniform;
	wstring mDebugName;
	Renderer* mRenderer;
	Scene* mScene;
	Camera* mCamera;
	vector<Mesh*> mMeshes;
	vector<Texture*> mTextures;
	vector<RenderTexture*> mRenderTextures;
	Shader* mShaders[Shader::ShaderType::COUNT];

	// one for each frame
	// TODO: hide API specific implementation in Renderer
	vector<ID3D12Resource*> mUniformBuffers;
	vector<vector<CD3DX12_CPU_DESCRIPTOR_HANDLE>> mRtvHandles; // [frameIndex][renderTextureIndex]
	vector<vector<CD3DX12_CPU_DESCRIPTOR_HANDLE>> mDsvHandles; // [frameIndex][renderTextureIndex]
	vector<D3D12_GPU_DESCRIPTOR_HANDLE> mCbvSrvUavDescriptorHeapTableHandles;
	vector<D3D12_GPU_DESCRIPTOR_HANDLE> mSamplerDescriptorHeapTableHandles;
	ID3D12PipelineState* mPso;
	ID3D12RootSignature* mRootSignature;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE mPrimitiveType;

	// these values are set using OMSet... functions in D3D12, so we cache them here
	bool mConstantBlendFactorsUsed;
	bool mStencilReferenceUsed;

	float mBlendConstants[4];
	uint32_t mStencilReference;

	int GetMaxMeshTextureCount();
};
