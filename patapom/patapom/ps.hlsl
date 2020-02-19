#include "GlobalInclude.hlsli"

PS_OUTPUT main(VS_OUTPUT input)
{
    PS_OUTPUT output;
    output.col1 = float4(1, 0, 0, 1);
    return output;
}