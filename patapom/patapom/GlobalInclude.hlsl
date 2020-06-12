#ifndef GLOBALINCLUDE_H
#define GLOBALINCLUDE_H

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

struct SurfaceDataOut
{
    float depth;
    float2 uv;
    float3 albedo;
    float3 posWorld;
    float3 norWorld;
    float3 tanWorld;
    float3 bitanWorld;
    float3 eyeDirWorld;
};

struct SurfaceDataIn
{
    float3 albedo;
    float3 posWorld;
    float3 norWorld;
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
    //
    float sSkyScatterG;
    uint sSkyMarchStep;
    uint sSkyMarchStepTr;
    float sSunAzimuth;
    //
    float sSunAltitude;
    float3 sSunRadiance;
    //
    uint sLightCount;
    uint sLightDebugOffset;
    uint sLightDebugCount;
    float sFresnel;
    //
    float4 sStandardColor;
    //
    float sRoughness;
    float sUseStandardTextures;
    float sMetallic;
    float sReflection;
    //
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
    float4x4 pView;
    float4x4 pViewInv;
    float4x4 pProj;
    float4x4 pProjInv;
    uint pPassIndex;
    float3 pEyePos;
    float pNearClipPlane;
    float pFarClipPlane;
    float pWidth;
    float pHeight;
    float pFov;
    uint pPADDING_0;
    uint pPADDING_1;
    uint pPADDING_2;
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

#ifdef VS_OUTPUT_LITE
struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};
#else
struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 posView : POSITION_VIEW;
    float3 posWorld : POSITION_WORLD;
    float3 norWorld : NORMAL_WORLD;
    float3 tanWorld : TANGENT_WORLD;
    float3 bitanWorld : BITANGENT_WORLD;
};
#endif

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^//
///////////////// VS /////////////////

///////////////// PS /////////////////
//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv//

struct PS_OUTPUT
{
#if PS_OUTPUT_COUNT >=1
    float4 col0 : SV_TARGET0; // rgb: albedo, a: quantized zView
#endif
    
#if PS_OUTPUT_COUNT >= 2
    float4 col1 : SV_TARGET1; // xyz: packedNorWorld, w: unused
#endif
    
#if PS_OUTPUT_COUNT >= 3
    float4 col2 : SV_TARGET2; // xyz: position, w: unused
#endif
};

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^//
///////////////// PS /////////////////

// uv in pixel shaders are expected to be sample-ready,
// meaning they are transformed to respective hlsl/glsl texture space convention.
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

float3 PackNormal(float3 normal)
{
    return normal * 0.5f + 0.5f;
}

float3 UnpackNormal(float3 col)
{
    return col * 2.0f - 1.0f;
}

PS_OUTPUT PackGbuffer(SurfaceDataOut sdo)
{
    PS_OUTPUT op;
#if PS_OUTPUT_COUNT >= 1
    op.col0 = float4(sdo.albedo, sdo.depth);
#endif
#if PS_OUTPUT_COUNT >= 2
    op.col1 = float4(PackNormal(sdo.norWorld), 1.0f);
#endif
#if PS_OUTPUT_COUNT >= 3
    op.col2 = float4(sdo.posWorld, 1.0f);
#endif
    return op;
}

//assuming depth ranges [0, 1] in NDC, maps to [near, far] in view space
float LinearizeDepth(float depth, float near, float far)
{
    return far * near / (far + (near - far) * depth);
}

//assuming z axis ranges [near, far] in view space, maps to [0, 1] in ndc
float DelinearizeDepth(float zView, float near, float far)
{
    return far * near / (zView * (near - far)) - far / (near - far);
    //return far * (near - zView) / (zView * (near - far));
}

//scale zView to [0, 1] linearly
float QuantizeDepth(float zView, float near, float far)
{
    return (zView - near) / (far - near);
}

//scale quantized zView back
float DequantizeDepth(float zViewQuantized, float near, float far)
{
    return zViewQuantized * (far - near) + near;
}

float2 ScreenToNDC(float2 screenPos, float2 screenSize)
{
    return screenPos / screenSize * float2(2.0f, -2.0f) - float2(1.0f, -1.0f);
}

float3 RestorePosFromDepth(float2 screenPos, float2 screenSize, float depth, float near, float far, float4x4 viewProjInv)
{
    float2 fragPos = ScreenToNDC(screenPos, screenSize);
    float4 posProj = float4(fragPos, depth, 1.0f) * LinearizeDepth(depth, near, far);
    return mul(viewProjInv, posProj).xyz;
}

float3 RestorePosFromViewZ(float2 screenPos, float2 screenSize, float zView, float fov, float near, float far, float4x4 viewInv)
{
    float2 fragPos = ScreenToNDC(screenPos, screenSize);
    float aspectRatio = screenSize.x / screenSize.y;
    float tanHalfFov = tan(radians(fov / 2.0f)); // 1.0f / pProj[0][0];
    float tanHalfFovOverA = tanHalfFov / aspectRatio; // 1.0f / pProj[1][1];
    float4 posView = float4(fragPos * zView * float2(tanHalfFov, tanHalfFovOverA), zView, 1);
    return mul(viewInv, posView).xyz;
}

float3 GammaCorrect(float3 input)
{
    return pow(input, 1.0f / 2.2f);
}

#endif
