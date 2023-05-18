#define VS_OUTPUT_LITE

#ifndef PS_COLOR_OUTPUT_COUNT
#define PS_COLOR_OUTPUT_COUNT 1
#endif

#include "ShaderInclude.hlsli"

// shadertoy glsl to hlsl
#define fract(x)			frac(x)
#define mix(x, y, a)		lerp(x, y, a)
#define mod(x, y)			((x) - (y) * floor((x)/(y))) // this is different from fmod(x, y)

PS_OUTPUT main(VS_OUTPUT input)
{
	PS_OUTPUT output;
	float2 fragCoord = input.pos.xy * float2(1, -1) + float2(0, uPass.mResolution.y);
	float2 uv = fragCoord / uPass.mResolution;
	output.col0 = float4(uv, 0, 1);
	return output;
}
