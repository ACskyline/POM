#pragma once

#include "GlobalInclude.h"
#include "../dependency/pix/pix3.h"

#ifdef USE_PIX
#define GPU_LABEL(cl, ...) PIXScopedEvent(cl.GetImpl(), PIX_COLOR(0xff, 0, 0), __VA_ARGS__)
#define GPU_LABEL_BEGIN(cl, ...) PIXBeginEvent(cl.GetImpl(), PIX_COLOR(0xff, 0, 0), __VA_ARGS__)
#define GPU_LABEL_END(cl) PIXEndEvent(cl.GetImpl())
#define GPU_MARKER(cl, ...) PIXSetMarker(cl.GetImpl(), PIX_COLOR(0, 0xff, 0), __VA_ARGS__)
#else // USE_PIX
#define GPU_LABEL(cl, ...)
#define GPU_LABEL_BEGIN(cl, ...)
#define GPU_LABEL_END(cl)
#define GPU_MARKER(cl, ...)
#endif // USE_PIX

class Store;
class Frame;
class Pass;
class Shader;
class Texture;
class RenderTexture;
class Mesh;
class Camera;
class OrbitCamera;

enum ReadFrom : u8 { INVALID = 0x00, COLOR = 0x01, DEPTH = 0x02, STENCIL = 0x04, MS = 0x10, COLOR_MS = 0x11, DEPTH_MS = 0x12, STENCIL_MS = 0x14 };
enum class CompareOp { INVALID, NEVER, LESS, EQUAL, LESS_EQUAL, GREATER, NOT_EQUAL, GREATER_EQUAL, ALWAYS, MINIMUM, MAXIMUM, COUNT };
enum class DebugMode { OFF, ON, GBV, COUNT }; // GBV = GPU based validation
enum class RegisterSpace { SCENE, FRAME, PASS, OBJECT, COUNT }; // maps to layout number in Vulkan
enum class RegisterType { CBV, SRV, SAMPLER, UAV, COUNT };
enum class TextureType { INVALID, TEX_2D, TEX_CUBE, TEX_VOLUME, COUNT };
enum ResourceDirtyFlag : u8 
{ 
	// uniform data is using root descriptor
	// texture and buffer are using the same heap so they are sharing the same flag
	// ("Only one descriptor heap of each type can be set at one time, which means a maximum of 2 heaps (one sampler, one CBV/SRV/UAV) can be set at one time.")
	ALL_CLEAN									= 0x00, 
	UNIFORM_DIRTY								= 0x01,
	SHADER_RESOURCE_DIRTY						= 0x02,
	SHADER_TARGET_DIRTY							= 0x04,
	WRITE_TARGET_DIRTY							= 0x08,

	UNIFORM_DIRTY_FRAME							= 0x11,
	SHADER_RESOURCE_DIRTY_FRAME					= 0x22,
	SHADER_TARGET_DIRTY_FRAME					= 0x44,
	WRITE_TARGET_DIRTY_FRAME					= 0x88
};

DEFINE_ENUM_BITWISE_OPERATIONS(ResourceDirtyFlag)

#define SET_UNIFORM_VAR(owner, uniform, var, val) \
(owner.uniform.var = val, owner.SetDirty(ResourceDirtyFlag::UNIFORM_DIRTY_FRAME))

typedef ID3D12Resource Resource;

int GetUniformSlot(RegisterSpace space, RegisterType type);

// this has to match the definition of Vertex
const D3D12_INPUT_ELEMENT_DESC VertexInputLayout[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
};

enum class ResourceLayout {
	INVALID,
	SHADER_READ,
	SHADER_WRITE,
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

	static BlendState AdditiveBlend()
	{
		return BlendState{
			true,
			false,
			BlendState::BlendFactor::ONE,
			BlendState::BlendFactor::ONE,
			BlendState::BlendOp::ADD,
			BlendState::BlendFactor::ZERO,
			BlendState::BlendFactor::ZERO,
			BlendState::BlendOp::ADD,
			BlendState::LogicOp::NOOP,
			BlendState::WriteMask::RED | BlendState::WriteMask::GREEN | BlendState::WriteMask::BLUE | BlendState::WriteMask::ALPHA
		};
	}

	static BlendState AlphaBlend()
	{
		return BlendState{
			true,
			false,
			BlendState::BlendFactor::SRC_ALPHA,
			BlendState::BlendFactor::INV_SRC_ALPHA,
			BlendState::BlendOp::ADD,
			BlendState::BlendFactor::SRC_ALPHA,
			BlendState::BlendFactor::INV_SRC_ALPHA,
			BlendState::BlendOp::ADD,
			BlendState::LogicOp::NOOP,
			BlendState::WriteMask::RED | BlendState::WriteMask::GREEN | BlendState::WriteMask::BLUE | BlendState::WriteMask::ALPHA
		};
	}

	static BlendState PremultipliedAlphaBlend()
	{
		return BlendState{
			true,
			false,
			BlendState::BlendFactor::ONE,
			BlendState::BlendFactor::INV_SRC_ALPHA,
			BlendState::BlendOp::ADD,
			BlendState::BlendFactor::ONE,
			BlendState::BlendFactor::INV_SRC_ALPHA,
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
	R16_FLOAT,
	R32_FLOAT,
	R32_UINT,
	D16_UNORM,
	D32_FLOAT,
	D24_UNORM_S8_UINT,
	COUNT
};

enum class PrimitiveType {
	INVALID,
	POINT,
	LINE,
	TRIANGLE,
	COUNT,
};

class ShaderResource
{
public:
	enum class Type { INVALID, TEXTURE, BUFFER, COUNT };
	bool IsTexture() { return mShaderResourceType == Type::TEXTURE; }
	bool IsBuffer() { return mShaderResourceType == Type::BUFFER; }
	bool IsInitialized() { return mInitialized; }
	const string& GetDebugName() const { return mDebugName; }

protected:
	string mDebugName;
	bool mInitialized;
	Type mShaderResourceType;

	ShaderResource(string debugName, Type srType) : mDebugName(debugName), mInitialized(false), mShaderResourceType(srType) {}
	virtual ~ShaderResource() {}
};

class DescriptorHeap
{
public:
	class Handle
	{
	public:
		Handle() : Handle(nullptr, { 0 }, { 0 }) {}
		Handle(DescriptorHeap* heap, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) 
			: mHeap(heap), mCpuHandle(cpuHandle), mGpuHandle(gpuHandle), mValid(false) {}
		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle() const { return mCpuHandle; }
		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle() const { return mGpuHandle; }
		void ResetHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle, bool valid) { mCpuHandle = cpuHandle; mGpuHandle = gpuHandle; mValid = valid; }
		void SetHeap(DescriptorHeap* heap) { mHeap = heap; }
		DescriptorHeap* GetDescriptorHeap() const { return mHeap; }
		void Offset(int slotCount, int sizeInBytePerSlot) { mCpuHandle.ptr += slotCount * sizeInBytePerSlot; mGpuHandle.ptr += slotCount * sizeInBytePerSlot; }
		bool IsValid() const { return mValid; }
		void SetInvalid() { mCpuHandle = { 0 }; mGpuHandle = { 0 }; mValid = false; }
	private:
		D3D12_CPU_DESCRIPTOR_HANDLE mCpuHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE mGpuHandle;
		bool mValid;
		DescriptorHeap* mHeap;
	};

	enum Type { CBV_SRV_UAV, SAMPLER, RTV, DSV, COUNT, INVALID };
	
	static void InitGlobalDescriptorHeap(Renderer* renderer, int countMax, Type type);
	
	static void ReleaseGlobalDescriptorHeap(bool checkOnly = false);

	DescriptorHeap();
	~DescriptorHeap();

	void Reset();
	void InitDescriptorHeap(int countMax, Type type);
	void Release();
	DescriptorHeap::Handle GetHeadHandle() const { return mHead; }
	DescriptorHeap::Handle GetCurrentFreeHandle() const { return mFree; }
	int GetHandleIncrementSize() { return mHandleIncrementSize; }
	int GetMaxCount() { return mMaxCount; }
	DescriptorHeap::Handle AllocateDsv(ID3D12Resource* resource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc);
	DescriptorHeap::Handle AllocateDsv(ID3D12Resource* resource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc, DescriptorHeap::Handle& offset);
	DescriptorHeap::Handle AllocateRtv(ID3D12Resource* resource); // null desc
	DescriptorHeap::Handle AllocateRtv(ID3D12Resource* resource, const D3D12_RENDER_TARGET_VIEW_DESC& desc);
	DescriptorHeap::Handle AllocateRtv(ID3D12Resource* resource, const D3D12_RENDER_TARGET_VIEW_DESC& desc, DescriptorHeap::Handle& offset);
	DescriptorHeap::Handle AllocateSrv(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc);
	DescriptorHeap::Handle AllocateSrv(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc, DescriptorHeap::Handle& offset);
	DescriptorHeap::Handle AllocateUav(ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc);
	DescriptorHeap::Handle AllocateUav(ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc, DescriptorHeap::Handle& offset);
	DescriptorHeap::Handle AllocateSampler(const D3D12_SAMPLER_DESC& desc);
	DescriptorHeap::Handle AllocateSampler(const D3D12_SAMPLER_DESC& desc, DescriptorHeap::Handle& offset);

	Type GetType();
	ID3D12DescriptorHeap* GetImpl();

private:
	static DescriptorHeap::Handle sHead[Type::COUNT];
	static DescriptorHeap::Handle sFree[Type::COUNT];
	static int sHandleIncrementSize[Type::COUNT];
	static ID3D12DescriptorHeap* sImpl[Type::COUNT];
	static Renderer* sRenderer[Type::COUNT];
	
	static DescriptorHeap::Handle AllocateDescriptorHeap(int countMax, Type type);
	
	DescriptorHeap::Handle AllocateRtvInternal(ID3D12Resource* resource, const D3D12_RENDER_TARGET_VIEW_DESC* desc, DescriptorHeap::Handle& offset);

	Renderer* mRenderer;
	DescriptorHeap::Handle mHead; // fixed
	DescriptorHeap::Handle mFree;
	int mHandleIncrementSize;
	ID3D12DescriptorHeap* mImpl;
	int mCurrentCount; // current count
	int mMaxCount; // max count
	Type mType;
};

class DynamicUniformBuffer 
{
public:
	struct Address
	{
		Address() : mCpuAddress(nullptr), mGpuAddress(NULL) {}
		bool IsValid() { return mCpuAddress && mGpuAddress; }
		void SetInvalid() { mCpuAddress = nullptr; mGpuAddress = { 0 }; }
		void* mCpuAddress;
		D3D12_GPU_VIRTUAL_ADDRESS mGpuAddress;
	};

	DynamicUniformBuffer();
	~DynamicUniformBuffer();

	void Reset();
	void InitDynamicUniformBuffer(Renderer* renderer, int sizeInByte);
	void Release(bool checkOnly = false);
	Address Allocate(int sizeInByte);

private:
	Renderer* mRenderer;
	ID3D12Resource* mImpl;
	D3D12_GPU_VIRTUAL_ADDRESS mHeadGpu;
	void* mHead;
	void* mFree;
	int mSizeInByte;
};

// TODO: add support to more command list type
class CommandList
{
public:
	static bool AllReleased();

	CommandList();
	~CommandList();

	bool InitCommandList(Renderer* renderer);
	void Release(bool checkOnly = false);
	bool Reset();
	HRESULT Close() { return mImpl->Close(); }
	ID3D12GraphicsCommandList* GetImpl() { return mImpl; }
	bool operator==(const CommandList& that) const { return mRenderer == that.mRenderer && mAllocator == that.mAllocator && mImpl == that.mImpl; }

private:
	static unordered_map<ID3D12CommandAllocator*, u8> sAllocatorRefMap;
	Renderer* mRenderer;
	ID3D12CommandAllocator* mAllocator;
	ID3D12GraphicsCommandList* mImpl;
};

#define DS_REVERSED_Z_SWITCH REVERSED_Z_SWITCH(DepthStencilState::Greater(), DepthStencilState::Less())
#define DS_EQUAL_REVERSED_Z_SWITCH REVERSED_Z_SWITCH(DepthStencilState::GreaterEqual(), DepthStencilState::LessEqual())
#define DS_EQUAL_NO_WRITE_REVERSED_Z_SWITCH REVERSED_Z_SWITCH(DepthStencilState::GreaterEqualNoWrite(), DepthStencilState::LessEqualNoWrite())

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
	static DXGI_FORMAT TranslateFormatToRead(Format format, ReadFrom readFrom);
	static D3D12_PRIMITIVE_TOPOLOGY_TYPE TranslatePrimitiveTopologyType(PrimitiveType primitiveType);
	static D3D12_PRIMITIVE_TOPOLOGY TranslatePrimitiveTopology(PrimitiveType primitiveType);
	static D3D12_DESCRIPTOR_HEAP_TYPE TranslateDescriptorHeapType(DescriptorHeap::Type type);
	static D3D12_FILTER ExtractFilter(Sampler sampler);
	static D3D12_TEXTURE_ADDRESS_MODE TranslateAddressMode(Sampler::AddressMode addressMode);
	static D3D12_RESOURCE_STATES TranslateResourceLayout(ResourceLayout resourceLayout);
	static D3D12_VIEWPORT TranslateViewport(Viewport viewport);
	static D3D12_RECT TranslateScissorRect(ScissorRect rect);
	static D3D12_RESOURCE_DIMENSION GetResourceDimensionFromTextureType(TextureType type);
	static D3D12_SAMPLER_DESC TranslateSamplerDesc(Sampler sampler);

	IDXGISwapChain3* GetSwapChain();
	DescriptorHeap::Handle GetRtvHandle(int frameIndex);
	DescriptorHeap::Handle GetDsvHandle(int frameIndex);
	DescriptorHeap& GetDynamicCbvSrvUavDescriptorHeap(int frameIndex);
	DescriptorHeap& GetDynamicRtvDescriptorHeap(int frameIndex);
	DescriptorHeap& GetDynamicDsvDescriptorHeap(int frameIndex);
	DynamicUniformBuffer::Address AllocateDynamicUniformBuffer(int frameIndex, int sizeInByte);
	ID3D12Resource* GetRenderTargetBuffer(int frameIndex);
	ID3D12Resource* GetDepthStencilBuffer(int frameIndex);
	ID3D12Resource* GetPreResolveBuffer(int frameIndex);
	vector<Frame>& GetFrames();
	Frame& GetFrame(int frameIndex);
	int GetMaxFrameTextureCount();
	bool IsMultiSampleUsed();
	bool IsResolveNeeded();

	void SetStore(Store* store);

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
	bool WaitForFrame(int frameIndex);
	void BeginFrame(int frameIndex);
	void EndFrame(int frameIndex);

	// explicit on command list for future multi thread support
	bool RecordBegin(int frameIndex, CommandList commandList);
	bool ResolveFrame(int frameIndex, CommandList commandList);
	bool RecordEnd(int frameIndex, CommandList commandList);
	bool Submit(int frameIndex, ID3D12CommandQueue* commandQueue, vector<CommandList> commandLists);
	bool Present();
	void WriteTextureDataToBufferSync(ID3D12Resource* dstBuffer, D3D12_RESOURCE_DESC textureDesc, vector<void*>& srcData, vector<int>& srcBytePerRow, vector<int>& srcBytePerSlice);
	void WriteDataToBufferAsync(CommandList commandList, ID3D12Resource* dstBuffer, ID3D12Resource* uploadBuffer, void* srcData, int srcSizeInByte);
	void WriteDataToBufferSync(ID3D12Resource* dstBuffer, void* srcData, int srcSizeInByte);
	void PrepareToReadDataFromBuffer(CommandList commandList, ID3D12Resource* srcBuffer, ID3D12Resource* readbackBuffer, int dstSizeInByte);
	void ReadDataFromReadbackBuffer(ID3D12Resource* readbackBuffer, void* dstData, int dstSizeInByte);
	void ReadDataFromBufferSync(ID3D12Resource* srcBuffer, void* dstData, int dstSizeInByte);

	u32 CalculateSubresource(u32 depthSlice, u32 mipSlice, u32 depth, u32 mipLevelCount);
	bool TransitionSingleTime(ID3D12Resource* resource, u32 subresource, ResourceLayout oldLayout, ResourceLayout newLayout);
	bool RecordTransition(CommandList commandList, ID3D12Resource* resource, u32 subresource, ResourceLayout oldLayout, ResourceLayout newLayout);
	bool RecordBarrierWithoutTransition(CommandList commandList, ID3D12Resource* resource);
	void RecordResolve(CommandList commandList, ID3D12Resource* src, u32 srcSubresource, ID3D12Resource* dst, u32 dstSubresource, Format format);

	void CreateDescriptorHeap(ID3D12DescriptorHeap** impl, DescriptorHeap::Type type, int size);
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
		const string& name);
	void CreateComputePSO(
		Pass& pass,
		ID3D12PipelineState** pso,
		ID3D12RootSignature* rootSignature,
		Shader* computeShader,
		const string& name);
	// explicit on command list for future multi thread support
	void RecordGraphicsPassInstanced(
		Pass& pass,
		CommandList commandList,
		u32 instanceCount,
		bool clearColor = false,
		bool clearDepth = false,
		bool clearStencil = false,
		XMFLOAT4 clearColorValue = XMFLOAT4(0, 0, 0, 0),
		float clearDepthValue = DEPTH_FAR_REVERSED_Z_SWITCH,
		u8 clearStencilValue = 0);
	void RecordGraphicsPass(
		Pass& pass,
		CommandList commandList,
		bool clearColor = false,
		bool clearDepth = false,
		bool clearStencil = false,
		XMFLOAT4 clearColorValue = XMFLOAT4(0, 0, 0, 0),
		float clearDepthValue = DEPTH_FAR_REVERSED_Z_SWITCH,
		u8 clearStencilValue = 0);
	void RecordComputePass(
		Pass& pass,
		CommandList commandList,
		int threadGroupCountX,
		int threadGroupCountY,
		int threadGroupCountZ);
	CommandList BeginSingleTimeCommands();
	void EndSingleTimeCommands();

	// TODO: hide unnecessary member variables
	IDXGIFactory4* mDxgiFactory;
	ID3D12Device* mDevice; // direct3d device
	ID3D12Debug* mDebugController;
	IDXGISwapChain3* mSwapChain; // swapchain used to switch between framebuffers
	ID3D12CommandQueue* mGraphicsCommandQueue; // TODO: add support to transfer queue
	vector<CommandList> mCommandLists; // arbitrary size, this way we can have more than 1 command list per frame
	stack<CommandList> mSingleTimeCommandLists;
	int mWidth;
	int mHeight;
	DebugMode mDebugMode;

	vector<ID3D12Fence*> mFences;
	HANDLE mFenceEvent; // a handle to an event when our fence is unlocked by the gpu
	vector<UINT64> mFenceValues;
	int mCurrentFramebufferIndex;
	int mFrameCount;
	int mFrameCountSinceGameStart;
	int mPixCaptureStartFrame;
	int mPixCaptureFrameCount;
	int mPixCapturedFrameCount;
	float mLastFrameTimeInSecond;
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

	CommandList BeginSingleTimeCommandsInternal();
	void EndSingleTimeCommandsInternal(ID3D12CommandQueue* commandQueue, CommandList commandList);
	void EnableDebugLayer();
	void EnableGBV();

	bool TransitionLayout(ID3D12Resource* resource, u32 subresource, ResourceLayout oldLayout, ResourceLayout newLayout, CD3DX12_RESOURCE_BARRIER& barrier);
	bool BarrierWithoutTransition(ID3D12Resource* resource, CD3DX12_RESOURCE_BARRIER& barrier);

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
		const string& name);
	void CreateComputePSOInternal(
		ID3D12Device* device,
		ID3D12PipelineState** pso,
		ID3D12RootSignature* rootSignature,
		Shader* computeShader,
		const string& name);

	DescriptorHeap mCbvSrvUavDescriptorHeap;
	DescriptorHeap mSamplerDescriptorHeap;
	DescriptorHeap mRtvDescriptorHeap;
	DescriptorHeap mDsvDescriptorHeap;

	vector<DescriptorHeap> mDynamicCbvSrvUavDescriptorHeap;
	vector<DescriptorHeap> mDynamicRtvDescriptorHeap;
	vector<DescriptorHeap> mDynamicDsvDescriptorHeap;
	vector<DynamicUniformBuffer> mDynamicUniformBuffers;

	Store* mStore;
	vector<ID3D12Resource*> mColorBuffers;
	vector<ID3D12Resource*> mDepthStencilBuffers;
	vector<ID3D12Resource*> mPreResolveColorBuffers;
	vector<DescriptorHeap::Handle> mDsvHandles;
	vector<DescriptorHeap::Handle> mRtvHandles;
	vector<DescriptorHeap::Handle> mPreResolvedRtvHandles;
};

const int gDynamicCbvSrvUavDescriptorCountPerFrame = 2 * 1024;
const int gDynamicRtvDescriptorCountPerFrame = 64;
const int gDynamicDsvDescriptorCountPerFrame = 64;
const int gDynamicUniformBufferSizePerFrameInByte = 1024 * 1024;
const int gWindowWidth = 960;
const int gWindowHeight = 960;
const int gWidthDeferred = 960;
const int gHeightDeferred = 960;
const int gWidthShadow = 960;
const int gHeightShadow = 960;
const int gFrameCount = 3;
const int gMultiSampleCount = 1; // msaa doesn't work on deferred back buffer
extern Format gSwapchainColorBufferFormat;
extern Format gSwapchainDepthStencilBufferFormat;
extern Sampler gSamplerLinear;
extern Sampler gSamplerPoint;
extern Mesh gCube;
extern Mesh gFullscreenTriangle;
extern Camera gCameraDummy;
extern OrbitCamera gCameraMain;
