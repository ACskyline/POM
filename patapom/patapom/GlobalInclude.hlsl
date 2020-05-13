#ifndef GLOBAL_INCLUDE
#define GLOBAL_INCLUDE

#define SCENE   0
#define FRAME   1
#define PASS    2
#define OBJECT  3

#define SPACE(x)    space ## x

#define PI          3.1415926535
#define HALF_PI     1.5707963267
#define TWO_PI      6.2831853071
#define MAX_LIGHTS_PER_SCENE 10

/////////////// UNIFORM ///////////////
//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv//

struct SurfaceData
{
    float3 albedo;
    float3 posWorld;
    float3 normalWorld;
    float3 geomNormalWorld;
    float3 eyeDirWorld;
    float3 sunDirWorld;
};

struct LightData
{
    float4x4 view;
    float4x4 viewInv;
    float4x4 proj;
    float4x4 projInv;
    float3 color;
    float nearClipPlane;
    float3 position;
    float farClipPlane;
    int textureIndex;
    uint PADDING0;
    uint PADDING1;
    uint PADDING2;
};

cbuffer SceneUniform : register(b0, SPACE(SCENE))
{
    uint sMode;
    uint sPomMarchStep;
    float sPomScale;
    float sPomBias;
    float sSkyScatterG;
    uint sSkyMarchStep;
    uint sSkyMarchStepTr;
    float sSunAzimuth;
    float sSunAltitude;
    float3 sSunRadiance;
    uint sLightCount;
    uint PADDING0;
    uint PADDING1;
    uint PADDING2;
    LightData sLights[MAX_LIGHTS_PER_SCENE];
};

cbuffer FrameUniform : register(b0, SPACE(FRAME))
{
    uint fFrameIndex;
};

cbuffer PassUniform : register(b0, SPACE(PASS))
{
    float4x4 pViewProj;
    float4x4 pViewProjInv;
    uint pPassIndex;
    float3 pEyePos;
    float pNearClipPlane;
    float pFarClipPlane;
    float pWidth;
    float pHeight;
};

cbuffer ObjectUniform : register(b0, SPACE(OBJECT))
{
    float4x4 oModel;
    float4x4 oModelInv;
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

// TODO: change this if using glsl
float2 TransformUV(float2 uv)
{
    return float2(uv.x, 1.0f - uv.y);
}

float3 GetSunPos(float azimuth, float altitude, float distance, float3 offset)
{
    azimuth *= TWO_PI;
    altitude *= TWO_PI;
    float height = distance * sin(altitude);
    float projLength = distance * cos(altitude);
    float3 relative = float3(projLength * cos(azimuth), height, projLength * sin(azimuth));
    return offset + relative;
}

float3 GetSunDir(float azimuth, float altitude)
{
    azimuth *= TWO_PI;
    altitude *= TWO_PI;
    float height = sin(altitude);
    float projLength = cos(altitude);
    float3 relative = float3(projLength * cos(azimuth), height, projLength * sin(azimuth));
    return relative;
}

#endif
