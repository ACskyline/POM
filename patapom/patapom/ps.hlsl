#include "GlobalInclude.hlsl"
#include "Lighting.hlsl"

#define SUN_RADIANCE_SCALING_FACTOR 0.05

Texture2D albedoTex : register(t0, SPACE(PASS));
SamplerState albedoSampler : register(s0, SPACE(PASS));

Texture2D normalTex : register(t1, SPACE(PASS));
SamplerState normalSampler : register(s1, SPACE(PASS));

Texture2D heightTex : register(t2, SPACE(PASS));
SamplerState heightSampler : register(s2, SPACE(PASS));

PS_OUTPUT main(VS_OUTPUT input)
{
    PS_OUTPUT output;
    float2 uv = input.uv;
    float3 posWorld = input.posWorld;
    float3 norWorld = normalize(input.norWorld);
    float3 tanWorld = normalize(input.tanWorld);
    float3 bitanWorld = normalize(input.bitanWorld);
    float3 eyeDir = normalize(pEyePos - posWorld);
    float3 sunDir = GetSunDir(sSunAzimuth, sSunAltitude);
    float3 col = 0.0f.xxx;
    
    if (sMode == 0)
    {
        float4 albedoCol = albedoTex.Sample(albedoSampler, uv);
        float4 normalCol = normalTex.Sample(normalSampler, uv);
        float3 paintedNormal = normalCol.rgb * 2.0f - 1.0f;
        float3x3 tanToWorld = float3x3(tanWorld, bitanWorld, norWorld);
        float3 mappedNormal = normalize(mul(paintedNormal, tanToWorld));
        
        float3 halfDir = normalize(eyeDir + sunDir);
        
        //col = albedoCol * (dot(mappedNormal, sunDir) * 0.5f + 0.5f) * atten; // half lambert
        if (dot(sunDir, mappedNormal) > 0.0f)
            col = albedoCol * saturate(pow(dot(halfDir, mappedNormal), 5.0f)); // blinn phong
    }
    else if (sMode == 1)
    {
        float3 marchVec = sPomScale * (eyeDir / dot(eyeDir, norWorld)); // normalize on normal direction and then scale it
        float3 marchUV = marchVec - dot(marchVec, norWorld) * norWorld;
        float2 projectedMarchUV = float2(dot(marchUV, tanWorld), dot(marchUV, bitanWorld));
        float curHeight = 1.0f - sPomBias;
        float curRefHeight = curHeight;
        float prevHeight = 1.0f - sPomBias;
        float prevRefHeight = prevHeight;
        float deltaHeight = -abs(dot(marchVec, norWorld)) / (float) sPomMarchStep; // change to marching inwards
        float3 deltaPos = -marchVec / (float) sPomMarchStep; // change to marching inwards
        float3 pomPosWorld = posWorld;
        float2 curUV = uv;
        float2 prevUV = curUV;
        float2 deltaUV = -projectedMarchUV / (float) sPomMarchStep; // change to marching inwards
        [loop]
        for (uint i = 0; i < sPomMarchStep; i++)
        {
            float refHeight = heightTex.Sample(heightSampler, curUV);
            curRefHeight = refHeight;
            if (curHeight < refHeight)
            {
                break;
            }
            else
            {
                prevRefHeight = curRefHeight;
                prevHeight = curHeight;
                prevUV = curUV;
                curHeight += deltaHeight;
                curUV += deltaUV;
                pomPosWorld += deltaPos;
            }
        }

        posWorld = pomPosWorld;
        float deltaHeightPrevAbs = abs(prevHeight - prevRefHeight);
        float deltaHeightCurAbs = abs(curHeight - curRefHeight);
        curUV = lerp(prevUV, curUV, deltaHeightPrevAbs / (deltaHeightPrevAbs + deltaHeightCurAbs));
        float viewPomScale = dot(eyeDir, norWorld);
        uv = lerp(uv, curUV, sin(HALF_PI * viewPomScale));
        float4 albedoCol = albedoTex.Sample(albedoSampler, uv);
        float4 normalCol = normalTex.Sample(normalSampler, uv);
        float3 paintedNormal = normalCol.rgb * 2.0f - 1.0f;
        float3x3 tanToWorld = float3x3(tanWorld, bitanWorld, norWorld);
        float3 mappedNormal = normalize(mul(paintedNormal, tanToWorld));
        
        // sun light
        // TODO: add support for sun light shadow map
        float3 halfDir = normalize(eyeDir + sunDir);
        float brdf = saturate(pow(dot(halfDir, mappedNormal), 5.0f)); // TODO: add support for GGX, blinn phong for now
        if (dot(sunDir, mappedNormal) > 0.0f)
            col += albedoCol.rgb * brdf * sSunRadiance * SUN_RADIANCE_SCALING_FACTOR;
    
        SurfaceData sd;
        sd.albedo = albedoCol;
        sd.posWorld = posWorld;
        sd.normalWorld = mappedNormal;
        sd.geomNormalWorld = norWorld;
        sd.eyeDirWorld = eyeDir;
        sd.sunDirWorld = sunDir;
        
        // scene lights
        col += albedoCol.rgb * brdf * Lighting(sd);
    }
    else if (sMode == 2) // albedo
        col = albedoTex.Sample(albedoSampler, uv).rgb;
    else if (sMode == 3) // normal
    {
        float4 albedoCol = albedoTex.Sample(albedoSampler, uv);
        float4 normalCol = normalTex.Sample(normalSampler, uv);
        float3 paintedNormal = normalCol.rgb * 2.0f - 1.0f;
        float3x3 tanToWorld = float3x3(tanWorld, bitanWorld, norWorld);
        float3 mappedNormal = normalize(mul(paintedNormal, tanToWorld));
        
        col = mappedNormal * 0.5f + 0.5f;
        //col = normalTex.Sample(normalSampler, uv).rgb;
    }
    else if (sMode == 4) // height
        col = heightTex.Sample(heightSampler, uv).rgb;
    else if (sMode == 5) // uv
        col = float3(uv.xy, 0.0f);
    else if (sMode == 6) // sunDir
        col = sunDir * 0.5f + 0.5f;
    else if (sMode == 7) // debug
        col = 0.0f.xxx;
    else if (sMode == 8) // debug
        col = 0.0f.xxx;
    
    output.col0 = float4(col, 1.0f);
    return output;
}
