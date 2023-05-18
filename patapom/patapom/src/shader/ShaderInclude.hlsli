#ifndef SHADERINCLUDE_H
#define SHADERINCLUDE_H

#define FLOAT4      float4
#define FLOAT3      float3
#define FLOAT2      float2
#define FLOAT4X4    float4x4
#define FLOAT3X3    float3x3
#define UINT        uint
#define UINT2       uint2
#define UINT3       uint3
#define UINT4       uint4
#define INT         int
#define INT2        int2
#define INT3        int3
#define INT4        int4

#define IDENTITY_3X3 float3x3(1, 0, 0, 0, 1, 0, 0, 0, 1)

#define SHARED_HEADER_HLSL
#include "../engine/SharedHeader.h"

#define SCENE   0
#define FRAME   1
#define PASS    2
#define OBJECT  3
#define SPACE(x)    PASTE(space, x)

/////////////// UNIFORM ///////////////
//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv//

struct SurfaceDataOut
{
    float mQuantizedZView;
	float mRoughness;
	float mFresnel;
	float mSpecularity;
	float mMetallic;
    float2 mUV;
    float3 albedo;
    float3 posWorld;
    float3 norWorld;
    float3 tanWorld;
    float3 bitanWorld;
    float3 eyeDirWorld;
};

struct SurfaceDataIn
{
    float3 mAlbedo;
    float3 mPosWorld;
    float3 mNorWorld;
	float mRoughness;
	float mFresnel;
	float mMetallic;
	float mSpecularity;
};

#ifndef CUSTOM_SCENE_UNIFORM
cbuffer SceneUniformBuffer : register(b0, SPACE(SCENE))
{
    SceneUniform uScene;
}
#endif

#ifndef CUSTOM_FRAME_UNIFORM
cbuffer FrameUniformBuffer : register(b0, SPACE(FRAME))
{
    FrameUniform uFrame;
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
    ObjectUniform uObject;
};
#endif

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^//
/////////////// UNIFORM ///////////////

SamplerState gSamplerLinear : register(s0, SPACE(SCENE));
SamplerState gSamplerPoint : register(s1, SPACE(SCENE));
SamplerState gSamplerIBL : register(s2, SPACE(SCENE));

///////////////// VS /////////////////
//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv//
struct VS_INPUT
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
    float3 nor : NORMAL;
	float4 tan : TANGENT;
#ifdef VS_INPUT_INSTANCE_ID
	uint instanceID : SV_InstanceID;
#endif
#ifdef VS_INPUT_VERTEX_ID
	uint vertexID : SV_VertexID;
#endif
};

#ifdef VS_OUTPUT_LITE
struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
	float4 col : COLOR;
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

void DiscardVertex(inout VS_OUTPUT vert)
{
    vert.pos = float4(-1, -1, -1, 1);
}

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^//
///////////////// VS /////////////////

///////////////// PS /////////////////
//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv//

struct PS_OUTPUT
{
#ifdef PS_DEPTH_OUTPUT
	float depth : SV_Depth;
#endif
#if PS_COLOR_OUTPUT_COUNT >= 1
    float4 col0 : SV_TARGET0;
#endif
#if PS_COLOR_OUTPUT_COUNT >= 2
    float4 col1 : SV_TARGET1;
#endif
#if PS_COLOR_OUTPUT_COUNT >= 3
    float4 col2 : SV_TARGET2;
#endif
#if PS_COLOR_OUTPUT_COUNT >= 4
	float4 col3 : SV_TARGET3;
#endif
};

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^//
///////////////// PS /////////////////

// transform UV before sampling on D3D12, because we are using OpenGL convension on CPU side, i.e.
//   0,1-----1,1
//    |       |
//    |       |
//   0,0-----1,0

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

// gbuffer0: rgb: albedo, a: quantized z view
// gbuffer1: rgb: packed normal, a: roughness
// gbuffer2: rgb: world position, a: fresnel
// gbuffer3: r: specularity, g: metallic, ba: unused
PS_OUTPUT PackGbuffer(SurfaceDataOut sdo)
{
    PS_OUTPUT op;
#if PS_COLOR_OUTPUT_COUNT >= 1
    op.col0 = float4(sdo.albedo, sdo.mQuantizedZView);
#endif
#if PS_COLOR_OUTPUT_COUNT >= 2
	op.col1 = float4(PackNormal(sdo.norWorld), sdo.mRoughness);
#endif
#if PS_COLOR_OUTPUT_COUNT >= 3
	op.col2 = float4(sdo.posWorld, sdo.mFresnel);
#endif
#if PS_COLOR_OUTPUT_COUNT >= 4
	op.col3 = float4(sdo.mSpecularity, sdo.mMetallic, 0.0f, 0.0f);
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
}

//scale zView to [0, 1] linearly
float QuantizeDepth(float zView, float near, float far)
{
    return (zView - near) / (far - near);
}

//scale quantized zView back
float DequantizeDepth(float depth, float near, float far)
{
    return depth * (far - near) + near;
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
    float tanHalfFov = tan(radians(fov / 2.0f));
    float tanHalfFovOverA = tanHalfFov / aspectRatio;
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
    uint bits = 0;
    for (uint j = 0; j < numSampleBits; j++)
    {
        bits = bits * 2 + (t - (2 * (t / 2)));
        t /= 2;
    }
    return float2(float(i), float(bits)) * invNumSamples;
}

//    +x => +z   => -x  => -z
// u: 0  => 0.25 => 0.5 => 0.75
//    -y => +y
// v: 0  => 1
float2 DirToUV(float3 dir)
{
    float tan = dir.z / dir.x;
    float u = atan(tan);
    if (dir.x < 0.0f)
        u += PI;
    if (u < 0.0f)
        u += TWO_PI; // [-PI/2,3PI/2) => [0, 2PI)
    u = u / TWO_PI;
    float v = 1.0f - acos(dir.y) / PI; // linear transform in uv space
    return float2(u, v);
}

//         z
//         |  /
//         | / u
// -x -----+----- x 
//         |
//         |
//        -z
float3 UVtoDir(float2 uv)
{
    float3 dir;
    float theta = uv.y * PI;
    float phi = uv.x * TWO_PI;
    dir.y = -cos(theta);
    dir.x = cos(phi) * sin(theta);
    dir.z = sin(phi) * sin(theta);
    return dir;
}

float3 FaceUVtoDir(uint face, float2 uv)
{
	float2 convertedUV = uv * 2.0f - 1.0f;
	float3 localDir;
	if (face == 0) // +x
		localDir = float3(1.0f, convertedUV.y, -convertedUV.x);
	else if (face == 1) // -x
		localDir = float3(-1.0f, convertedUV.y, convertedUV.x);
	else if (face == 2) // +y
		localDir = float3(convertedUV.x, 1.0f, -convertedUV.y);
	else if (face == 3) // -y
		localDir = float3(convertedUV.x, -1.0f, convertedUV.y);
	else if (face == 4) // +z
		localDir = float3(convertedUV.x, convertedUV.y, 1.0f);
	else if (face == 5) // -z
		localDir = float3(-convertedUV.x, convertedUV.y, -1.0f);

	return normalize(localDir);
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

float GGX_PDF(float3 wo, float3 wh, float3 nor, float alpha)
{
	float cosTheta = CosTheta(wh, nor);
	return GGX_D(cosTheta, alpha) * abs(cosTheta) / (4.0f * dot(wo, wh));
}

// wi = lightDir, wo = eyeDir
float GGX_NoFresnel(float3 wi, float3 wo, float3 wh, float3 N, float roughness)
{
    return saturate(GGX_D(CosTheta(wh, N), roughness) * GGX_G(TanTheta(wi, N), TanTheta(wo, N), roughness) / (4.0f * CosTheta(wi, N) * CosTheta(wo, N)));
}

float3 ImportanceSampleGGX(float2 xi, float roughness, float3 N)
{
	float a = roughness * roughness;
	float Phi = 2 * PI * xi.x;
	float CosTheta = sqrt((1 - xi.y) / (1 + (a * a - 1) * xi.y));
	float SinTheta = sqrt(1 - CosTheta * CosTheta);
	float3 H;
	H.x = SinTheta * cos(Phi);
	H.y = SinTheta * sin(Phi);
	H.z = CosTheta;
	float3 UpVector = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
	float3 tangentX = normalize(cross(UpVector, N));
	float3 tangentY = cross(N, tangentX);
	// tangent to world space
	return tangentX * H.x + tangentY * H.y + N * H.z;
}

// Schlick approximation
float Fresnel(float vDotH, float F0)
{
	return F0 + (1.0f - F0) * pow(1.0f - vDotH, 5.0f);
}

// wi = lightDir, wo = eyeDir
float GGX(float3 wi, float3 wo, float3 wh, float3 N, float roughness)
{
	float fresnel = Fresnel(dot(wh, wo), uScene.mFresnel);
	return saturate(fresnel * GGX_NoFresnel(wi, wo, wh, N, roughness));
}

float3 BRDF_GGX(SurfaceDataIn sdi, float3 wi, float3 wo)
{
	if (dot(wi, sdi.mNorWorld) < 0.0f || dot(wo, sdi.mNorWorld) < 0.0f)
		return 0.0f.xxx;

	float3 wh = (wo + wi);
	if (length(wh) == 0.0f)
	{
		// if wo and wi point to the opposite directions
		// use the perpendicular on the normal's side as the wh
		float3 tangent = normalize(cross(sdi.mNorWorld, wi));
		wh = normalize(cross(wi, tangent));
	}
	else
		wh = normalize(wh);

	float specular = GGX(wi, wo, wh, sdi.mNorWorld, sdi.mRoughness);
	float diffuse = ONE_OVER_PI; // TODO: use Disney diffuse
	return sdi.mAlbedo * (diffuse * (1.0f - sdi.mMetallic) + specular * sdi.mMetallic) * saturate(CosTheta(wi, sdi.mNorWorld));
}

// pseudo random number generator
// from https://blog.selfshadow.com/sandbox/multi_fresnel.html
uint hash(uint seed)
{
	seed = (seed ^ 61u) ^ (seed >> 16u);
	seed *= 9u;
	seed = seed ^ (seed >> 4u);
	seed *= 0x27d4eb2du;
	seed = seed ^ (seed >> 15u);
	return seed;
}

uint random(inout uint state)
{
	// LCG values from Numerical Recipes
	state = 1664525u * state + 1013904223u;
	return state;
}

// Random float in range [0, 1)
// Implementation adapted from ispc
float frandom(inout uint state)
{
	uint irand = random(state);
	irand &= 0x007FFFFFu;
	irand |= 0x3F800000u;
	return asfloat(irand) - 1.0f;
}

float2 frandom2(inout uint state)
{
	return float2(frandom(state), frandom(state));
}

float3 frandom3(inout uint state)
{
	return float3(frandom(state), frandom(state), frandom(state));
}

float3 frandom3once(uint state)
{
	return frandom3(state);
}

bool IsNotBlack(float3 col)
{
	return (col.r > 0.0f || col.g > 0.0f || col.b > 0.0f);
}

float3x3 Inverse(float3x3 m)
{
    float det = m._11 * m._22 * m._33 +
        m._12 * m._23 * m._31 +
        m._13 * m._21 * m._32 -
        m._13 * m._22 * m._31 -
        m._12 * m._21 * m._33 -
        m._11 * m._23 * m._32;

    float3x3 ret;
    ret._11 = m._22 * m._33 - m._23 * m._32;
    ret._12 = m._13 * m._32 - m._12 * m._33;
    ret._13 = m._12 * m._23 - m._13 * m._22;
    ret._21 = m._23 * m._31 - m._21 * m._33;
    ret._22 = m._11 * m._33 - m._13 * m._31;
    ret._23 = m._13 * m._21 - m._11 * m._23;
    ret._31 = m._21 * m._32 - m._22 * m._31;
    ret._32 = m._12 * m._31 - m._11 * m._32;
    ret._33 = m._11 * m._22 - m._12 * m._21;

    if (det == 0.0f)
        return 0;
    else
        return ret / det;
}
#endif
