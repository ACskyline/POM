#define VS_OUTPUT_LITE
#define VS_INPUT_INSTANCE_ID
#define VS_INPUT_VERTEX_ID

#include "GlobalInclude.hlsl"

StructuredBuffer<Ray> gDebugRayBuffer : register(t0, SPACE(PASS));

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	uint rayIndex = clamp(input.instanceID / PATH_TRACER_DEBUG_LINE_COUNT_PER_RAY, 0, PATH_TRACER_MAX_DEPTH_MAX);
	uint debugLineIndex = input.instanceID % PATH_TRACER_DEBUG_LINE_COUNT_PER_RAY;
	Ray ray = gDebugRayBuffer[rayIndex];
	output.pos = float4(0, 0, -1, 1); // initialize the line to be clipped
	if (ray.mReadyForDebug)
	{
		float3 pos = 0.0f.xxx;
		float4 col = float4(1.0f, 1.0f, 0.0f, 1.0f);
		if (debugLineIndex == 0) // main ray
		{
			if ((input.vertexID & 1) == 0) // start of the line
			{
				pos = ray.mOri;
				col = float4(1.0f, 0.0f, 0.0f, 1.0f);
			}
			else if ((input.vertexID & 1) == 1) // end of the line
			{
				if (HitAnything(ray))
					pos = ray.mEnd;
				else // if the doesn't hit anything
					pos = ray.mOri + uScene.mPathTracerDebugDirLength * normalize(ray.mNextDir.xyz);
				if (ray.mTerminated)
					col = float4(0.0f, 0.0f, 1.0f, 1.0f);
				else
					col = float4(0.0f, 1.0f, 0.0f, 1.0f);
			}
		}
		else if (debugLineIndex == 1 && uScene.mPathTracerEnableDebugSampleRay) // sample light
		{
			if ((input.vertexID & 1) == 0) // start of the line
			{
				pos = ray.mEnd;
				col = float4(0.0f, 1.0f, 0.0f, 1.0f);
			}
			else if ((input.vertexID & 1) == 1) // end of the line
			{
				pos = ray.mLightSampleRayEnd.w ? ray.mLightSampleRayEnd.xyz : ray.mEnd + uScene.mPathTracerDebugDirLength * normalize(ray.mLightSampleRayEnd.xyz);
				col = float4(0.0f, 1.0f, 1.0f, 1.0f);
			}
		}
		else if (debugLineIndex == 2 && uScene.mPathTracerEnableDebugSampleRay) // sample material
		{
			if ((input.vertexID & 1) == 0) // start of the line
			{
				pos = ray.mEnd;
				col = float4(0.0f, 1.0f, 0.0f, 1.0f);
			}
			else if ((input.vertexID & 1) == 1) // end of the line
			{
				pos = ray.mMaterialSampleRayEnd.w ? ray.mMaterialSampleRayEnd.xyz : ray.mEnd + uScene.mPathTracerDebugDirLength * normalize(ray.mMaterialSampleRayEnd.xyz);
				col = float4(1.0f, 1.0f, 1.0f, 1.0f);
			}
		}
		else
		{
			// if pathTracerEnableDebugSampleRay is false, clip the line
			return output;
		}
		output.pos = mul(uPass.mProj, mul(uPass.mView, float4(pos, 1.0f)));
		output.col = col;
	}
	return output;
}
