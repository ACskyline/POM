#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

RWStructuredBuffer<WaterSimCell> gWaterSimCellBuffer : register(u0, SPACE(PASS));
RWStructuredBuffer<WaterSimCellFace> gWaterSimCellFaceBuffer : register(u1, SPACE(PASS));
RWStructuredBuffer<WaterSimParticle> gWaterSimParticleBuffer : register(u2, SPACE(PASS));

#include "WaterSimCellResourceUtil.hlsli"
#include "WaterSimCellFaceResourceUtil.hlsli"

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
			if (ShouldApplyForce())
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
				uint3 indexXYZ = GetFloorCellMinIndex(particle.mPos - 0.5f, cellIndex); // GetFloorCellMinIndex(particle.mPos, cellIndex);

				// J = 0 means the volume becomes zero. In the real world, this will never happen
				// J < 0 means inverted material. We skip both situations by checking nan down below, otherwise log(J) is invalid
				float J = 
#if WATERSIM_DEBUG
					particle.mJ;
#else
					determinant(particle.mF);
#endif

#if !WATERSIM_STABLE_NEO_HOOKEAN
				J = max(J, 1.0e-10); // for log(J)
#endif
				float volume = J * particle.mVolume0; // mpm-course eq 153
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
				float3x3 P = ELASTIC_MU * (F_minus_F_inv_T) + ELASTIC_LAMDA * log(J) * F_inv_T;
#endif

				float Dinv = 4 / (uPass.mCellSize * uPass.mCellSize);
				float3x3 stress = (1.0f / J) * mul(P, F_T);
				float3 cellDiff = particle.mPos / uPass.mCellSize - indexXYZ; // particle.mPos / uPass.mCellSize - indexXYZ - 0.5f;
				float3 weights[3];
				weights[0] = 0.5f * SQR(1.5f - cellDiff);
				weights[1] = 0.75f - SQR(cellDiff - 1.0f);
				weights[2] = 0.5f * SQR(cellDiff - 0.5);

				for (uint dx = 0; dx < 3; dx++)
				{
					for (uint dy = 0; dy < 3; dy++)
					{
						for (uint dz = 0; dz < 3; dz++)
						{
							uint3 dIndexXYZ = indexXYZ + uint3(dx, dy, dz);
							uint dIndex = FlattenCellIndexClamp(dIndexXYZ);
							if (CellExistXYZ(dIndexXYZ))
							{
								float dWeight = weights[dx].x * weights[dy].y * weights[dz].z;
								float3 dCellDiff = (float3(dx, dy, dz) - cellDiff) * uPass.mCellSize;
								float massContrib = dWeight; // assume mass is 1
								float3x3 force = -volume * Dinv * WaterSimTimeStep * stress;
								float3x3 affine = force + particle.mC;
								float3 weightedMomentum = massContrib * (particle.mVelocity + mul(affine, dCellDiff));

#if WATERSIM_DEBUG
								if (!any(isnan(weightedMomentum)))
#endif
								{
									InterLockedAddCellMass(dIndex, massContrib);
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
