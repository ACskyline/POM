#define CUSTOM_PASS_UNIFORM

#include "GlobalInclude.hlsl"

cbuffer PassUniformBuffer : register(b0, SPACE(PASS))
{
	PassUniformPathTracerBuildBVH uPass;
};

[numthreads(PATH_TRACER_THREAD_COUNT_X, PATH_TRACER_THREAD_COUNT_Y, 1)]
void main(uint3 gDispatchThreadID : SV_DispatchThreadID)
{
	// TODO:
}
