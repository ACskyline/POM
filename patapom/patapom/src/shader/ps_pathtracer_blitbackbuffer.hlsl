#define VS_OUTPUT_LITE
#define PS_COLOR_OUTPUT_COUNT 1

#include "ShaderInclude.hlsli"

Texture2D gBackbufferPT : register(t0, SPACE(PASS));
Texture2D gDebugBackbufferPT : register(t1, SPACE(PASS));

PS_OUTPUT main(VS_OUTPUT input)
{
	PS_OUTPUT output;
	float2 uv = input.uv;
	float4 col = gBackbufferPT.Sample(gSamplerLinear, TransformUV(uv));
	float4 colDebug = 0.0f.xxxx;
	if (uScene.mPathTracerEnableDebug)
		colDebug = gDebugBackbufferPT.Sample(gSamplerLinear, TransformUV(uv));
	output.col0 = float4(col.rgb * (1.0f - colDebug.a) + colDebug.rgb * colDebug.a, 1.0f);
	return output;
}
