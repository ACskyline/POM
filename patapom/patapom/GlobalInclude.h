#pragma once

#include <dxgi1_4.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <string>
#include "Dependencies/d3dx12.h"

using namespace DirectX;
using namespace std;

#define FLOAT4 XMFLOAT4
#define FLOAT3 XMFLOAT3
#define FLOAT2 XMFLOAT2

#include "SharedHeader.h"

#define EPSILON 0.00000001f
#define EQUALF(a, b) (abs(a - b) < EPSILON)

#define KEYDOWN(name, key) ((name)[(key)] & 0x80)
#define BUTTONDOWN(button) ((button) & 0x80)

#define ASSERT 1

#ifdef ASSERT
	#define assertOnly(x) x
	#define println(...) { printf("[%d:%d] ", gRenderer.mFrameCountTotal, gRenderer.mCurrentFrameIndex); printf(__VA_ARGS__); printf("\n"); }
	#define fatalAssert(x) { if(!(x)) __debugbreak(); }
	#define fatalAssertf(x, ...) { if(!(x)) { println(__VA_ARGS__); __debugbreak();} }
	#define assertf(x, ...) { if(!(x)) { println("%s, %s, line %d:", __FILE__, __FUNCTION__, __LINE__); println(__VA_ARGS__); } }
	#define fatalf(...) { println(__VA_ARGS__); __debugbreak(); }
	#define displayf(...) println(__VA_ARGS__)
	#define debugbreak(x) { x; __debugbreak(); }
#else
	#define assertOnly(x)
	#define println(...)
	#define fatalAssert(x)
	#define fatalAssertf(x, ...)
	#define assertf(x, ...)
	#define fatalf(...)
	#define displayf(...)
	#define debugbreak(x)
#endif

#if ASSERT >= 2
	#define assertf2(x, ...) { if(!(x)) { println("%s, %s, line %d:", __FILE__, __FUNCTION__, __LINE__); println(__VA_ARGS__); } }
#else
	#define assertf2(x, ...)
#endif

#define SAFE_RELEASE(p, checkOnly) { if (checkOnly) { fatalAssertf(p==nullptr, "pointer %p is not null", p); } else if (p!=nullptr) { p->Release(); p = nullptr; } }
#define SAFE_RELEASE_NO_CHECK(p) SAFE_RELEASE(p, false)
#define SAFE_RELEASE_ARRAY(p, checkOnly) { int n = _countof(p); for(int i = 0;i<n;i++){ SAFE_RELEASE(p[i], checkOnly); } }

#define USE_REVERSED_Z

#ifdef USE_REVERSED_Z
	#define REVERSED_Z_SWITCH(x, y) (x)
#else
	#define REVERSED_Z_SWITCH(x, y) (y)
#endif

class Renderer;

extern Renderer gRenderer;

bool CheckError(HRESULT hr, ID3D12Device* device = nullptr, ID3D10Blob* error_message = nullptr);

typedef uint8_t		u8;
typedef uint16_t	u16;
typedef uint32_t	u32;
typedef uint64_t	u64;
