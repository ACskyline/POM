#ifndef GLOBALINCLUDE_H
#define GLOBALINCLUDE_H

#define SCENE   0
#define FRAME   1
#define PASS    2
#define OBJECT  3

#define SPACE(x)    space ## x

#define PI          3.1415926535
#define ONE_OVER_PI 0.3183098861
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

struct SceneUniformDefault
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
    float sSpecularity;
    //
    uint sSampleNumIBL;
    uint sShowReferenceIBL;
    uint sUseSceneLight;
    uint sUseSunLight;
    //
    uint sUseIBL;
    uint sPrefilteredEnvMapMipLevelCount;
    uint PADDING_1;
    uint PADDING_2;
    //
    LightData sLights[MAX_LIGHTS_PER_SCENE];
};

struct FrameUniformDefault
{
    uint fFrameIndex;
};

struct PassUniformDefault
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

struct ObjectUniformDefault
{
    float4x4 oModel;
    float4x4 oModelInv;
};

#ifndef CUSTOM_SCENE_UNIFORM
cbuffer SceneUniformBuffer : register(b0, SPACE(SCENE))
{
    SceneUniformDefault uScene;
}
#endif

#ifndef CUSTOM_FRAME_UNIFORM
cbuffer FrameUniformBuffer : register(b0, SPACE(FRAME))
{
    FrameUniformDefault uFrame;
};
#endif

#ifndef CUSTOM_PASS_UNIFORM
cbuffer PassUniformBuffer : register(b0, SPACE(PASS))
{
    PassUniformDefault uPass;
};
#endif

#ifndef CUSTOM_OBJECT_UNIFORM
cbuffer ObjectUniformBuffer : register(b0, SPACE(OBJECT))
{
    ObjectUniformDefault uObject;
};
#endif

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
//float2 TransformUV(float2 uv)
//{
//    return float2(uv.x, 1.0f - uv.y);
//}
#define TransformUV(uv) float2(uv.x, 1.0f - uv.y)

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

float3 LinearToGamma(float3 input)
{
    return pow(input, 1.0f / 2.2f);
}

float2 Hammersley(uint index, uint num)
{
    const uint numSampleBits = uint(log2(float(num)));
    const float invNumSamples = 1.0 / float(num);
    uint i = uint(index);
    uint t = i;
    uint bits = 0u;
    for (uint j = 0u; j < numSampleBits; j++)
    {
        bits = bits * 2u + (t - (2u * (t / 2u)));
        t /= 2u;
    }
    return float2(float(i), float(bits)) * invNumSamples;
}

//    +z => +x   => -z  => -x
// u: 0  => 0.25 => 0.5 => 0.75
//    +y => -y
// v: 1  => 0
float2 DirToUV(float3 dir)
{
    float tan = dir.x / dir.z;
    float u = atan(tan);
    if (dir.z < 0)
        u += PI;
    if (u < 0)
        u += TWO_PI; // [-PI/2,3PI/2) => [0, 2PI)
    u = u / TWO_PI;
    float v = 1.0f - acos(dir.y) / PI; // linear transform in uv space
    return float2(u, v);
}

//         x
//         |  /
//         | / u
// -z -----+----- z 
//         |
//         |
//        -x
float3 UVtoDir(float2 uv)
{
    float3 dir;
    float theta = (1.0f - uv.y) * PI;
    float phi = uv.x * TWO_PI;
    dir.y = cos(theta);
    dir.x = sin(phi) * sin(theta);
    dir.z = cos(phi) * sin(theta);
    return dir;
}

float3 ImportanceSampleGGX(float2 Xi, float roughness, float3 N)
{
    float a = roughness * roughness;
    float Phi = 2 * PI * Xi.x;
    float CosTheta = sqrt((1 - Xi.y) / (1 + (a * a - 1) * Xi.y));
    float SinTheta = sqrt(1 - CosTheta * CosTheta);
    float3 H;
    H.x = SinTheta * cos(Phi);
    H.y = SinTheta * sin(Phi);
    H.z = CosTheta;
    float3 UpVector = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 TangentX = normalize(cross(UpVector, N));
    float3 TangentY = cross(N, TangentX);
    // Tangent to world space
    return TangentX * H.x + TangentY * H.y + N * H.z;
}

float CosTheta(float3 w, float3 nor)
{
    return dot(w, nor);
}

float TanTheta(float3 w, float3 nor)
{
    float b = dot(w, nor);
    float a = length(w - b * nor);
    return a / b;
}

// Trowbridge-Reitz micro-facet distribution function (GGX)
float GGX_D(float cosThetaH, float alpha)
{
    float temp = cosThetaH * cosThetaH * (alpha * alpha - 1.0f) + 1.0f;
    return alpha * alpha / (PI * temp * temp);
}

// Trowbridge-Reitz masking shadowing function
float GGX_G(float tanThetaI, float tanThetaO, float alpha)
{
    float alpha2 = alpha * alpha;
    float tanThetaI2 = tanThetaI * tanThetaI;
    float tanThetaO2 = tanThetaO * tanThetaO;
    float AI = (-1.0f + sqrt(1.0f + alpha2 * tanThetaI2)) / 2.0f;
    float AO = (-1.0f + sqrt(1.0f + alpha2 * tanThetaO2)) / 2.0f;
    return 1.0f / (1.0f + AI + AO);
}

// wi = lightDir, wo = eyeDir
float GGX_NoFresnel(float3 wi, float3 wo, float3 wh, float3 N, float roughness)
{
    return saturate(GGX_D(CosTheta(wh, N), roughness) * GGX_G(TanTheta(wi, N), TanTheta(wo, N), roughness) / (4.0f * CosTheta(wi, N) * CosTheta(wo, N)));
}

#endif
