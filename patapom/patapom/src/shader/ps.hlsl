#define PS_COLOR_OUTPUT_COUNT 4

#include "ShaderInclude.hlsli"
#include "Lighting.hlsli"

Texture2D pAlbedoTex : register(t0, SPACE(PASS));
Texture2D pNormalTex : register(t1, SPACE(PASS));

Texture2D oAlbedoTex : register(t0, SPACE(OBJECT));
Texture2D oNormalTex : register(t1, SPACE(OBJECT));

PS_OUTPUT main(VS_OUTPUT input)
{
    SurfaceDataOut sdo;
    sdo.mUV = input.uv;
    sdo.posWorld = input.posWorld;
    sdo.norWorld = normalize(input.norWorld);
    sdo.tanWorld = normalize(input.tanWorld);
    sdo.bitanWorld = normalize(input.bitanWorld);
    sdo.eyeDirWorld = normalize(uPass.mEyePos - sdo.posWorld);
    sdo.albedo = uScene.mStandardColor.rgb;
	// TODO: add support to per object parameter
	sdo.mRoughness = uScene.mRoughness;
	sdo.mFresnel = uScene.mFresnel;
	sdo.mSpecularity = uScene.mSpecularity;
	sdo.mMetallic = uScene.mMetallic;

#ifdef USE_POM
    float3 newPosWorld;
    float2 newUV;
    newUV = POM(sdo, newPosWorld);
    sdo.mUV = newUV;
    sdo.posWorld = newPosWorld;
#endif
    
    // recalculate depth after pom
    float zView = mul(uPass.mView, float4(sdo.posWorld, 1.0f)).z;
    sdo.mQuantizedZView = QuantizeDepth(zView, uPass.mNearClipPlane, uPass.mFarClipPlane);
    
    // sample textures
	float4 normalCol = 0.0f.xxxx;
    if (uScene.mUsePerPassTextures)
    {
        sdo.albedo = pAlbedoTex.Sample(gSamplerLinear, TransformUV(sdo.mUV)).rgb;
        normalCol = pNormalTex.Sample(gSamplerLinear, TransformUV(sdo.mUV));
    }
	else
	{
		sdo.albedo = oAlbedoTex.Sample(gSamplerLinear, TransformUV(sdo.mUV)).rgb;
		normalCol = oNormalTex.Sample(gSamplerLinear, TransformUV(sdo.mUV));
	}
	float3x3 tanToWorld = float3x3(sdo.tanWorld, sdo.bitanWorld, sdo.norWorld); // constructed row by row
	float3 paintedNormal = UnpackNormal(normalCol.rgb);
	sdo.norWorld = normalize(mul(paintedNormal, tanToWorld)); // post multiply because tanToWorld is constructed row by row
    
    return PackGbuffer(sdo);
}
