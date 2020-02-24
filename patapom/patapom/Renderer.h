#pragma once

#include "GlobalInclude.h"

using namespace DirectX;
using namespace std;

class Level;
class Frame;
class Pass;
class Shader;
class Renderer;
class RenderTexture;

enum class DebugMode { OFF, ON, GBV, COUNT }; // GBV = GPU based validation
enum class UNIFORM_REGISTER_SPACE { SCENE, FRAME, PASS, OBJECT, COUNT }; // maps to layout number in Vulkan
enum class UNIFORM_TYPE { CONSTANT, TEXTURE_TABLE, SAMPLER_TABLE, COUNT };

int GetUniformSlot(UNIFORM_REGISTER_SPACE space, UNIFORM_TYPE type);

const D3D12_INPUT_ELEMENT_DESC VertexInputLayout[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
};

enum class CompareOp { INVALID, NEVER, LESS, EQUAL, LESS_EQUAL, GREATER, NOT_EQUAL, GREATER_EQUAL, ALWAYS, MINIMUM, MAXIMUM, COUNT };

enum class TextureLayout {
	INVALID,
	SHADER_READ,
	RENDER_TARGET,
	// D3D12 doesn't seem to support different access qualifiers for depth and stencil channel 
	// unlike Vulkan's VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL and etc.
	DEPTH_STENCIL_READ,
	DEPTH_STENCIL_WRITE,
	COPY_SRC,
	COPY_DEST,
	RESOLVE_SRC,
	RESOLVE_DEST,
	PRESENT,
	COUNT
};

struct Vertex {
	Vertex() : Vertex(0, 0, 0, 0, 0, 0, 0, 0) {}
	Vertex(float x, float y, float z, float u, float v) : Vertex(x, y, z, u, v, 0, 0, 0) {}
	Vertex(float x, float y, float z, float u, float v, float nx, float ny, float nz) : pos(x, y, z), texCoord(u, v), nor(nx, ny, nz) {}
	XMFLOAT3 pos;
	XMFLOAT2 texCoord;
	XMFLOAT3 nor;
};

struct Viewport {
	float mTopLeftX;
	float mTopLeftY;
	float mWidth;
	float mHeight;
	float mMinDepth;
	float mMaxDepth;
};

struct ScissorRect {
	float mLeft;
	float mTop;
	float mRight;
	float mBottom;
};

struct BlendState {
	enum class BlendFactor {
		INVALID,
		ZERO,
		ONE,
		SRC_COLOR,
		INV_SRC_COLOR,
		SRC_ALPHA,
		INV_SRC_ALPHA,
		DEST_ALPHA,
		INV_DEST_ALPHA,
		DEST_COLOR,
		INV_DEST_COLOR,
		SRC_ALPHA_SAT,
		CONSTANT, // indicating a constant value is needed for blending, D3D12 set this constant using OMSetBlendFactor, Vulkan put it in VkPipelineColorBlendStateCreateInfo 
		INV_CONSTANT, // same as above
		SRC1_COLOR,
		INV_SRC1_COLOR,
		SRC1_ALPHA,
		INV_SRC1_ALPHA,
		COUNT
	};
	enum class BlendOp { INVALID, ADD, SUBTRACT, REV_SUBTRACT, MIN, MAX, COUNT };
	enum class LogicOp { INVALID, CLEAR, SET, COPY, COPY_INVERTED, NOOP, INVERT, AND, NAND, OR, NOR, XOR, EQUIV, AND_REVERSE, AND_INVERTED, OR_REVERSE, OR_INVERTED, COUNT };
	enum WriteMask : uint8_t { INVALID, RED = 0x01, GREEN = 0x02, BLUE = 0x04, ALPHA = 0x08 };

	bool mBlendEnable;
	bool mLogicOpEnable;
	BlendFactor mSrcBlend;
	BlendFactor mDestBlend;
	BlendOp mBlendOp;
	BlendFactor mSrcBlendAlpha;
	BlendFactor mDestBlendAlpha;
	BlendOp mBlendOpAlpha;
	LogicOp mLogicOp;
	uint8_t mWriteMask;
	float mBlendConstants[4]; // D3D12 set this constant using OMSetBlendFactor, Vulkan put it in VkPipelineColorBlendStateCreateInfo 

	D3D12_RENDER_TARGET_BLEND_DESC mImpl; // TODO: hide API specific implementation in Renderer
	static BlendState Default()
	{
		return {
			false,
				false,
				BlendState::BlendFactor::ZERO,
				BlendState::BlendFactor::ZERO,
				BlendState::BlendOp::ADD,
				BlendState::BlendFactor::ZERO,
				BlendState::BlendFactor::ZERO,
				BlendState::BlendOp::ADD,
				BlendState::LogicOp::NOOP,
				BlendState::WriteMask::RED | BlendState::WriteMask::GREEN | BlendState::WriteMask::BLUE | BlendState::WriteMask::ALPHA
		};
	}
};

struct DepthStencilState {
	enum class StencilOp { INVALID, KEEP, ZERO, REPLACE, INCR_SAT, DECR_SAT, INVERT, INCR, DECR, COUNT };
	struct StencilOpSet {
		StencilOp mFailOp;
		StencilOp mPassOp;
		StencilOp mDepthFailOp;
		CompareOp mStencilCompareOp;
	};
	bool mDepthTestEnable;
	bool mDepthWriteEnable;
	bool mStencilTestEnable; // stencil write depends on the stencil op
	bool mDepthBoundsTestEnable; // depth bound test
	CompareOp mDepthCompareOp;
	StencilOpSet mFrontStencilOpSet;
	StencilOpSet mBackStencilOpSet;
	uint8_t mStencilReadMask;
	uint8_t mStencilWriteMask;
	uint32_t mStencilReference; // D3D12 set this reference using OMSetStencilRef, Vulkan put it in VkStencilOpState  
	float mDepthBoundMin; // D3D12 set this bound using OMSetDepthBounds, Vulkan put it in VkPipelineDepthStencilStateCreateInfo 
	float mDepthBoundMax; // same as above

	D3D12_DEPTH_STENCIL_DESC mImpl; // TODO: hide API specific implementation in Renderer
	static DepthStencilState Default()
	{
		return {
			true,
			true,
			false,
			false,
			CompareOp::LESS,
			DepthStencilState::StencilOpSet {
				DepthStencilState::StencilOp::KEEP,
				DepthStencilState::StencilOp::KEEP,
				DepthStencilState::StencilOp::KEEP,
				CompareOp::NEVER },
			DepthStencilState::StencilOpSet {
				DepthStencilState::StencilOp::KEEP,
				DepthStencilState::StencilOp::KEEP,
				DepthStencilState::StencilOp::KEEP,
				CompareOp::NEVER }
		};
	}
};

struct View {
	enum class Type { INVALID, SRV, RTV, DSV, COUNT };
	Type mType;

	union {
		D3D12_SHADER_RESOURCE_VIEW_DESC mSrvDesc; // TODO: hide API specific implementation in Renderer
		D3D12_RENDER_TARGET_VIEW_DESC mRtvDesc; // TODO: hide API specific implementation in Renderer
		D3D12_DEPTH_STENCIL_VIEW_DESC mDsvDesc; // TODO: hide API specific implementation in Renderer
	};
};

struct Sampler {
	enum class Filter { INVALID, POINT, LINEAR, COUNT };
	enum class AddressMode { INVALID, WRAP, MIRROR, CLAMP, BORDER, MIRROR_ONCE, COUNT };

	Filter mMinFilter;
	Filter mMagFilter;
	Filter mMipFilter;
	AddressMode mAddressModeU;
	AddressMode mAddressModeV;
	AddressMode mAddressModeW;
	float mMipLodBias;
	bool mUseAnisotropy;
	float mMaxAnisotropy;
	bool mUseCompare;
	CompareOp mCompareOp;
	float mMinLod;
	float mMaxLod;
	float mBorderColor[4];

	D3D12_SAMPLER_DESC mImpl; // TODO: hide API specific implementation in Renderer
};

enum class Format {
	INVALID,
	R8G8B8A8_UNORM,
	R16G16B16A16_UNORM,
	R16G16B16A16_FLOAT,
	D16_UNORM,
	D32_FLOAT,
	D24_UNORM_S8_UINT,
	COUNT
};

class DescriptorHeap
{
public:
	enum class Type { INVALID, SRV_CBV_UAV, SAMPLER, RTV, DSV, COUNT };

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

	Type GetType();
	ID3D12DescriptorHeap*& GetHeapRef();
	ID3D12DescriptorHeap* GetHeap();
private:

	Renderer* mRenderer;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mHeadGpu; // fixed
	CD3DX12_CPU_DESCRIPTOR_HANDLE mHead; // fixed
	CD3DX12_CPU_DESCRIPTOR_HANDLE mFree; // can grow
	int mHandleIncrementSize;
	ID3D12DescriptorHeap* mImpl;
	int mCount; // current count
	int mCountMax; // max count
	Type mType;
};

class Renderer
{
public:
	Renderer();
	~Renderer();

	static D3D12_DEPTH_STENCIL_DESC TranslateDepthStencilState(DepthStencilState state);
	static D3D12_RENDER_TARGET_BLEND_DESC TranslateBlendState(BlendState state);
	static D3D12_BLEND TranslateBlendFactor(BlendState::BlendFactor factor);
	static D3D12_BLEND_OP TranslateBlendOp(BlendState::BlendOp op);
	static D3D12_LOGIC_OP TranslateLogicOp(BlendState::LogicOp op);
	static D3D12_COMPARISON_FUNC TranslateCompareOp(CompareOp op);
	static D3D12_DEPTH_STENCILOP_DESC TranslateStencilOpSet(DepthStencilState::StencilOpSet opSet);
	static D3D12_STENCIL_OP TranslateStencilOp(DepthStencilState::StencilOp op);
	static DXGI_FORMAT TranslateFormat(Format format);
	static D3D12_DESCRIPTOR_HEAP_TYPE TranslateDescriptorHeapType(DescriptorHeap::Type type);
	static D3D12_FILTER ExtractFilter(Sampler sampler);
	static D3D12_TEXTURE_ADDRESS_MODE TranslateAddressMode(Sampler::AddressMode addressMode);
	static D3D12_RESOURCE_STATES TranslateTextureLayout(TextureLayout textureLayout);
	static D3D12_VIEWPORT TranslateViewport(Viewport viewport);
	static D3D12_RECT TranslateScissorRect(ScissorRect rect);

	IDXGISwapChain3* GetSwapChain();
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtvHandle(int frameIndex);
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsvHandle(int frameIndex);
	ID3D12Resource* GetRenderTargetBuffer(int frameIndex);
	ID3D12Resource* GetDepthStencilBuffer(int frameIndex);
	vector<Frame>& GetFrames();
	Frame& GetFrame(int frameIndex);
	int GetMaxFrameTextureCount();

	void SetLevel(Level* level);

	// TODO: this is the only Init function that returns a boolean result, we assume other Init function always work
	bool InitRenderer(
		HWND hwnd,
		int frameCount,
		int multiSampleCount,
		int width,
		int height,
		DebugMode debugMode = DebugMode::OFF,
		BlendState blendState = BlendState::Default(),
		DepthStencilState depthStencilState = DepthStencilState::Default(),
		Format colorBufferFormat = Format::R8G8B8A8_UNORM,
		Format depthBufferStencilFormat = Format::D24_UNORM_S8_UINT);
	void CreateDepthStencilBuffers(Format format);
	void CreateColorBuffers();
	void Release();
	void WaitAllFrames();
	bool WaitForFrame(int frameINdex);

	bool RecordBegin(int frameIndex, ID3D12GraphicsCommandList* commandList);
	bool RecordEnd(int frameIndex, ID3D12GraphicsCommandList* commandList);
	bool SubmitCommands(int frameIndex, ID3D12CommandQueue* commandQueue, vector<ID3D12GraphicsCommandList*> commandLists);
	bool Present();
	void UploadTextureDataToBuffer(vector<void*>& srcData, vector<int>& srcBytePerRow, vector<int>& srcBytePerSlice, D3D12_RESOURCE_DESC textureDesc, ID3D12Resource* dstBuffer);
	void UploadDataToBuffer(void* srcData, int srcBytePerRow, int srcBytePerSlice, int copyBufferSize, ID3D12Resource* dstBuffer);

	bool Transition(ID3D12Resource* resource, TextureLayout oldLayout, TextureLayout newLayout);
	bool RecordTransition(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource, TextureLayout& oldLayout, TextureLayout newLayout);
	bool CacheTransition(vector<CD3DX12_RESOURCE_BARRIER>& transitions, ID3D12Resource* resource, TextureLayout& oldLayout, TextureLayout newLayout);
	bool RecordCachedTransitions(ID3D12GraphicsCommandList* commandList, vector<CD3DX12_RESOURCE_BARRIER>& cachedTransitions);

	void CreateDescriptorHeap(DescriptorHeap& srvDescriptorHeap, int size);
	void CreateGraphicsRootSignature(
		ID3D12RootSignature** rootSignature,
		int maxTextureCount[(int)UNIFORM_REGISTER_SPACE::COUNT]);
	void CreatePSO(
		vector<RenderTexture*>& renderTextureVec,
		ID3D12PipelineState** pso,
		ID3D12RootSignature* rootSignature,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveType,
		Shader* vertexShader,
		Shader* hullShader,
		Shader* domainShader,
		Shader* geometryShader,
		Shader* pixelShader,
		const wstring& name);
	void RecordPass(
		ID3D12GraphicsCommandList* commandList,
		Pass& pass,
		bool clearColor = true,
		bool clearDepth = true,
		bool clearStencil = true,
		XMFLOAT4 clearColorValue = XMFLOAT4(0, 0, 0, 0),
		float clearDepthValue = 1.0f,
		uint8_t clearStencilValue = 0);

	IDXGIFactory4* mDxgiFactory;
	ID3D12Device* mDevice; // direct3d device
	ID3D12Debug* mDebugController;
	IDXGISwapChain3* mSwapChain; // swapchain used to switch between framebuffers
	ID3D12CommandQueue* mGraphicsCommandQueue; // TODO: add support to transfer queue
	vector<ID3D12CommandAllocator*> mGraphicsCommandAllocators; // size of mFrameCount
	vector<ID3D12GraphicsCommandList*> mCommandLists; // arbitrary size, this way we can have more than 1 command list per frame
	ID3D12CommandAllocator* mCopyCommandAllocator;
	int mWidth;
	int mHeight;
	DebugMode mDebugMode;

	vector<ID3D12Fence*> mFences;
	HANDLE mFenceEvent; // a handle to an event when our fence is unlocked by the gpu
	vector<UINT64> mFenceValues;
	int mCurrentFrameIndex;
	int mFrameCount;
	int mFrameCountTotal;
	int mMultiSampleCount;
	BlendState mBlendState;
	DepthStencilState mDepthStencilState;
	Format mColorBufferFormat;
	Format mDepthStencilBufferFormat;
	vector<Frame> mFrames;
	bool mInitialized;

private:
	int EstimateTotalCbvSrvUavCount();
	int EstimateTotalSamplerCount();
	int EstimateTotalRtvCount();
	int EstimateTotalDsvCount();

	ID3D12GraphicsCommandList* BeginSingleTimeCommands(ID3D12CommandAllocator* commandAllocator);
	void EndSingleTimeCommands(ID3D12CommandQueue* commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12GraphicsCommandList* commandList);
	void EnableDebugLayer();
	void EnableGBV();

	bool TransitionLayout(ID3D12Resource* resource, TextureLayout& oldLayout, TextureLayout newLayout, CD3DX12_RESOURCE_BARRIER& barrier);

	void CreatePSO(
		ID3D12Device* device,
		ID3D12PipelineState** pso,
		ID3D12RootSignature* rootSignature,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveType,
		D3D12_BLEND_DESC blendDesc,
		D3D12_DEPTH_STENCIL_DESC dsDesc,
		const vector<DXGI_FORMAT>& rtvFormat,
		DXGI_FORMAT dsvFormat,
		int multiSampleCount,
		Shader* vertexShader,
		Shader* hullShader,
		Shader* domainShader,
		Shader* geometryShader,
		Shader* pixelShader,
		const wstring& name);

	DescriptorHeap mCbvSrvUavDescriptorHeap;
	DescriptorHeap mSamplerDescriptorHeap;
	DescriptorHeap mRtvDescriptorHeap;
	DescriptorHeap mDsvDescriptorHeap;

	Level* mLevel;
	vector<ID3D12Resource*> mColorBuffers;
	vector<ID3D12Resource*> mDepthStencilBuffers;
	vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> mDsvHandles;
	vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> mRtvHandles;
};
