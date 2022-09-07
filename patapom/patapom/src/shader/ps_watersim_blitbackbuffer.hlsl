#define VS_OUTPUT_LITE
#define CUSTOM_PASS_UNIFORM

#ifndef PS_COLOR_OUTPUT_COUNT
#define PS_COLOR_OUTPUT_COUNT 1
#endif

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

Texture2D<uint> gWaterSimDepthBufferMax : register(t0, SPACE(PASS));
Texture2D<uint> gWaterSimDepthBufferMin : register(t1, SPACE(PASS));
Texture2D<float4> gWaterSimDebugBackbuffer : register(t2, SPACE(PASS));

PS_OUTPUT main(VS_OUTPUT input)
{
	PS_OUTPUT output;
	int3 uvw = int3(TransformUV(input.uv) * uPass.mWaterSimBackbufferSize, 0);
	float depthMax = UnpackWaterSimParticleDepth(gWaterSimDepthBufferMax.Load(uvw).r);
	float depthMin = UnpackWaterSimParticleDepth(gWaterSimDepthBufferMin.Load(uvw).r);
	float4 debugCol = gWaterSimDebugBackbuffer.Load(uvw);
	output.col0.rgb = debugCol.a * debugCol.rgb + (1.0f - debugCol.a) * float3(0.0f, depthMax, depthMin);
	output.col0.a = 1.0f;
	return output;
}
