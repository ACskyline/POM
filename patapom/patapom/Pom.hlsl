#ifndef POM_H
#define POM_H

#include "GlobalInclude.hlsl"

Texture2D heightTex : register(t2, SPACE(PASS));
SamplerState heightSampler : register(s2, SPACE(PASS));

float2 POM(SurfaceDataOut sd, out float3 newPosWorld)
{
    float3 marchVec = sPomScale * (sd.eyeDirWorld / dot(sd.eyeDirWorld, sd.norWorld)); // normalize on normal direction and then scale it
    float3 marchUV = marchVec - dot(marchVec, sd.norWorld) * sd.norWorld;
    float2 projectedMarchUV = float2(dot(marchUV, sd.tanWorld), dot(marchUV, sd.bitanWorld));
    float curHeight = 1.0f - sPomBias;
    float curRefHeight = curHeight;
    float prevHeight = 1.0f - sPomBias;
    float prevRefHeight = prevHeight;
    float deltaHeight = -abs(dot(marchVec, sd.norWorld)) / (float) sPomMarchStep; // change to marching inwards
    float3 deltaPos = -marchVec / (float) sPomMarchStep; // change to marching inwards
    float3 pomPosWorld = sd.posWorld;
    float2 curUV = sd.uv;
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

    newPosWorld = pomPosWorld;
    float deltaHeightPrevAbs = abs(prevHeight - prevRefHeight);
    float deltaHeightCurAbs = abs(curHeight - curRefHeight);
    curUV = lerp(prevUV, curUV, deltaHeightPrevAbs / (deltaHeightPrevAbs + deltaHeightCurAbs));
    float viewPomScale = dot(sd.eyeDirWorld, sd.norWorld);
    return lerp(sd.uv, curUV, sin(HALF_PI * viewPomScale));
}

#endif