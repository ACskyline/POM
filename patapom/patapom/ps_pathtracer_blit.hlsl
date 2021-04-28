#define VS_OUTPUT_LITE

#ifndef PS_COLOR_OUTPUT_COUNT
#define PS_COLOR_OUTPUT_COUNT 1
#endif

#include "GlobalInclude.hlsl"

Texture2D gBackbufferPT : register(t0, SPACE(PASS));
SamplerState gBackbufferSamplerPT : register(s0, SPACE(PASS));

Texture2D gDebugBackbufferPT : register(t1, SPACE(PASS));
SamplerState gDebugBackbufferSamplerPT : register(s1, SPACE(PASS));

PS_OUTPUT main(VS_OUTPUT input)
{
	PS_OUTPUT output;
	float2 uv = input.uv;
	float4 col = gBackbufferPT.Sample(gBackbufferSamplerPT, TransformUV(uv));
	float4 colDebug = 0.0f.xxxx;
	if (uScene.mPathTracerEnableDebug)
		colDebug = gDebugBackbufferPT.Sample(gDebugBackbufferSamplerPT, TransformUV(uv));
	output.col0 = float4(col.rgb * (1.0f - colDebug.a) + colDebug.rgb * colDebug.a, 1.0f);
	return output;
}
