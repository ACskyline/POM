#pragma once

#include <dxgi1_4.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <stdlib.h>
#include <string>
#include <locale>
#include <codecvt>
#include "Dependencies/d3dx12.h"

using namespace DirectX;
using namespace std;

#define KEYDOWN(name, key) ((name)[(key)] & 0x80)
#define BUTTONDOWN(button) ((button) & 0x80)
#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(x, minA, maxB) MAX(MIN(x, maxB), minA)
#define ABS(x) (x > 0 ? x : -x)
#define ROUNDUP_DIVISION(a, b) ((a + b - 1) / b)

#define ASSERT 1

#ifdef ASSERT
	#define assertOnly(x) x
	#define println(...) (printf("[%d:%d] ", gRenderer.mFrameCountSinceGameStart, gRenderer.mCurrentFramebufferIndex), (printf(__VA_ARGS__), printf("\n")))
	#define print(...) printf(__VA_ARGS__)
	#define fatalAssert(x) { if(!(x)) __debugbreak(); }
	#define fatalAssertf(x, ...) { if(!(x)) { println(__VA_ARGS__); __debugbreak();} }
	#define assertf(x, ...) { if(!(x)) { println("%s, %s, line %d:", __FILE__, __FUNCTION__, __LINE__); println(__VA_ARGS__); } }
	#define fatalf(...) { println(__VA_ARGS__); __debugbreak(); }
	#define displayfln(...) println(__VA_ARGS__)
	#define displayf(...) print(__VA_ARGS__)
	#define debugbreak(x) { x; __debugbreak(); }
	#define verifyf(x, ...) (x ? true : (println(__VA_ARGS__), false))
#else
	#define assertOnly(x)
	#define println(...)
	#define fatalAssert(x)
	#define fatalAssertf(x, ...)
	#define assertf(x, ...)
	#define fatalf(...)
	#define displayfln(...)
	#define displayf(...)
	#define debugbreak(x)
	#define verifyf(x, ...) x
#endif

#if ASSERT >= 2
	#define assertf2(x, ...) { if(!(x)) { println("%s, %s, line %d:", __FILE__, __FUNCTION__, __LINE__); println(__VA_ARGS__); } }
#else
	#define assertf2(x, ...)
#endif

#define SAFE_RELEASE(p, checkOnly) { if (checkOnly) { fatalAssertf(p==nullptr, "pointer %p is not null", p); } else if (p!=nullptr) { p->Release(); p = nullptr; } }
#define SAFE_RELEASE_NO_CHECK(p) SAFE_RELEASE(p, false)
#define SAFE_RELEASE_ARRAY(p, checkOnly) { int n = _countof(p); for(int i = 0;i<n;i++){ SAFE_RELEASE(p[i], checkOnly); } }

class Renderer;
class Scene;
extern Renderer gRenderer;
extern Scene gSceneDefault;

// return true if it is error
bool CheckError(HRESULT hr, ID3D12Device* device = nullptr, ID3D10Blob* error_message = nullptr);

static wstring StringToWstring(const string& str)
{
	size_t convertedCars = 0;
	wstring wstr;
	wstr.resize(str.length());
	mbstowcs_s(&convertedCars, &wstr[0], wstr.length(), str.c_str(), str.length());
	return wstr;
}

static string WstringToString(const wstring& wstr)
{
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::string str = converter.to_bytes(wstr);
	return str;
}

typedef uint8_t		u8;
typedef uint16_t	u16;
typedef uint32_t	u32;
typedef uint64_t	u64;
typedef int32_t		i32;
typedef int64_t		i64;

#define FLOAT4 XMFLOAT4
#define FLOAT3 XMFLOAT3
#define FLOAT2 XMFLOAT2
#define FLOAT4X4 XMFLOAT4X4
#define FLOAT3X3 XMFLOAT3X3
#define UINT u32

#define SHARED_HEADER_CPP
#include "SharedHeader.h"
