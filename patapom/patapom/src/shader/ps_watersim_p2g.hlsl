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
			if (uPass.mApplyForce)
			{
				if (uPass.mApplyExplosion)
				{
					particle.mVelocity += ApplyExplosion(particle.mPos, uPass.mExplosionPos, uPass.mExplosionForceScale);
				}
			}

			uint cellIndex;
			uint3 indexXYZ = GetFloorCellMinIndex(particle.mPos, cellIndex);
			uint3 dIndexXYZ = indexXYZ + input.dxyz - 1;

			// J = 0 means the volume becomes zero. In the real world, this will never happen
			// J < 0 means inverted material. We skip both situations in VS, otherwise log(J) is invalid
			float J = particle.mJ; // determinant(particle.mF);
			float volume = J * particle.mVolume0;
			// useful matrices for Neo-Hookean model
			float3x3 F_T = transpose(particle.mF);
			float3x3 F_inv_T = Inverse(F_T);
			float3x3 F_minus_F_inv_T = particle.mF - F_inv_T;

			// MPM course equation 48
			float3x3 P_term_0 = ELASTIC_MU * (F_minus_F_inv_T);
			float3x3 P_term_1 = ELASTIC_LAMDA * log(J) * F_inv_T;
			float3x3 P = P_term_0 + P_term_1;

			// cauchy_stress = (1 / det(F)) * P * F_T
			// equation 38, MPM course
			float3x3 stress = (1.0f / J) * mul(P, F_T);

			// (M_p)^-1 = 4, see APIC paper and MPM course page 42
			// this term is used in MLS-MPM paper eq. 16. with quadratic weights, Mp = (1/4) * (delta_x)^2.
			// in this simulation, delta_x = 1, because i scale the rendering of the domain rather than the domain itself.
			// we multiply by dt as part of the process of fusing the momentum and force update for MLS-MPM
			float3x3 eq_16_term_0 = -volume * 4 * stress * WaterSimTimeStep;

			float3 cellDiff = particle.mPos / uPass.mCellSize - indexXYZ - 0.5f;
			float3 weights[3];
			weights[0] = 0.5f * pow(0.5f - cellDiff, 2.0f);
			weights[1] = 0.75f - pow(cellDiff, 2.0f); // is this correct ?
			weights[2] = 0.5f * pow(0.5f + cellDiff, 2.0f);

			// dxyz
			float dWeight = weights[input.dxyz.x].x * weights[input.dxyz.y].y * weights[input.dxyz.z].z;
			float3 dCellDiff = (dIndexXYZ + 0.5f) * uPass.mCellSize - particle.mPos;
			float3 Q = mul(particle.mC, dCellDiff);
			float massContrib = dWeight; // assume mass is 1
			float3 weightedMomentum = massContrib * (particle.mVelocity + Q); // TODO: adding Q makes particles go up for some reason

			// fused force/momentum update from MLS-MPM
			// see MLS-MPM paper, equation listed after eqn. 28
			weightedMomentum += mul(eq_16_term_0 * dWeight, dCellDiff);

			if (!any(isnan(weightedMomentum)))
			{
				output.col0 = float4(weightedMomentum, massContrib);
			}
		}
	}
	return output;
}
