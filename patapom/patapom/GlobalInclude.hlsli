#ifndef GLOBAL_INCLUDE
#define GLOBAL_INCLUDE

#define SCENE   0
#define FRAME   1
#define PASS    2
#define OBJECT  3

#define SPACE(x)    space ## x

#define Pi          3.1415926535
#define Half_Pi     1.5707963267

/////////////// UNIFORM ///////////////
//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv//

// TODO: add support for shader space

cbuffer SceneUniform : register(b0, SPACE(SCENE))
{
    uint mode;
    uint marchStep;
    float marchScale;
    float marchBias;
};

cbuffer FrameUniform : register(b0, SPACE(FRAME))
{
    uint frameIndex;
};

cbuffer PassUniform : register(b0, SPACE(PASS))
{
    uint passIndex;
    float3 eyePos;
    float4x4 viewProj;
    float4x4 viewProjInv;
};

cbuffer ObjectUniform : register(b0, SPACE(OBJECT))
{
    float4x4 model;
    float4x4 modelInv;
};

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^//
/////////////// UNIFORM ///////////////

///////////////// VS /////////////////
//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv//
struct VS_INPUT
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
    float3 nor : NORMAL;
    float4 tan : TANGENT;
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 posWorld : POSITION_WORLD;
    float3 norWorld : NORMAL_WORLD;
    float3 tanWorld : TANGENT_WORLD;
    float3 bitanWorld : BITANGENT_WORLD;
};
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^//
///////////////// VS /////////////////

///////////////// PS /////////////////
//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv//
struct PS_OUTPUT
{
    float4 col0 : SV_TARGET0;
};
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^//
///////////////// PS /////////////////

	// CW
	// i+1-----i+2
	// |        |
	// |        |
	// i-------i+3

	// uv
	// 0,1------1,1
	//  |        |
	//  |        |
	//  |        |
	// 0.0------1,0

// TODO: change this for glsl
float2 TransformUV(float2 uv)
{
    return float2(uv.x, 1.0f - uv.y);
}

#endif
