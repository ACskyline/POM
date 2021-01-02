#pragma once

#include "GlobalInclude.h"

class Store;
class Frame;
class Pass;
class Shader;
class Texture;
class RenderTexture;
class Mesh;
class Camera;

enum ReadFrom : u8 { INVALID = 0x00, COLOR = 0x01, DEPTH = 0x02, STENCIL = 0x04, MS = 0x10, COLOR_MS = 0x11, DEPTH_MS = 0x12, STENCIL_MS = 0x14 };
enum class CompareOp { INVALID, NEVER, LESS, EQUAL, LESS_EQUAL, GREATER, NOT_EQUAL, GREATER_EQUAL, ALWAYS, MINIMUM, MAXIMUM, COUNT };
enum class DebugMode { OFF, ON, GBV, COUNT }; // GBV = GPU based validation
enum class RegisterSpace { SCENE, FRAME, PASS, OBJECT, COUNT }; // maps to layout number in Vulkan
enum class RegisterType { CBV, SRV, SAMPLER, UAV, COUNT };
enum class TextureType { INVALID, DEFAULT, CUBE, VOLUME, COUNT };

typedef ID3D12Resource Resource;

int GetUniformSlot(RegisterSpace space, RegisterType type);

const D3D12_INPUT_ELEMENT_DESC VertexInputLayout[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
};

struct Vertex {
	XMFLOAT3 pos;
	XMFLOAT2 uv;
	XMFLOAT3 nor;
	XMFLOAT4 tan;
};

enum class ResourceLayout {
	INVALID,
	SHADER_READ,
	RENDER_TARGET,
	// D3D12 doesn't seem to support different access qualifiers for depth and stencil channel 
	// unlike Vulkan's VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL and etc.
	DEPTH_STENCIL_READ,
	DEPTH_STENCIL_WRITE,
	COPY_SRC,
	COPY_DST,
	RESOLVE_SRC,
	RESOLVE_DST,
	PRESENT,
	UPLOAD, // D3D12_RESOURCE_STATE_GENERIC_READ is a logically OR'd combination of other read-state bits. This is the required starting state for an upload heap.
	COUNT
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

	static BlendState NoBlend()
	{
		return BlendState{
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

	static DepthStencilState None()
	{
		return DepthStencilState{
			false,
			false,
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

	static DepthStencilState Less()
	{
		return DepthStencilState {
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

	static DepthStencilState Greater()
	{
		return DepthStencilState{
			true,
			true,
			false,
			false,
			CompareOp::GREATER,
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

	static DepthStencilState LessEqual()
	{
		return DepthStencilState{
			true,
			true,
			false,
			false,
			CompareOp::LESS_EQUAL,
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

	static DepthStencilState GreaterEqual()
	{
		return DepthStencilState{
			true,
			true,
			false,
			false,
			CompareOp::GREATER_EQUAL,
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

	static DepthStencilState LessEqualNoWrite()
	{
		return DepthStencilState{
			true,
			false,
			false,
			false,
			CompareOp::LESS_EQUAL,
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

	static DepthStencilState GreaterEqualNoWrite()
	{
		return DepthStencilState{
			true,
			false,
			false,
			false,
			CompareOp::GREATER_EQUAL,
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
	enum class Type { INVALID, SRV, RTV, DSV, UAV, COUNT };
	Type mType;

	union {
		D3D12_SHADER_RESOURCE_VIEW_DESC mSrvDesc; // TODO: hide API specific implementation in Renderer
		D3D12_RENDER_TARGET_VIEW_DESC mRtvDesc; // TODO: hide API specific implementation in Renderer
		D3D12_DEPTH_STENCIL_VIEW_DESC mDsvDesc; // TODO: hide API specific implementation in Renderer
		D3D12_UNORDERED_ACCESS_VIEW_DESC mUavDesc;
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

	D3D12_SAMPLER_DESC GetDesc();
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

class ShaderResource
{
public:
	enum class Type { INVALID, TEXTURE, BUFFER, COUNT };
	bool IsTexture() { return mShaderResourceType == Type::TEXTURE; }
	bool IsBuffer() { return mShaderResourceType == Type::BUFFER; }

protected:
	wstring mDebugName;
	bool mInitialized;
	Type mShaderResourceType;

	ShaderResource(wstring debugName, Type srType) : mDebugName(debugName), mInitialized(false), mShaderResourceType(srType) {}
	virtual ~ShaderResource() {}
};

class DescriptorHeap
{
public:
	enum class Type { INVALID, SRV_CBV_UAV, SAMPLER, RTV, DSV, COUNT };

	DescriptorHeap();
	~DescriptorHeap();

	void InitDescriptorHeap(Renderer* renderer, int countMax, Type type);
	void Release(bool checkOnly = false);
	CD3DX12_CPU_DESCRIPTOR_HANDLE AllocateDsv(ID3D12Resource* resource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc, int count);
	CD3DX12_CPU_DESCRIPTOR_HANDLE AllocateRtv(ID3D12Resource* resource, const D3D12_RENDER_TARGET_VIEW_DESC& desc, int count);
	CD3DX12_CPU_DESCRIPTOR_HANDLE AllocateRtv(ID3D12Resource* resource, int count); // null desc
	CD3DX12_CPU_DESCRIPTOR_HANDLE AllocateSrv(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc, int count);
	CD3DX12_CPU_DESCRIPTOR_HANDLE AllocateUav(ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc, int count);
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

#define DS_REVERSED_Z_SWITCH REVERSED_Z_SWITCH(DepthStencilState::Greater(), DepthStencilState::Less())
#define DS_EQUAL_REVERSED_Z_SWITCH REVERSED_Z_SWITCH(DepthStencilState::GreaterEqual(), DepthStencilState::LessEqual())
#define DS_EQUAL_NO_WRITE_REVERSED_Z_SWITCH REVERSED_Z_SWITCH(DepthStencilState::GreaterEqualNoWrite(), DepthStencilState::LessEqualNoWrite())
#define DEPTH_FARTHEST_REVERSED_Z_SWITCH REVERSED_Z_SWITCH(0.0f, 1.0f)

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
	static DXGI_FORMAT TranslateToTypelessFormat(Format format);
	static DXGI_FORMAT TranslateFormatToAccess(Format format, ReadFrom readFrom);
	static D3D12_DESCRIPTOR_HEAP_TYPE TranslateDescriptorHeapType(DescriptorHeap::Type type);
	static D3D12_FILTER ExtractFilter(Sampler sampler);
	static D3D12_TEXTURE_ADDRESS_MODE TranslateAddressMode(Sampler::AddressMode addressMode);
	static D3D12_RESOURCE_STATES TranslateResourceLayout(ResourceLayout resourceLayout);
	static D3D12_VIEWPORT TranslateViewport(Viewport viewport);
	static D3D12_RECT TranslateScissorRect(ScissorRect rect);
	static D3D12_RESOURCE_DIMENSION GetResourceDimensionFromTextureType(TextureType type);
	static D3D12_SAMPLER_DESC TranslateSamplerDesc(Sampler sampler);

	IDXGISwapChain3* GetSwapChain();
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtvHandle(int frameIndex);
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsvHandle(int frameIndex);
	ID3D12Resource* GetRenderTargetBuffer(int frameIndex);
	ID3D12Resource* GetDepthStencilBuffer(int frameIndex);
	ID3D12Resource* GetPreResolveBuffer(int frameIndex);
	vector<Frame>& GetFrames();
	Frame& GetFrame(int frameIndex);
	int GetMaxFrameTextureCount();
	bool IsMultiSampleUsed();
	bool IsResolveNeeded();

	void SetStore(Store* level);

	// TODO: this is the only Init function that returns a boolean result, we assume other Init function always work
	bool InitRenderer(
		HWND hwnd,
		int frameCount,
		int multiSampleCount,
		int width,
		int height,
		Format colorBufferFormat = Format::R8G8B8A8_UNORM,
		Format depthStencilBufferFormat = Format::D24_UNORM_S8_UINT,
		DebugMode debugMode = DebugMode::OFF,
		BlendState blendState = BlendState::NoBlend(),
		DepthStencilState depthStencilState = DS_REVERSED_Z_SWITCH);
	void CreateDepthStencilBuffers(Format format);
	void CreatePreResolveBuffers(Format format);
	void CreateColorBuffers();
	void Release(bool checkOnly = false);
	void WaitAllFrames();
	bool WaitForFrame(int frameINdex);

	// explicit on command list for future multi thread support
	bool RecordBegin(int frameIndex, ID3D12GraphicsCommandList* commandList);
	bool ResolveFrame(int frameIndex, ID3D12GraphicsCommandList* commandList);
	bool RecordEnd(int frameIndex, ID3D12GraphicsCommandList* commandList);
	bool Submit(int frameIndex, ID3D12CommandQueue* commandQueue, vector<ID3D12GraphicsCommandList*> commandLists);
	bool Present();
	void UploadTextureDataToBuffer(vector<void*>& srcData, vector<int>& srcBytePerRow, vector<int>& srcBytePerSlice, D3D12_RESOURCE_DESC textureDesc, ID3D12Resource* dstBuffer);
	void UploadDataToBuffer(void* srcData, int srcBytePerRow, int srcBytePerSlice, int copyBufferSize, ID3D12Resource* dstBuffer);

	u32 CalculateSubresource(u32 depthSlice, u32 mipSlice, u32 depth, u32 mipLevelCount);
	bool TransitionSingleTime(ID3D12Resource* resource, u32 subresource, ResourceLayout oldLayout, ResourceLayout newLayout);
	bool RecordTransition(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource, u32 subresource, ResourceLayout oldLayout, ResourceLayout newLayout);
	void RecordResolve(ID3D12GraphicsCommandList* commandList, ID3D12Resource* src, u32 srcSubresource, ID3D12Resource* dst, u32 dstSubresource, Format format);

	void CreateDescriptorHeap(DescriptorHeap& srvDescriptorHeap, int size);
	void CreateRootSignature(
		ID3D12RootSignature** rootSignature,
		int maxShaderResourceCount[(int)RegisterSpace::COUNT],
		int maxUavCount[(int)RegisterSpace::COUNT]);
	void CreateGraphicsPSO(
		Pass& pass,
		ID3D12PipelineState** pso,
		ID3D12RootSignature* rootSignature,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveType,
		Shader* vertexShader,
		Shader* hullShader,
		Shader* domainShader,
		Shader* geometryShader,
		Shader* pixelShader,
		const wstring& name);
	void CreateComputePSO(
		Pass& pass,
		ID3D12PipelineState** pso,
		ID3D12RootSignature* rootSignature,
		Shader* computeShader,
		const wstring& name);
	// explicit on command list for future multi thread support
	void RecordPass(
		Pass& pass,
		ID3D12GraphicsCommandList* commandList,
		bool clearColor = true,
		bool clearDepth = true,
		bool clearStencil = true,
		XMFLOAT4 clearColorValue = XMFLOAT4(0, 0, 0, 0),
		float clearDepthValue = REVERSED_Z_SWITCH(0.0f, 1.0f),
		uint8_t clearStencilValue = 0);
	void RecordComputePass(
		Pass& pass,
		ID3D12GraphicsCommandList* commandList,
		int threadGroupCountX,
		int threadGroupCountY,
		int threadGroupCountZ);
	ID3D12GraphicsCommandList* BeginSingleTimeCommands();
	void EndSingleTimeCommands(ID3D12GraphicsCommandList* commandList);

	// TODO: hide unnecessary member variables
	IDXGIFactory4* mDxgiFactory;
	ID3D12Device* mDevice; // direct3d device
	ID3D12Debug* mDebugController;
	IDXGISwapChain3* mSwapChain; // swapchain used to switch between framebuffers
	ID3D12CommandQueue* mGraphicsCommandQueue; // TODO: add support to transfer queue
	vector<ID3D12CommandAllocator*> mGraphicsCommandAllocators; // size of mFrameCount
	vector<ID3D12GraphicsCommandList*> mCommandLists; // arbitrary size, this way we can have more than 1 command list per frame
	ID3D12CommandAllocator* mSingleTimeCommandAllocator;
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

	bool TransitionLayout(ID3D12Resource* resource, u32 subresource, ResourceLayout oldLayout, ResourceLayout newLayout, CD3DX12_RESOURCE_BARRIER& barrier);
	
	void CreateGraphicsPSOInternal(
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
	void CreateComputePSOInternal(
		ID3D12Device* device,
		ID3D12PipelineState** pso,
		ID3D12RootSignature* rootSignature,
		Shader* computeShader,
		const wstring& name);

	DescriptorHeap mCbvSrvUavDescriptorHeap;
	DescriptorHeap mSamplerDescriptorHeap;
	DescriptorHeap mRtvDescriptorHeap;
	DescriptorHeap mDsvDescriptorHeap;

	Store* mStore;
	vector<ID3D12Resource*> mColorBuffers;
	vector<ID3D12Resource*> mDepthStencilBuffers;
	vector<ID3D12Resource*> mPreResolveColorBuffers;
	vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> mDsvHandles;
	vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> mRtvHandles;
	vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> mPreResolvedRtvHandles;
};

const int gWidth = 960;
const int gHeight = 960;

extern Sampler gSampler;
extern Sampler gSamplerPoint;
extern Shader gDeferredVS;
extern Mesh gCube;
extern Mesh gFullscreenTriangle;
extern Camera gCameraDummy;
