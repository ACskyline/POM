#define VS_OUTPUT_LITE
#define PS_DEPTH_OUTPUT
#define PS_COLOR_OUTPUT_COUNT 0

#include "ShaderInclude.hlsli"

Texture2D SourceDepth : register(t0, SPACE(PASS));

PS_OUTPUT main(VS_OUTPUT input)
{
	PS_OUTPUT output;
	float2 uv = input.uv;
	output.depth = SourceDepth.Sample(gSamplerLinear, TransformUV(uv));
	return output;
}
