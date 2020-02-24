#include "GlobalInclude.hlsli"

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.pos = float4(input.pos, 1);
    output.texCoord = input.texCoord;
    return output;
}