#include "GlobalInclude.hlsli"

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
    float3 lightPosWorld = float3(2.0f, 2.0f, 2.0f);
    float3 eyeDir = normalize(eyePos - posWorld);
    float3 lightDir = normalize(lightPosWorld - posWorld);
    float3 col = 0.0f.xxx;
    
    if (mode == 0)
    {
        float4 albedoCol = albedoTex.Sample(albedoSampler, uv);
        float4 normalCol = normalTex.Sample(normalSampler, uv);
        float3 paintedNormal = normalCol.rgb * 2.0f - 1.0f;
        float3x3 tanToWorld = float3x3(tanWorld, bitanWorld, norWorld);
        float3 mappedNormal = normalize(mul(paintedNormal, tanToWorld));
        
        float3 halfDir = normalize((eyeDir + lightDir) / 2.0f);
        float atten = exp(-length(lightPosWorld - posWorld) / 8.0f); // exponential fake light attenuation
        
        //col = albedoCol * (dot(mappedNormal, lightDir) * 0.5f + 0.5f) * atten; // half lambert
        col = albedoCol * saturate(pow(dot(halfDir, mappedNormal), 5.0f)) * atten; // blinn phong
    }
    else if(mode == 1)
    {
        float3 marchVec = marchScale * (eyeDir / dot(eyeDir, norWorld)); // normalize on normal direction and then scale it
        float3 marchUV = marchVec - dot(marchVec, norWorld) * norWorld;
        float2 projectedMarchUV = float2(dot(marchUV, tanWorld), dot(marchUV, bitanWorld));
        float curHeight = 1.0f - marchBias;
        float curRefHeight = curHeight;
        float prevHeight = 1.0f - marchBias;
        float prevRefHeight = prevHeight;
        float deltaHeight = -abs(dot(marchVec, norWorld)) / (float) marchStep;
        float2 curUV = uv;
        float2 prevUV = curUV;
        float2 deltaUV = -projectedMarchUV / (float)marchStep;
        [loop]
        for (uint i = 0; i < marchStep; i++)
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
            }
        }

        float deltaHeightPrevAbs = abs(prevHeight - prevRefHeight);
        float deltaHeightCurAbs = abs(curHeight - curRefHeight);
        curUV = lerp(prevUV, curUV, deltaHeightPrevAbs / (deltaHeightPrevAbs + deltaHeightCurAbs));
        float pomScale = dot(eyeDir, norWorld);
        uv = lerp(uv, curUV, sin(Half_Pi * pomScale));
        float4 albedoCol = albedoTex.Sample(albedoSampler, uv);
        float4 normalCol = normalTex.Sample(normalSampler, uv);
        float3 paintedNormal = normalCol.rgb * 2.0f - 1.0f;
        float3x3 tanToWorld = float3x3(tanWorld, bitanWorld, norWorld);
        float3 mappedNormal = normalize(mul(paintedNormal, tanToWorld));
        
        float3 halfDir = normalize((eyeDir + lightDir) / 2.0f);
        float atten = exp(-length(lightPosWorld - posWorld) / 8.0f); // exponential fake light attenuation
        
        //col = albedoCol * (dot(mappedNormal, lightDir) * 0.5f + 0.5f) * atten; // half lambert
        col = albedoCol * saturate(pow(dot(halfDir, mappedNormal), 5.0f)) * atten; // blinn phong
        //col = float3(uv, 0.0f);
    }
    else if(mode == 2)
        col = albedoTex.Sample(albedoSampler, uv).rgb;
    else if (mode == 3)
        col = normalTex.Sample(normalSampler, uv).rgb;
    else if (mode == 4)
        col = heightTex.Sample(heightSampler, uv).rgb;
    
    output.col0 = float4(col, 1.0f);
    return output;
}
