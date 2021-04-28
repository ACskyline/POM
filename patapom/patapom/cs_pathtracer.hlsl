#define CUSTOM_PASS_UNIFORM

#include "GlobalInclude.hlsl"

#define POINT_RADIUS									0.0001f
#define PATH_TRACER_SPAWN_RAY_BIAS						0.001f
#define PATH_TRACER_INTERSECTION_TYPE_MASK				0x00000003			// ... 0000 0011
#define PATH_TRACER_INTERSECTION_TYPE_MATERIAL			0x00000001			// ... 0000 0001
#define PATH_TRACER_INTERSECTION_TYPE_LIGHT				0x00000002			// ... 0000 0010
#define PATH_TRACER_INTERSECTION_LIGHT_TYPE_SHIFT		2
#define PATH_TRACER_INTERSECTION_LIGHT_TYPE_MASK		0x0000000C			// ... 0000 1100
#define PATH_TRACER_LIGHT_TYPE_TO_INTERSECTION(x)		(x << PATH_TRACER_INTERSECTION_LIGHT_TYPE_SHIFT)
#define PATH_TRACER_INTERSECTION_LIGHT_TYPE_INVALID		PATH_TRACER_LIGHT_TYPE_TO_INTERSECTION(LIGHT_TYPE_INVALID)	// ... 0000 0000
#define PATH_TRACER_INTERSECTION_LIGHT_TYPE_QUAD		PATH_TRACER_LIGHT_TYPE_TO_INTERSECTION(LIGHT_TYPE_QUAD)		// ... 0000 0010
#define PATH_TRACER_INTERSECTION_LIGHT_TYPE_POINT		PATH_TRACER_LIGHT_TYPE_TO_INTERSECTION(LIGHT_TYPE_POINT)	// ... 0000 0100
#define PATH_TRACER_INTERSECTION_LIGHT_TYPE_SPHERE		PATH_TRACER_LIGHT_TYPE_TO_INTERSECTION(LIGHT_TYPE_SPHERE)	// ... 0000 0110

cbuffer PassUniformBuffer : register(b0, SPACE(PASS))
{
	PassUniformPathTracer uPass;
};

RWStructuredBuffer<Ray> gRayBuffer : register(u0, SPACE(PASS));
RWTexture2D<float4> gBackbufferPT : register(u1, SPACE(PASS));
RWStructuredBuffer<Ray> gDebugRayBuffer : register(u2, SPACE(PASS));
RWTexture2D<float> gDepthbufferPT : register(u3, SPACE(PASS));
StructuredBuffer<TrianglePT> gTriangleBufferPT : register(t0, SPACE(PASS));
StructuredBuffer<MeshPT> gMeshBufferPT : register(t1, SPACE(PASS));
StructuredBuffer<LightData> gLightDataBufferPT : register(t2, SPACE(PASS));
StructuredBuffer<MeshBVH> gMeshBvhBuffer : register(t3, SPACE(PASS));
StructuredBuffer<TriangleBVH> gTriangleBvhBuffer : register(t4, SPACE(PASS));
Texture2D gTextures[] : register(t5, SPACE(PASS)); // unbounded texture array
SamplerState gSamplers[] : register(s5, SPACE(PASS)); // unbounded sampler array

bool gDebugPixel = false;

struct Intersection 
{
	uint mTriangleIndex;
	uint mLightIndex;
	float3 mPointWorld;
	float3 mPointModel;
	uint mIntersectionFlags; // to cache small but important information so that we don't need to constantly evaluate surface during intersection test
};

struct SurfaceDataInPT : SurfaceDataIn
{
	float2 mUV;
	float4 mTanWorld;
	uint mMaterialType;
	float3 mEmissive;
};

Ray InitRay(uint maxDepth, uint seed, float3 ori, float3 dir)
{
	Ray ray = (Ray)0;
	ray.mReadyForDebug = false;
	ray.mRemainingDepth = maxDepth;
	ray.mTerminated = false;
	ray.mSeed = seed;
	ray.mResultWritten = false;
	ray.mIsLastBounceSpecular = false;
	ray.mOri = ori;
	ray.mDir = dir;
	ray.mNextOri = ori;
	ray.mNextDir = dir;
	ray.mEnd = ray.mOri;
	ray.mThroughput = 1.0f.xxx;
	ray.mResult = 0.0f.xxx;
	return ray;
}

float RaySphere(float3 p, float r, float3 ori, float3 dir)
{
	float3 v = p - ori;
	float t = dot(v, dir);
	if (t > 0.0f && length(v - t * dir) < r)
		return t;
	else
		return -1.0f;
}

float RayPlane(float3 p, float3 nor, float3 ori, float3 dir)
{
	// return 0.0f when ray is parallel to plane
	float norDotDir = dot(nor, dir);
	if (norDotDir == 0.0f)
		return 0.0f;
	else
		return dot(nor, (p - ori)) / norDotDir;
}

// done in local space
float RayQuad(float4x4 modelMatInv, float3 pos, float3 nor, float3 scale, float3 dir, float3 ori)
{
	// early reject
	float t = RayPlane(pos, nor, dir, ori);
	if (t <= 0.0f)
		return t;

	float3 oriLocal = mul(modelMatInv, float4(ori, 0.0f)).xyz;
	float3 dirLocal = mul(modelMatInv, float4(dir, 0.0f)).xyz;
	float tLocal = t * length(dirLocal) / length(dir);

	float3 pLocal = oriLocal + dirLocal * tLocal;
	if (pLocal.x > -scale.x && pLocal.x < scale.x && pLocal.y > -scale.y && pLocal.y < scale.y)
		return t;
	else
		return t = -1.0f;
}

float RayTriangle(TrianglePT tri, float3 ori, float3 dir)
{
	// early reject
	float t = RayPlane(tri.mVertices[0].pos, tri.mVertices[0].nor, ori, dir);
	if (t <= 0.0f) 
		return t;

	float3 p = ori + t * dir;
	float s = length(cross(tri.mVertices[0].pos - tri.mVertices[1].pos, tri.mVertices[0].pos - tri.mVertices[2].pos));
	float s1 = length(cross(p - tri.mVertices[1].pos, p - tri.mVertices[2].pos));
	float s2 = length(cross(p - tri.mVertices[2].pos, p - tri.mVertices[0].pos));
	float s3 = length(cross(p - tri.mVertices[0].pos, p - tri.mVertices[1].pos));

	if (s1 >= 0.0f && s1 <= s && s2 >= 0.0f && s2 <= s && s3 >= 0.0f && s3 <= s && EQUALF(s1 + s2 + s3, s))
		return t;
	else
		return -1.0f;
}

float RayLight(LightData ld, float3 ori, float3 dir)
{
	float t = -1.0f;
	if (ld.mLightType == LIGHT_TYPE_POINT)
	{
		// geometrically treat point light as sphere for now
		t = RaySphere(ld.mPosWorld, POINT_RADIUS, ori, dir);
	}
	else if (ld.mLightType == LIGHT_TYPE_QUAD)
	{
		// use light view matrix as inverse model matrix to transform dir and ori from world space to local space
		t = RayQuad(ld.mView, ld.mPosWorld, ld.mDirWorld, ld.mScale, dir, ori);
	}
	else if (ld.mLightType == LIGHT_TYPE_SPHERE)
	{
		// TODO: add RaySphere
	}
	return t;
}

float IntersectLights(inout Intersection it, float3 ori, float3 dir)
{
	float tmin = FLOAT_MAX;
	for (uint i = 0; i < uPass.mLightCountPT; i++)
	{
		float t = RayLight(gLightDataBufferPT[i], ori, dir);
		if (t > 0.0f && t < tmin)
		{
			tmin = t;
			it.mLightIndex = i;
			it.mPointWorld = ori + dir * t;
			it.mPointModel = it.mPointWorld; // lights are in world space
			uint bitsToUnset = ~(PATH_TRACER_INTERSECTION_LIGHT_TYPE_MASK | PATH_TRACER_INTERSECTION_TYPE_MASK);
			it.mIntersectionFlags &= bitsToUnset;
			it.mIntersectionFlags |= (PATH_TRACER_INTERSECTION_TYPE_LIGHT | PATH_TRACER_LIGHT_TYPE_TO_INTERSECTION(gLightDataBufferPT[i].mLightType));
		}
	}
	return tmin;
}

float IntersectTriangles(inout Intersection it, float3 ori, float3 dir)
{
	float tmin = FLOAT_MAX;
	float4x4 modelInv;
	float4x4 model;
	uint meshIndexLast = uPass.mMeshCountPT;
	for (uint i = 0; i < uPass.mTriangleCountPT; i++)
	{
		TrianglePT tri = gTriangleBufferPT[i];
		if (tri.mMeshIndex != meshIndexLast)
		{
			model = gMeshBufferPT[tri.mMeshIndex].mModel;
			modelInv = gMeshBufferPT[tri.mMeshIndex].mModelInv;
			meshIndexLast = tri.mMeshIndex;
		}
		float3 oriModel = mul(modelInv, float4(ori, 1.0f)).xyz;
		float3 dirModel = mul(modelInv, float4(dir, 0.0f)).xyz;
		float dirModelLength = length(dirModel);
		dirModel = normalize(dirModel);
		float tModel = RayTriangle(tri, oriModel, dirModel);
		float t = tModel / dirModelLength; // modelInv * (ori + t * dir) = oriModel + tModel * dirModel, if length(dir) == length(dirModel) == 1, then t * modelINv * dir = tModel * dirModel
		if (t > 0.0f && t < tmin)
		{
			tmin = t;
			it.mTriangleIndex = i;
			it.mPointModel = oriModel + dirModel * tModel;
			it.mPointWorld = mul(model, float4(it.mPointModel, 1)).xyz;
			uint bitsToUnset = ~(PATH_TRACER_INTERSECTION_TYPE_MASK);
			it.mIntersectionFlags &= bitsToUnset;
			it.mIntersectionFlags |= (PATH_TRACER_INTERSECTION_TYPE_MATERIAL);
		}
	}
	return tmin;
}

float IntersectInternal(inout Intersection it, float3 ori, float3 dir)
{
	float tmin = FLOAT_MAX;
	Intersection itTriangles = (Intersection)0;
	Intersection itLights = (Intersection)0;
	ori = ori + dir * PATH_TRACER_SPAWN_RAY_BIAS;
	float tTriangles = IntersectTriangles(itTriangles, ori, dir);
	float tLights = IntersectLights(itLights, ori, dir);
	if (tTriangles < tLights)
	{
		tmin = tTriangles;
		it = itTriangles;
	}
	else
	{
		tmin = tLights;
		it = itLights;
	}
	return tmin;
}

bool Intersect(inout Intersection it, float3 ori, float3 dir)
{
	float tmin = IntersectInternal(it, ori, dir);
	if (tmin > 0.0f && tmin < FLOAT_MAX)
		return true;
	else
		return false;
}

bool IsLightVisibleInternal(uint lightIndex, float3 ori, float3 dir, inout float3 p)
{
	Intersection it = (Intersection)0;
	if(Intersect(it, ori, dir))
	{
		if((it.mIntersectionFlags & PATH_TRACER_INTERSECTION_TYPE_LIGHT) && (lightIndex == it.mLightIndex))
		{
			p = it.mPointWorld;
			return true;
		}
	}
	return false;
}

bool IsLightVisible(uint lightIndex, float3 ori, float3 dir)
{
	float3 p = 0.0f.xxx;
	return IsLightVisibleInternal(lightIndex, ori, dir, p);
}

void BarycentricInterpolate(float3 p, TrianglePT tri, inout float2 uv, inout float3 nor, inout float4 tan)
{
	float s = 0.5f * length(cross(tri.mVertices[0].pos - tri.mVertices[1].pos, tri.mVertices[0].pos - tri.mVertices[2].pos));
	float s0 = 0.5f * length(cross(p - tri.mVertices[1].pos, p - tri.mVertices[2].pos)) / s;
	float s1 = 0.5f * length(cross(p - tri.mVertices[2].pos, p - tri.mVertices[0].pos)) / s;
	float s2 = 0.5f * length(cross(p - tri.mVertices[0].pos, p - tri.mVertices[1].pos)) / s;
	uv = s0 * tri.mVertices[0].uv + s1 * tri.mVertices[1].uv + s2 * tri.mVertices[2].uv;
	nor = s0 * tri.mVertices[0].nor + s1 * tri.mVertices[1].nor + s2 * tri.mVertices[2].nor;
	tan = s0 * tri.mVertices[0].tan + s1 * tri.mVertices[1].tan + s2 * tri.mVertices[2].tan;
}

void EvaluateSurface(inout SurfaceDataInPT sdiPT, Intersection it)
{
	if (it.mIntersectionFlags & PATH_TRACER_INTERSECTION_TYPE_MATERIAL)
	{
		TrianglePT tri = gTriangleBufferPT[it.mTriangleIndex];
		MeshPT mesh = gMeshBufferPT[tri.mMeshIndex];
		float4x4 model = mesh.mModel;
		float4x4 modelInv = mesh.mModelInv;
		float2 uv;
		float3 normalModel;
		float4 tanModel;
		BarycentricInterpolate(it.mPointModel, tri, uv, normalModel, tanModel);
		sdiPT.mUV = uv;
		sdiPT.mNorWorld = mul(float4(normalModel, 0.0f), modelInv).xyz;
		sdiPT.mTanWorld = float4(mul(model, float4(tanModel.xyz, 0.0f)).xyz, tanModel.w);
		sdiPT.mAlbedo = uScene.mStandardColor.rgb;
		sdiPT.mRoughness = uScene.mRoughness;
		sdiPT.mFresnel = uScene.mFresnel;
		sdiPT.mSpecularity = uScene.mSpecularity;
		sdiPT.mMetallic = uScene.mMetallic;
		if (!uScene.mUsePerPassTextures)
		{
			if (mesh.mTextureStartIndex >= 0 && mesh.mTextureCount >= 0)
			{
				// currently only albedo
				sdiPT.mAlbedo = gTextures[mesh.mTextureStartIndex].SampleLevel(gSamplers[mesh.mTextureStartIndex], TransformUV(sdiPT.mUV), 0.0f).rgb;
			}
		}
		sdiPT.mPosWorld = it.mPointWorld;
		// TODO: add support to per object parameter
		//sdiPT.mRoughness = mesh.mRoughness;
		//sdiPT.mFresnel = mesh.mFresnel;
		//sdiPT.mMetallic = mesh.mMetallic;
		sdiPT.mMaterialType = mesh.mMaterialType;
		if (mesh.mMaterialType & MATERIAL_TYPE_EMISSIVE)
			sdiPT.mEmissive = mesh.mEmissive;
	}
	else if (it.mIntersectionFlags & PATH_TRACER_INTERSECTION_TYPE_LIGHT)
	{
		LightData ld = gLightDataBufferPT[it.mLightIndex];
		sdiPT.mEmissive = ld.mColor;
	}
}

float3 EvaluateLight(float3 itPos, float3 lightPos, uint lightIndex, inout float3 wi, inout float pdf)
{
	float3 result = 0.0f.xxx;
	float3 nLight = 0.0f.xxx;
	if (gLightDataBufferPT[lightIndex].mLightType == LIGHT_TYPE_POINT)
	{
		pdf = 0.0f; // delta light has 0 chance to be hit
		nLight = lightPos - itPos;
	}
	else if (gLightDataBufferPT[lightIndex].mLightType == LIGHT_TYPE_QUAD)
	{
		pdf = 1.0f / gLightDataBufferPT[lightIndex].mArea;
		nLight = float3(0.0f, 0.0f, 1.0f);
		nLight = mul(float4(nLight, 0.0f), gLightDataBufferPT[lightIndex].mView).xyz;
	}
	else if (gLightDataBufferPT[lightIndex].mLightType == LIGHT_TYPE_SPHERE)
	{
		//TODO: add support for sphere light
	}
	float3 itPosToLight = lightPos - itPos;
	wi = normalize(itPosToLight);
	float absDot = abs(dot(nLight, -wi)); // solid angle
	if (absDot == 0)
	{
		result = 0.0f.xxx;
		pdf = 0.0f;
	}
	else
	{
		result = gLightDataBufferPT[lightIndex].mColor;
		float d = length(itPosToLight);
		pdf *= d * d / absDot; // adjust due to solid angle
		pdf /= uPass.mLightCountPT; // adjust due to light count
	}
	return result;
}

float3 EvaluateLight(float3 itPos, float3 lightPos, uint lightIndex, inout float pdf)
{
	float3 wi = 0.0f.xxx;
	return EvaluateLight(itPos, lightPos, lightIndex, wi, pdf);
}

// TODO: evaluate and pass in light surface if needed
float3 SampleLight(float3 itPos, float2 xi, uint lightIndex, inout float3 wi, inout float pdf)
{
	float3 result = 0.0f.xxx;
	float3 p = 0.0f.xxx;
	float3 n = 0.0f.xxx;
	if (gLightDataBufferPT[lightIndex].mLightType == LIGHT_TYPE_POINT)
	{
		p = gLightDataBufferPT[lightIndex].mPosWorld;
	}
	else if (gLightDataBufferPT[lightIndex].mLightType == LIGHT_TYPE_QUAD)
	{
		p = float3(xi * 2.0f - 1.0f, 0.0f) * gLightDataBufferPT[lightIndex].mScale;
		// local to world space
		p = mul(gLightDataBufferPT[lightIndex].mViewInv, float4(p, 1.0f)).xyz;
	}
	else if (gLightDataBufferPT[lightIndex].mLightType == LIGHT_TYPE_SPHERE)
	{
		//TODO: add support to sphere light
	}
	return EvaluateLight(itPos, p, lightIndex, wi, pdf);
}

// cosine term in LTE is handled in BRDF functions
float3 EvaluateMaterial(SurfaceDataInPT sdiPT, float3 wo, float3 wi, inout float pdf)
{
	float3 result = 0.0f.xxx;
	if (sdiPT.mMaterialType == MATERIAL_TYPE_GGX)
	{
		float3 wh = normalize(wi + wo);
		pdf = GGX_PDF(wo, wh, sdiPT.mNorWorld, sdiPT.mRoughness);
		SurfaceDataIn sdi = (SurfaceDataIn)sdiPT;
		result = BRDF_GGX(sdi, wi, wo);
	}
	else if (sdiPT.mMaterialType == MATERIAL_TYPE_SPECULAR_REFLECTIIVE)
	{
		// specular material can't be evaluated but only sampled
		pdf = 0.0f;
	}
	else if (sdiPT.mMaterialType == MATERIAL_TYPE_SPECULAR_TRANSMISSIVE)
	{
		// specular material can't be evaluated but only sampled
		pdf = 0.0f;
	}
	return result;
}

float3 SampleMaterial(SurfaceDataInPT sdiPT, float2 xi, float3 wo, inout float3 wi, inout float pdf)
{
	if (sdiPT.mMaterialType == MATERIAL_TYPE_GGX)
	{
		float3 wh = ImportanceSampleGGX(xi, sdiPT.mRoughness, sdiPT.mNorWorld);
		wi = 2.0f * dot(wo, wh) * wh - wo;
	}
	else if (sdiPT.mMaterialType == MATERIAL_TYPE_SPECULAR_REFLECTIIVE)
	{
		// TODO: add support for specular reflective material
	}
	else if (sdiPT.mMaterialType == MATERIAL_TYPE_SPECULAR_TRANSMISSIVE)
	{
		// TODO: add support for specular transimissive material
	}
	return EvaluateMaterial(sdiPT, wo, wi, pdf);
}

float PowerHeuristic(int nf, float fPdf, int ng, float gPdf)
{
	float f = nf * fPdf;
	float g = ng * gPdf;
	return (f * f) / (f * f + g * g);
}

// termination doesn't mean hitting nothing necessarily
bool PathTraceCommon(inout uint seed, inout uint remainingDepth, inout bool isLastBounceSpecular, inout float3 color, inout float3 throughput, inout float3 ori, inout float3 dir, out float3 pos, out bool terminated, uint minDepth, uint maxDepth)
{
	bool hitAnything = false;
	terminated = true;
	float3 wo = -dir;
	// 1. intersection detection
	Intersection it = (Intersection)0;
	if (Intersect(it, ori, dir))
	{
		pos = it.mPointWorld;
		hitAnything = true;

		if (uScene.mPathTracerMode == PATH_TRACER_MODE_DEBUG_TRIANGLE_INTERSECTION) // debug triangle intersection
		{
			color = float3((it.mTriangleIndex + 1.0f) / uPass.mTriangleCountPT, 0.0f, 0.0f);
			return hitAnything;
		}
		else if (uScene.mPathTracerMode == PATH_TRACER_MODE_DEBUG_LIGHT_INTERSECTION) // debug light intersection
		{
			color = float3(0.0f, (it.mLightIndex + 1.0f) / uPass.mLightCountPT, 0.0f);
			return hitAnything;
		}
		else if (uScene.mPathTracerMode == PATH_TRACER_MODE_DEBUG_MESH_INTERSECTION) // debug mesh intersection
		{
			color = float3(0.0f, 0.0f, (gTriangleBufferPT[it.mTriangleIndex].mMeshIndex + 1.0f) / uPass.mMeshCountPT);
			return hitAnything;
		}

		SurfaceDataInPT sdiPT = (SurfaceDataInPT)0;
		EvaluateSurface(sdiPT, it);

		if (uScene.mPathTracerMode == PATH_TRACER_MODE_DEBUG_ALBEDO)
		{
			color = sdiPT.mAlbedo;
			return hitAnything;
		}
		else if (uScene.mPathTracerMode == PATH_TRACER_MODE_DEBUG_NORMAL)
		{
			color = sdiPT.mNorWorld * 0.5f + 0.5f;
			return hitAnything;
		}

		// no need to include light source except for first bounce or specular bounce, 
		// because both direct and indirect light bounces are covered explicitly
		if (remainingDepth == maxDepth || isLastBounceSpecular)
			color += throughput * sdiPT.mEmissive;

		// terminate on lights
		if (it.mIntersectionFlags & PATH_TRACER_INTERSECTION_TYPE_LIGHT)
			return hitAnything;

		float3 Ld = 0.0f.xxx;

		// 2. sample light
		float lightPdf = 1.0f;
		float3 lightWi = 0.0f.xxx;
		float2 lightXi = frandom2(seed);
		uint lightIndex = frandom(seed) * uPass.mLightCountPT;
		float3 lightCol = SampleLight(it.mPointWorld, lightXi, lightIndex, lightWi, lightPdf);
		if (IsNotBlack(lightCol))
		{
			float lightMaterialPdf = 1.0f;
			float3 lightMaterialCol = EvaluateMaterial(sdiPT, wo, lightWi, lightMaterialPdf);
			if (IsNotBlack(lightMaterialCol) && lightMaterialPdf > 0.0f && IsLightVisible(lightIndex, it.mPointWorld, lightWi))
			{
				if (lightPdf > 0.0f)
				{
					float weight = PowerHeuristic(1.0f, lightPdf, 1.0f, lightMaterialPdf);
					Ld += lightCol * lightMaterialCol * weight / lightPdf;
				}
				else // delta light
					Ld += lightCol * lightMaterialCol;
			}
		}

		// 3. sample material
		if (lightPdf > 0.0f)
		{
			float materialPdf = 1.0f;
			float3 materialLightPoint = 0.0f.xxx;
			float3 materialWi = 0.0f.xxx;
			float2 materialXi = frandom2(seed);
			float3 materialCol = SampleMaterial(sdiPT, materialXi, wo, materialWi, materialPdf);
			if (IsNotBlack(materialCol) && materialPdf > 0.0f && IsLightVisibleInternal(lightIndex, it.mPointWorld, materialWi, materialLightPoint))
			{
				float materialLightPdf = 1.0f;
				float3 materialLightCol = EvaluateLight(it.mPointWorld, materialLightPoint, lightIndex, materialLightPdf);
				if (materialLightPdf > 0.0f)
				{
					float weight = PowerHeuristic(1.0f, materialPdf, 1.0f, materialLightPdf);
					Ld += materialCol * materialLightCol * weight / materialPdf;
				}
				// no delta light when sampling material
			}
		}

		// 4. final color
		color += Ld * throughput * uPass.mLightCountPT;

		// 5. GI
		float giPdf = 0.0f;
		float3 giWi = 0.0f.xxx;
		float2 giXi = frandom2(seed);
		float3 giColor = SampleMaterial(sdiPT, giXi, wo, giWi, giPdf);
		if (IsNotBlack(giColor) && giPdf > 0.0f)
		{
			isLastBounceSpecular = (sdiPT.mMaterialType & (MATERIAL_TYPE_SPECULAR_REFLECTIIVE | MATERIAL_TYPE_SPECULAR_TRANSMISSIVE));
			throughput *= giColor / giPdf;
		}
		else
			return hitAnything;

		// 6. spawn new ray
		ori = it.mPointWorld;
		dir = giWi;
	}
	else
		return hitAnything;

	// 2. russian roulette
	if (maxDepth - remainingDepth >= minDepth)
	{
		float russian = 0.0f;
		float throughputMax = max(max(throughput.x, throughput.y), throughput.z);
		if (throughputMax < russian)
			return terminated;
		throughput /= throughputMax;
	}

	remainingDepth--;
	terminated = bool(remainingDepth <= 0);
	return hitAnything;
}

bool PathTrace(uint seed, float3 eyePos, float3 eyeDir, uint minDepth, uint maxDepth, out float3 finalColor, out float3 firstPos, bool debugPixel)
{
	uint remainingDepth = maxDepth;
	float3 throughput = 1.0f.xxx;
	float3 color = 0.0f.xxx;
	float3 ori = eyePos;
	float3 dir = eyeDir;
	bool terminated = false;
	bool hitAnything = false;
	bool firstBounce = true;

	while (!terminated && remainingDepth > 0)
	{
		float3 pos = 0.0f.xxx;
		float3 oldOri = ori;
		float3 oldDir = dir;
		bool isLastBounceSpecular = false;
		bool hitSomething = PathTraceCommon(seed, remainingDepth, isLastBounceSpecular, color, throughput, ori, dir, pos, terminated, minDepth, maxDepth);
		hitAnything = hitAnything || hitSomething;
		
		if (firstBounce && hitSomething)
			firstPos = pos;

		if (debugPixel)
		{
			Ray ray = InitRay(remainingDepth, seed, oldOri, oldDir);
			ray.mNextOri = ori;
			ray.mNextDir = dir;
			ray.mReadyForDebug = hitSomething;
			ray.mTerminated = terminated;
			ray.mResultWritten = false;
			ray.mIsLastBounceSpecular = isLastBounceSpecular;
			ray.mEnd = pos;
			ray.mThroughput = throughput;
			ray.mResult = color;
			if (maxDepth > remainingDepth)
				gDebugRayBuffer[maxDepth - remainingDepth - 1] = ray;
		}

		firstBounce = false;
	}
	finalColor = color;
	return hitAnything;
}

bool PathTraceOnce(inout Ray ray, uint minDepth, uint maxDepth, out float3 pos, bool debugPixel)
{
	uint seed = ray.mSeed;
	uint remainingDepth = ray.mRemainingDepth;
	bool terminated = ray.mTerminated; // TODO: order rays so that we only launch compute shaders on non-terminated rays, this will require a new way to address pixel at a given screen position
	bool isLastBounceSpecular = ray.mIsLastBounceSpecular;
	float3 color = ray.mResult;
	float3 throughput = ray.mThroughput;
	float3 ori = ray.mNextOri;
	float3 dir = ray.mNextDir;
	bool hitAnything = false;

	if (!terminated && remainingDepth > 0)
	{
		ray.mOri = ori;
		ray.mDir = dir;
		hitAnything = PathTraceCommon(seed, remainingDepth, isLastBounceSpecular, color, throughput, ori, dir, pos, terminated, minDepth, maxDepth);
		ray.mNextOri = ori;
		ray.mNextDir = dir;
		ray.mReadyForDebug = hitAnything;
		ray.mRemainingDepth = remainingDepth;
		ray.mTerminated = terminated;
		ray.mSeed = seed;
		ray.mEnd = pos;
		ray.mThroughput = throughput;
		ray.mResult = color;
		if (debugPixel && maxDepth > remainingDepth)
			gDebugRayBuffer[maxDepth - remainingDepth - 1] = ray;
	}
	return hitAnything;
}

[numthreads(PATH_TRACER_THREAD_COUNT_X, PATH_TRACER_THREAD_COUNT_Y, PATH_TRACER_THREAD_COUNT_Z)]
void main(uint3 gDispatchThreadID : SV_DispatchThreadID)
{
	uint2 screenPos = gDispatchThreadID.xy;
	uint2 screenSize = uint2(PATH_TRACER_BACKBUFFER_WIDTH, PATH_TRACER_BACKBUFFER_HEIGHT);
	if (screenPos.x < screenSize.x && screenPos.y < screenSize.y)
	{
		bool debugPixel = (uScene.mPathTracerUpdateDebug && screenPos.x == uScene.mPathTracerDebugPixelX && screenPos.y == uScene.mPathTracerDebugPixelY);
		// set up RNG, dir
		uint rng_seed = hash(hash(hash(uFrame.mFrameCountSinceGameStart) + screenPos.x) + screenPos.y);
		float2 screenPosJitter = frandom2(rng_seed);
		float4 ndcNearPos = float4(float2(screenPos + screenPosJitter) / float2(screenSize) * float2(2.0f, -2.0f) - float2(1.0f, -1.0f), REVERSED_Z_SWITCH(0.0f, 1.0f), 1.0f);
		float4 viewNearPos = mul(uPass.mProjInv, ndcNearPos * uPass.mNearClipPlane);
		viewNearPos /= viewNearPos.w;
		float3 viewDir = normalize(viewNearPos.xyz);
		float3 viewDirWorld = normalize(mul(uPass.mViewInv, float4(viewDir, 0.0f)).xyz);
		// init buffers and debug rays
		if (!uScene.mPathTracerCurrentSampleIndex)
		{
			gBackbufferPT[screenPos] = 0.0f.xxxx;
			gDepthbufferPT[screenPos] = DEPTH_FAR_REVERSED_Z_SWITCH;
		}
		
		// multi pass path tracing
		if (uScene.mPathTracerMode == PATH_TRACER_MODE_PROGRESSIVE)
		{
			// 1 depth per sample per frame
			uint pixelIndex = screenPos.x + screenPos.y * screenSize.x;
			Ray ray = gRayBuffer[pixelIndex];
			bool firstBounce = false;
			// init debug ray
			// multi pass path tracing renders 1 depth per sample per frame
			// so we init the debug ray buffer when current depth is 0
			if (!uScene.mPathTracerCurrentDepth && debugPixel)
			{
				for (int i = 0; i < PATH_TRACER_MAX_DEPTH_MAX; i++)
				{
					gDebugRayBuffer[i] = InitRay(uScene.mPathTracerMaxDepth, rng_seed, uPass.mEyePos, viewDirWorld);
				}
			}
			// init current ray
			if (!uScene.mPathTracerCurrentDepth)
			{
				ray = InitRay(uScene.mPathTracerMaxDepth, rng_seed, uPass.mEyePos, viewDirWorld);
				firstBounce = true;
			}
			float3 firstPos = 0.0f.xxx;
			float firstDepth = DEPTH_FAR_REVERSED_Z_SWITCH;
			bool hitAnything = PathTraceOnce(ray, uScene.mPathTracerMinDepth, uScene.mPathTracerMaxDepth, firstPos, debugPixel);
			// average the total estimation, write to backbuffer
			if (ray.mTerminated && !ray.mResultWritten)
			{
				ray.mResultWritten = true; // this is to prevent adding early terminated rays multiple times
				gBackbufferPT[screenPos] = (gBackbufferPT[screenPos] * (float)uScene.mPathTracerCurrentSampleIndex + float4(ray.mResult, 1.0f)) / (uScene.mPathTracerCurrentSampleIndex + 1.0f);
			}
			// transform world position to depth, write to depthbuffer
			if (hitAnything && firstBounce)
			{
				float4 posProj = mul(uPass.mProj, mul(uPass.mView, float4(firstPos, 1.0f)));
				firstDepth = posProj.z / posProj.w;
			}
			gDepthbufferPT[screenPos] = firstDepth;
			// write to ray buffer
			gRayBuffer[pixelIndex] = ray;
		}
		else // single pass path tracing
		{
			// 1 sample per frame
			float3 finalColor = 0.0f.xxx;
			float3 firstPos = 0.0f.xxx;
			float firstDepth = DEPTH_FAR_REVERSED_Z_SWITCH;
			// init debug ray
			// single pass path tracing renders all depth of 1 sample per frame
			// so we init the debug ray buffer every frame
			if (debugPixel)
			{
				for (int i = 0; i < PATH_TRACER_MAX_DEPTH_MAX; i++)
				{
					gDebugRayBuffer[i] = InitRay(uScene.mPathTracerMaxDepth, rng_seed, uPass.mEyePos, viewDirWorld);
				}
			}
			bool hitAnything = PathTrace(rng_seed, uPass.mEyePos, viewDirWorld, uScene.mPathTracerMinDepth, uScene.mPathTracerMaxDepth, finalColor, firstPos, debugPixel);
			// average the total estimation, write to backbuffer
			gBackbufferPT[screenPos] = (gBackbufferPT[screenPos] * (float)uScene.mPathTracerCurrentSampleIndex + float4(finalColor, 1.0f)) / (uScene.mPathTracerCurrentSampleIndex + 1.0f);
			// transform world position to depth, write to depthbuffer
			if (hitAnything)
			{
				float4 posProj = mul(uPass.mProj, mul(uPass.mView, float4(firstPos, 1.0f)));
				firstDepth = posProj.z / posProj.w;
			}
			gDepthbufferPT[screenPos] = firstDepth;
		}
	}
}
