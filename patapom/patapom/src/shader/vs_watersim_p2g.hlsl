#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

StructuredBuffer<WaterSimParticle> gWaterSimParticleBuffer : register(t0, SPACE(PASS));

VS_OUTPUT_P2G main(uint instanceID : SV_InstanceID)
{
	VS_OUTPUT_P2G output = (VS_OUTPUT_P2G)0;
	output.pos = float4(-2.0f, -2.0f, -2.0f, 1.0f); // clipped by default
	uint particleIndex = instanceID / 27;
	if (particleIndex < uPass.mParticleCount)
	{
		WaterSimParticle particle = gWaterSimParticleBuffer[particleIndex];
		if (particle.mAlive && particle.mJ > 0.0f)
		{
			uint dxyz = instanceID % 27;
			uint dx = dxyz / 9;
			uint dy = (dxyz - 9 * dx) / 3;
			uint dz = (dxyz - 9 * dx - 3 * dy);
			uint cellIndex;
			uint3 indexXYZ = GetFloorCellMinIndex(particle.mPos, cellIndex);
			uint3 dIndexXYZ = indexXYZ + uint3(dx, dy, dz) - 1;
			uint dIndex = FlattenCellIndexClamp(dIndexXYZ);
			if (CellExistXYZ(dIndexXYZ))
			{
				output.pos = CellIndexToPixelCoords(dIndex);
				output.cellIndex = dIndex;
				output.particleIndex = particleIndex;
				output.dxyz = uint3(dx, dy, dz);
			}
		}
	}
	return output;
}
