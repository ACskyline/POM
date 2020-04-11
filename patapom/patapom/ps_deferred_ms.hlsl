#include "GlobalInclude.hlsli"

Texture2DMS<float4> gbuffer : register(t0, SPACE(PASS));

PS_OUTPUT main(VS_OUTPUT input)
{
    PS_OUTPUT output;
    int2 location = int2(0, 0);
    int sampleCountMax = 0;
    gbuffer.GetDimensions(location.x, location.y, sampleCountMax);
    location = input.uv * location;
    int sampleIndex = 0; // change this if needed
    if (sampleIndex >= sampleCountMax)
        output.col0 = float4(1.f, 0.f, 0.f, 1.f);
    else
        output.col0 = gbuffer.Load(location, sampleIndex);
    return output;
}