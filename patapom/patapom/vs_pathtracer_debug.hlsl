#define VS_OUTPUT_LITE
#define VS_INPUT_INSTANCE_ID
#define VS_INPUT_VERTEX_ID

#include "GlobalInclude.hlsl"

StructuredBuffer<Ray> gDebugRayBuffer : register(t0, SPACE(PASS));

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	uint rayIndex = clamp(input.instanceID, 0, PATH_TRACER_MAX_DEPTH_MAX);
	Ray ray = gDebugRayBuffer[rayIndex];
	if (ray.mReadyForDebug)
	{
		float3 pos = 0.0f.xxx;
		float4 col = float4(1.0f, 1.0f, 0.0f, 1.0f);
		if ((input.vertexID & 1) == 0) // start of the line
		{
			pos = ray.mOri;
			col = float4(1.0f, 0.0f, 0.0f, 1.0f);
		}
		else if ((input.vertexID & 1) == 1) // end of the line
		{
			pos = ray.mEnd;
			if (ray.mTerminated)
				col = float4(0.0f, 0.0f, 1.0f, 1.0f);
			else
				col = float4(0.0f, 1.0f, 0.0f, 1.0f);
		}
		output.pos = mul(uPass.mProj, mul(uPass.mView, float4(pos, 1.0f)));
		output.col = col;
	}
	else // clip if not ready for debug
		output.pos = float4(0, 0, -1, 1);
	return output;
}
