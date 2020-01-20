#pragma once

#include "GlobalInclude.h"
#include "Scene.h"
#include "Frame.h"

class Level;

class DescriptorHeap
{
public:
	enum Type { INVALID, SRV_CBV_UAV, SAMPLER, RTV, DSV, COUNT };

	DescriptorHeap();
	~DescriptorHeap();

	void InitDescriptorHeap(Renderer* renderer, int countMax, Type type);
	void Release();
	CD3DX12_CPU_DESCRIPTOR_HANDLE AllocateDsv(ID3D12Resource* resource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc, int count);
	CD3DX12_CPU_DESCRIPTOR_HANDLE AllocateRtv(ID3D12Resource* resource, const D3D12_RENDER_TARGET_VIEW_DESC& desc, int count);
	CD3DX12_CPU_DESCRIPTOR_HANDLE AllocateRtv(ID3D12Resource* resource, int count); // null desc
	CD3DX12_CPU_DESCRIPTOR_HANDLE AllocateSrv(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc, int count);
	CD3DX12_CPU_DESCRIPTOR_HANDLE AllocateSampler(const D3D12_SAMPLER_DESC& desc, int count);
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetCurrentFreeGpuAddress();

	inline Type GetType();
	inline ID3D12DescriptorHeap*& GetHeapRef();
	inline ID3D12DescriptorHeap* GetHeap();
private:

	Renderer* mRenderer;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mHeadGpu; // fixed
	CD3DX12_CPU_DESCRIPTOR_HANDLE mHead; // fixed
	CD3DX12_CPU_DESCRIPTOR_HANDLE mFree; // can grow
	int mHandleIncrementSize;
	ID3D12DescriptorHeap* mHeap;
	int mCount; // current count
	int mCountMax; // max count
	Type mType;
};

class Renderer
{
public:
	Renderer();
	~Renderer();

	static inline D3D12_DEPTH_STENCIL_DESC NoDepthStencilTest();
	static inline D3D12_BLEND_DESC AdditiveBlend();
	static inline D3D12_BLEND_DESC NoBlend();
	static inline DXGI_FORMAT TranslateFormat(Format format);
	static inline D3D12_DESCRIPTOR_HEAP_TYPE TranslateDescriptorHeapType(DescriptorHeap::Type type);
	static inline D3D12_FILTER ExtractFilter(Sampler sampler);
	static inline D3D12_TEXTURE_ADDRESS_MODE TranslateAddressMode(Sampler::AddressMode addressMode);
	static inline D3D12_COMPARISON_FUNC TranslateCompareOp(CompareOp compareOp);
	static inline D3D12_RESOURCE_STATES TranslateTextureLayout(TextureLayout textureLayout);

	IDXGISwapChain3* GetSwapChain();
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtvHandle(int frameIndex);
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsvHandle(int frameIndex);
	ID3D12Resource* GetRenderTargetBuffer(int frameIndex);
	ID3D12Resource* GetDepthStencilBuffer(int frameIndex);
	vector<Frame>& GetFrames();
	Frame& GetFrame(int frameIndex);
	int GetMaxFrameTextureCount();

	// TODO: this is the only Init function that returns a boolean result, we assume other Init function always work
	bool InitRenderer(HWND hwnd, int frameCount, int msaaCount, int width, int height);
	void CreateDepthStencilBuffers();
	void CreateRenderTargetBuffers();
	void Release();

	void RecordBegin(int frameIndex, ID3D12GraphicsCommandList* commandList);
	void RecordEnd(int frameIndex, ID3D12GraphicsCommandList* commandList);
	void UploadTextureDataToBuffer(void* srcData, int srcBytePerRow, int srcBytePerSlice, D3D12_RESOURCE_DESC textureDesc, ID3D12Resource* dstBuffer);
	void UploadDataToBuffer(void* srcData, int srcBytePerRow, int srcBytePerSlice, int copyBufferSize, ID3D12Resource* dstBuffer);
	
	void CreateDescriptorHeap(DescriptorHeap& srvDescriptorHeap, int size);
	void CreateGraphicsRootSignature(
		ID3D12RootSignature** rootSignature,
		int maxTextureCount[UNIFORM_REGISTER_SPACE::COUNT]);
	void CreatePSO(
		ID3D12Device* device,
		ID3D12PipelineState** pso,
		ID3D12RootSignature* rootSignature,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTType,
		D3D12_BLEND_DESC blendDesc,
		D3D12_DEPTH_STENCIL_DESC dsDesc,
		const vector<DXGI_FORMAT>& rtvFormat,
		DXGI_FORMAT dsvFormat,
		Shader* vertexShader,
		Shader* hullShader,
		Shader* domainShader,
		Shader* geometryShader,
		Shader* pixelShader,
		const wstring& name);
	void RecordPass(
		ID3D12GraphicsCommandList* commandList,
		Pass* pass,
		bool clearColor = true,
		bool clearDepth = true,
		XMFLOAT4 clearColorValue = XMFLOAT4(0, 0, 0, 0),
		float clearDepthValue = 1.0f);

	IDXGIFactory4* mDxgiFactory;
	ID3D12Device* mDevice; // direct3d device
	IDXGISwapChain3* mSwapChain; // swapchain used to switch between framebuffers
	ID3D12CommandQueue* mGraphicsCommandQueue; // TODO: add support to transfer queue
#ifdef MY_DEBUG
	ID3D12Debug* debugController;
	bool gEnableDebugLayer = false;
	bool gEnableShaderBasedValidation = false;
#endif
	vector<ID3D12CommandAllocator*> mGraphicsCommandAllocators;
	vector<ID3D12GraphicsCommandList*> mCommandLists;
	ID3D12CommandAllocator* mCopyCommandAllocator;
	int mWidth;
	int mHeight;

	vector<ID3D12Fence*> mFences;
	HANDLE mFenceEvent; // a handle to an event when our fence is unlocked by the gpu
	vector<UINT64> mFenceValues;
	int mCurrentFrameIndex;
	int mFrameCount;
	int mMultiSampleCount;

private:
	ID3D12GraphicsCommandList* BeginSingleTimeCommands(ID3D12CommandAllocator* commandAllocator);
	void EndSingleTimeCommands(ID3D12CommandQueue* commandQueue, ID3D12GraphicsCommandList* commandList);
	
	DescriptorHeap mCbvSrvUavDescriptorHeap;
	DescriptorHeap mSamplerDescriptorHeap;
	DescriptorHeap mRtvDescriptorHeap;
	DescriptorHeap mDsvDescriptorHeap;

	Level* mLevel;
	vector<Frame> mFrames;
	vector<ID3D12Resource*> mRenderTargetBuffers;
	vector<ID3D12Resource*> mDepthStencilBuffers;
	vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> mDsvHandles;
	vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> mRtvHandles;
};
