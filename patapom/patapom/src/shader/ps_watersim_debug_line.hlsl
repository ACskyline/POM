#define VS_OUTPUT_LITE
#define PS_COLOR_OUTPUT_COUNT 1

#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

PS_OUTPUT main(VS_OUTPUT input)
{
	PS_OUTPUT output;
	output.col0 = input.col;
	return output;
}
