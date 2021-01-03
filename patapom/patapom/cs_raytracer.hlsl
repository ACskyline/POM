#include "GlobalInclude.hlsl"

StructuredBuffer<Triangle> triangleBuffer : register(t0, SPACE(PASS));
RWTexture2D<float4> ptBackbuffer : register(u0, SPACE(PASS));

[numthreads(PATH_TRACER_THREAD_COUNT_X, PATH_TRACER_THREAD_COUNT_Y, PATH_TRACER_THREAD_COUNT_Z)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	uint2 pixelPos = DTid.xy;
	uint2 textureSize = uint2(PATH_TRACER_BACKBUFFER_WIDTH, PATH_TRACER_BACKBUFFER_HEIGHT);
	if (pixelPos.x < textureSize.x && pixelPos.y < textureSize.y);
	{
		ptBackbuffer[pixelPos] = float4(float2(pixelPos)/float2(textureSize), 0.0f, 1.0f);
	}
}
