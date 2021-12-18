#define VS_OUTPUT_LITE

#ifndef PS_COLOR_OUTPUT_COUNT
#define PS_COLOR_OUTPUT_COUNT 1
#endif

#include "ShaderInclude.hlsli"

#define CURSOR_RADIUS 10.0f

PS_OUTPUT main(VS_OUTPUT input)
{
	PS_OUTPUT output;
	float2 mouseScreenPos = float2(uScene.mPathTracerDebugPixelX, uScene.mPathTracerDebugPixelY);
	float2 screenPos = input.pos.xy;
	float dist = length(mouseScreenPos - screenPos);
	if (dist < CURSOR_RADIUS) // 10 pixel radius
	{
		output.col0 = float4(1.0f, 0, 1.0f, 1.0f - dist / CURSOR_RADIUS);
	}
	else
	{
		discard; // make sure we don't overwrite the alpha of unwanted pixels (from debug line pass)
	}
	return output;
}
