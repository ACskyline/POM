#define VS_OUTPUT_LITE
#define VS_INPUT_INSTANCE_ID
#define VS_INPUT_VERTEX_ID

#include "ShaderInclude.hlsli"

StructuredBuffer<MeshPT> gMeshBufferPT : register(t0, SPACE(PASS));
StructuredBuffer<BVH> gMeshWorldBvhBuffer : register(t1, SPACE(PASS));
StructuredBuffer<BVH> gTriangleModelBvhBuffer : register(t2, SPACE(PASS));

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	BVH bvh;
	uint meshBvhCount = uScene.mPathTracerMeshBvhCount;
	uint triangleBvhCount = uScene.mPathTracerTriangleBvhCount;
	// initialize the cube to be clipped
	float4 pos = float4(0, 0, -1, 1);
	// only render 2 instances for now, 1 for mesh bvh and 1 for triangle bvh
	bool isMeshBVH = input.instanceID == 0;
	bool isTriangleBVH = !isMeshBVH;
	uint meshBvhIndex = uScene.mPathTracerDebugMeshBvhIndex;
	uint triangleBvhIndex = uScene.mPathTracerDebugTriangleBvhIndex;
	if (meshBvhIndex >= uScene.mPathTracerMeshBvhCount)
		meshBvhIndex = 0;
	if (triangleBvhIndex >= uScene.mPathTracerTriangleBvhCount)
		triangleBvhIndex = 0;
	if (isMeshBVH)
		bvh = gMeshWorldBvhBuffer[meshBvhIndex];
	else if (isTriangleBVH)
		bvh = gTriangleModelBvhBuffer[triangleBvhIndex];
	float4x4 model = isMeshBVH ? float4x4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0, 0.0, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f) : gMeshBufferPT[bvh.mMeshIndex].mModel;
	float3 translate = (bvh.mAABB.mMax + bvh.mAABB.mMin) / 2.0f;
	float3 scale = abs(bvh.mAABB.mMax - bvh.mAABB.mMin);
	if (scale.x < 0.1f)
		scale.x = 0.3f;
	else if (scale.y < 0.1f)
		scale.y = 0.3f;
	else if (scale.z < 0.1f)
		scale.z = 0.3f;
	float4x4 local = float4x4(scale.x, 0.0f, 0.0f, translate.x,
							  0.0f, scale.y, 0.0f, translate.y,
							  0.0f, 0.0f, scale.z, translate.z,
							  0.0f, 0.0f, 0.0f, 1.0f);
	if ((uScene.mPathTracerDebugMeshBVH && isMeshBVH) || (uScene.mPathTracerDebugTriangleBVH && isTriangleBVH))
		pos = mul(uPass.mProj, mul(uPass.mView, mul(model, mul(local, float4(input.pos, 1.0f)))));
	output.col = isMeshBVH ? float4(0.8f, 0.0f, 0.0f, 0.5f) : (isTriangleBVH ? float4(0.0f, 0.8f, 0.0f, 0.5f) : 1.0f.xxxx);
	output.pos = pos;
	return output;
}
