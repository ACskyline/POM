#pragma once

#include <dxgi1_4.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <string>
#include "Dependencies/d3dx12.h"

#define MY_DEBUG 1

#define SAFE_RELEASE(p) { if ( (p) ) { (p)->Release(); (p) = 0; } }
#define SAFE_RELEASE_ARRAY(p) { int n = _countof(p); for(int i = 0;i<n;i++){ SAFE_RELEASE(p[i]); } }
#define KEYDOWN(name, key) ((name)[(key)] & 0x80)
#define EPSILON 0.00000001
#define SIZEOF_ARRAY(arr) sizeof()

#define fatalAssert(x) { if(!x) __debugbreak(); }
#define fatalAssertf(x, ...) { if(!x) { printf(__VA_ARGS__); __debugbreak();} }
#define assertf(x, ...) { if(!x) { printf("%s, %s, line %d:", __FILE__, __FUNCTION__, __LINE__); printf(__VA_ARGS__); } }
#define fatalf(...) { printf(__VA_ARGS__); __debugbreak(); }

using namespace DirectX;
using namespace std;

enum UNIFORM_REGISTER_SPACE { SCENE, FRAME, PASS, OBJECT, COUNT }; // maps to layout number in Vulkan
enum UNIFORM_TYPE { CONSTANT, TEXTURE_TABLE, SAMPLER_TABLE, COUNT };

inline int GetUniformSlot(int space, int type)
{
	fatalAssertf((type > 0 && type < UNIFORM_TYPE::COUNT) && (space > 0 && space < UNIFORM_REGISTER_SPACE::COUNT), "uniform slot out of bound");
	GetUniformSlot((UNIFORM_TYPE)type, (UNIFORM_REGISTER_SPACE)space);
}

inline int GetUniformSlot(UNIFORM_REGISTER_SPACE space, UNIFORM_TYPE type)
{
	// constant buffer is root parameter, texture and sampler are stored in table
	// |0              |1              |2             |3               |4             |5             |6            |7              |8             |9             |10           |11
	// |scene constant |frame constant |pass constant |object constant |scene table 1 |frame table 1 |pass table 1 |object table 1 |scene table 2 |frame table 2 |pass table 2 |object table 2
	// |-              |-              |-             |-               |texture       |texture       |texture      |texture        |sampler       |sampler       |sampler      |sampler     
	return space + type * UNIFORM_REGISTER_SPACE::COUNT;
}

const D3D12_INPUT_ELEMENT_DESC VertexInputLayout[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
};

struct Vertex {
	Vertex() : Vertex(0, 0, 0, 0, 0, 0, 0, 0) {}
	Vertex(float x, float y, float z, float u, float v) : Vertex(x, y, z, u, v, 0, 0, 0) {}
	Vertex(float x, float y, float z, float u, float v, float nx, float ny, float nz) : pos(x, y, z), texCoord(u, v), nor(nx, ny, nz) {}
	XMFLOAT3 pos;
	XMFLOAT2 texCoord;
	XMFLOAT3 nor;
};

// TODO: add viewport and scissor rect to camera

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

bool CheckError(HRESULT hr, ID3D10Blob* error_message = nullptr);