#define VS_OUTPUT_LITE
#define PS_COLOR_OUTPUT_COUNT 1

#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

PS_OUTPUT main(VS_OUTPUT input)
{
	PS_OUTPUT output;
	if (any(abs(input.col.xy - 0.5f) > 0.45f))
		output.col0 = float4(0.1f, 0.8f, 0.2f, 0.5f);
	else
		discard;
	return output;
}
