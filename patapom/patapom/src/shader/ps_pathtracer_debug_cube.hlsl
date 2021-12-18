#define VS_OUTPUT_LITE

#ifndef PS_COLOR_OUTPUT_COUNT
#define PS_COLOR_OUTPUT_COUNT 1
#endif

#include "ShaderInclude.hlsli"

PS_OUTPUT main(VS_OUTPUT input)
{
	PS_OUTPUT output;
	output.col0 = input.col;
	return output;
}
