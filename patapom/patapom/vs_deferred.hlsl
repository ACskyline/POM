#define VS_OUTPUT_LITE

#include "GlobalInclude.hlsl"

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.pos = float4(input.pos, 1.0f);
    output.uv = input.uv;
    return output;
}
