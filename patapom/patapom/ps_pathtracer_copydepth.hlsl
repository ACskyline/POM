#define VS_OUTPUT_LITE

#define PS_DEPTH_OUTPUT
#ifndef PS_COLOR_OUTPUT_COUNT
#define PS_COLOR_OUTPUT_COUNT 0
#endif

#include "GlobalInclude.hlsl"

Texture2D SourceDepth : register(t0, SPACE(PASS));
SamplerState SourceDepthSampler : register(s0, SPACE(PASS));

PS_OUTPUT main(VS_OUTPUT input)
{
	PS_OUTPUT output;
	float2 uv = input.uv;
	output.depth = SourceDepth.Sample(SourceDepthSampler, TransformUV(uv));
	return output;
}
