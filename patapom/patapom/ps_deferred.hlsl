#include "GlobalInclude.hlsli"

Texture2D gbuffer : register(t0, SPACE(PASS));
SamplerState gbufferSampler : register(s0, SPACE(PASS));

PS_OUTPUT main(VS_OUTPUT input)
{
    PS_OUTPUT output;
    output.col1 = gbuffer.Sample(gbufferSampler, input.texCoord);
    return output;
}