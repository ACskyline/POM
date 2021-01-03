#pragma once

#include "GlobalInclude.h"

class Pass;
class Texture;
class Light;
class DescriptorHeap;

const uint32_t MAX_LIGHTS_PER_SCENE = 10;

struct LightData {
	XMFLOAT4X4 view;
	XMFLOAT4X4 viewInv;
	XMFLOAT4X4 proj;
	XMFLOAT4X4 projInv;
	XMFLOAT3 color = { 0, 0, 0 };
	float nearClipPlane = 0.0f;
	XMFLOAT3 position = { 0, 0, 0 };
	float farClipPlane = 0.0f;
	int32_t textureIndex = -1;
	uint32_t PADDING0;
	uint32_t PADDING1;
	uint32_t PADDING2;
};

class Scene
{
public:
	struct SceneUniform
	{
		uint32_t mode;
		uint32_t pomMarchStep;
		float pomScale;
		float pomBias;
		//
		float skyScatterG;
		uint32_t skyMarchStep;
		uint32_t skyMarchStepTr;
		float sunAzimuth; // horizontal
		//
		float sunAltitude; // vertical
		XMFLOAT3 sunRadiance;
		//
		uint32_t lightCount;
		uint32_t lightDebugOffset;
		uint32_t lightDebugCount;
		float fresnel;
		//
		XMFLOAT4 standardColor;
		//
		float roughness;
		float useStandardTextures;
		float metallic;
		float specularity;
		//
		uint32_t sampleNumIBL;
		uint32_t showReferenceIBL;
		uint32_t useSceneLight;
		uint32_t useSunLight;
		//
		uint32_t useIBL;
		uint32_t prefilteredEnvMapLevelCount;
		uint32_t pathTracerMode;
		uint32_t PADDING_1;
		//
		LightData lights[MAX_LIGHTS_PER_SCENE];
	};

	Scene(const wstring& debugName = L"unnamed scene");
	~Scene();
	void AddPass(Pass* pass);
	void AddTexture(Texture* texture);
	void AddLight(Light* light);
	
	vector<Pass*>& GetPasses();
	int GetTextureCount();
	int GetLightCount();
	D3D12_GPU_VIRTUAL_ADDRESS GetUniformBufferGpuAddress(int frameIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE GetCbvSrvUavDescriptorHeapTableHandle(int frameIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerDescriptorHeapTableHandle(int frameIndex);

	void InitScene(
		Renderer* renderer, 
		int frameCount,
		DescriptorHeap& cbvSrvUavDescriptorHeap,
		DescriptorHeap& samplerDescriptorHeap);
	void CreateUniformBuffer(int frameCount);
	void UpdateUniformBuffer(int frameIndex);
	void Release(bool checkOnly = false);
	
	SceneUniform mSceneUniform;

private:
	wstring mDebugName;
	Renderer* mRenderer;
	vector<Pass*> mPasses;
	vector<Texture*> mTextures;
	vector<Light*> mLights;

	// one for each frame
	// TODO: hide API specific implementation in Renderer
	vector<ID3D12Resource*> mUniformBuffers;
	vector<ID3D12Resource*> mLightBuffers;
	vector<D3D12_GPU_DESCRIPTOR_HANDLE> mCbvSrvUavDescriptorHeapTableHandles;
	vector<D3D12_GPU_DESCRIPTOR_HANDLE> mSamplerDescriptorHeapTableHandles;
};
