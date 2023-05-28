#define VS_OUTPUT_LITE
#define CUSTOM_PASS_UNIFORM
#define PS_COLOR_OUTPUT_COUNT 1

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

Texture2D<uint> gWaterSimDepthBufferMax : register(t0, SPACE(PASS));
Texture2D<uint> gWaterSimDepthBufferMin : register(t1, SPACE(PASS));
Texture2D<float4> gWaterSimDebugBackbuffer : register(t2, SPACE(PASS));

PS_OUTPUT main(VS_OUTPUT input)
{
	PS_OUTPUT output;
	int3 uvw = int3(TransformUV(input.uv) * uPass.mWaterSimBackbufferSize, 0);
	uint packedMax = gWaterSimDepthBufferMax.Load(uvw).r;
	uint packedMin = gWaterSimDepthBufferMin.Load(uvw).r;
	bool hasWater = (packedMax != 0) && (packedMin != WATERSIM_DEPTHBUFFER_MAX);
	float depthMax = UnpackWaterSimParticleDepth(packedMax);
	float depthMin = UnpackWaterSimParticleDepth(packedMin);
	float4 debugCol = gWaterSimDebugBackbuffer.Load(uvw);
	float3 waterCol = float3(0.0f, 0.0f, saturate(depthMax - depthMin));
	// blend in debug ui
	output.col0.rgb = debugCol.a * debugCol.rgb + (1.0f - debugCol.a) * waterCol;
	output.col0.a = hasWater ? 1.0f : 0.0f; // any(output.col0.rgb > 0.0f.xxx) ? 0.0f : 0.0f; // 
	return output;
}
