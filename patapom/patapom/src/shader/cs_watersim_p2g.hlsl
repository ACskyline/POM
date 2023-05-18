#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

#define YOUNGS_MODULUS	10000.0f
#define POISSON_RATIO	0.2f
#define ELASTIC_MU		(YOUNGS_MODULUS / (2.0f * (1.0f + POISSON_RATIO)))
#define ELASTIC_LAMDA	YOUNGS_MODULUS * POISSON_RATIO / ((1.0f + POISSON_RATIO) * (1.0f - 2.0f * POISSON_RATIO))

RWStructuredBuffer<WaterSimCell> gWaterSimCellBuffer : register(u0, SPACE(PASS));
RWStructuredBuffer<WaterSimCellFace> gWaterSimCellFaceBuffer : register(u1, SPACE(PASS));
RWStructuredBuffer<WaterSimParticle> gWaterSimParticleBuffer : register(u2, SPACE(PASS));

#include "WaterSimCellResourceUtil.hlsli"
#include "WaterSimCellFaceResourceUtil.hlsli"

float3 ApplyExplosion(float3 particlePos, float3 explosionPos, float3 explosionScale)
{
	float3 dir = normalize(particlePos - explosionPos);
	float distanceScale = 1.0f / max(1.0f, length(particlePos - explosionPos));
	return max(-WATERSIM_VELOCITY_MAX, min(WATERSIM_VELOCITY_MAX, dir * distanceScale * explosionScale * WaterSimTimeStep));
}

[numthreads(WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_X, WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_Y, WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_Z)]
void main(uint3 gGroupID : SV_GroupID, uint gGroupIndex : SV_GroupIndex)
{
	uint particleIndex = GetParticleThreadIndex(gGroupID, gGroupIndex);
	if (particleIndex < uPass.mParticleCount)
	{
		WaterSimParticle particle = gWaterSimParticleBuffer[particleIndex];
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

			if (uPass.mWaterSimMode == 0 || uPass.mWaterSimMode ==  1) // flip
			{
				uint3 minFaceFlattenedIndices, maxFaceFlattenedIndices;
				float3 minFaceWeights, maxFaceWeights;
				uint cellIndex = GetCellFaceIndicesAndWeight(particle.mPos, minFaceFlattenedIndices, maxFaceFlattenedIndices, minFaceWeights, maxFaceWeights);
				uint3 cellIndexXYZ = DeflattenCellIndexXYZ(cellIndex);
				if (gWaterSimCellBuffer[cellIndex].mType != 1.0f)
				{
					uint indexPosI = FlattenCellIndexClamp(cellIndexXYZ + uint3(1, 0, 0));
					uint indexNegI = FlattenCellIndexClamp(cellIndexXYZ - uint3(1, 0, 0));
					uint indexPosJ = FlattenCellIndexClamp(cellIndexXYZ + uint3(0, 1, 0));
					uint indexNegJ = FlattenCellIndexClamp(cellIndexXYZ - uint3(0, 1, 0));
					uint indexPosK = FlattenCellIndexClamp(cellIndexXYZ + uint3(0, 0, 1));
					uint indexNegK = FlattenCellIndexClamp(cellIndexXYZ - uint3(0, 0, 1));

					bool solidCellPosI = gWaterSimCellBuffer[indexPosI].mType == 1;
					bool solidCellNegI = gWaterSimCellBuffer[indexNegI].mType == 1;
					bool solidCellPosJ = gWaterSimCellBuffer[indexPosJ].mType == 1;
					bool solidCellNegJ = gWaterSimCellBuffer[indexNegJ].mType == 1;
					bool solidCellPosK = gWaterSimCellBuffer[indexPosK].mType == 1;
					bool solidCellNegK = gWaterSimCellBuffer[indexNegK].mType == 1;

					if (solidCellPosI || solidCellNegI)
					{
						if (solidCellPosI && solidCellNegI)
						{
							maxFaceWeights.x = 0.0f;
							minFaceWeights.x = 0.0f;
						}
						else if (solidCellPosI)
						{
							maxFaceWeights.x = 0.0f;
							minFaceWeights.x = 1.0f;
						}
						else
						{
							maxFaceWeights.x = 1.0f;
							minFaceWeights.x = 0.0f;
						}
					}

					if (solidCellPosJ || solidCellNegJ)
					{
						if (solidCellPosJ && solidCellNegJ)
						{
							maxFaceWeights.y = 0.0f;
							minFaceWeights.y = 0.0f;
						}
						else if (solidCellPosJ)
						{
							maxFaceWeights.y = 0.0f;
							minFaceWeights.y = 1.0f;
						}
						else
						{
							maxFaceWeights.y = 1.0f;
							minFaceWeights.y = 0.0f;
						}
					}

					if (solidCellPosK || solidCellNegK)
					{
						if (solidCellPosK && solidCellNegK)
						{
							maxFaceWeights.z = 0.0f;
							minFaceWeights.z = 0.0f;
						}
						else if (solidCellPosK)
						{
							maxFaceWeights.z = 0.0f;
							minFaceWeights.z = 1.0f;
						}
						else
						{
							maxFaceWeights.z = 1.0f;
							minFaceWeights.z = 0.0f;
						}
					}

					uint oldParticleCount = gWaterSimCellBuffer[cellIndex].mParticleCount;
					while (oldParticleCount < WATERSIM_PARTICLE_COUNT_PER_CELL)
					{
						uint originalParticleCount;
						InterlockedCompareExchange(gWaterSimCellBuffer[cellIndex].mParticleCount, oldParticleCount, oldParticleCount + 1, originalParticleCount);
						if (originalParticleCount == oldParticleCount)
						{
							float3 minFaceVelocity = minFaceWeights * particle.mVelocity;
							float3 maxFaceVelocity = maxFaceWeights * particle.mVelocity;

							if (!solidCellNegI)
							{
								InterLockedAddCellFaceVelocity(minFaceFlattenedIndices.x, minFaceVelocity.x);
								InterLockedAddCellFaceWeight(minFaceFlattenedIndices.x, minFaceWeights.x);
							}
							if (!solidCellNegJ)
							{
								InterLockedAddCellFaceVelocity(minFaceFlattenedIndices.y, minFaceVelocity.y);
								InterLockedAddCellFaceWeight(minFaceFlattenedIndices.y, minFaceWeights.y);
							}
							if (!solidCellNegK)
							{
								InterLockedAddCellFaceVelocity(minFaceFlattenedIndices.z, minFaceVelocity.z);
								InterLockedAddCellFaceWeight(minFaceFlattenedIndices.z, minFaceWeights.z);
							}
							break;
						}
						oldParticleCount = gWaterSimCellBuffer[cellIndex].mParticleCount;
					}

					if (oldParticleCount >= WATERSIM_PARTICLE_COUNT_PER_CELL)
					{
						particle.mAlive = 0.0f;
						gWaterSimParticleBuffer[particleIndex] = particle;
					}
				}
			}
			else if (uPass.mWaterSimMode == 2 || uPass.mWaterSimMode == 3) // mpm
			{
				// mls-mpm
				uint cellIndex;
				uint3 indexXYZ = GetFloorCellMinIndex(particle.mPos, cellIndex);

				// otherwise
				float J = determinant(particle.mF);
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
				// TODO: use volume to calculate stress to update grid momentum
				for (uint dx = 0; dx < 3; dx++)
				{
					for (uint dy = 0; dy < 3; dy++)
					{
						for (uint dz = 0; dz < 3; dz++)
						{
							uint3 dIndexXYZ = indexXYZ + uint3(dx, dy, dz) - 1;
							uint dIndex = FlattenCellIndexClamp(dIndexXYZ);
							if (CellExistXYZ(dIndexXYZ))
							{
								float dWeight = weights[dx].x * weights[dy].y * weights[dz].z;
								float3 dCellDiff = (dIndexXYZ + 0.5f) * uPass.mCellSize - particle.mPos;
								float3 Q = mul(particle.mC, dCellDiff);
								float massContrib = dWeight; // assume mass is 1
								float3 weightedMomentum = massContrib * (particle.mVelocity + Q); // TODO: adding Q makes particles go up for some reason

								// fused force/momentum update from MLS-MPM
								// see MLS-MPM paper, equation listed after eqn. 28
								weightedMomentum += mul(eq_16_term_0 * dWeight, dCellDiff);

								if (!any(isnan(weightedMomentum)))
								{
									InterLockedAddCellMass(dIndex, massContrib);
									// TODO: rearrange the sequence based on particle index to avoid potential wait when writing to the same component of the cell's velocity
									InterLockedAddCellVelocityX(dIndex, weightedMomentum.x);
									InterLockedAddCellVelocityY(dIndex, weightedMomentum.y);
									InterLockedAddCellVelocityZ(dIndex, weightedMomentum.z);
								}
							}
						} // for dz
					} // for dy
				} // for dx
			}
		} // (particle.mAlive)
	} // (particleIndex < uPass.mParticleCount)
}
