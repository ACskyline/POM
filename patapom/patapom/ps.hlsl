#include "GlobalInclude.hlsli"

Texture2D foam : register(t0, SPACE(PASS));
SamplerState foamSampler : register(s0, SPACE(PASS));

PS_OUTPUT main(VS_OUTPUT input)
{
    PS_OUTPUT output;
    output.col1 = foam.Sample(foamSampler, input.texCoord);
    return output;
}