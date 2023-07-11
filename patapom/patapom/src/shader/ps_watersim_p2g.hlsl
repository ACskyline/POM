#define CUSTOM_PASS_UNIFORM
#define PS_COLOR_OUTPUT_COUNT 1

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

StructuredBuffer<WaterSimParticle> gWaterSimParticleBuffer : register(t0, SPACE(PASS));

PS_OUTPUT main(VS_OUTPUT_P2G input)
{
	PS_OUTPUT output = (PS_OUTPUT)0;
	uint particleIndex = input.particleIndex;
	// already checked in VS but just in case
	if (particleIndex < uPass.mParticleCount)
	{
		WaterSimParticle particle = gWaterSimParticleBuffer[input.particleIndex];
		if (particle.mAlive)
		{
			// apply particle force
			if (ShouldApplyForce())
			{
				if (uPass.mApplyExplosion)
				{
					particle.mVelocity += ApplyExplosion(particle.mPos, uPass.mExplosionPos, uPass.mExplosionForceScale);
				}
			}

			uint cellIndex;
			uint3 indexXYZ = GetFloorCellMinIndex(particle.mPos - 0.5f.xxx, cellIndex);
			uint3 dIndexXYZ = indexXYZ + input.dxyz;

			// J = 0 means the volume becomes zero. In the real world, this will never happen
			// J < 0 means inverted material. We skip both situations in VS, otherwise log(J) is invalid
			float J = 
#if WATERSIM_DEBUG
				particle.mJ;
#else
				determinant(particle.mF);
#endif

#if !WATERSIM_STABLE_NEO_HOOKEAN
			J = max(J, 1.0e-10); // for log(J)
#endif
			float volume = J * particle.mVolume0;  // mpm-course eq 153
			float3x3 F_T = transpose(particle.mF);

#if WATERSIM_STABLE_NEO_HOOKEAN
			float3x3 F_T_F = mul(F_T, particle.mF);
			float Ic = F_T_F[0][0] + F_T_F[1][1] + F_T_F[2][2];
			float alpha = 1.0f + 0.75f * ELASTIC_MU / ELASTIC_LAMDA;
			float3 column0 = cross(F_T[1], F_T[2]);
			float3 column1 = cross(F_T[2], F_T[0]);
			float3 column2 = cross(F_T[0], F_T[1]);
			float3x3 dJdF = float3x3(float3(column0.x, column1.x, column2.x),
				float3(column0.y, column1.y, column2.y),
				float3(column0.z, column1.z, column2.z));
			float3x3 P = ELASTIC_MU * (1.0f - 1.0f / (Ic + 1.0f)) * particle.mF + ELASTIC_LAMDA * (J - alpha) * dJdF;
#else
			float3x3 F_inv_T = Inverse(F_T);
			float3x3 F_minus_F_inv_T = particle.mF - F_inv_T;
			// MPM course equation 48
			float3x3 P = ELASTIC_MU * (F_minus_F_inv_T)+ELASTIC_LAMDA * log(J) * F_inv_T;
#endif

			float Dinv = 4.0f / (uPass.mCellSize * uPass.mCellSize);
			float3x3 stress = (1.0f / J) * mul(P, F_T);
			float3 cellDiff = particle.mPos / uPass.mCellSize - indexXYZ;
			float3 weights[3];
			weights[0] = 0.5f * SQR(1.5f - cellDiff);
			weights[1] = 0.75f - SQR(cellDiff - 1.0f);
			weights[2] = 0.5f * SQR(cellDiff - 0.5);

			// dxyz
			float dWeight = weights[input.dxyz.x].x * weights[input.dxyz.y].y * weights[input.dxyz.z].z;
			float3 dCellDiff = (float3(input.dxyz) - cellDiff) * uPass.mCellSize;
			float massContrib = dWeight; // assume mass is 1
			float3x3 force = -volume * Dinv * WaterSimTimeStep * stress;
			float3x3 affine = force + particle.mC;
			float3 weightedMomentum = massContrib * (particle.mVelocity + mul(affine, dCellDiff));

#if WATERSIM_DEBUG
			if (!any(isnan(weightedMomentum)))
#endif
			{
				output.col0 = float4(weightedMomentum, massContrib);
			}
		}
	}
	return output;
}
