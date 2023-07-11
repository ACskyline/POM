#pragma once

#include <dxgi1_4.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <stdlib.h>
#include <string>
#include <locale>
#include <codecvt>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <stack>
#include "../dependency/d3dx12.h"

using namespace DirectX;
using namespace std;

// platform config strings
#ifdef _X64
#define PLATFORM_STR "x64"
#else // X64
#define PLATFORM_STR "other"
#endif // X64

#ifdef _DEBUG
#define CONFIG_STR "Debug"
#define USE_PIX
#else // _DEBUG
#define CONFIG_STR "Release"
#endif // _DEBUG

// common util macros
#define KEYHELD(name, key) ((name)[(key)] & 0x80)
#define BUTTONHELD(button) ((button) & 0x80)
#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(x, minA, maxB) MAX(MIN(x, maxB), minA)
#define ABS(x) (x > 0 ? x : -x)
#define ROUNDUP_DIVISION(a, b) ((a + b - 1) / b)
#define COUNT_OF(a) _countof(a)

#define _CONSOLE 1 // create console window
#define _OUTPUT 1 // write to std output
#define _VSOUTPUT 1 // write to VS output window
#if _VSOUTPUT
#include <windows.h>
static void VsOutput(const char* szFormat, ...)
{
	char szBuff[1024];
	va_list arg;
	va_start(arg, szFormat);
	_vsnprintf_s(szBuff, sizeof(szBuff), _TRUNCATE, szFormat, arg);
	va_end(arg);
	OutputDebugStringA(szBuff);
#if _OUTPUT
	printf(szBuff);
#endif
}
#define vsdisplayf(...) VsOutput(__VA_ARGS__)
#elif _OUTPUT
#define vsdisplayf(...) printf(__VA_ARGS__)
#else
#define vsdisplayf(x)
#endif

// ASSERT related macros
#if _DEBUG
#define ASSERT 1 // only allow level 1 assert
#endif // _DEBUG

#if defined(ASSERT)
#define assertOnly(x)			x
#define fatalAssert(x)			{ if(!(x)) __debugbreak(); }
#define fatalAssertf(x, ...)	{ if(!(x)) { displayfln(__VA_ARGS__); __debugbreak();} }
#define assertf(x, ...)			{ if(!(x)) { displayfln("%s, %s, line %d:", __FILE__, __FUNCTION__, __LINE__); displayfln(__VA_ARGS__); } }
#define fatalf(...)				{ displayfln(__VA_ARGS__); __debugbreak(); }
#define displayfln(...)			(vsdisplayf("[%d:%d] ", gFrameCountSinceGameStart, gCurrentFramebufferIndex), (vsdisplayf(__VA_ARGS__), vsdisplayf("\n")))
#define displayf(...)			(vsdisplayf("[%d:%d] ", gFrameCountSinceGameStart, gCurrentFramebufferIndex), vsdisplayf(__VA_ARGS__))
#define debugbreak(x)			{ x; __debugbreak(); }
#define verifyf(x, ...)			(x ? true : (displayfln(__VA_ARGS__), false))
#else // defined(ASSERT)
#define assertOnly(x)
#define fatalAssert(x)
#define fatalAssertf(x, ...)
#define assertf(x, ...)
#define fatalf(...)
#define displayfln(...)
#define displayf(...)
#define debugbreak(x)
#define verifyf(x, ...) x
#endif // ASSERT

#if defined(ASSERT) && ASSERT >= 2
#define assertf2(...)			assertf(__VA_ARGS__)
#else // ASSERT >= 2
#define assertf2(x)
#endif // ASSERT >= 2

// d3d12 macros
#define SAFE_RELEASE(p, checkOnly) { if (checkOnly) { fatalAssertf(p==nullptr, "pointer %p is not null", p); } else if (p!=nullptr) { p->Release(); p = nullptr; } }
#define SAFE_RELEASE_NO_CHECK(p) SAFE_RELEASE(p, false)
#define SAFE_RELEASE_ARRAY(p, checkOnly) { int n = _countof(p); for(int i = 0;i<n;i++){ SAFE_RELEASE(p[i], checkOnly); } }

class Renderer;
class Scene;
extern Renderer gRenderer;
extern Scene gSceneDefault;
extern int gFrameCountSinceGameStart;
extern int gCurrentFramebufferIndex;
extern const string ShaderSrcPath;
extern const string ShaderBuildPath;
extern const string AssetPath;

// return true if it is error
static bool CheckError(HRESULT hr, ID3D12Device* device = nullptr, ID3D10Blob* error_message = nullptr)
{
	if (FAILED(hr))
	{
		displayfln("FAILED: 0x%x", hr);
		if (error_message != nullptr)
			displayfln("return value: %d, error message: %s", hr, (char*)error_message->GetBufferPointer());
		if (hr == 0x887A0005 && device != nullptr)
			displayfln("device removed reason: 0x%x", device->GetDeviceRemovedReason());
		fatalf("check error failed!");
		return true;
	}
	return false;
}

static wstring StringToWstring(const string& str)
{
	size_t convertedCars = 0;
	wchar_t *wcs = new wchar_t[str.length() + 1];
	mbstowcs_s(&convertedCars, wcs, str.length() + 1, str.c_str(), str.length());
	wstring wstr(wcs);
	delete[] wcs;
	return wstr;
}

static string WstringToString(const wstring& wstr)
{
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::string str = converter.to_bytes(wstr);
	return str;
}

template<typename Functor>
static size_t ReadFile(string filePathName, Functor functor, bool binary = true, bool freeMemory = true)
{
	fstream file;
	file.open(filePathName, binary ? ios::in | ios::binary : ios::in);
	fatalAssertf(file.is_open(), "can't open file %s", filePathName.c_str());
	file.seekg(0, ios::end);
	size_t length = file.tellg();
	file.seekg(0, ios::beg);
	if (length)
	{
		char* buf = new char[length];
		file.read(buf, length);
		if (!binary) // text file
			buf[length - 1] = '\0';
		functor(buf);
		file.close();
		if (freeMemory)
			delete[] buf;
	}
	return length; // return size in byte
}

// type defines
typedef uint8_t		u8;
typedef uint16_t	u16;
typedef uint32_t	u32;
typedef uint64_t	u64;
typedef int32_t		i32;
typedef int64_t		i64;

// enum bitwise operations, copied from DEFINE_ENUM_FLAG_OPERATORS in winnt.h
template <size_t S>
struct _ENUM_FLAG_UINT_FOR_SIZE;

template <>
struct _ENUM_FLAG_UINT_FOR_SIZE<1>
{
	typedef u8 type;
};

template <>
struct _ENUM_FLAG_UINT_FOR_SIZE<2>
{
	typedef u16 type;
};

template <>
struct _ENUM_FLAG_UINT_FOR_SIZE<4>
{
	typedef u32 type;
};

template <>
struct _ENUM_FLAG_UINT_FOR_SIZE<8>
{
	typedef u64 type;
};

template <class T>
struct _ENUM_FLAG_SIZED_UINT
{
	typedef typename _ENUM_FLAG_UINT_FOR_SIZE<sizeof(T)>::type type;
};

#define DEFINE_ENUM_BITWISE_OPERATIONS(ENUMTYPE) \
inline ENUMTYPE operator | (ENUMTYPE a, ENUMTYPE b) { return ENUMTYPE(((_ENUM_FLAG_SIZED_UINT<ENUMTYPE>::type)a) | ((_ENUM_FLAG_SIZED_UINT<ENUMTYPE>::type)b)); } \
inline ENUMTYPE &operator |= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((_ENUM_FLAG_SIZED_UINT<ENUMTYPE>::type &)a) |= ((_ENUM_FLAG_SIZED_UINT<ENUMTYPE>::type)b)); } \
inline ENUMTYPE operator & (ENUMTYPE a, ENUMTYPE b) { return ENUMTYPE(((_ENUM_FLAG_SIZED_UINT<ENUMTYPE>::type)a) & ((_ENUM_FLAG_SIZED_UINT<ENUMTYPE>::type)b)); } \
inline ENUMTYPE &operator &= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((_ENUM_FLAG_SIZED_UINT<ENUMTYPE>::type &)a) &= ((_ENUM_FLAG_SIZED_UINT<ENUMTYPE>::type)b)); } \
inline ENUMTYPE operator ~ (ENUMTYPE a) { return ENUMTYPE(~((_ENUM_FLAG_SIZED_UINT<ENUMTYPE>::type)a)); } \
inline ENUMTYPE operator ^ (ENUMTYPE a, ENUMTYPE b) { return ENUMTYPE(((_ENUM_FLAG_SIZED_UINT<ENUMTYPE>::type)a) ^ ((_ENUM_FLAG_SIZED_UINT<ENUMTYPE>::type)b)); } \
inline ENUMTYPE &operator ^= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((_ENUM_FLAG_SIZED_UINT<ENUMTYPE>::type &)a) ^= ((_ENUM_FLAG_SIZED_UINT<ENUMTYPE>::type)b)); } 

// shader shared var types
#define FLOAT4		XMFLOAT4
#define FLOAT3		XMFLOAT3
#define FLOAT2		XMFLOAT2
#define FLOAT4X4	XMFLOAT4X4
#define FLOAT3X3	XMFLOAT3X3
#define UINT		u32
#define UINT2		XMUINT2
#define UINT3		XMUINT3
#define UINT4		XMUINT4
#define INT			int
#define INT2		XMINT2
#define INT3		XMINT3
#define INT4		XMINT4

#define SHARED_HEADER_CPP
#include "SharedHeader.h"
#include "CommandLineArg.h"
