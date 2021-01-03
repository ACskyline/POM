#define VS_OUTPUT_LITE

#ifndef PS_OUTPUT_COUNT
#define PS_OUTPUT_COUNT 1
#endif

#include "GlobalInclude.hlsl"

Texture2D ptBackbuffer : register(t0, SPACE(PASS));
SamplerState ptBackbufferSampler : register(s0, SPACE(PASS));

PS_OUTPUT main(VS_OUTPUT input)
{
	PS_OUTPUT output;
	float2 uv = input.uv;
	output.col0 = ptBackbuffer.Sample(ptBackbufferSampler, TransformUV(uv));
	return output;
}
