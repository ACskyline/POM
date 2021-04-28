#ifndef POM_H
#define POM_H

#include "GlobalInclude.hlsl"

Texture2D pHeightMap : register(t2, SPACE(PASS));
SamplerState pHeightMapSampler : register(s2, SPACE(PASS));

Texture2D oHeightMap : register(t2, SPACE(OBJECT));
SamplerState oHeightMapSampler : register(s2, SPACE(OBJECT));

float2 POM(SurfaceDataOut sd, out float3 newPosWorld)
{
    float3 marchVec = -uScene.mPomScale * (sd.eyeDirWorld / dot(sd.eyeDirWorld, sd.norWorld)); // normalize on normal direction and then scale it
    float3 marchUV = marchVec - dot(marchVec, sd.norWorld) * sd.norWorld;
    float2 projectedMarchUV = float2(dot(marchUV, sd.tanWorld), dot(marchUV, sd.bitanWorld));
    float curHeight = 1.0f - uScene.mPomBias;
    float curRefHeight = curHeight;
    float prevHeight = 1.0f - uScene.mPomBias;
    float prevRefHeight = prevHeight;
    float deltaHeight = -abs(dot(marchVec, sd.norWorld)) / (float) uScene.mPomMarchStep; // change to marching inwards
    float3 deltaPos = marchVec / (float) uScene.mPomMarchStep; // change to marching inwards
    float3 pomPosWorld = sd.posWorld;
    float2 curUV = sd.mUV;
    float2 prevUV = curUV;
    float2 deltaUV = projectedMarchUV / (float) uScene.mPomMarchStep; // change to marching inwards
    
    [loop]
    for (uint i = 0; i < uScene.mPomMarchStep; i++)
    {

		float refHeight = 0.0f;
		if(uScene.mUsePerPassTextures)
			refHeight = pHeightMap.Sample(pHeightMapSampler, TransformUV(curUV));
		else
			refHeight = oHeightMap.Sample(oHeightMapSampler, TransformUV(curUV));
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
    return lerp(sd.mUV, curUV, sin(HALF_PI * viewPomScale));
}

#endif